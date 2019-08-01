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

#include "impl/transact_log_handler.hpp"

#include "binding_context.hpp"
#include "impl/collection_notifier.hpp"
#include "index_set.hpp"
#include "shared_realm.hpp"

#include <realm/group_shared.hpp>
#include <realm/lang_bind_helper.hpp>

#include <algorithm>
#include <numeric>

using namespace realm;

namespace {

class KVOAdapter : public _impl::TransactionChangeInfo {
public:
    KVOAdapter(std::vector<BindingContext::ObserverState>& observers, BindingContext* context);

    void before(SharedGroup& sg);
    void after(SharedGroup& sg);

private:
    BindingContext* m_context;
    std::vector<BindingContext::ObserverState>& m_observers;
    std::vector<void *> m_invalidated;

    struct ListInfo {
        BindingContext::ObserverState* observer;
        _impl::CollectionChangeBuilder builder;
        size_t col;
        size_t initial_size;
    };
    std::vector<ListInfo> m_lists;
    VersionID m_version;

    size_t new_table_ndx(size_t ndx) const { return ndx < table_indices.size() ? table_indices[ndx] : ndx; }
};

KVOAdapter::KVOAdapter(std::vector<BindingContext::ObserverState>& observers, BindingContext* context)
: _impl::TransactionChangeInfo{}
, m_context(context)
, m_observers(observers)
{
    if (m_observers.empty())
        return;

    std::vector<size_t> tables_needed;
    for (auto& observer : observers) {
        tables_needed.push_back(observer.table_ndx);
    }
    std::sort(begin(tables_needed), end(tables_needed));
    tables_needed.erase(std::unique(begin(tables_needed), end(tables_needed)), end(tables_needed));

    auto realm = context->realm.lock();
    auto& group = realm->read_group();
    for (auto& observer : observers) {
        auto table = group.get_table(observer.table_ndx);
        for (size_t i = 0, count = table->get_column_count(); i < count; ++i) {
            auto type = table->get_column_type(i);
            if (type == type_LinkList)
                m_lists.push_back({&observer, {}, i, size_t(-1)});
            else if (type == type_Table)
                m_lists.push_back({&observer, {}, i, table->get_subtable_size(i, observer.row_ndx)});
        }
    }

    auto max = std::max_element(begin(tables_needed), end(tables_needed));
    if (*max >= table_modifications_needed.size())
        table_modifications_needed.resize(*max + 1, false);
    if (*max >= table_moves_needed.size())
        table_moves_needed.resize(*max + 1, false);
    for (auto& tbl : tables_needed) {
        table_modifications_needed[tbl] = true;
        table_moves_needed[tbl] = true;
    }
    for (auto& list : m_lists)
        lists.push_back({list.observer->table_ndx, list.observer->row_ndx, list.col, &list.builder});
}

void KVOAdapter::before(SharedGroup& sg)
{
    if (!m_context)
        return;

    m_version = sg.get_version_of_current_transaction();
    if (tables.empty())
        return;

    for (auto& observer : m_observers) {
        size_t table_ndx = new_table_ndx(observer.table_ndx);
        if (table_ndx >= tables.size())
            continue;

        auto const& table = tables[table_ndx];
        auto const& moves = table.moves;
        auto idx = observer.row_ndx;
        auto it = lower_bound(begin(moves), end(moves), idx,
                              [](auto const& a, auto b) { return a.from < b; });
        if (it != moves.end() && it->from == idx)
            idx = it->to;
        else if (table.deletions.contains(idx)) {
            m_invalidated.push_back(observer.info);
            continue;
        }
        else
            idx = table.insertions.shift(table.deletions.unshift(idx));
        if (table.modifications.contains(idx)) {
            observer.changes.resize(table.columns.size());
            size_t i = 0;
            for (auto& c : table.columns) {
                auto& change = observer.changes[i];
                if (table_ndx >= column_indices.size() || column_indices[table_ndx].empty())
                    change.initial_column_index = i;
                else if (i >= column_indices[table_ndx].size())
                    change.initial_column_index = i - column_indices[table_ndx].size() + column_indices[table_ndx].back() + 1;
                else
                    change.initial_column_index = column_indices[table_ndx][i];
                if (change.initial_column_index != npos && c.contains(idx))
                    change.kind = BindingContext::ColumnInfo::Kind::Set;
                ++i;
            }
        }
    }

    for (auto& list : m_lists) {
        if (list.builder.empty()) {
            // We may have pre-emptively marked the column as modified if the
            // LinkList was selected but the actual changes made ended up being
            // a no-op
            if (list.col < list.observer->changes.size())
                list.observer->changes[list.col].kind = BindingContext::ColumnInfo::Kind::None;
            continue;
        }
        // If the containing row was deleted then changes will be empty
        if (list.observer->changes.empty()) {
            REALM_ASSERT_DEBUG(tables[new_table_ndx(list.observer->table_ndx)].deletions.contains(list.observer->row_ndx));
            continue;
        }
        // otherwise the column should have been marked as modified
        REALM_ASSERT(list.col < list.observer->changes.size());
        auto& builder = list.builder;
        auto& changes = list.observer->changes[list.col];

        builder.modifications.remove(builder.insertions);

        // KVO can't express moves (becaue NSArray doesn't have them), so
        // transform them into a series of sets on each affected index when possible
        if (!builder.moves.empty() && builder.insertions.count() == builder.moves.size() && builder.deletions.count() == builder.moves.size()) {
            changes.kind = BindingContext::ColumnInfo::Kind::Set;
            changes.indices = builder.modifications;
            changes.indices.add(builder.deletions);

            // Iterate over each of the rows which may have been shifted by
            // the moves and check if it actually has been, or if it's ended
            // up in the same place as it started (either because the moves were
            // actually a swap that doesn't effect the rows in between, or the
            // combination of moves happen to leave some intermediate rows in
            // the same place)
            auto in_range = [](auto& it, auto end, size_t i) {
                if (it != end && i >= it->second)
                    ++it;
                return it != end && i >= it->first && i < it->second;
            };

            auto del_it = builder.deletions.begin(), del_end = builder.deletions.end();
            auto ins_it = builder.insertions.begin(), ins_end = builder.insertions.end();
            size_t start = std::min(ins_it->first, del_it->first);
            size_t end = std::max(std::prev(ins_end)->second, std::prev(del_end)->second);
            ptrdiff_t shift = 0;
            for (size_t i = start; i < end; ++i) {
                if (in_range(del_it, del_end, i))
                    --shift;
                else if (in_range(ins_it, ins_end, i + shift))
                    ++shift;
                if (shift != 0)
                    changes.indices.add(i);
            }
        }
        // KVO can't express multiple types of changes at once
        else if (builder.insertions.empty() + builder.modifications.empty() + builder.deletions.empty() < 2) {
            changes.kind = BindingContext::ColumnInfo::Kind::SetAll;
        }
        else if (!builder.insertions.empty()) {
            changes.kind = BindingContext::ColumnInfo::Kind::Insert;
            changes.indices = builder.insertions;
        }
        else if (!builder.modifications.empty()) {
            changes.kind = BindingContext::ColumnInfo::Kind::Set;
            changes.indices = builder.modifications;
        }
        else {
            REALM_ASSERT(!builder.deletions.empty());
            changes.kind = BindingContext::ColumnInfo::Kind::Remove;
            // Table clears don't come with the size, so we need to fix up the
            // notification to make it just delete all rows that actually existed
            if (std::prev(builder.deletions.end())->second > list.initial_size)
                changes.indices.set(list.initial_size);
            else
                changes.indices = builder.deletions;
        }
    }
    m_context->will_change(m_observers, m_invalidated);
}

void KVOAdapter::after(SharedGroup& sg)
{
    if (!m_context)
        return;
    m_context->did_change(m_observers, m_invalidated,
                          m_version != VersionID{} &&
                          m_version != sg.get_version_of_current_transaction());
}

template<typename Derived>
struct MarkDirtyMixin  {
    bool mark_dirty(size_t row, size_t col, _impl::Instruction instr=_impl::instr_Set)
    {
        // Ignore SetDefault and SetUnique as those conceptually cannot be
        // changes to existing rows
        if (instr == _impl::instr_Set)
            static_cast<Derived *>(this)->mark_dirty(row, col);
        return true;
    }

