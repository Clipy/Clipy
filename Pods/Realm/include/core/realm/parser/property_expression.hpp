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

#ifndef REALM_PROPERTY_EXPRESSION_HPP
#define REALM_PROPERTY_EXPRESSION_HPP

#include <realm/parser/keypath_mapping.hpp>
#include <realm/query.hpp>
#include <realm/table.hpp>

namespace realm {
namespace parser {

struct PropertyExpression
{
    Query &query;
    std::vector<KeyPathElement> link_chain;
    DataType get_dest_type() const;
    size_t get_dest_ndx() const;
    ConstTableRef get_dest_table() const;
    bool dest_type_is_backlink() const;

    PropertyExpression(Query &q, const std::string &key_path_string, parser::KeyPathMapping& mapping);

    Table* table_getter() const;

    template <typename RetType>
    auto value_of_type_for_query() const
    {
        return this->table_getter()->template column<RetType>(get_dest_ndx());
    }
};

inline DataType PropertyExpression::get_dest_type() const
{
    REALM_ASSERT_DEBUG(link_chain.size() > 0);
    return link_chain.back().col_type;
}

inline bool PropertyExpression::dest_type_is_backlink() const
{
    REALM_ASSERT_DEBUG(link_chain.size() > 0);
    return link_chain.back().is_backlink;
}

inline size_t PropertyExpression::get_dest_ndx() const
{
    REALM_ASSERT_DEBUG(link_chain.size() > 0);
    return link_chain.back().col_ndx;
}

inline ConstTableRef PropertyExpression::get_dest_table() const
{
    REALM_ASSERT_DEBUG(link_chain.size() > 0);
    return link_chain.back().table;
}


} // namespace parser
} // namespace realm

#endif // REALM_PROPERTY_EXPRESSION_HPP

