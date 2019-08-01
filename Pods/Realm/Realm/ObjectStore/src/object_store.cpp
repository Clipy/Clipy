////////////////////////////////////////////////////////////////////////////
//
// Copyright 2015 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#include "object_store.hpp"

#include "feature_checks.hpp"
#include "object_schema.hpp"
#include "schema.hpp"
#include "shared_realm.hpp"
#include "sync/partial_sync.hpp"

#include <realm/descriptor.hpp>
#include <realm/group.hpp>
#include <realm/table.hpp>
#include <realm/table_view.hpp>
#include <realm/util/assert.hpp>

#if REALM_ENABLE_SYNC
#include <realm/sync/object.hpp>
#include <realm/sync/permissions.hpp>
#include <realm/sync/instruction_replication.hpp>
#endif // REALM_ENABLE_SYNC

#include <string.h>

using namespace realm;

constexpr uint64_t ObjectStore::NotVersioned;

namespace {
const char * const c_metadataTableName = "metadata";
const char * const c_versionColumnName = "version";
const size_t c_versionColumnIndex = 0;

const char * const c_primaryKeyTableName = "pk";
const char * const c_primaryKeyObjectClassColumnName = "pk_table";
const size_t c_primaryKeyObjectClassColumnIndex =  0;
const char * const c_primaryKeyPropertyNameColumnName = "pk_property";
const size_t c_primaryKeyPropertyNameColumnIndex =  1;

const size_t c_zeroRowIndex = 0;

const char c_object_table_prefix[] = "class_";

void create_metadata_tables(Group& group) {
    // The tables 'pk' and 'metadata' are treated specially by Sync. The 'pk' table
    // is populated by `sync::create_table` and friends, while the 'metadata' table
    // is simply ignored.
    TableRef pk_table = group.get_or_add_table(c_primaryKeyTableName);
    TableRef metadata_table = group.get_or_add_table(c_metadataTableName);

    if (metadata_table->get_column_count() == 0) {
        metadata_table->insert_column(c_versionColumnIndex, type_Int, c_versionColumnName);
        metadata_table->add_empty_row();
        // set initial version
        metadata_table->set_int(c_versionColumnIndex, c_zeroRowIndex, ObjectStore::NotVersioned);
    }

    if (pk_table->get_column_count() == 0) {
        pk_table->insert_column(c_primaryKeyObjectClassColumnIndex, type_String, c_primaryKeyObjectClassColumnName);
        pk_table->insert_column(c_primaryKeyPropertyNameColumnIndex, type_String, c_primaryKeyPropertyNameColumnName);
    }
    pk_table->add_search_index(c_primaryKeyObjectClassColumnIndex);
}

void set_schema_version(Group& group, uint64_t version) {
    TableRef table = group.get_table(c_metadataTableName);
    table->set_int(c_versionColumnIndex, c_zeroRowIndex, version);
}

template<typename Group>
auto table_for_object_schema(Group& group, ObjectSchema const& object_schema)
{
    return ObjectStore::table_for_object_type(group, object_schema.name);
}

DataType to_core_type(PropertyType type)
{
    REALM_ASSERT(type != PropertyType::Object); // Link columns have to be handled differently
    REALM_ASSERT(type != PropertyType::Any); // Mixed columns can't be created
    switch (type & ~PropertyType::Flags) {
        case PropertyType::Int:    return type_Int;
        case PropertyType::Bool:   return type_Bool;
        case PropertyType::Float:  return type_Float;
        case PropertyType::Double: return type_Double;
        case PropertyType::String: return type_String;
        case PropertyType::Date:   return type_Timestamp;
        case PropertyType::Data:   return type_Binary;
        default: REALM_COMPILER_HINT_UNREACHABLE();
    }
}

void insert_column(Group& group, Table& table, Property const& property, size_t col_ndx)
{
    // Cannot directly insert a LinkingObjects column (a computed property).
    // LinkingObjects must be an artifact of an existing link column.
    REALM_ASSERT(property.type != PropertyType::LinkingObjects);

    if (property.type == PropertyType::Object) {
        auto target_name = ObjectStore::table_name_for_object_type(property.object_type);
        TableRef link_table = group.get_table(target_name);
        REALM_ASSERT(link_table);
        table.insert_column_link(col_ndx, is_array(property.type) ? type_LinkList : type_Link,
                                 property.name, *link_table);
    }
    else if (is_array(property.type)) {
        DescriptorRef desc;
        table.insert_column(col_ndx, type_Table, property.name, &desc);
        desc->add_column(to_core_type(property.type & ~PropertyType::Flags), ObjectStore::ArrayColumnName,
                         nullptr, is_nullable(property.type));
    }
    else {
        table.insert_column(col_ndx, to_core_type(property.type), property.name, is_nullable(property.type));
        if (property.requires_index())
            table.add_search_index(col_ndx);
    }
}

void add_column(Group& group, Table& table, Property const& property)
{
    insert_column(group, table, property, table.get_column_count());
}

void replace_column(Group& group, Table& table, Property const& old_property, Property const& new_property)
{
    insert_column(group, table, new_property, old_property.table_column);
    table.remove_column(old_property.table_column + 1);
}

TableRef create_table(Group& group, ObjectSchema const& object_schema)
{
    auto name = ObjectStore::table_name_for_object_type(object_schema.name);

    TableRef table;
#if REALM_ENABLE_SYNC
    if (auto* pk_property = object_schema.primary_key_property()) {
        table = sync::create_table_with_primary_key(group, name, to_core_type(pk_property->type),
                                                    pk_property->name, is_nullable(pk_property->type));
    }
    else {
        table = sync::create_table(group, name);
    }
#else
    table = group.get_or_add_table(name);
    ObjectStore::set_primary_key_for_object(group, object_schema.name, object_schema.primary_key);
#endif // REALM_ENABLE_SYNC

    return table;
}

void add_initial_columns(Group& group, ObjectSchema const& object_schema)
{
    auto name = ObjectStore::table_name_for_object_type(object_schema.name);
    TableRef table = group.get_table(name);

    for (auto const& prop : object_schema.persisted_properties) {
#if REALM_ENABLE_SYNC
        // The sync::create_table* functions create the PK column for us.
        if (prop.is_primary)
            continue;
#endif // REALM_ENABLE_SYNC
        add_column(group, *table, prop);
    }
}

void copy_property_values(Property const& prop, Table& table)
{
    auto copy_property_values = [&](auto getter, auto setter) {
        for (size_t i = 0, count = table.size(); i < count; i++) {
            bool is_default = false;
            (table.*setter)(prop.table_column, i, (table.*getter)(prop.table_column + 1, i),
                            is_default);
        }
    };

    switch (prop.type & ~PropertyType::Flags) {
        case PropertyType::Int:
            copy_property_values(&Table::get_int, &Table::set_int);
            break;
        case PropertyType::Bool:
            copy_property_values(&Table::get_bool, &Table::set_bool);
            break;
        case PropertyType::Float:
            copy_property_values(&Table::get_float, &Table::set_float);
            break;
        case PropertyType::Double:
            copy_property_values(&Table::get_double, &Table::set_double);
            break;
        case PropertyType::String:
            copy_property_values(&Table::get_string, &Table::set_string);
            break;
        case PropertyType::Data:
            copy_property_values(&Table::get_binary, &Table::set_binary);
            break;
        case PropertyType::Date:
            copy_property_values(&Table::get_timestamp, &Table::set_timestamp);
            break;
        default:
            break;
    }
}

void make_property_optional(Group& group, Table& table, Property property)
{
    property.type |= PropertyType::Nullable;
    insert_column(group, table, property, property.table_column);
    copy_property_values(property, table);
    table.remove_column(property.table_column + 1);
}

void make_property_required(Group& group, Table& table, Property property)
{
    property.type &= ~PropertyType::Nullable;
    insert_column(group, table, property, property.table_column);
    table.remove_column(property.table_column + 1);
}

void validate_primary_column_uniqueness(Group const& group, StringData object_type, StringData primary_property)
{
    auto table = ObjectStore::table_for_object_type(group, object_type);
    if (table->get_distinct_view(table->get_column_index(primary_property)).size() != table->size()) {
        throw DuplicatePrimaryKeyValueException(object_type, primary_property);
    }
}

void validate_primary_column_uniqueness(Group const& group)
{
    auto pk_table = group.get_table(c_primaryKeyTableName);
    for (size_t i = 0, count = pk_table->size(); i < count; ++i) {
        validate_primary_column_uniqueness(group,
                                           pk_table->get_string(c_primaryKeyObjectClassColumnIndex, i),
                                           pk_table->get_string(c_primaryKeyPropertyNameColumnIndex, i));
    }
}
} // anonymous namespace