    bool set_int(size_t c, size_t r, int_fast64_t, _impl::Instruction i, size_t) { return mark_dirty(r, c, i); }
    bool set_bool(size_t c, size_t r, bool, _impl::Instruction i) { return mark_dirty(r, c, i); }
    bool set_float(size_t c, size_t r, float, _impl::Instruction i) { return mark_dirty(r, c, i); }
    bool set_double(size_t c, size_t r, double, _impl::Instruction i) { return mark_dirty(r, c, i); }
    bool set_string(size_t c, size_t r, StringData, _impl::Instruction i, size_t) { return mark_dirty(r, c, i); }
    bool set_binary(size_t c, size_t r, BinaryData, _impl::Instruction i) { return mark_dirty(r, c, i); }
    bool set_olddatetime(size_t c, size_t r, OldDateTime, _impl::Instruction i) { return mark_dirty(r, c, i); }
    bool set_timestamp(size_t c, size_t r, Timestamp, _impl::Instruction i) { return mark_dirty(r, c, i); }
    bool set_table(size_t c, size_t r, _impl::Instruction i) { return mark_dirty(r, c, i); }
    bool set_mixed(size_t c, size_t r, const Mixed&, _impl::Instruction i) { return mark_dirty(r, c, i); }
    bool set_link(size_t c, size_t r, size_t, size_t, _impl::Instruction i) { return mark_dirty(r, c, i); }
    bool set_null(size_t c, size_t r, _impl::Instruction i, size_t) { return mark_dirty(r, c, i); }

