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

#include "impl/object_notifier.hpp"

#include "shared_realm.hpp"

using namespace realm;
using namespace realm::_impl;

ObjectNotifier::ObjectNotifier(std::shared_ptr<Realm> realm, TableKey table, ObjKey obj)
: CollectionNotifier(std::move(realm))
, m_table(table)
, m_obj(obj)
{
}

bool ObjectNotifier::do_add_required_change_info(TransactionChangeInfo& info)
{
    m_info = &info;
    info.tables[m_table.value];
    return false;
}

void ObjectNotifier::run()
{
    if (!m_table)
        return;

    auto it = m_info->tables.find(m_table.value);
    if (it == m_info->tables.end())
        return;
    auto& change = it->second;

    if (change.deletions_contains(m_obj.value)) {
        m_change.deletions.add(0);
        m_table = {};
        m_obj = {};
        return;
    }

    auto column_modifications = change.get_columns_modified(m_obj.value);
    if (!column_modifications)
        return;
    m_change.modifications.add(0);
    for (auto col : *column_modifications) {
        m_change.columns[col].add(0);
    }
}