void ObjectStore::set_schema_version(Group& group, uint64_t version) {
    ::create_metadata_tables(group);
    ::set_schema_version(group, version);
}

uint64_t ObjectStore::get_schema_version(Group const& group) {
    ConstTableRef table = group.get_table(c_metadataTableName);
    if (!table || table->get_column_count() == 0) {
        return ObjectStore::NotVersioned;
    }
    return table->get_int(c_versionColumnIndex, c_zeroRowIndex);
}

StringData ObjectStore::get_primary_key_for_object(Group const& group, StringData object_type) {
    ConstTableRef table = group.get_table(c_primaryKeyTableName);
    if (!table) {
        return "";
    }
    size_t row = table->find_first_string(c_primaryKeyObjectClassColumnIndex, object_type);
    if (row == not_found) {
        return "";
    }
    return table->get_string(c_primaryKeyPropertyNameColumnIndex, row);
}

void ObjectStore::set_primary_key_for_object(Group& group, StringData object_type, StringData primary_key) {
    TableRef table = group.get_table(c_primaryKeyTableName);

    size_t row = table->find_first_string(c_primaryKeyObjectClassColumnIndex, object_type);

#if REALM_ENABLE_SYNC
    // sync::create_table* functions should have already updated the pk table.
    if (sync::has_object_ids(group)) {
        if (primary_key.size() == 0)
            REALM_ASSERT(row == not_found);
        else {
             REALM_ASSERT(row != not_found);
             REALM_ASSERT(table->get_string(c_primaryKeyPropertyNameColumnIndex, row) == primary_key);
        }
        return;
    }
#endif // REALM_ENABLE_SYNC

    if (row == not_found && primary_key.size()) {
        row = table->add_empty_row();
        table->set_string_unique(c_primaryKeyObjectClassColumnIndex, row, object_type);
        table->set_string(c_primaryKeyPropertyNameColumnIndex, row, primary_key);
        return;
    }
    // set if changing, or remove if setting to nil
    if (primary_key.size() == 0) {
        if (row != not_found) {
            table->move_last_over(row);
        }
    }
    else {
        table->set_string(c_primaryKeyPropertyNameColumnIndex, row, primary_key);
    }
}

