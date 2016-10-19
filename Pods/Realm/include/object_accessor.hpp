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

#ifndef REALM_OBJECT_ACCESSOR_HPP
#define REALM_OBJECT_ACCESSOR_HPP

#include "list.hpp"
#include "object_schema.hpp"
#include "object_store.hpp"
#include "results.hpp"
#include "schema.hpp"
#include "shared_realm.hpp"
#include "util/format.hpp"

#include <string>
#include <realm/link_view.hpp>
#include <realm/table_view.hpp>

namespace realm {

    class Object {
    public:
        Object(SharedRealm r, const ObjectSchema &s, Row o) : m_realm(r), m_object_schema(&s), m_row(o) {}

        // property getter/setter
        template<typename ValueType, typename ContextType>
        inline void set_property_value(ContextType ctx, std::string prop_name, ValueType value, bool try_update);

        template<typename ValueType, typename ContextType>
        inline ValueType get_property_value(ContextType ctx, std::string prop_name);

        // create an Object from a native representation
        template<typename ValueType, typename ContextType>
        static inline Object create(ContextType ctx, SharedRealm realm, const ObjectSchema &object_schema, ValueType value, bool try_update);

        template<typename ValueType, typename ContextType>
        static Object get_for_primary_key(ContextType ctx, SharedRealm realm, const ObjectSchema &object_schema, ValueType primary_value);

        SharedRealm realm() const { return m_realm; }
        const ObjectSchema &get_object_schema() const { return *m_object_schema; }
        Row row() const { return m_row; }

        bool is_valid() const { return m_row.is_attached(); }

    private:
        SharedRealm m_realm;
        const ObjectSchema *m_object_schema;
        Row m_row;

        template<typename ValueType, typename ContextType>
        inline void set_property_value_impl(ContextType ctx, const Property &property, ValueType value, bool try_update);
        template<typename ValueType, typename ContextType>
        inline ValueType get_property_value_impl(ContextType ctx, const Property &property);

        template<typename ValueType, typename ContextType>
        static size_t get_for_primary_key_impl(ContextType ctx, const ConstTableRef &table, const Property &primary_prop, ValueType primary_value);
        
        inline void verify_attached();
    };

    //
    // Value converters - template specializations must be implemented for each platform in order to call templated methods on Object
    //
    template<typename ValueType, typename ContextType>
    class NativeAccessor {
    public:
        static bool dict_has_value_for_key(ContextType ctx, ValueType dict, const std::string &prop_name);
        static ValueType dict_value_for_key(ContextType ctx, ValueType dict, const std::string &prop_name);

        static bool has_default_value_for_property(ContextType ctx, Realm *realm, const ObjectSchema &object_schema, const std::string &prop_name);
        static ValueType default_value_for_property(ContextType ctx, Realm *realm, const ObjectSchema &object_schema, const std::string &prop_name);

        static bool to_bool(ContextType, ValueType &);
        static ValueType from_bool(ContextType, bool);
        static long long to_long(ContextType, ValueType &);
        static ValueType from_long(ContextType, long long);
        static float to_float(ContextType, ValueType &);
        static ValueType from_float(ContextType, float);
        static double to_double(ContextType, ValueType &);
        static ValueType from_double(ContextType, double);
        static std::string to_string(ContextType, ValueType &);
        static ValueType from_string(ContextType, StringData);
        static std::string to_binary(ContextType, ValueType &);
        static ValueType from_binary(ContextType, BinaryData);
        static Timestamp to_timestamp(ContextType, ValueType &);
        static ValueType from_timestamp(ContextType, Timestamp);

        static bool is_null(ContextType, ValueType &);
        static ValueType null_value(ContextType);

        // convert value to persisted object
        // for existing objects return the existing row index
        // for new/updated objects return the row index
        static size_t to_object_index(ContextType ctx, SharedRealm realm, ValueType &val, const std::string &type, bool try_update);
        static ValueType from_object(ContextType ctx, Object);

        // object index for an existing object
        static size_t to_existing_object_index(ContextType ctx, SharedRealm realm, ValueType &val);

        // list value accessors
        static size_t list_size(ContextType ctx, ValueType &val);
        static ValueType list_value_at_index(ContextType ctx, ValueType &val, size_t index);
        static ValueType from_list(ContextType ctx, List);

