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

#ifndef REALM_OBJECT_CHANGESET_HPP
#define REALM_OBJECT_CHANGESET_HPP

#include "collection_notifications.hpp"

#include <realm/keys.hpp>
#include <realm/util/optional.hpp>

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace realm {

class ObjectChangeSet {
public:
    using ColKeyType = decltype(realm::ColKey::value);
    using ObjectKeyType = decltype(realm::ObjKey::value);
    using ObjectSet = std::unordered_set<ObjectKeyType>;
    using ObjectMapToColumnSet = std::unordered_map<ObjectKeyType, std::unordered_set<ColKeyType>>;

    ObjectChangeSet() = default;
    ObjectChangeSet(ObjectChangeSet const&) = default;
    ObjectChangeSet(ObjectChangeSet&&) = default;
    ObjectChangeSet& operator=(ObjectChangeSet const&) = default;
    ObjectChangeSet& operator=(ObjectChangeSet&&) = default;

    void insertions_add(ObjectKeyType obj);
    void modifications_add(ObjectKeyType obj, ColKeyType col);
    void deletions_add(ObjectKeyType obj);
    void clear(size_t old_size);

    bool insertions_remove(ObjectKeyType obj);
    bool modifications_remove(ObjectKeyType obj);
    bool deletions_remove(ObjectKeyType obj);

    bool insertions_contains(ObjectKeyType obj) const;
    bool modifications_contains(ObjectKeyType obj) const;
    bool deletions_contains(ObjectKeyType obj) const;
    // if the specified object has not been modified, returns nullptr
    // if the object has been modified, returns a pointer to the ObjectSet
    const ObjectSet* get_columns_modified(ObjectKeyType obj) const;

    bool insertions_empty() const noexcept { return m_insertions.empty(); }
    bool modifications_empty() const noexcept { return m_modifications.empty(); }
    bool deletions_empty() const noexcept { return m_deletions.empty(); }

    size_t insertions_size() const noexcept { return m_insertions.size(); }
    size_t modifications_size() const noexcept { return m_modifications.size(); }
    size_t deletions_size() const noexcept { return m_deletions.size(); }

    bool clear_did_occur() const noexcept
    {
        return m_clear_did_occur;
    }
    bool empty() const noexcept
    {
        return m_deletions.empty() && m_insertions.empty() && m_modifications.empty() && !m_clear_did_occur;
    }

    void merge(ObjectChangeSet&& other);
    void verify();

    const ObjectSet& get_deletions() const noexcept { return m_deletions; }
    const ObjectMapToColumnSet& get_modifications() const noexcept { return m_modifications; }
    const ObjectSet& get_insertions() const noexcept { return m_insertions; }

private:
    ObjectSet m_deletions;
    ObjectSet m_insertions;
    ObjectMapToColumnSet m_modifications;
    bool m_clear_did_occur = false;
};

} // end namespace realm

#endif // REALM_OBJECT_CHANGESET_HPP