StringData ObjectStore::object_type_for_table_name(StringData table_name) {
    if (table_name.begins_with(c_object_table_prefix)) {
        return table_name.substr(sizeof(c_object_table_prefix) - 1);
    }
    return StringData();
}

std::string ObjectStore::table_name_for_object_type(StringData object_type) {
    return std::string(c_object_table_prefix) + std::string(object_type);
}

TableRef ObjectStore::table_for_object_type(Group& group, StringData object_type) {
    auto name = table_name_for_object_type(object_type);
    return group.get_table(name);
}

ConstTableRef ObjectStore::table_for_object_type(Group const& group, StringData object_type) {
    auto name = table_name_for_object_type(object_type);
    return group.get_table(name);
}

namespace {
struct SchemaDifferenceExplainer {
    std::vector<ObjectSchemaValidationException> errors;

    void operator()(schema_change::AddTable op)
    {
        errors.emplace_back("Class '%1' has been added.", op.object->name);
    }

    void operator()(schema_change::RemoveTable)
    {
        // We never do anything for RemoveTable
    }

    void operator()(schema_change::AddInitialProperties)
    {
        // Nothing. Always preceded by AddTable.
    }

    void operator()(schema_change::AddProperty op)
    {
        errors.emplace_back("Property '%1.%2' has been added.", op.object->name, op.property->name);
    }

    void operator()(schema_change::RemoveProperty op)
    {
        errors.emplace_back("Property '%1.%2' has been removed.", op.object->name, op.property->name);
    }

    void operator()(schema_change::ChangePropertyType op)
    {
        errors.emplace_back("Property '%1.%2' has been changed from '%3' to '%4'.",
                            op.object->name, op.new_property->name,
                            op.old_property->type_string(),
                            op.new_property->type_string());
    }

    void operator()(schema_change::MakePropertyNullable op)
    {
        errors.emplace_back("Property '%1.%2' has been made optional.", op.object->name, op.property->name);
    }

    void operator()(schema_change::MakePropertyRequired op)
    {
        errors.emplace_back("Property '%1.%2' has been made required.", op.object->name, op.property->name);
    }

    void operator()(schema_change::ChangePrimaryKey op)
    {
        if (op.property && !op.object->primary_key.empty()) {
            errors.emplace_back("Primary Key for class '%1' has changed from '%2' to '%3'.",
                                op.object->name, op.object->primary_key, op.property->name);
        }
        else if (op.property) {
            errors.emplace_back("Primary Key for class '%1' has been added.", op.object->name);
        }
        else {
            errors.emplace_back("Primary Key for class '%1' has been removed.", op.object->name);
        }
    }

    void operator()(schema_change::AddIndex op)
    {
        errors.emplace_back("Property '%1.%2' has been made indexed.", op.object->name, op.property->name);
    }

    void operator()(schema_change::RemoveIndex op)
    {
        errors.emplace_back("Property '%1.%2' has been made unindexed.", op.object->name, op.property->name);
    }
};

class TableHelper {
public:
    TableHelper(Group& g) : m_group(g) { }

