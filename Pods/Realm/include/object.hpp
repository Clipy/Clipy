////////////////////////////////////////////////////////////////////////////
//
// Copyright 2017 Realm Inc.
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

#ifndef REALM_OS_OBJECT_HPP
#define REALM_OS_OBJECT_HPP

#include "impl/collection_notifier.hpp"
#include "shared_realm.hpp"

#include <realm/row.hpp>

namespace realm {
class ObjectSchema;

namespace _impl {
    class ObjectNotifier;
}

class Object {
public:
    Object();
    Object(SharedRealm r, ObjectSchema const& s, BasicRowExpr<Table> const& o);
    Object(SharedRealm r, ObjectSchema const& s, Row const& o);

    Object(Object const&);
    Object(Object&&);
    Object& operator=(Object const&);
    Object& operator=(Object&&);

    ~Object();

    // property getter/setter
    template<typename ValueType, typename ContextType>
    void set_property_value(ContextType ctx, std::string prop_name,
                            ValueType value, bool try_update);

    template<typename ValueType, typename ContextType>
    ValueType get_property_value(ContextType ctx, std::string prop_name);

    // create an Object from a native representation
    template<typename ValueType, typename ContextType>
    static Object create(ContextType ctx, SharedRealm realm,
                         const ObjectSchema &object_schema, ValueType value,
                         bool try_update);

    template<typename ValueType, typename ContextType>
    static Object get_for_primary_key(ContextType ctx, SharedRealm realm,
                                      const ObjectSchema &object_schema,
                                      ValueType primary_value);

    SharedRealm const& realm() const { return m_realm; }
    ObjectSchema const& get_object_schema() const { return *m_object_schema; }
    Row row() const { return m_row; }

    bool is_valid() const { return m_row.is_attached(); }

    NotificationToken add_notification_block(CollectionChangeCallback callback) &;

private:
    SharedRealm m_realm;
    const ObjectSchema *m_object_schema;
    Row m_row;
    _impl::CollectionNotifier::Handle<_impl::ObjectNotifier> m_notifier;

    template<typename ValueType, typename ContextType>
    void set_property_value_impl(ContextType ctx, const Property &property, ValueType value, bool try_update);
    template<typename ValueType, typename ContextType>
    ValueType get_property_value_impl(ContextType ctx, const Property &property);

    template<typename ValueType, typename ContextType>
    static size_t get_for_primary_key_impl(ContextType ctx, Table const& table,
                                           const Property &primary_prop, ValueType primary_value);

    inline void verify_attached();
};
} // namespace realm

#endif // REALM_OS_OBJECT_HPP
