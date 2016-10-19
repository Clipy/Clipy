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

#include "impl/handover.hpp"

using namespace realm;
using namespace realm::_impl;

AnyHandover::AnyHandover(AnyHandover&& handover)
{
    switch (handover.m_type) {
        case AnyThreadConfined::Type::Object:
            new (&m_object.row_handover) RowHandover(std::move(handover.m_object.row_handover));
            new (&m_object.object_schema_name) std::string(std::move(handover.m_object.object_schema_name));
            break;

        case AnyThreadConfined::Type::List:
            new (&m_list.link_view_handover) LinkViewHandover(std::move(handover.m_list.link_view_handover));
            break;

        case AnyThreadConfined::Type::Results:
            new (&m_results.query_handover) QueryHandover(std::move(handover.m_results.query_handover));
            new (&m_results.sort_order) SortDescriptor::HandoverPatch(std::move(handover.m_results.sort_order));
            break;
    }
    new (&m_type) AnyThreadConfined::Type(handover.m_type);
}

AnyHandover& AnyHandover::operator=(AnyHandover&& handover)
{
    this->~AnyHandover();
    new (this) AnyHandover(std::move(handover));
    return *this;
}

AnyHandover::~AnyHandover()
{
    switch (m_type) {
        case AnyThreadConfined::Type::Object:
            m_object.row_handover.~unique_ptr();
            break;

        case AnyThreadConfined::Type::List:
            m_list.link_view_handover.~unique_ptr();
            break;

        case AnyThreadConfined::Type::Results:
            m_results.query_handover.~unique_ptr();
            m_results.sort_order.~unique_ptr();
            break;
    }
}

AnyThreadConfined AnyHandover::import_from_handover(SharedRealm realm) &&
{
    SharedGroup& shared_group = Realm::Internal::get_shared_group(*realm);
    switch (m_type) {
        case AnyThreadConfined::Type::Object: {
            auto row = shared_group.import_from_handover(std::move(m_object.row_handover));
            auto object_schema = realm->schema().find(m_object.object_schema_name);
            REALM_ASSERT_DEBUG(object_schema != realm->schema().end());
            return AnyThreadConfined(Object(std::move(realm), *object_schema, std::move(*row)));
        }
        case AnyThreadConfined::Type::List: {
            auto link_view_ref = shared_group.import_linkview_from_handover(std::move(m_list.link_view_handover));
            return AnyThreadConfined(List(std::move(realm), std::move(link_view_ref)));
        }
        case AnyThreadConfined::Type::Results: {
            auto query = shared_group.import_from_handover(std::move(m_results.query_handover));
            auto& table = *query->get_table();
            return AnyThreadConfined(Results(std::move(realm), std::move(*query),
                                             SortDescriptor::create_from_and_consume_patch(m_results.sort_order, table)));
        }
    }
    REALM_UNREACHABLE();
}