    Table& operator()(const ObjectSchema* object_schema)
    {
        if (object_schema != m_current_object_schema) {
            m_current_table = table_for_object_schema(m_group, *object_schema);
            m_current_object_schema = object_schema;
        }
        REALM_ASSERT(m_current_table);
        return *m_current_table;
    }

private:
    Group& m_group;
    const ObjectSchema* m_current_object_schema = nullptr;
    TableRef m_current_table;
};

template<typename ErrorType, typename Verifier>
void verify_no_errors(Verifier&& verifier, std::vector<SchemaChange> const& changes)
{
    for (auto& change : changes) {
        change.visit(verifier);
    }

    if (!verifier.errors.empty()) {
        throw ErrorType(verifier.errors);
    }
}
} // anonymous namespace

bool ObjectStore::needs_migration(std::vector<SchemaChange> const& changes)
{
    using namespace schema_change;
    struct Visitor {
        bool operator()(AddIndex) { return false; }
        bool operator()(AddInitialProperties) { return false; }
        bool operator()(AddProperty) { return true; }
        bool operator()(AddTable) { return false; }
        bool operator()(RemoveTable) { return false; }
        bool operator()(ChangePrimaryKey) { return true; }
        bool operator()(ChangePropertyType) { return true; }
        bool operator()(MakePropertyNullable) { return true; }
        bool operator()(MakePropertyRequired) { return true; }
        bool operator()(RemoveIndex) { return false; }
        bool operator()(RemoveProperty) { return true; }
    };

    return std::any_of(begin(changes), end(changes),
                       [](auto&& change) { return change.visit(Visitor()); });
}

void ObjectStore::verify_no_changes_required(std::vector<SchemaChange> const& changes)
{
    verify_no_errors<SchemaMismatchException>(SchemaDifferenceExplainer(), changes);
}

void ObjectStore::verify_no_migration_required(std::vector<SchemaChange> const& changes)
{
    using namespace schema_change;
    struct Verifier : SchemaDifferenceExplainer {
        using SchemaDifferenceExplainer::operator();

        // Adding a table or adding/removing indexes can be done automatically.
        // All other changes require migrations.
        void operator()(AddTable) { }
        void operator()(AddInitialProperties) { }
        void operator()(AddIndex) { }
        void operator()(RemoveIndex) { }
    } verifier;
    verify_no_errors<SchemaMismatchException>(verifier, changes);
}

bool ObjectStore::verify_valid_additive_changes(std::vector<SchemaChange> const& changes, bool update_indexes)
{
    using namespace schema_change;
    struct Verifier : SchemaDifferenceExplainer {
        using SchemaDifferenceExplainer::operator();

        bool index_changes = false;
        bool other_changes = false;

        // Additive mode allows adding things, extra columns, and adding/removing indexes
        void operator()(AddTable) { other_changes = true; }
        void operator()(AddInitialProperties) { other_changes = true; }
        void operator()(AddProperty) { other_changes = true; }
        void operator()(RemoveProperty) { }
        void operator()(AddIndex) { index_changes = true; }
        void operator()(RemoveIndex) { index_changes = true; }
    } verifier;
    verify_no_errors<InvalidSchemaChangeException>(verifier, changes);
    return verifier.other_changes || (verifier.index_changes && update_indexes);
}

void ObjectStore::verify_valid_external_changes(std::vector<SchemaChange> const& changes)
{
    using namespace schema_change;
    struct Verifier : SchemaDifferenceExplainer {
        using SchemaDifferenceExplainer::operator();

        // Adding new things is fine
        void operator()(AddTable) { }
        void operator()(AddInitialProperties) { }
        void operator()(AddProperty) { }
        void operator()(AddIndex) { }
        void operator()(RemoveIndex) { }

        // Deleting tables is not okay
        void operator()(RemoveTable op) {
            errors.emplace_back("Class '%1' has been removed.", op.object->name);
        }
    } verifier;
    verify_no_errors<InvalidExternalSchemaChangeException>(verifier, changes);
}

void ObjectStore::verify_compatible_for_immutable_and_readonly(std::vector<SchemaChange> const& changes)
{
    using namespace schema_change;
    struct Verifier : SchemaDifferenceExplainer {
        using SchemaDifferenceExplainer::operator();

        void operator()(AddTable) { }
        void operator()(AddInitialProperties) { }
        void operator()(RemoveProperty) { }
        void operator()(AddIndex) { }
        void operator()(RemoveIndex) { }
    } verifier;
    verify_no_errors<InvalidSchemaChangeException>(verifier, changes);
}

