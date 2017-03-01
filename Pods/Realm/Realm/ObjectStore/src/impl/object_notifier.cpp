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

ObjectNotifier::ObjectNotifier(Row const& row, std::shared_ptr<Realm> realm)
: CollectionNotifier(std::move(realm))
{
    REALM_ASSERT(row.get_table());
    set_table(*row.get_table());

    auto& sg = Realm::Internal::get_shared_group(*get_realm());
    m_handover = sg.export_for_handover(row);
}

void ObjectNotifier::release_data() noexcept
{
    m_row = nullptr;
}

void ObjectNotifier::do_attach_to(SharedGroup& sg)
{
    REALM_ASSERT(m_handover);
    REALM_ASSERT(!m_row);
    m_row = sg.import_from_handover(std::move(m_handover));
}

void ObjectNotifier::do_detach_from(SharedGroup& sg)
{
    REALM_ASSERT(!m_handover);
    if (m_row) {
        m_handover = sg.export_for_handover(*m_row);
        m_row = nullptr;
    }
}

bool ObjectNotifier::do_add_required_change_info(TransactionChangeInfo& info)
{
    REALM_ASSERT(!m_handover);
    m_info = &info;
    if (m_row && m_row->is_attached()) {
        size_t table_ndx = m_row->get_table()->get_index_in_group();
        if (table_ndx >= info.table_modifications_needed.size())
            info.table_modifications_needed.resize(table_ndx + 1);
        info.table_modifications_needed[table_ndx] = true;
    }
    return false;
}

void ObjectNotifier::run()
{
    if (!m_row)
        return;
    if (!m_row->is_attached()) {
        m_change.deletions.add(0);
        m_row = nullptr;
        return;
    }

    size_t table_ndx = m_row->get_table()->get_index_in_group();
    if (table_ndx >= m_info->tables.size())
        return;
    auto& change = m_info->tables[table_ndx];
    if (!change.modifications.contains(m_row->get_index()))
        return;
    m_change.modifications.add(0);
    m_change.columns.reserve(change.columns.size());
    for (auto& col : change.columns) {
        m_change.columns.emplace_back();
        if (col.contains(m_row->get_index()))
            m_change.columns.back().add(0);
    }
}

void ObjectNotifier::do_prepare_handover(SharedGroup&)
{
    add_changes(std::move(m_change));
}
