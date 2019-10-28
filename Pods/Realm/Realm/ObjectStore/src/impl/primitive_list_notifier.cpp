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

#include "impl/primitive_list_notifier.hpp"

#include "shared_realm.hpp"

#include <realm/link_view.hpp>

using namespace realm;
using namespace realm::_impl;

PrimitiveListNotifier::PrimitiveListNotifier(TableRef table, std::shared_ptr<Realm> realm)
: CollectionNotifier(std::move(realm))
, m_prev_size(table->size())
{
    set_table(*table->get_parent_table());
    m_table_handover = source_shared_group().export_table_for_handover(table);
}

void PrimitiveListNotifier::release_data() noexcept
{
    m_table.reset();
}

void PrimitiveListNotifier::do_attach_to(SharedGroup& sg)
{
    REALM_ASSERT(!m_table);
    if (m_table_handover)
        m_table = sg.import_table_from_handover(std::move(m_table_handover));
}

void PrimitiveListNotifier::do_detach_from(SharedGroup& sg)
{
    REALM_ASSERT(!m_table_handover);
    if (m_table) {
        if (m_table->is_attached())
            m_table_handover = sg.export_table_for_handover(m_table);
        m_table = {};
    }
}

bool PrimitiveListNotifier::do_add_required_change_info(TransactionChangeInfo& info)
{
    REALM_ASSERT(!m_table_handover);
    if (!m_table || !m_table->is_attached()) {
        return false; // origin row was deleted after the notification was added
    }

    auto& table = *m_table->get_parent_table();
    size_t row_ndx = m_table->get_parent_row_index();
    size_t col_ndx = find_container_column(table, row_ndx, m_table, type_Table, &Table::get_subtable);
    info.lists.push_back({table.get_index_in_group(), row_ndx, col_ndx, &m_change});

    m_info = &info;
    return true;
}

void PrimitiveListNotifier::run()
{
    if (!m_table || !m_table->is_attached()) {
        // Table was deleted entirely, so report all of the rows being removed
        // if this is the first run after that
        if (m_prev_size) {
            m_change.deletions.set(m_prev_size);
            m_prev_size = 0;
        }
        else {
            m_change = {};
        }
        return;
    }

    if (!m_change.deletions.empty() && m_change.deletions.begin()->second == std::numeric_limits<size_t>::max()) {
        // Table was cleared, so set the deletions to the actual previous size
        m_change.deletions.set(m_prev_size);
    }

    m_prev_size = m_table->size();
}

void PrimitiveListNotifier::do_prepare_handover(SharedGroup&)
{
    add_changes(std::move(m_change));
}