static void apply_non_migration_changes(Group& group, std::vector<SchemaChange> const& changes)
{
    using namespace schema_change;
    struct Applier : SchemaDifferenceExplainer {
        Applier(Group& group) : group{group}, table{group} { }
        Group& group;
        TableHelper table;

        // Produce an exception listing the unsupported schema changes for
        // everything but the explicitly supported ones
        using SchemaDifferenceExplainer::operator();

        void operator()(AddTable op) { create_table(group, *op.object); }
        void operator()(AddInitialProperties op) { add_initial_columns(group, *op.object); }
        void operator()(AddIndex op) { table(op.object).add_search_index(op.property->table_column); }
        void operator()(RemoveIndex op) { table(op.object).remove_search_index(op.property->table_column); }
    } applier{group};
    verify_no_errors<SchemaMismatchException>(applier, changes);
}

static void create_initial_tables(Group& group, std::vector<SchemaChange> const& changes)
{
    using namespace schema_change;
    struct Applier {
        Applier(Group& group) : group{group}, table{group} { }
        Group& group;
        TableHelper table;

        void operator()(AddTable op) { create_table(group, *op.object); }
        void operator()(RemoveTable) { }
        void operator()(AddInitialProperties op) { add_initial_columns(group, *op.object); }

        // Note that in normal operation none of these will be hit, as if we're
        // creating the initial tables there shouldn't be anything to update.
        // Implementing these makes us better able to handle weird
        // not-quite-correct files produced by other things and has no obvious
        // downside.
        void operator()(AddProperty op) { add_column(group, table(op.object), *op.property); }
        void operator()(RemoveProperty op) { table(op.object).remove_column(op.property->table_column); }
        void operator()(MakePropertyNullable op) { make_property_optional(group, table(op.object), *op.property); }
        void operator()(MakePropertyRequired op) { make_property_required(group, table(op.object), *op.property); }
        void operator()(ChangePrimaryKey op) { ObjectStore::set_primary_key_for_object(group, op.object->name, op.property ? StringData{op.property->name} : ""); }
        void operator()(AddIndex op) { table(op.object).add_search_index(op.property->table_column); }
        void operator()(RemoveIndex op) { table(op.object).remove_search_index(op.property->table_column); }

        void operator()(ChangePropertyType op)
        {
            replace_column(group, table(op.object), *op.old_property, *op.new_property);
        }
    } applier{group};

    for (auto& change : changes) {
        change.visit(applier);
    }
}

void ObjectStore::apply_additive_changes(Group& group, std::vector<SchemaChange> const& changes, bool update_indexes)
{
    using namespace schema_change;
    struct Applier {
        Applier(Group& group, bool update_indexes)
        : group{group}, table{group}, update_indexes{update_indexes} { }
        Group& group;
        TableHelper table;
        bool update_indexes;

        void operator()(AddTable op) { create_table(group, *op.object); }
        void operator()(RemoveTable) { }
        void operator()(AddInitialProperties op) { add_initial_columns(group, *op.object); }
        void operator()(AddProperty op) { add_column(group, table(op.object), *op.property); }
        void operator()(AddIndex op) { if (update_indexes) table(op.object).add_search_index(op.property->table_column); }
        void operator()(RemoveIndex op) { if (update_indexes) table(op.object).remove_search_index(op.property->table_column); }
        void operator()(RemoveProperty) { }

        // No need for errors for these, as we've already verified that they aren't present
        void operator()(ChangePrimaryKey) { }
        void operator()(ChangePropertyType) { }
        void operator()(MakePropertyNullable) { }
        void operator()(MakePropertyRequired) { }
    } applier{group, update_indexes};

    for (auto& change : changes) {
        change.visit(applier);
    }
}

static void apply_pre_migration_changes(Group& group, std::vector<SchemaChange> const& changes)
{
    using namespace schema_change;
    struct Applier {
        Applier(Group& group) : group{group}, table{group} { }
        Group& group;
        TableHelper table;

        void operator()(AddTable op) { create_table(group, *op.object); }
        void operator()(RemoveTable) { }
        void operator()(AddInitialProperties op) { add_initial_columns(group, *op.object); }
        void operator()(AddProperty op) { add_column(group, table(op.object), *op.property); }
        void operator()(RemoveProperty) { /* delayed until after the migration */ }
        void operator()(ChangePropertyType op) { replace_column(group, table(op.object), *op.old_property, *op.new_property); }
        void operator()(MakePropertyNullable op) { make_property_optional(group, table(op.object), *op.property); }
        void operator()(MakePropertyRequired op) { make_property_required(group, table(op.object), *op.property); }
        void operator()(ChangePrimaryKey op) { ObjectStore::set_primary_key_for_object(group, op.object->name.c_str(), op.property ? op.property->name.c_str() : ""); }
        void operator()(AddIndex op) { table(op.object).add_search_index(op.property->table_column); }
        void operator()(RemoveIndex op) { table(op.object).remove_search_index(op.property->table_column); }
    } applier{group};

    for (auto& change : changes) {
        change.visit(applier);
    }
}