        // results value accessors
        static ValueType from_results(ContextType ctx, Results);

        //
        // Deprecated
        //
        static Mixed to_mixed(ContextType, ValueType&) { throw std::logic_error("'Any' type is unsupported"); }
    };

    struct InvalidatedObjectException : public std::logic_error {
        InvalidatedObjectException(const std::string& object_type, const std::string& message) :
            std::logic_error(message), object_type(object_type) {}
        const std::string object_type;
    };
    
    struct InvalidPropertyException : public std::logic_error {
        InvalidPropertyException(const std::string& object_type, const std::string& property_name, const std::string& message) :
            std::logic_error(message), object_type(object_type), property_name(property_name) {}
        const std::string object_type;
        const std::string property_name;
    };

    struct MissingPropertyValueException : public std::logic_error {
        MissingPropertyValueException(const std::string& object_type, const std::string& property_name, const std::string& message) :
            std::logic_error(message), object_type(object_type), property_name(property_name) {}
        const std::string object_type;
        const std::string property_name;
    };

    struct MissingPrimaryKeyException : public std::logic_error {
        MissingPrimaryKeyException(const std::string& object_type, const std::string& message) : std::logic_error(message), object_type(object_type) {}
        const std::string object_type;
    };

    struct ReadOnlyPropertyException : public std::logic_error {
        ReadOnlyPropertyException(const std::string& object_type, const std::string& property_name, const std::string& message) :
            std::logic_error(message), object_type(object_type), property_name(property_name) {}
        const std::string object_type;
        const std::string property_name;
    };

    struct MutationOutsideTransactionException : public std::logic_error {
        MutationOutsideTransactionException(const std::string& message) : std::logic_error(message) {}
    };

    //
    // template method implementations
    //
    template <typename ValueType, typename ContextType>
    inline void Object::set_property_value(ContextType ctx, std::string prop_name, ValueType value, bool try_update)
    {
        const Property *prop = m_object_schema->property_for_name(prop_name);
        if (!prop) {
            throw InvalidPropertyException(m_object_schema->name, prop_name,
                "Setting invalid property '" + prop_name + "' on object '" + m_object_schema->name + "'.");
        }
        set_property_value_impl(ctx, *prop, value, try_update);
    }

    template <typename ValueType, typename ContextType>
    inline ValueType Object::get_property_value(ContextType ctx, std::string prop_name)
    {
        const Property *prop = m_object_schema->property_for_name(prop_name);
        if (!prop) {
            throw InvalidPropertyException(m_object_schema->name, prop_name,
                "Getting invalid property '" + prop_name + "' on object '" + m_object_schema->name + "'.");
        }
        return get_property_value_impl<ValueType>(ctx, *prop);
    }

    template <typename ValueType, typename ContextType>
    inline void Object::set_property_value_impl(ContextType ctx, const Property &property, ValueType value, bool try_update)
    {
        using Accessor = NativeAccessor<ValueType, ContextType>;

        verify_attached();

        if (!m_realm->is_in_transaction()) {
            throw MutationOutsideTransactionException("Can only set property values within a transaction.");
        }

        size_t column = property.table_column;
        if (property.is_nullable && Accessor::is_null(ctx, value)) {
            if (property.type == PropertyType::Object) {
                m_row.nullify_link(column);
            }
            else {
                m_row.set_null(column);
            }
            return;
        }

        switch (property.type) {
            case PropertyType::Bool:
                m_row.set_bool(column, Accessor::to_bool(ctx, value));
                break;
            case PropertyType::Int:
                if (property.is_primary)
                    m_row.set_int_unique(column, Accessor::to_long(ctx, value));
                else
                    m_row.set_int(column, Accessor::to_long(ctx, value));
                break;
            case PropertyType::Float:
                m_row.set_float(column, Accessor::to_float(ctx, value));
                break;
            case PropertyType::Double:
                m_row.set_double(column, Accessor::to_double(ctx, value));
                break;
            case PropertyType::String: {
                auto string_value = Accessor::to_string(ctx, value);
                if (property.is_primary)
                    m_row.set_string_unique(column, string_value);
                else
                    m_row.set_string(column, string_value);
                break;
            }
            case PropertyType::Data:
                m_row.set_binary(column, BinaryData(Accessor::to_binary(ctx, value)));
                break;
            case PropertyType::Any:
                m_row.set_mixed(column, Accessor::to_mixed(ctx, value));
                break;
            case PropertyType::Date:
                m_row.set_timestamp(column, Accessor::to_timestamp(ctx, value));
                break;
            case PropertyType::Object: {
                if (Accessor::is_null(ctx, value)) {
                    m_row.nullify_link(column);
                }
                else {
                    m_row.set_link(column, Accessor::to_object_index(ctx, m_realm, value, property.object_type, try_update));
                }
                break;
            }
            case PropertyType::Array: {
                realm::LinkViewRef link_view = m_row.get_linklist(column);
                link_view->clear();
                if (!Accessor::is_null(ctx, value)) {
                    size_t count = Accessor::list_size(ctx, value);
                    for (size_t i = 0; i < count; i++) {
                        ValueType element = Accessor::list_value_at_index(ctx, value, i);
                        link_view->add(Accessor::to_object_index(ctx, m_realm, element, property.object_type, try_update));
                    }
                }
                break;
            }
            case PropertyType::LinkingObjects:
                throw ReadOnlyPropertyException(m_object_schema->name, property.name,
                                                util::format("Cannot modify read-only property '%1.%2'",
                                                             m_object_schema->name, property.name));
        }
    }

