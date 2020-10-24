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

#include "object_changeset.hpp"

using namespace realm;

void ObjectChangeSet::insertions_add(ObjectKeyType obj)
{
    m_insertions.insert(obj);
}

void ObjectChangeSet::modifications_add(ObjectKeyType obj, ColKeyType col)
{
    // don't report modifications on new objects
    if (m_insertions.find(obj) == m_insertions.end()) {
        m_modifications[obj].insert(col);
    }
}

void ObjectChangeSet::deletions_add(ObjectKeyType obj)
{
    m_modifications.erase(obj);
    size_t num_inserts_removed = m_insertions.erase(obj);
    if (num_inserts_removed == 0) {
        m_deletions.insert(obj);
    }
}

void ObjectChangeSet::clear(size_t old_size)
{
    static_cast<void>(old_size); // unused
    m_clear_did_occur = true;
    m_insertions.clear();
    m_modifications.clear();
    m_deletions.clear();
}

bool ObjectChangeSet::insertions_remove(ObjectKeyType obj)
{
    return m_insertions.erase(obj) > 0;
}

bool ObjectChangeSet::modifications_remove(ObjectKeyType obj)
{
    return m_modifications.erase(obj) > 0;
}

bool ObjectChangeSet::deletions_remove(ObjectKeyType obj)
{
    return m_deletions.erase(obj) > 0;
}

bool ObjectChangeSet::deletions_contains(ObjectKeyType obj) const
{
    if (m_clear_did_occur) {
        // FIXME: what are the expected notifications when an object is deleted
        // and then another object is inserted with the same key?
        return m_insertions.count(obj) == 0;
    }
    return m_deletions.count(obj) > 0;
}

bool ObjectChangeSet::insertions_contains(ObjectKeyType obj) const
{
    return m_insertions.count(obj) > 0;
}

bool ObjectChangeSet::modifications_contains(ObjectKeyType obj) const
{
    return m_modifications.count(obj) > 0;
}

const ObjectChangeSet::ObjectSet* ObjectChangeSet::get_columns_modified(ObjectKeyType obj) const
{
    auto it = m_modifications.find(obj);
    if (it == m_modifications.end()) {
        return nullptr;
    }
    return &it->second;
}

void ObjectChangeSet::merge(ObjectChangeSet&& other)
{
    if (other.empty())
        return;
    if (empty()) {
        *this = std::move(other);
        return;
    }
    m_clear_did_occur = m_clear_did_occur || other.m_clear_did_occur;

    verify();
    other.verify();

    // Drop any inserted-then-deleted rows, then merge in new insertions
    for (auto it = other.m_deletions.begin(); it != other.m_deletions.end();) {
        auto previously_inserted = m_insertions.find(*it);
        auto previously_modified = m_modifications.find(*it);
        if (previously_modified != m_modifications.end()) {
            m_modifications.erase(previously_modified);
        }
        if (previously_inserted != m_insertions.end()) {
            m_insertions.erase(previously_inserted);
            it = m_deletions.erase(it);
        }
        else {
            ++it;
        }
    }
    if (!other.m_insertions.empty()) {
        m_insertions.insert(other.m_insertions.begin(), other.m_insertions.end());
    }
    if (!other.m_deletions.empty()) {
        m_deletions.insert(other.m_deletions.begin(), other.m_deletions.end());
    }
    for (auto it = other.m_modifications.begin(); it != other.m_modifications.end(); ++it) {
        m_modifications[it->first].insert(it->second.begin(), it->second.end());
    }

    verify();

    other = {};
}

void ObjectChangeSet::verify()
{
#ifdef REALM_DEBUG
    for (auto it = m_deletions.begin(); it != m_deletions.end(); ++it) {
        REALM_ASSERT_EX(m_insertions.find(*it) == m_insertions.end(), *it);
    }
#endif
}