enum class DidRereadSchema { Yes, No };

static void apply_post_migration_changes(Group& group, std::vector<SchemaChange> const& changes, Schema const& initial_schema,
                                         DidRereadSchema did_reread_schema)
{
    using namespace schema_change;
    struct Applier {
        Applier(Group& group, Schema const& initial_schema, DidRereadSchema did_reread_schema)
        : group{group}, initial_schema(initial_schema), table(group)
        , did_reread_schema(did_reread_schema == DidRereadSchema::Yes)
        { }
        Group& group;
        Schema const& initial_schema;
        TableHelper table;
        bool did_reread_schema;

        void operator()(RemoveProperty op)
        {
            if (!initial_schema.empty() && !initial_schema.find(op.object->name)->property_for_name(op.property->name))
                throw std::logic_error(util::format("Renamed property '%1.%2' does not exist.", op.object->name, op.property->name));
            auto table = table_for_object_schema(group, *op.object);
            table->remove_column(op.property->table_column);
        }

        void operator()(ChangePrimaryKey op)
        {
            if (op.property) {
                validate_primary_column_uniqueness(group, op.object->name, op.property->name);
            }
        }

        void operator()(AddTable op) { create_table(group, *op.object); }

        void operator()(AddInitialProperties op) {
            if (did_reread_schema)
                add_initial_columns(group, *op.object);
            else {
                // If we didn't re-read the schema then AddInitialProperties was already taken care of
                // during apply_pre_migration_changes.
            }
        }

        void operator()(AddIndex op) { table(op.object).add_search_index(op.property->table_column); }
        void operator()(RemoveIndex op) { table(op.object).remove_search_index(op.property->table_column); }

        void operator()(RemoveTable) { }
        void operator()(ChangePropertyType) { }
        void operator()(MakePropertyNullable) { }
        void operator()(MakePropertyRequired) { }
        void operator()(AddProperty) { }
    } applier{group, initial_schema, did_reread_schema};

    for (auto& change : changes) {
        change.visit(applier);
    }
}

static void create_default_permissions(Group& group, std::vector<SchemaChange> const& changes,
                                       std::string const& sync_user_id)
{
#if !REALM_ENABLE_SYNC
    static_cast<void>(group);
    static_cast<void>(changes);
    static_cast<void>(sync_user_id);
#else
    _impl::initialize_schema(group);
    sync::set_up_basic_permissions(group, true);

    // Ensure that this user exists so that local privileges checks work immediately
    sync::add_user_to_role(group, sync_user_id, "everyone");

    // Ensure that the user's private role exists so that local privilege checks work immediately.
    ObjectStore::ensure_private_role_exists_for_user(group, sync_user_id);

    // Mark all tables we just created as fully world-accessible
    // This has to be done after the first pass of schema init is done so that we can be
    // sure that the permissions tables actually exist.
    using namespace schema_change;
    struct Applier {
        Group& group;
        void operator()(AddTable op)
        {
            sync::set_class_permissions_for_role(group, op.object->name, "everyone",
                                                 static_cast<int>(ComputedPrivileges::All));
        }

        void operator()(RemoveTable) { }
        void operator()(AddInitialProperties) { }
        void operator()(AddProperty) { }
        void operator()(RemoveProperty) { }
        void operator()(MakePropertyNullable) { }
        void operator()(MakePropertyRequired) { }
        void operator()(ChangePrimaryKey) { }
        void operator()(AddIndex) { }
        void operator()(RemoveIndex) { }
        void operator()(ChangePropertyType) { }
    } applier{group};

    for (auto& change : changes) {
        change.visit(applier);
    }
#endif
}

#if REALM_ENABLE_SYNC
void ObjectStore::ensure_private_role_exists_for_user(Group& group, StringData sync_user_id)
{
    std::string private_role_name = util::format("__User:%1", sync_user_id);

    TableRef roles = ObjectStore::table_for_object_type(group, "__Role");
    size_t private_role_ndx = roles->find_first_string(roles->get_column_index("name"), private_role_name);
    if (private_role_ndx != npos) {
        // The private role already exists, so there's nothing for us to do.
        return;
    }

    // Add the user to the private role, creating the private role in the process.
    sync::add_user_to_role(group, sync_user_id, private_role_name);

    // Set the private role on the user.
    private_role_ndx = roles->find_first_string(roles->get_column_index("name"), private_role_name);
    TableRef users = ObjectStore::table_for_object_type(group, "__User");
    size_t user_ndx = users->find_first_string(users->get_column_index("id"), sync_user_id);
    users->set_link(users->get_column_index("role"), user_ndx, private_role_ndx);
}
#endif