    bool add_int(size_t col, size_t row, int_fast64_t) { return mark_dirty(row, col); }
    bool nullify_link(size_t col, size_t row, size_t) { return mark_dirty(row, col); }
    bool insert_substring(size_t col, size_t row, size_t, StringData) { return mark_dirty(row, col); }
    bool erase_substring(size_t col, size_t row, size_t, size_t) { return mark_dirty(row, col); }

    bool set_int_unique(size_t, size_t, size_t, int_fast64_t) { return true; }
    bool set_string_unique(size_t, size_t, size_t, StringData) { return true; }

    bool add_row_with_key(size_t, size_t, size_t, int64_t) { return true; }
};

class TransactLogValidationMixin {
    // Index of currently selected table
    size_t m_current_table = 0;

    REALM_NORETURN
    REALM_NOINLINE
    void schema_error()
    {
        throw _impl::UnsupportedSchemaChange();
    }

protected:
    size_t current_table() const noexcept { return m_current_table; }

public:

    bool select_table(size_t group_level_ndx, int, const size_t*) noexcept
    {
        m_current_table = group_level_ndx;
        return true;
    }

    // Removing or renaming things while a Realm is open is never supported
    bool erase_group_level_table(size_t, size_t) { schema_error(); }
    bool rename_group_level_table(size_t, StringData) { schema_error(); }
    bool erase_column(size_t) { schema_error(); }
    bool erase_link_column(size_t, size_t, size_t) { schema_error(); }
    bool rename_column(size_t, StringData) { schema_error(); }

    // Schema changes which don't involve a change in the schema version are
    // allowed
    bool add_search_index(size_t) { return true; }
    bool remove_search_index(size_t) { return true; }

    // Additive changes and reorderings are supported
    bool insert_group_level_table(size_t, size_t, StringData) { return true; }
    bool insert_column(size_t, DataType, StringData, bool) { return true; }
    bool insert_link_column(size_t, DataType, StringData, size_t, size_t) { return true; }
    bool set_link_type(size_t, LinkType) { return true; }
    bool move_column(size_t, size_t) { return true; }
    bool move_group_level_table(size_t, size_t) { return true; }

