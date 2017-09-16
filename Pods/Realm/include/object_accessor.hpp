////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
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

#ifndef REALM_OS_OBJECT_ACCESSOR_HPP
#define REALM_OS_OBJECT_ACCESSOR_HPP

#include "object.hpp"

#include "feature_checks.hpp"
#include "list.hpp"
#include "object_schema.hpp"
#include "object_store.hpp"
#include "results.hpp"
#include "schema.hpp"
#include "shared_realm.hpp"
#include "util/format.hpp"

#include <realm/link_view.hpp>
#include <realm/util/assert.hpp>
#include <realm/table_view.hpp>

#if REALM_HAVE_SYNC_STABLE_IDS
#include <realm/sync/object.hpp>
#endif // REALM_HAVE_SYNC_STABLE_IDS

#include <string>

namespace realm {
template <typename ValueType, typename ContextType>
void Object::set_property_value(ContextType& ctx, StringData prop_name, ValueType value, bool try_update)
{
    verify_attached();
    m_realm->verify_in_write();
    auto& property = property_for_name(prop_name);

    // Modifying primary keys is allowed in migrations to make it possible to
    // add a new primary key to a type (or change the property type), but it
    // is otherwise considered the immutable identity of the row
    if (property.is_primary && !m_realm->is_in_migration())
        throw std::logic_error("Cannot modify primary key after creation");

    set_property_value_impl(ctx, property, value, try_update);
}

template <typename ValueType, typename ContextType>
ValueType Object::get_property_value(ContextType& ctx, StringData prop_name)
{
    return get_property_value_impl<ValueType>(ctx, property_for_name(prop_name));
}

template <typename ValueType, typename ContextType>
void Object::set_property_value_impl(ContextType& ctx, const Property &property,
                                     ValueType value, bool try_update, bool is_default)
{
    ctx.will_change(*this, property);

    auto& table = *m_row.get_table();
    size_t col = property.table_column;
    size_t row = m_row.get_index();
    if (is_nullable(property.type) && ctx.is_null(value)) {
        if (property.type == PropertyType::Object) {
            if (!is_default)
                table.nullify_link(col, row);
        }
        else {
            table.set_null(col, row, is_default);
        }

        ctx.did_change();
        return;
    }

    if (is_array(property.type)) {
        if (property.type == PropertyType::LinkingObjects)
            throw ReadOnlyPropertyException(m_object_schema->name, property.name);

        ContextType child_ctx(ctx, property);
        List list(m_realm, *m_row.get_table(), col, m_row.get_index());
        list.assign(child_ctx, value, try_update);
        ctx.did_change();
        return;
    }

    switch (property.type & ~PropertyType::Nullable) {
        case PropertyType::Object: {
            ContextType child_ctx(ctx, property);
            auto link = child_ctx.template unbox<RowExpr>(value, true, try_update);
            table.set_link(col, row, link.get_index(), is_default);
            break;
        }
        case PropertyType::Bool:
            table.set(col, row, ctx.template unbox<bool>(value), is_default);
            break;
        case PropertyType::Int:
            table.set(col, row, ctx.template unbox<int64_t>(value), is_default);
            break;
        case PropertyType::Float:
            table.set(col, row, ctx.template unbox<float>(value), is_default);
            break;
        case PropertyType::Double:
            table.set(col, row, ctx.template unbox<double>(value), is_default);
            break;
        case PropertyType::String:
            table.set(col, row, ctx.template unbox<StringData>(value), is_default);
            break;
        case PropertyType::Data:
            table.set(col, row, ctx.template unbox<BinaryData>(value), is_default);
            break;
        case PropertyType::Any:
            throw std::logic_error("not supported");
        case PropertyType::Date:
            table.set(col, row, ctx.template unbox<Timestamp>(value), is_default);
            break;
        default:
            REALM_COMPILER_HINT_UNREACHABLE();
    }
    ctx.did_change();
}

template <typename ValueType, typename ContextType>
ValueType Object::get_property_value_impl(ContextType& ctx, const Property &property)
{
    verify_attached();

    size_t column = property.table_column;
    if (is_nullable(property.type) && m_row.is_null(column))
        return ctx.null_value();
    if (is_array(property.type) && property.type != PropertyType::LinkingObjects)
        return ctx.box(List(m_realm, *m_row.get_table(), column, m_row.get_index()));

    switch (property.type & ~PropertyType::Flags) {
        case PropertyType::Bool:   return ctx.box(m_row.get_bool(column));
        case PropertyType::Int:    return ctx.box(m_row.get_int(column));
        case PropertyType::Float:  return ctx.box(m_row.get_float(column));
        case PropertyType::Double: return ctx.box(m_row.get_double(column));
        case PropertyType::String: return ctx.box(m_row.get_string(column));
        case PropertyType::Data:   return ctx.box(m_row.get_binary(column));
        case PropertyType::Date:   return ctx.box(m_row.get_timestamp(column));
        case PropertyType::Any:    return ctx.box(m_row.get_mixed(column));
        case PropertyType::Object: {
            auto linkObjectSchema = m_realm->schema().find(property.object_type);
            TableRef table = ObjectStore::table_for_object_type(m_realm->read_group(), property.object_type);
            return ctx.box(Object(m_realm, *linkObjectSchema, table->get(m_row.get_link(column))));
        }
        case PropertyType::LinkingObjects: {
            auto target_object_schema = m_realm->schema().find(property.object_type);
            auto link_property = target_object_schema->property_for_name(property.link_origin_property_name);
            TableRef table = ObjectStore::table_for_object_type(m_realm->read_group(), target_object_schema->name);
            auto tv = m_row.get_table()->get_backlink_view(m_row.get_index(), table.get(), link_property->table_column);
            return ctx.box(Results(m_realm, std::move(tv)));
        }
        default: REALM_UNREACHABLE();
    }
}

template<typename ValueType, typename ContextType>
Object Object::create(ContextType& ctx, std::shared_ptr<Realm> const& realm,
                      StringData object_type, ValueType value,
                      bool try_update, Row* out_row)
{
    auto object_schema = realm->schema().find(object_type);
    REALM_ASSERT(object_schema != realm->schema().end());
    return create(ctx, realm, *object_schema, value, try_update, out_row);
}

template<typename ValueType, typename ContextType>
Object Object::create(ContextType& ctx, std::shared_ptr<Realm> const& realm,
                      ObjectSchema const& object_schema, ValueType value,
                      bool try_update, Row* out_row)
{
    realm->verify_in_write();

    // get or create our accessor
    bool created = false;

    // try to get existing row if updating
    size_t row_index = realm::not_found;
    TableRef table = ObjectStore::table_for_object_type(realm->read_group(), object_schema.name);

    bool skip_primary = true;
    if (auto primary_prop = object_schema.primary_key_property()) {
        // search for existing object based on primary key type
        auto primary_value = ctx.value_for_property(value, primary_prop->name,
                                                    primary_prop - &object_schema.persisted_properties[0]);
        if (!primary_value)
            primary_value = ctx.default_value_for_property(object_schema, primary_prop->name);
        if (!primary_value) {
            if (!is_nullable(primary_prop->type))
                throw MissingPropertyValueException(object_schema.name, primary_prop->name);
            primary_value = ctx.null_value();
        }
        row_index = get_for_primary_key_impl(ctx, *table, *primary_prop, *primary_value);

        if (row_index == realm::not_found) {
            created = true;
            if (primary_prop->type == PropertyType::Int) {
#if REALM_HAVE_SYNC_STABLE_IDS
                row_index = sync::create_object_with_primary_key(realm->read_group(), *table, ctx.template unbox<util::Optional<int64_t>>(*primary_value));
#else
                row_index = table->add_empty_row();
                if (ctx.is_null(*primary_value))
                    table->set_null_unique(primary_prop->table_column, row_index);
                else
                    table->set_unique(primary_prop->table_column, row_index, ctx.template unbox<int64_t>(*primary_value));
#endif // REALM_HAVE_SYNC_STABLE_IDS
            }
            else if (primary_prop->type == PropertyType::String) {
                auto value = ctx.template unbox<StringData>(*primary_value);
#if REALM_HAVE_SYNC_STABLE_IDS
                row_index = sync::create_object_with_primary_key(realm->read_group(), *table, value);
#else
                row_index = table->add_empty_row();
                table->set_unique(primary_prop->table_column, row_index, value);
#endif // REALM_HAVE_SYNC_STABLE_IDS
            }
            else {
                REALM_TERMINATE("Unsupported primary key type.");
            }
        }
        else if (!try_update) {
            if (realm->is_in_migration()) {
                // Creating objects with duplicate primary keys is allowed in migrations
                // as long as there are no duplicates at the end, as adding an entirely
                // new column which is the PK will inherently result in duplicates at first
                row_index = table->add_empty_row();
                created = true;
                skip_primary = false;
            }
            else {
                throw std::logic_error(util::format("Attempting to create an object of type '%1' with an existing primary key value '%2'.",
                                                    object_schema.name, ctx.print(*primary_value)));
            }
        }
    }
    else {
#if REALM_HAVE_SYNC_STABLE_IDS
        row_index = sync::create_object(realm->read_group(), *table);
#else
        row_index = table->add_empty_row();
#endif // REALM_HAVE_SYNC_STABLE_IDS
        created = true;
    }

    // populate
    Object object(realm, object_schema, table->get(row_index));
    if (out_row)
        *out_row = object.row();
    for (size_t i = 0; i < object_schema.persisted_properties.size(); ++i) {
        auto& prop = object_schema.persisted_properties[i];
        if (skip_primary && prop.is_primary)
            continue;

        auto v = ctx.value_for_property(value, prop.name, i);
        if (!created && !v)
            continue;

        bool is_default = false;
        if (!v) {
            v = ctx.default_value_for_property(object_schema, prop.name);
            is_default = true;
        }
        if ((!v || ctx.is_null(*v)) && !is_nullable(prop.type) && !is_array(prop.type)) {
            if (prop.is_primary || !ctx.allow_missing(value))
                throw MissingPropertyValueException(object_schema.name, prop.name);
        }
        if (v)
            object.set_property_value_impl(ctx, prop, *v, try_update, is_default);
    }
    return object;
}

template<typename ValueType, typename ContextType>
Object Object::get_for_primary_key(ContextType& ctx, std::shared_ptr<Realm> const& realm,
                      StringData object_type, ValueType primary_value)
{
    auto object_schema = realm->schema().find(object_type);
    REALM_ASSERT(object_schema != realm->schema().end());
    return get_for_primary_key(ctx, realm, *object_schema, primary_value);
}

template<typename ValueType, typename ContextType>
Object Object::get_for_primary_key(ContextType& ctx, std::shared_ptr<Realm> const& realm,
                                   const ObjectSchema &object_schema,
                                   ValueType primary_value)
{
    auto primary_prop = object_schema.primary_key_property();
    if (!primary_prop) {
        throw MissingPrimaryKeyException(object_schema.name);
    }

    auto table = ObjectStore::table_for_object_type(realm->read_group(), object_schema.name);
    if (!table)
        return Object(realm, object_schema, RowExpr());
    auto row_index = get_for_primary_key_impl(ctx, *table, *primary_prop, primary_value);

    return Object(realm, object_schema, row_index == realm::not_found ? Row() : Row(table->get(row_index)));
}

template<typename ValueType, typename ContextType>
size_t Object::get_for_primary_key_impl(ContextType& ctx, Table const& table,
                                        const Property &primary_prop,
                                        ValueType primary_value) {
    bool is_null = ctx.is_null(primary_value);
    if (is_null && !is_nullable(primary_prop.type))
        throw std::logic_error("Invalid null value for non-nullable primary key.");
    if (primary_prop.type == PropertyType::String) {
        return table.find_first(primary_prop.table_column,
                                ctx.template unbox<StringData>(primary_value));
    }
    if (is_nullable(primary_prop.type))
        return table.find_first(primary_prop.table_column,
                                ctx.template unbox<util::Optional<int64_t>>(primary_value));
    return table.find_first(primary_prop.table_column,
                            ctx.template unbox<int64_t>(primary_value));
}

} // namespace realm

#endif // REALM_OS_OBJECT_ACCESSOR_HPP