    template <typename ValueType, typename ContextType>
    inline ValueType Object::get_property_value_impl(ContextType ctx, const Property &property)
    {
        using Accessor = NativeAccessor<ValueType, ContextType>;

        verify_attached();

        size_t column = property.table_column;
        if (property.is_nullable && m_row.is_null(column)) {
            return Accessor::null_value(ctx);
        }

        switch (property.type) {
            case PropertyType::Bool:
                return Accessor::from_bool(ctx, m_row.get_bool(column));
            case PropertyType::Int:
                return Accessor::from_long(ctx, m_row.get_int(column));
            case PropertyType::Float:
                return Accessor::from_float(ctx, m_row.get_float(column));
            case PropertyType::Double:
                return Accessor::from_double(ctx, m_row.get_double(column));
            case PropertyType::String:
                return Accessor::from_string(ctx, m_row.get_string(column));
            case PropertyType::Data:
                return Accessor::from_binary(ctx, m_row.get_binary(column));
            case PropertyType::Any:
                throw "Any not supported";
            case PropertyType::Date:
                return Accessor::from_timestamp(ctx, m_row.get_timestamp(column));
            case PropertyType::Object: {
                auto linkObjectSchema = m_realm->schema().find(property.object_type);
                TableRef table = ObjectStore::table_for_object_type(m_realm->read_group(), linkObjectSchema->name);
                if (m_row.is_null_link(property.table_column)) {
                    return Accessor::null_value(ctx);
                }
                return Accessor::from_object(ctx, std::move(Object(m_realm, *linkObjectSchema, table->get(m_row.get_link(column)))));
            }
            case PropertyType::Array:
                return Accessor::from_list(ctx, List(m_realm, static_cast<LinkViewRef>(m_row.get_linklist(column))));
            case PropertyType::LinkingObjects: {
                auto target_object_schema = m_realm->config().schema->find(property.object_type);
                auto link_property = target_object_schema->property_for_name(property.link_origin_property_name);
                TableRef table = ObjectStore::table_for_object_type(m_realm->read_group(), target_object_schema->name);
                auto tv = m_row.get_table()->get_backlink_view(m_row.get_index(), table.get(), link_property->table_column);
                Results results(m_realm, std::move(tv));
                return Accessor::from_results(ctx, std::move(results));
            }
        }
    }

