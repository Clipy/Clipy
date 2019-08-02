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

#ifndef REALM_KEYPATH_MAPPING_HPP
#define REALM_KEYPATH_MAPPING_HPP

#include <realm/table.hpp>

#include "parser_utils.hpp"

#include <unordered_map>
#include <string>

namespace realm {
namespace parser {

struct KeyPathElement
{
    ConstTableRef table;
    size_t col_ndx;
    DataType col_type;
    bool is_backlink;
};

class BacklinksRestrictedError : public std::runtime_error {
public:
    BacklinksRestrictedError(const std::string& msg) : std::runtime_error(msg) {}
    /// runtime_error::what() returns the msg provided in the constructor.
};

struct TableAndColHash {
    std::size_t operator () (const std::pair<ConstTableRef, std::string> &p) const;
};


// This class holds state which allows aliasing variable names in key paths used in queries.
// It is used to allow variable naming in subqueries such as 'SUBQUERY(list, $obj, $obj.intCol = 5).@count'
// It can also be used to allow querying named backlinks if bindings provide the mappings themselves.
class KeyPathMapping
{
public:
    KeyPathMapping();
    // returns true if added, false if duplicate key already exists
    bool add_mapping(ConstTableRef table, std::string name, std::string alias);
    void remove_mapping(ConstTableRef table, std::string name);
    bool has_mapping(ConstTableRef table, std::string name);
    KeyPathElement process_next_path(ConstTableRef table, KeyPath& path, size_t& index);
    void set_allow_backlinks(bool allow);
    bool backlinks_allowed() const
    {
        return m_allow_backlinks;
    }
    void set_backlink_class_prefix(std::string prefix);
    static Table* table_getter(TableRef table, const std::vector<KeyPathElement>& links);
protected:
    bool m_allow_backlinks;
    std::string m_backlink_class_prefix;
    std::unordered_map<std::pair<ConstTableRef, std::string>, std::string, TableAndColHash> m_mapping;
};

} // namespace parser
} // namespace realm

#endif // REALM_KEYPATH_MAPPING_HPP