    // Non-schema changes are all allowed
    void parse_complete() { }
    bool select_descriptor(int, const size_t*) { return true; }
    bool select_link_list(size_t, size_t, size_t) { return true; }
    bool insert_empty_rows(size_t, size_t, size_t, bool) { return true; }
    bool erase_rows(size_t, size_t, size_t, bool) { return true; }
    bool swap_rows(size_t, size_t) { return true; }
    bool move_row(size_t, size_t) { return true; }
    bool clear_table(size_t=0) noexcept { return true; }
    bool link_list_set(size_t, size_t, size_t) { return true; }
    bool link_list_insert(size_t, size_t, size_t) { return true; }
    bool link_list_erase(size_t, size_t) { return true; }
    bool link_list_nullify(size_t, size_t) { return true; }
    bool link_list_clear(size_t) { return true; }
    bool link_list_move(size_t, size_t) { return true; }
    bool link_list_swap(size_t, size_t) { return true; }
    bool merge_rows(size_t, size_t) { return true; }
    bool optimize_table() { return true; }
};


// A transaction log handler that just validates that all operations made are
// ones supported by the object store
struct TransactLogValidator : public TransactLogValidationMixin, public MarkDirtyMixin<TransactLogValidator> {
    void mark_dirty(size_t, size_t) { }
};

// Move the value at container[from] to container[to], shifting everything in
// between, or do nothing if either are out of bounds
template<typename Container>
void rotate(Container& container, size_t from, size_t to)
{
    REALM_ASSERT(from != to);
    if (from >= container.size() && to >= container.size())
        return;
    if (from >= container.size() || to >= container.size())
        container.resize(std::max(from, to) + 1);
    if (from < to)
        std::rotate(begin(container) + from, begin(container) + from + 1, begin(container) + to + 1);
    else
        std::rotate(begin(container) + to, begin(container) + from, begin(container) + from + 1);
}

// Insert a default-initialized value at pos if there is anything after pos in the container.
template<typename Container>
void insert_empty_at(Container& container, size_t pos)
{
    if (pos < container.size())
        container.emplace(container.begin() + pos);
}

// Shift `value` to reflect a move from `from` to `to`
void adjust_for_move(size_t& value, size_t from, size_t to)
{
    if (value == from)
        value = to;
    else if (value > from && value <= to)
        --value;
    else if (value < from && value >= to)
        ++value;
}

void adjust_for_move(std::vector<size_t>& values, size_t from, size_t to)
{
    for (auto& value : values)
        adjust_for_move(value, from, to);
}

void expand_to(std::vector<size_t>& cols, size_t i)
{
    auto old_size = cols.size();
    if (old_size > i)
        return;

    cols.resize(std::max(old_size * 2, i + 1));
    std::iota(begin(cols) + old_size, end(cols), old_size == 0 ? 0 : cols[old_size - 1] + 1);
}

void adjust_ge(std::vector<size_t>& values, size_t i)
{
    for (auto& value : values) {
        if (value >= i)
            ++value;
    }
}

// Extends TransactLogValidator to track changes made to LinkViews
class TransactLogObserver : public TransactLogValidationMixin, public MarkDirtyMixin<TransactLogObserver> {
    _impl::TransactionChangeInfo& m_info;
    _impl::CollectionChangeBuilder* m_active_list = nullptr;
    _impl::CollectionChangeBuilder* m_active_table = nullptr;
    _impl::CollectionChangeBuilder* m_active_descriptor = nullptr;

    bool m_need_move_info = false;
    bool m_is_top_level_table = true;

    _impl::CollectionChangeBuilder* find_list(size_t tbl, size_t col, size_t row)
    {
        // When there are multiple source versions there could be multiple
        // change objects for a single LinkView, in which case we need to use
        // the last one
        for (auto it = m_info.lists.rbegin(), end = m_info.lists.rend(); it != end; ++it) {
            if (it->table_ndx == tbl && it->row_ndx == row && it->col_ndx == col)
                return it->changes;
        }
        return nullptr;
    }

public:
    TransactLogObserver(_impl::TransactionChangeInfo& info)
    : m_info(info) { }

    void mark_dirty(size_t row, size_t col)
    {
        if (m_active_table)
            m_active_table->modify(row, col);
    }

    void parse_complete()
    {
        for (auto& table : m_info.tables)
            table.parse_complete();
        for (auto& list : m_info.lists)
            list.changes->clean_up_stale_moves();
    }

    bool select_descriptor(int levels, const size_t*) noexcept
    {
        if (levels == 0) // schema of selected table is being modified
            m_active_descriptor = m_active_table;
        else // schema of subtable is being modified; currently don't need to track this
            m_active_descriptor = nullptr;
        return true;
    }

