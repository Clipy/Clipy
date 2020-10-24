/*************************************************************************
 *
 * Copyright 2017 Realm Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 **************************************************************************/

#ifndef REALM_SYNC_OBJECT_ID_HPP
#define REALM_SYNC_OBJECT_ID_HPP

#include <functional> // std::hash
#include <string>
#include <iosfwd> // operator<<
#include <map>
#include <set>

#include <realm/global_key.hpp>
#include <realm/string_data.hpp>
#include <realm/data_type.hpp>
#include <realm/keys.hpp>
#include <realm/db.hpp>
#include <realm/util/metered/map.hpp>
#include <realm/util/metered/set.hpp>
#include <realm/util/metered/string.hpp>


namespace realm {

class Group;

namespace sync {

// ObjectIDSet is a set of (table name, object id)
class ObjectIDSet {
public:

    void insert(StringData table, GlobalKey object_id);
    void erase(StringData table, GlobalKey object_id);
    bool contains(StringData table, GlobalKey object_id) const noexcept;
    bool empty() const noexcept;

    // A map from table name to a set of object ids.
    util::metered::map<std::string, util::metered::set<GlobalKey>> m_objects;
};

// FieldSet is a set of fields in tables. A field is defined by a
// table name, a column in the table and an object id for the row.
class FieldSet {
public:

    void insert(StringData table, StringData column, GlobalKey object_id);
    void erase(StringData table, StringData column, GlobalKey object_id);
    bool contains(StringData table, GlobalKey object_id) const noexcept;
    bool contains(StringData table, StringData column, GlobalKey object_id) const noexcept;
    bool empty() const noexcept;

    // A map from table name to a map from column name to a set of
    // object ids.
    util::metered::map<
        std::string,
        util::metered::map<std::string, util::metered::set<GlobalKey>>
    >  m_fields;
};

struct GlobalID {
    StringData table_name;
    GlobalKey object_id;

    bool operator==(const GlobalID& other) const;
    bool operator!=(const GlobalID& other) const;
    bool operator<(const GlobalID& other) const;
};

/// Implementation:


inline bool GlobalID::operator==(const GlobalID& other) const
{
    return object_id == other.object_id && table_name == other.table_name;
}

inline bool GlobalID::operator!=(const GlobalID& other) const
{
    return !(*this == other);
}

inline bool GlobalID::operator<(const GlobalID& other) const
{
    if (table_name == other.table_name)
        return object_id < other.object_id;
    return table_name < other.table_name;
}

inline bool ObjectIDSet::empty() const noexcept
{
    return m_objects.empty();
}

inline bool FieldSet::empty() const noexcept
{
    return m_fields.empty();
}

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_OBJECT_ID_HPP

