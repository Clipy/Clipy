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
#include "util/format.hpp"

using namespace realm;

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

Object::Object(SharedRealm r, ObjectSchema const& s, RowExpr const& o)
: m_realm(std::move(r)), m_object_schema(&s), m_row(o) { }

Object::Object(SharedRealm r, StringData object_type, size_t ndx)
: m_realm(std::move(r))
, m_object_schema(&*m_realm->schema().find(object_type))
, m_row(ObjectStore::table_for_object_type(m_realm->read_group(), object_type)->get(ndx))
{ }

Object::Object() = default;
Object::~Object() = default;
Object::Object(Object const&) = default;
Object::Object(Object&&) = default;
Object& Object::operator=(Object const&) = default;
Object& Object::operator=(Object&&) = default;

NotificationToken Object::add_notification_callback(CollectionChangeCallback callback) &
{
    if (!m_notifier) {
        m_notifier = std::make_shared<_impl::ObjectNotifier>(m_row, m_realm);
        _impl::RealmCoordinator::register_notifier(m_notifier);
    }
    return {m_notifier, m_notifier->add_callback(std::move(callback))};
}

void Object::verify_attached() const
{
    m_realm->verify_thread();
    if (!m_row.is_attached()) {
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