    bool select_table(size_t group_level_ndx, int len, size_t const* path) noexcept
    {
        TransactLogValidationMixin::select_table(group_level_ndx, len, path);
        m_active_table = nullptr;
        m_is_top_level_table = true;

        // Nested subtables currently not supported
        if (len > 1) {
            m_is_top_level_table = false;
            return true;
        }

        auto tbl_ndx = current_table();
        if (!m_info.track_all && (tbl_ndx >= m_info.table_modifications_needed.size() || !m_info.table_modifications_needed[tbl_ndx]))
            return true;

        m_need_move_info = m_info.track_all || (tbl_ndx < m_info.table_moves_needed.size() &&
                                                m_info.table_moves_needed[tbl_ndx]);
        if (m_info.tables.size() <= tbl_ndx)
            m_info.tables.resize(std::max(m_info.tables.size() * 2, tbl_ndx + 1));
        m_active_table = &m_info.tables[tbl_ndx];

        if (len == 1) {
            // Mark the cell containing the subtable as modified since selecting
            // a table is always followed by a modification of some sort
            size_t col = path[0];
            size_t row = path[1];
            mark_dirty(row, col);

            m_active_table = nullptr;
            m_is_top_level_table = false;
            if (auto table = find_list(current_table(), col, row)) {
                m_active_table = table;
                m_need_move_info = true;
            }
        }
        return true;
    }

    bool select_link_list(size_t col, size_t row, size_t)
    {
        mark_dirty(row, col);
        m_active_list = find_list(current_table(), col, row);
        return true;
    }

    bool link_list_set(size_t index, size_t, size_t)
    {
        if (m_active_list)
            m_active_list->modify(index);
        return true;
    }

    bool link_list_insert(size_t index, size_t, size_t)
    {
        if (m_active_list)
            m_active_list->insert(index);
        return true;
    }

    bool link_list_erase(size_t index, size_t)
    {
        if (m_active_list)
            m_active_list->erase(index);
        return true;
    }

    bool link_list_nullify(size_t index, size_t prior_size)
    {
        return link_list_erase(index, prior_size);
    }

    bool link_list_swap(size_t index1, size_t index2)
    {
        link_list_set(index1, 0, npos);
        link_list_set(index2, 0, npos);
        return true;
    }

    bool link_list_clear(size_t old_size)
    {
        if (m_active_list)
            m_active_list->clear(old_size);
        return true;
    }

    bool link_list_move(size_t from, size_t to)
    {
        if (m_active_list)
            m_active_list->move(from, to);
        return true;
    }

    bool insert_empty_rows(size_t row_ndx, size_t num_rows_to_insert, size_t, bool)
    {
        if (m_active_table)
            m_active_table->insert(row_ndx, num_rows_to_insert, m_need_move_info);
        if (!m_is_top_level_table)
            return true;
        for (auto& list : m_info.lists) {
            if (list.table_ndx == current_table() && list.row_ndx >= row_ndx)
                list.row_ndx += num_rows_to_insert;
        }
        return true;
    }

    bool add_row_with_key(size_t row_ndx, size_t prior_num_rows, size_t, int64_t)
    {
        insert_empty_rows(row_ndx, 1, prior_num_rows, false);
        return true;
    }

    bool erase_rows(size_t row_ndx, size_t, size_t prior_num_rows, bool unordered)
    {
        if (!unordered) {
            if (m_active_table)
                m_active_table->erase(row_ndx);
            return true;
        }
        size_t last_row = prior_num_rows - 1;
        if (m_active_table)
            m_active_table->move_over(row_ndx, last_row, m_need_move_info);

        if (!m_is_top_level_table)
            return true;
        for (size_t i = 0; i < m_info.lists.size(); ++i) {
            auto& list = m_info.lists[i];
            if (list.table_ndx != current_table())
                continue;
            if (list.row_ndx == row_ndx) {
                if (i + 1 < m_info.lists.size())
                    m_info.lists[i] = std::move(m_info.lists.back());
                m_info.lists.pop_back();
                continue;
            }
            if (list.row_ndx == last_row)
                list.row_ndx = row_ndx;
        }

        return true;
    }

    bool swap_rows(size_t row_ndx_1, size_t row_ndx_2) {
        REALM_ASSERT(row_ndx_1 < row_ndx_2);
        if (!m_is_top_level_table) {
            if (m_active_table) {
                m_active_table->move(row_ndx_1, row_ndx_2);
                if (row_ndx_1 + 1 != row_ndx_2)
                    m_active_table->move(row_ndx_2 - 1, row_ndx_1);
            }
            return true;
        }

        if (m_active_table)
            m_active_table->swap(row_ndx_1, row_ndx_2, m_need_move_info);
        for (auto& list : m_info.lists) {
            if (list.table_ndx == current_table()) {
                if (list.row_ndx == row_ndx_1)
                    list.row_ndx = row_ndx_2;
                else if (list.row_ndx == row_ndx_2)
                    list.row_ndx = row_ndx_1;
            }
        }
        return true;
    }

