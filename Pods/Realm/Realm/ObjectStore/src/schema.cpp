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

#include "schema.hpp"

#include "object_schema.hpp"
#include "object_store.hpp"
#include "object_schema.hpp"
#include "property.hpp"

#include <algorithm>

using namespace realm;

namespace realm {
bool operator==(Schema const& a, Schema const& b) noexcept
{
    return static_cast<Schema::base const&>(a) == static_cast<Schema::base const&>(b);
}
}

Schema::Schema() noexcept = default;
Schema::~Schema() = default;
Schema::Schema(Schema const&) = default;
Schema::Schema(Schema &&) noexcept = default;
Schema& Schema::operator=(Schema const&) = default;
Schema& Schema::operator=(Schema&&) noexcept = default;

Schema::Schema(std::initializer_list<ObjectSchema> types) : Schema(base(types)) { }

Schema::Schema(base types) noexcept
: base(std::move(types))
{
    std::sort(begin(), end(), [](ObjectSchema const& lft, ObjectSchema const& rgt) {
        return lft.name < rgt.name;
    });
}

Schema::iterator Schema::find(StringData name) noexcept
{
    auto it = std::lower_bound(begin(), end(), name, [](ObjectSchema const& lft, StringData rgt) {
        return lft.name < rgt;
    });
    if (it != end() && it->name != name) {
        it = end();
    }
    return it;
}

Schema::const_iterator Schema::find(StringData name) const noexcept
{
    return const_cast<Schema *>(this)->find(name);
}

Schema::iterator Schema::find(ObjectSchema const& object) noexcept
{
    return find(object.name);
}

Schema::const_iterator Schema::find(ObjectSchema const& object) const noexcept
{
    return const_cast<Schema *>(this)->find(object);
}

void Schema::validate() const
{
    std::vector<ObjectSchemaValidationException> exceptions;

    // As the types are added sorted by name, we can detect duplicates by just looking at the following element.
    auto find_next_duplicate = [&](const_iterator start) {
        return std::adjacent_find(start, cend(), [](ObjectSchema const& lft, ObjectSchema const& rgt) {
            return lft.name == rgt.name;
        });
    };

    for (auto it = find_next_duplicate(cbegin()); it != cend(); it = find_next_duplicate(++it)) {
        exceptions.push_back(ObjectSchemaValidationException("Type '%1' appears more than once in the schema.",
                                                             it->name));
    }

    for (auto const& object : *this) {
        object.validate(*this, exceptions);
    }

    if (exceptions.size()) {
        throw SchemaValidationException(exceptions);
    }
}

static void compare(ObjectSchema const& existing_schema,
                    ObjectSchema const& target_schema,
                    std::vector<SchemaChange>& changes)
{
    for (auto& current_prop : existing_schema.persisted_properties) {
        auto target_prop = target_schema.property_for_name(current_prop.name);

        if (!target_prop) {
            changes.emplace_back(schema_change::RemoveProperty{&existing_schema, &current_prop});
            continue;
        }
        if (target_schema.property_is_computed(*target_prop)) {
            changes.emplace_back(schema_change::RemoveProperty{&existing_schema, &current_prop});
            continue;
        }
        if (current_prop.type != target_prop->type ||
            current_prop.object_type != target_prop->object_type ||
            is_array(current_prop.type) != is_array(target_prop->type)) {

            changes.emplace_back(schema_change::ChangePropertyType{&existing_schema, &current_prop, target_prop});
            continue;
        }
        if (is_nullable(current_prop.type) != is_nullable(target_prop->type)) {
            if (is_nullable(current_prop.type))
                changes.emplace_back(schema_change::MakePropertyRequired{&existing_schema, &current_prop});
            else
                changes.emplace_back(schema_change::MakePropertyNullable{&existing_schema, &current_prop});
        }
        if (target_prop->requires_index()) {
            if (!current_prop.is_indexed)
                changes.emplace_back(schema_change::AddIndex{&existing_schema, &current_prop});
        }
        else if (current_prop.requires_index()) {
            changes.emplace_back(schema_change::RemoveIndex{&existing_schema, &current_prop});
        }
    }

    for (auto& target_prop : target_schema.persisted_properties) {
        if (!existing_schema.property_for_name(target_prop.name)) {
            changes.emplace_back(schema_change::AddProperty{&existing_schema, &target_prop});
        }
    }

    if (existing_schema.primary_key != target_schema.primary_key) {
        changes.emplace_back(schema_change::ChangePrimaryKey{&existing_schema, target_schema.primary_key_property()});
    }
}