    template<typename ValueType, typename ContextType>
    inline Object Object::create(ContextType ctx, SharedRealm realm, const ObjectSchema &object_schema, ValueType value, bool try_update)
    {
        using Accessor = NativeAccessor<ValueType, ContextType>;

        if (!realm->is_in_transaction()) {
            throw MutationOutsideTransactionException("Can only create objects within a transaction.");
        }

        // get or create our accessor
        bool created = false;

        // try to get existing row if updating
        size_t row_index = realm::not_found;
        realm::TableRef table = ObjectStore::table_for_object_type(realm->read_group(), object_schema.name);
        const Property *primary_prop = object_schema.primary_key_property();

        if (primary_prop) {
            // search for existing object based on primary key type
            ValueType primary_value = Accessor::dict_value_for_key(ctx, value, object_schema.primary_key);
            row_index = get_for_primary_key_impl(ctx, table, *primary_prop, primary_value);

            if (row_index == realm::not_found) {
                row_index = table->add_empty_row();
                created = true;
                Object object(realm, object_schema, table->get(row_index));
                object.set_property_value_impl(ctx, *primary_prop, primary_value, try_update);
            }
            else if (!try_update) {
                throw std::logic_error(util::format("Attempting to create an object of type '%1' with an existing primary key value.", object_schema.name));
            }
        }
        else {
            row_index = table->add_empty_row();
            created = true;
        }

        // populate
        Object object(realm, object_schema, table->get(row_index));
        for (const Property& prop : object_schema.persisted_properties) {
            if (!prop.is_primary) {
                if (Accessor::dict_has_value_for_key(ctx, value, prop.name)) {
                    object.set_property_value_impl(ctx, prop, Accessor::dict_value_for_key(ctx, value, prop.name), try_update);
                }
                else if (created) {
                    if (Accessor::has_default_value_for_property(ctx, realm.get(), object_schema, prop.name)) {
                        object.set_property_value_impl(ctx, prop, Accessor::default_value_for_property(ctx, realm.get(), object_schema, prop.name), try_update);
                    }
                    else if (prop.is_nullable || prop.type == PropertyType::Array) {
                        object.set_property_value_impl(ctx, prop, Accessor::null_value(ctx), try_update);
                    }
                    else {
                        throw MissingPropertyValueException(object_schema.name, prop.name,
                            "Missing property value for property " + prop.name);
                    }
                }
            }
        }
        return object;
    }

    template<typename ValueType, typename ContextType>
    inline Object Object::get_for_primary_key(ContextType ctx, SharedRealm realm, const ObjectSchema &object_schema, ValueType primary_value)
    {
        auto primary_prop = object_schema.primary_key_property();
        if (!primary_prop) {
            throw MissingPrimaryKeyException(object_schema.name, object_schema.name + " does not have a primary key");
        }

        auto table = ObjectStore::table_for_object_type(realm->read_group(), object_schema.name);
        auto row_index = get_for_primary_key_impl(ctx, table, *primary_prop, primary_value);

        return Object(realm, object_schema, row_index == realm::not_found ? Row() : table->get(row_index));
    }

    template<typename ValueType, typename ContextType>
    inline size_t Object::get_for_primary_key_impl(ContextType ctx, const ConstTableRef &table, const Property &primary_prop, ValueType primary_value) {
        using Accessor = NativeAccessor<ValueType, ContextType>;

        if (primary_prop.type == PropertyType::String) {
            auto primary_string = Accessor::to_string(ctx, primary_value);
            return table->find_first_string(primary_prop.table_column, primary_string);
        }
        else {
            return table->find_first_int(primary_prop.table_column, Accessor::to_long(ctx, primary_value));
        }
    }

    inline void Object::verify_attached() {
        if (!m_row.is_attached()) {
            throw InvalidatedObjectException(m_object_schema->name,
                "Accessing object of type " + m_object_schema->name + " which has been deleted"
            );
        }
    }

    //
    // List implementation
    //
    template<typename ValueType, typename ContextType>
    void List::add(ContextType ctx, ValueType value)
    {
        add(NativeAccessor<ValueType, ContextType>::to_object_index(ctx, m_realm, value, get_object_schema().name, false));
    }

    template<typename ValueType, typename ContextType>
    void List::insert(ContextType ctx, ValueType value, size_t list_ndx)
    {
        insert(list_ndx, NativeAccessor<ValueType, ContextType>::to_object_index(ctx, m_realm, value, get_object_schema().name, false));
    }

    template<typename ValueType, typename ContextType>
    void List::set(ContextType ctx, ValueType value, size_t list_ndx)
    {
        set(list_ndx, NativeAccessor<ValueType, ContextType>::to_object_index(ctx, m_realm, value, get_object_schema().name, false));
    }
}

#endif /* defined(REALM_OBJECT_ACCESSOR_HPP) */