    bool move_row(size_t from_ndx, size_t to_ndx) {
        // Move row is not supported for top level tables:
        REALM_ASSERT(!m_active_table || !m_is_top_level_table);

        if (m_active_table)
            m_active_table->move(from_ndx, to_ndx);
        return true;
    }

    bool merge_rows(size_t from, size_t to)
    {
        if (m_active_table)
            m_active_table->subsume(from, to, m_need_move_info);
        if (!m_is_top_level_table)
            return true;
        for (auto& list : m_info.lists) {
            if (list.table_ndx == current_table() && list.row_ndx == from)
                list.row_ndx = to;
        }
        return true;
    }

    bool clear_table(size_t=0)
    {
        auto tbl_ndx = current_table();
        if (m_active_table)
            m_active_table->clear(std::numeric_limits<size_t>::max());
        if (!m_is_top_level_table)
            return true;
        auto it = remove_if(begin(m_info.lists), end(m_info.lists),
                            [&](auto const& lv) { return lv.table_ndx == tbl_ndx; });
        m_info.lists.erase(it, end(m_info.lists));
        return true;
    }

    bool insert_column(size_t ndx, DataType, StringData, bool)
    {
        m_info.schema_changed = true;

        if (m_active_descriptor)
            m_active_descriptor->insert_column(ndx);
        if (m_active_descriptor != m_active_table || !m_is_top_level_table)
            return true;
        for (auto& list : m_info.lists) {
            if (list.table_ndx == current_table() && list.col_ndx >= ndx)
                ++list.col_ndx;
        }
        if (m_info.column_indices.size() <= current_table())
            m_info.column_indices.resize(current_table() + 1);
        auto& indices = m_info.column_indices[current_table()];
        expand_to(indices, ndx);
        insert_empty_at(indices, ndx);
        indices[ndx] = npos;
        return true;
    }

    void prepare_table_indices()
    {
        if (m_info.table_indices.empty() && !m_info.table_modifications_needed.empty()) {
            m_info.table_indices.resize(m_info.table_modifications_needed.size());
            std::iota(begin(m_info.table_indices), end(m_info.table_indices), 0);
        }
    }

    bool insert_group_level_table(size_t ndx, size_t, StringData)
    {
        m_info.schema_changed = true;

        for (auto& list : m_info.lists) {
            if (list.table_ndx >= ndx)
                ++list.table_ndx;
        }
        prepare_table_indices();
        adjust_ge(m_info.table_indices, ndx);
        insert_empty_at(m_info.tables, ndx);
        insert_empty_at(m_info.table_moves_needed, ndx);
        insert_empty_at(m_info.table_modifications_needed, ndx);
        return true;
    }

    bool move_column(size_t from, size_t to)
    {
        m_info.schema_changed = true;

        if (m_active_descriptor)
            m_active_descriptor->move_column(from, to);
        if (m_active_descriptor != m_active_table || !m_is_top_level_table)
            return true;
        for (auto& list : m_info.lists) {
            if (list.table_ndx == current_table())
                adjust_for_move(list.col_ndx, from, to);
        }
        if (m_info.column_indices.size() <= current_table())
            m_info.column_indices.resize(current_table() + 1);
        expand_to(m_info.column_indices[current_table()], std::max(from, to) + 1);
        rotate(m_info.column_indices[current_table()], from, to);
        return true;
    }

    bool move_group_level_table(size_t from, size_t to)
    {
        m_info.schema_changed = true;

        for (auto& list : m_info.lists)
            adjust_for_move(list.table_ndx, from, to);

        prepare_table_indices();
        adjust_for_move(m_info.table_indices, from, to);
        rotate(m_info.tables, from, to);
        rotate(m_info.table_modifications_needed, from, to);
        rotate(m_info.table_moves_needed, from, to);
        return true;
    }

    bool insert_link_column(size_t ndx, DataType type, StringData name, size_t, size_t) { return insert_column(ndx, type, name, false); }
};

class KVOTransactLogObserver : public TransactLogObserver {
    KVOAdapter m_adapter;
    _impl::NotifierPackage& m_notifiers;
    SharedGroup& m_sg;

public:
    KVOTransactLogObserver(std::vector<BindingContext::ObserverState>& observers,
                           BindingContext* context,
                           _impl::NotifierPackage& notifiers,
                           SharedGroup& sg)
    : TransactLogObserver(m_adapter)
    , m_adapter(observers, context)
    , m_notifiers(notifiers)
    , m_sg(sg)
    {
    }

