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

#include "thread_confined.hpp"

#include "impl/handover.hpp"

using namespace realm;

AnyThreadConfined::AnyThreadConfined(const AnyThreadConfined& thread_confined)
{
    switch (thread_confined.m_type) {
        case Type::Object:
            new (&m_object) Object(thread_confined.m_object);
            break;

        case Type::List:
            new (&m_list) List(thread_confined.m_list);
            break;

        case Type::Results:
            new (&m_results) Results(thread_confined.m_results);
            break;
    }
    new (&m_type) Type(thread_confined.m_type);
}

AnyThreadConfined& AnyThreadConfined::operator=(const AnyThreadConfined& thread_confined)
{
    this->~AnyThreadConfined();
    new (this) AnyThreadConfined(thread_confined);
    return *this;
}

AnyThreadConfined::AnyThreadConfined(AnyThreadConfined&& thread_confined)
{
    switch (thread_confined.m_type) {
        case Type::Object:
            new (&m_object) Object(std::move(thread_confined.m_object));
            break;

        case Type::List:
            new (&m_list) List(std::move(thread_confined.m_list));
            break;

        case Type::Results:
            new (&m_results) Results(std::move(thread_confined.m_results));
            break;
    }
    new (&m_type) Type(std::move(thread_confined.m_type));
}

AnyThreadConfined& AnyThreadConfined::operator=(AnyThreadConfined&& thread_confined)
{
    this->~AnyThreadConfined();
    new (this) AnyThreadConfined(std::move(thread_confined));
    return *this;
}

AnyThreadConfined::~AnyThreadConfined()
{
    switch (m_type) {
        case Type::Object:
            m_object.~Object();
            break;

        case Type::List:
            m_list.~List();
            break;

        case Type::Results:
            m_results.~Results();
            break;
    }
}

SharedRealm AnyThreadConfined::get_realm() const
{
    switch (m_type) {
        case Type::Object:
            return m_object.realm();

        case Type::List:
            return m_list.get_realm();

        case Type::Results:
            return m_results.get_realm();
    }
    REALM_UNREACHABLE();
}

_impl::AnyHandover AnyThreadConfined::export_for_handover() const
{
    SharedGroup& shared_group = Realm::Internal::get_shared_group(*get_realm());
    switch (m_type) {
        case AnyThreadConfined::Type::Object:
            return _impl::AnyHandover(shared_group.export_for_handover(m_object.row()),
                                      m_object.get_object_schema().name);

        case AnyThreadConfined::Type::List:
            return _impl::AnyHandover(shared_group.export_linkview_for_handover(m_list.m_link_view));

        case AnyThreadConfined::Type::Results: {
            SortDescriptor::HandoverPatch sort_order;
            SortDescriptor::generate_patch(m_results.get_sort(), sort_order);
            return _impl::AnyHandover(shared_group.export_for_handover(m_results.get_query(), ConstSourcePayload::Copy),
                                      std::move(sort_order));
        }
    }
    REALM_UNREACHABLE();
}