void ObjectStore::apply_schema_changes(Group& group, uint64_t schema_version,
                                       Schema& target_schema, uint64_t target_schema_version,
                                       SchemaMode mode, std::vector<SchemaChange> const& changes,
                                       util::Optional<std::string> sync_user_id,
                                       std::function<void()> migration_function)
{
    create_metadata_tables(group);

    if (mode == SchemaMode::Additive) {
        bool target_schema_is_newer = (schema_version < target_schema_version
            || schema_version == ObjectStore::NotVersioned);

        // With sync v2.x, indexes are no longer synced, so there's no reason to avoid creating them.
        bool update_indexes = true;
        apply_additive_changes(group, changes, update_indexes);

        if (target_schema_is_newer)
            set_schema_version(group, target_schema_version);

        if (sync_user_id)
            create_default_permissions(group, changes, *sync_user_id);

        set_schema_columns(group, target_schema);
        return;
    }

    if (schema_version == ObjectStore::NotVersioned) {
        create_initial_tables(group, changes);
        set_schema_version(group, target_schema_version);
        set_schema_columns(group, target_schema);
        return;
    }

    if (mode == SchemaMode::Manual) {
        set_schema_columns(group, target_schema);
        if (migration_function) {
            migration_function();
        }

        verify_no_changes_required(schema_from_group(group).compare(target_schema));
        validate_primary_column_uniqueness(group);
        set_schema_columns(group, target_schema);
        set_schema_version(group, target_schema_version);
        return;
    }

    if (schema_version == target_schema_version) {
        apply_non_migration_changes(group, changes);
        set_schema_columns(group, target_schema);
        return;
    }

    auto old_schema = schema_from_group(group);
    apply_pre_migration_changes(group, changes);
    if (migration_function) {
        set_schema_columns(group, target_schema);
        migration_function();

        // Migration function may have changed the schema, so we need to re-read it
        auto schema = schema_from_group(group);
        apply_post_migration_changes(group, schema.compare(target_schema), old_schema, DidRereadSchema::Yes);
        validate_primary_column_uniqueness(group);
    }
    else {
        apply_post_migration_changes(group, changes, {}, DidRereadSchema::No);
    }

    set_schema_version(group, target_schema_version);
    set_schema_columns(group, target_schema);
}

Schema ObjectStore::schema_from_group(Group const& group) {
    std::vector<ObjectSchema> schema;
    schema.reserve(group.size());
    for (size_t i = 0; i < group.size(); i++) {
        auto object_type = object_type_for_table_name(group.get_table_name(i));
        if (object_type.size()) {
            schema.emplace_back(group, object_type, i);
        }
    }
    return schema;
}

util::Optional<Property> ObjectStore::property_for_column_index(ConstTableRef& table, size_t column_index)
{
    StringData column_name = table->get_column_name(column_index);

#if REALM_ENABLE_SYNC
    // The object ID column is an implementation detail, and is omitted from the schema.
    // FIXME: Consider filtering out all column names starting with `!`.
    if (column_name == sync::object_id_column_name)
        return util::none;
#endif

    if (table->get_column_type(column_index) == type_Table) {
        auto subdesc = table->get_subdescriptor(column_index);
        if (subdesc->get_column_count() != 1 || subdesc->get_column_name(0) != ObjectStore::ArrayColumnName)
            return util::none;
    }

    Property property;
    property.name = column_name;
    property.type = ObjectSchema::from_core_type(*table->get_descriptor(), column_index);
    property.is_indexed = table->has_search_index(column_index);
    property.table_column = column_index;

    if (property.type == PropertyType::Object) {
        // set link type for objects and arrays
        ConstTableRef linkTable = table->get_link_target(column_index);
        property.object_type = ObjectStore::object_type_for_table_name(linkTable->get_name().data());
    }
    return property;
}

void ObjectStore::set_schema_columns(Group const& group, Schema& schema)
{
    for (auto& object_schema : schema) {
        auto table = table_for_object_schema(group, object_schema);
        if (!table) {
            continue;
        }
        for (auto& property : object_schema.persisted_properties) {
            property.table_column = table->get_column_index(property.name);
        }
    }
}

void ObjectStore::delete_data_for_object(Group& group, StringData object_type) {
    if (TableRef table = table_for_object_type(group, object_type)) {
        group.remove_table(table->get_index_in_group());
        ObjectStore::set_primary_key_for_object(group, object_type, "");
    }
}

bool ObjectStore::is_empty(Group const& group) {
    for (size_t i = 0; i < group.size(); i++) {
        ConstTableRef table = group.get_table(i);
        auto object_type = object_type_for_table_name(table->get_name());
        if (object_type.size() == 0 || object_type.begins_with("__")) {
            continue;
        }
        if (!table->is_empty()) {
            return false;
        }
    }
    return true;
}