template<typename T, typename U, typename Func>
void Schema::zip_matching(T&& a, U&& b, Func&& func) noexcept
{
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        auto& object_schema = a[i];
        auto& matching_schema = b[j];
        int cmp = object_schema.name.compare(matching_schema.name);
        if (cmp == 0) {
            func(&object_schema, &matching_schema);
            ++i;
            ++j;
        }
        else if (cmp < 0) {
            func(&object_schema, nullptr);
            ++i;
        }
        else {
            func(nullptr, &matching_schema);
            ++j;
        }
    }
    for (; i < a.size(); ++i)
        func(&a[i], nullptr);
    for (; j < b.size(); ++j)
        func(nullptr, &b[j]);

}

std::vector<SchemaChange> Schema::compare(Schema const& target_schema, bool include_table_removals) const
{
    std::vector<SchemaChange> changes;

    // Add missing tables
    zip_matching(target_schema, *this, [&](const ObjectSchema* target, const ObjectSchema* existing) {
        if (target && !existing) {
            changes.emplace_back(schema_change::AddTable{target});
        }
        else if (include_table_removals && existing && !target) {
            changes.emplace_back(schema_change::RemoveTable{existing});
        }
    });

    // Modify columns
    zip_matching(target_schema, *this, [&](const ObjectSchema* target, const ObjectSchema* existing) {
        if (target && existing)
            ::compare(*existing, *target, changes);
        else if (target) {
            // Target is a new table -- add all properties
            changes.emplace_back(schema_change::AddInitialProperties{target});
        }
        // nothing for tables in existing but not target
    });
    return changes;
}

void Schema::copy_keys_from(realm::Schema const& other) noexcept
{
    zip_matching(*this, other, [&](ObjectSchema* existing, const ObjectSchema* other) {
        if (!existing || !other)
            return;

        existing->table_key = other->table_key;
        for (auto& current_prop : other->persisted_properties) {
            auto target_prop = existing->property_for_name(current_prop.name);
            if (target_prop) {
                target_prop->column_key = current_prop.column_key;
            }
        }
    });
}

namespace realm {
bool operator==(SchemaChange const& lft, SchemaChange const& rgt) noexcept
{
    if (lft.m_kind != rgt.m_kind)
        return false;

    using namespace schema_change;
    struct Visitor {
        SchemaChange const& value;

        #define REALM_SC_COMPARE(type, ...) \
            bool operator()(type rgt) const \
            { \
                auto cmp = [](auto&& v) { return std::tie(__VA_ARGS__); }; \
                return cmp(value.type) == cmp(rgt); \
            }

        REALM_SC_COMPARE(AddIndex, v.object, v.property)
        REALM_SC_COMPARE(AddProperty, v.object, v.property)
        REALM_SC_COMPARE(AddInitialProperties, v.object)
        REALM_SC_COMPARE(AddTable, v.object)
        REALM_SC_COMPARE(RemoveTable, v.object)
        REALM_SC_COMPARE(ChangePrimaryKey, v.object, v.property)
        REALM_SC_COMPARE(ChangePropertyType, v.object, v.old_property, v.new_property)
        REALM_SC_COMPARE(MakePropertyNullable, v.object, v.property)
        REALM_SC_COMPARE(MakePropertyRequired, v.object, v.property)
        REALM_SC_COMPARE(RemoveIndex, v.object, v.property)
        REALM_SC_COMPARE(RemoveProperty, v.object, v.property)

        #undef REALM_SC_COMPARE
    } visitor{lft};
    return rgt.visit(visitor);
}
} // namespace realm