    ~KVOTransactLogObserver()
    {
        m_adapter.after(m_sg);
    }

    void parse_complete()
    {
        TransactLogObserver::parse_complete();
        m_adapter.before(m_sg);

        using sgf = _impl::SharedGroupFriend;
        m_notifiers.package_and_wait(sgf::get_version_of_latest_snapshot(m_sg));
        m_notifiers.before_advance();
    }
};

template<typename Func>
void advance_with_notifications(BindingContext* context, const std::unique_ptr<SharedGroup>& sg,
                                Func&& func, _impl::NotifierPackage& notifiers)
{
    auto old_version = sg->get_version_of_current_transaction();
    std::vector<BindingContext::ObserverState> observers;
    if (context) {
        observers = context->get_observed_rows();
    }

    // Advancing to the latest version with notifiers requires using the full
    // transaction log observer so that we have a point where we know what
    // version we're going to before we actually advance to that version
    if (observers.empty() && (!notifiers || notifiers.version())) {
        notifiers.before_advance();
        func(TransactLogValidator());
        auto new_version = sg->get_version_of_current_transaction();
        if (context && old_version != new_version)
            context->did_change({}, {});
        if (!sg) // did_change() could close the Realm. Just return if it does.
            return;
        if (context)
            context->will_send_notifications();
        if (!sg) // will_send_notifications() could close the Realm. Just return if it does.
            return;
        // did_change() can change the read version, and if it does we can't
        // deliver notifiers
        if (new_version == sg->get_version_of_current_transaction())
            notifiers.deliver(*sg);
        notifiers.after_advance();
        if (sg && context)
            context->did_send_notifications();
        return;
    }

    if (context)
        context->will_send_notifications();
    func(KVOTransactLogObserver(observers, context, notifiers, *sg));
    notifiers.package_and_wait(sg->get_version_of_current_transaction().version); // is a no-op if parse_complete() was called
    notifiers.deliver(*sg);
    notifiers.after_advance();
    if (context)
        context->did_send_notifications();
}

} // anonymous namespace

namespace realm {
namespace _impl {

UnsupportedSchemaChange::UnsupportedSchemaChange()
: std::logic_error("Schema mismatch detected: another process has modified the Realm file's schema in an incompatible way")
{
}

namespace transaction {
void advance(SharedGroup& sg, BindingContext*, VersionID version)
{
    LangBindHelper::advance_read(sg, TransactLogValidator(), version);
}

void advance(const std::unique_ptr<SharedGroup>& sg, BindingContext* context, NotifierPackage& notifiers)
{
    advance_with_notifications(context, sg, [&](auto&&... args) {
        LangBindHelper::advance_read(*sg, std::move(args)..., notifiers.version().value_or(VersionID{}));
    }, notifiers);
}

void begin_without_validation(SharedGroup& sg)
{
    LangBindHelper::promote_to_write(sg);
}

void begin(const std::unique_ptr<SharedGroup>& sg, BindingContext* context, NotifierPackage& notifiers)
{
    advance_with_notifications(context, sg, [&](auto&&... args) {
        LangBindHelper::promote_to_write(*sg, std::move(args)...);
    }, notifiers);
}

void commit(SharedGroup& sg)
{
    LangBindHelper::commit_and_continue_as_read(sg);
}

void cancel(SharedGroup& sg, BindingContext* context)
{
    std::vector<BindingContext::ObserverState> observers;
    if (context) {
        observers = context->get_observed_rows();
    }
    if (observers.empty()) {
        LangBindHelper::rollback_and_continue_as_read(sg);
        return;
    }

    _impl::NotifierPackage notifiers;
    LangBindHelper::rollback_and_continue_as_read(sg, KVOTransactLogObserver(observers, context, notifiers, sg));
}

void advance(SharedGroup& sg, TransactionChangeInfo& info, VersionID version)
{
    if (!info.track_all && info.table_modifications_needed.empty() && info.lists.empty()) {
        LangBindHelper::advance_read(sg, version);
    }
    else {
        LangBindHelper::advance_read(sg, TransactLogObserver(info), version);
    }
}

} // namespace transaction
} // namespace _impl
} // namespace realm
