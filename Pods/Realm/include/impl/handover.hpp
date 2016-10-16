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

#ifndef REALM_HANDOVER_HPP
#define REALM_HANDOVER_HPP

#include "list.hpp"
#include "object_accessor.hpp"
#include "results.hpp"
#include "thread_confined.hpp"

#include <realm/group_shared.hpp>
#include <realm/link_view.hpp>
#include <realm/query.hpp>
#include <realm/row.hpp>
#include <realm/table_view.hpp>

namespace realm {
namespace _impl {

// Type-erased wrapper for a `Handover` of an `AnyThreadConfined` value
class AnyHandover {
public:
    AnyHandover(const AnyHandover&) = delete;
    AnyHandover& operator=(const AnyHandover&) = delete;
    AnyHandover(AnyHandover&&);
    AnyHandover& operator=(AnyHandover&&);
    ~AnyHandover();

    // Destination `Realm` version must match that of the source Realm at the time of export
    AnyThreadConfined import_from_handover(SharedRealm realm) &&;

private:
    friend AnyThreadConfined;

    using RowHandover      = std::unique_ptr<SharedGroup::Handover<Row>>;
    using QueryHandover    = std::unique_ptr<SharedGroup::Handover<Query>>;
    using LinkViewHandover = std::unique_ptr<SharedGroup::Handover<LinkView>>;

    AnyThreadConfined::Type m_type;
    union {
        struct {
            RowHandover row_handover;
            std::string object_schema_name;
        } m_object;

        struct {
            LinkViewHandover link_view_handover;
        } m_list;

        struct {
            QueryHandover query_handover;
            SortDescriptor::HandoverPatch sort_order;
        } m_results;
    };

    AnyHandover(RowHandover row_handover, std::string object_schema_name)
    : m_type(AnyThreadConfined::Type::Object), m_object({std::move(row_handover), std::move(object_schema_name)}) {}

    AnyHandover(LinkViewHandover link_view)
    : m_type(AnyThreadConfined::Type::List), m_list({std::move(link_view)}) {}

    AnyHandover(QueryHandover query_handover, SortDescriptor::HandoverPatch sort_order)
    : m_type(AnyThreadConfined::Type::Results), m_results({std::move(query_handover), std::move(sort_order)}) {}
};
}
}

#endif /* REALM_HANDOVER_HPP */
