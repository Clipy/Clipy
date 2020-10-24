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

#include "object.hpp"

#include "impl/object_notifier.hpp"
#include "impl/realm_coordinator.hpp"
#include "object_schema.hpp"
#include "object_store.hpp"

#include <realm/table.hpp>

using namespace realm;

Object Object::freeze(std::shared_ptr<Realm> frozen_realm) const
{
    return Object(frozen_realm, frozen_realm->import_copy_of(m_obj));
}

bool Object::is_frozen() const noexcept
{
    return m_realm->is_frozen();
}

InvalidatedObjectException::InvalidatedObjectException(const std::string& object_type)
: std::logic_error("Accessing object of type " + object_type + " which has been invalidated or deleted")
, object_type(object_type)
{}

InvalidPropertyException::InvalidPropertyException(const std::string& object_type, const std::string& property_name)
: std::logic_error(util::format("Property '%1.%2' does not exist", object_type, property_name))
, object_type(object_type), property_name(property_name)
{}

MissingPropertyValueException::MissingPropertyValueException(const std::string& object_type, const std::string& property_name)
: std::logic_error(util::format("Missing value for property '%1.%2'", object_type, property_name))
, object_type(object_type), property_name(property_name)
{}

MissingPrimaryKeyException::MissingPrimaryKeyException(const std::string& object_type)
: std::logic_error(util::format("'%1' does not have a primary key defined", object_type))
, object_type(object_type)
{}

ReadOnlyPropertyException::ReadOnlyPropertyException(const std::string& object_type, const std::string& property_name)
: std::logic_error(util::format("Cannot modify read-only property '%1.%2'", object_type, property_name))
, object_type(object_type), property_name(property_name) {}

ModifyPrimaryKeyException::ModifyPrimaryKeyException(const std::string& object_type, const std::string& property_name)
: std::logic_error(util::format("Cannot modify primary key after creation: '%1.%2'", object_type, property_name))
, object_type(object_type), property_name(property_name) {}

Object::Object(SharedRealm r, ObjectSchema const& s, Obj const& o)
: m_realm(std::move(r)), m_object_schema(&s), m_obj(o) { }

Object::Object(SharedRealm r, Obj const& o)
: m_realm(std::move(r))
, m_object_schema(&*m_realm->schema().find(ObjectStore::object_type_for_table_name(o.get_table()->get_name())))
, m_obj(o)
{
    REALM_ASSERT(!m_obj.get_table() || (&m_realm->read_group() == _impl::TableFriend::get_parent_group(*m_obj.get_table())));
}

Object::Object(SharedRealm r, StringData object_type, ObjKey key)
: m_realm(std::move(r))
, m_object_schema(&*m_realm->schema().find(object_type))
, m_obj(ObjectStore::table_for_object_type(m_realm->read_group(), object_type)->get_object(key))
{
    REALM_ASSERT(!m_obj.get_table() || (&m_realm->read_group() == _impl::TableFriend::get_parent_group(*m_obj.get_table())));
}

Object::Object(SharedRealm r, StringData object_type, size_t index)
: m_realm(std::move(r))
, m_object_schema(&*m_realm->schema().find(object_type))
, m_obj(ObjectStore::table_for_object_type(m_realm->read_group(), object_type)->get_object(index))
{
    REALM_ASSERT(!m_obj.get_table() || (&m_realm->read_group() == _impl::TableFriend::get_parent_group(*m_obj.get_table())));
}

Object::Object() = default;
Object::~Object() = default;
Object::Object(Object const&) = default;
Object::Object(Object&&) = default;
Object& Object::operator=(Object const&) = default;
Object& Object::operator=(Object&&) = default;

NotificationToken Object::add_notification_callback(CollectionChangeCallback callback) &
{
    verify_attached();
    m_realm->verify_notifications_available();
    if (!m_notifier) {
        m_notifier = std::make_shared<_impl::ObjectNotifier>(m_realm, m_obj.get_table()->get_key(), m_obj.get_key());
        _impl::RealmCoordinator::register_notifier(m_notifier);
    }
    return {m_notifier, m_notifier->add_callback(std::move(callback))};
}

void Object::verify_attached() const
{
    m_realm->verify_thread();
    if (!m_obj.is_valid()) {
        throw InvalidatedObjectException(m_object_schema->name);
    }
}

Property const& Object::property_for_name(StringData prop_name) const
{
    auto prop = m_object_schema->property_for_name(prop_name);
    if (!prop) {
        throw InvalidPropertyException(m_object_schema->name, prop_name);
    }
    return *prop;
}

void Object::validate_property_for_setter(Property const& property) const
{
    verify_attached();
    m_realm->verify_in_write();

    // Modifying primary keys is allowed in migrations to make it possible to
    // add a new primary key to a type (or change the property type), but it
    // is otherwise considered the immutable identity of the row
    if (property.is_primary) {
        if (!m_realm->is_in_migration())
            throw ModifyPrimaryKeyException(m_object_schema->name, property.name);
        // Modifying the PK property while it's the PK will corrupt the table,
        // so remove it and then restore it at the end of the migration (which will rebuild the table)
        m_obj.get_table()->set_primary_key_column({});
    }
}

#if REALM_ENABLE_SYNC
void Object::ensure_user_in_everyone_role()
{
    if (auto role_table = m_realm->read_group().get_table("class___Role")) {
        if (ObjKey ndx = role_table->find_first_string(role_table->get_column_key("name"), "everyone")) {
            auto role = role_table->get_object(ndx);
            auto users = role.get_linklist(role_table->get_column_key("members"));
            if (users.find_first(m_obj.get_key()) == realm::npos) {
                users.add(m_obj.get_key());
            }
        }
    }
}

void Object::ensure_private_role_exists_for_user()
{
    auto user_id = m_obj.get<StringData>("id");
    ObjectStore::ensure_private_role_exists_for_user(static_cast<Transaction&>(m_realm->read_group()), user_id);
}
#endif