void ObjectStore::rename_property(Group& group, Schema& target_schema, StringData object_type, StringData old_name, StringData new_name)
{
    TableRef table = table_for_object_type(group, object_type);
    if (!table) {
        throw std::logic_error(util::format("Cannot rename properties for type '%1' because it does not exist.", object_type));
    }

    auto target_object_schema = target_schema.find(object_type);
    if (target_object_schema == target_schema.end()) {
        throw std::logic_error(util::format("Cannot rename properties for type '%1' because it has been removed from the Realm.", object_type));
    }

    if (target_object_schema->property_for_name(old_name)) {
        throw std::logic_error(util::format("Cannot rename property '%1.%2' to '%3' because the source property still exists.",
                                            object_type, old_name, new_name));
    }

    ObjectSchema table_object_schema(group, object_type);
    Property *old_property = table_object_schema.property_for_name(old_name);
    if (!old_property) {
        throw std::logic_error(util::format("Cannot rename property '%1.%2' because it does not exist.", object_type, old_name));
    }

    Property *new_property = table_object_schema.property_for_name(new_name);
    if (!new_property) {
        // New property doesn't exist in the table, which means we're probably
        // renaming to an intermediate property in a multi-version migration.
        // This is safe because the migration will fail schema validation unless
        // this property is renamed again to a valid name before the end.
        table->rename_column(old_property->table_column, new_name);
        return;
    }

    if (old_property->type != new_property->type || old_property->object_type != new_property->object_type) {
        throw std::logic_error(util::format("Cannot rename property '%1.%2' to '%3' because it would change from type '%4' to '%5'.",
                                            object_type, old_name, new_name, old_property->type_string(), new_property->type_string()));
    }

    if (is_nullable(old_property->type) && !is_nullable(new_property->type)) {
        throw std::logic_error(util::format("Cannot rename property '%1.%2' to '%3' because it would change from optional to required.",
                                            object_type, old_name, new_name));
    }

    size_t column_to_remove = new_property->table_column;
    table->rename_column(old_property->table_column, new_name);
    table->remove_column(column_to_remove);

    // update table_column for each property since it may have shifted
    for (auto& current_prop : target_object_schema->persisted_properties) {
        if (current_prop.table_column == column_to_remove)
            current_prop.table_column = old_property->table_column;
        else if (current_prop.table_column > column_to_remove)
            --current_prop.table_column;
    }

    // update nullability for column
    if (is_nullable(new_property->type) && !is_nullable(old_property->type)) {
        auto prop = *new_property;
        prop.table_column = old_property->table_column;
        make_property_optional(group, *table, prop);
    }
}

InvalidSchemaVersionException::InvalidSchemaVersionException(uint64_t old_version, uint64_t new_version)
: logic_error(util::format("Provided schema version %1 is less than last set version %2.", new_version, old_version))
, m_old_version(old_version), m_new_version(new_version)
{
}

DuplicatePrimaryKeyValueException::DuplicatePrimaryKeyValueException(std::string object_type, std::string property)
: logic_error(util::format("Primary key property '%1.%2' has duplicate values after migration.", object_type, property))
, m_object_type(object_type), m_property(property)
{
}

SchemaValidationException::SchemaValidationException(std::vector<ObjectSchemaValidationException> const& errors)
: std::logic_error([&] {
    std::string message = "Schema validation failed due to the following errors:";
    for (auto const& error : errors) {
        message += std::string("\n- ") + error.what();
    }
    return message;
}())
{
}

SchemaMismatchException::SchemaMismatchException(std::vector<ObjectSchemaValidationException> const& errors)
: std::logic_error([&] {
    std::string message = "Migration is required due to the following errors:";
    for (auto const& error : errors) {
        message += std::string("\n- ") + error.what();
    }
    return message;
}())
{
}

InvalidSchemaChangeException::InvalidSchemaChangeException(std::vector<ObjectSchemaValidationException> const& errors)
: std::logic_error([&] {
    std::string message = "The following changes cannot be made in additive-only schema mode:";
    for (auto const& error : errors) {
        message += std::string("\n- ") + error.what();
    }
    return message;
}())
{
}

InvalidExternalSchemaChangeException::InvalidExternalSchemaChangeException(std::vector<ObjectSchemaValidationException> const& errors)
: std::logic_error([&] {
    std::string message =
        "Unsupported schema changes were made by another client or process. For a "
        "synchronized Realm, this may be due to the server reverting schema changes which "
        "the local user did not have permission to make.";
    for (auto const& error : errors) {
        message += std::string("\n- ") + error.what();
    }
    return message;
}())
{
}
