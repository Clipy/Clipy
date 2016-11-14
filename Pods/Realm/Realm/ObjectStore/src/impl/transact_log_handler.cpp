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

using namespace realm;

namespace {
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
};

class TransactLogValidationMixin {
    // Index of currently selected table
    size_t m_current_table = 0;

    // Tables which were created during the transaction being processed, which
    // can have columns inserted without a schema version bump
    std::vector<size_t> m_new_tables;

    REALM_NORETURN
    REALM_NOINLINE
    void schema_error()
    {
        throw std::logic_error("Schema mismatch detected: another process has modified the Realm file's schema in an incompatible way");
    }

    // Throw an exception if the currently modified table already existed before
    // the current set of modifications
    bool schema_error_unless_new_table()
    {
        if (schema_mode == SchemaMode::Additive) {
            return true;
        }
        if (std::find(begin(m_new_tables), end(m_new_tables), m_current_table) != end(m_new_tables)) {
            return true;
        }
        schema_error();
    }

protected:
    size_t current_table() const noexcept { return m_current_table; }

public:
    SchemaMode schema_mode;

    // Schema changes which don't involve a change in the schema version are
    // allowed
    bool add_search_index(size_t) { return true; }
    bool remove_search_index(size_t) { return true; }

    // Creating entirely new tables without a schema version bump is allowed, so
    // we need to track if new columns are being added to a new table or an
    // existing one
    bool insert_group_level_table(size_t table_ndx, size_t, StringData)
    {
        // Shift any previously added tables after the new one
        for (auto& table : m_new_tables) {
            if (table >= table_ndx)
                ++table;
        }
        m_new_tables.push_back(table_ndx);
        m_current_table = table_ndx;
        return true;
    }
    bool insert_column(size_t, DataType, StringData, bool) { return schema_error_unless_new_table(); }
    bool insert_link_column(size_t, DataType, StringData, size_t, size_t) { return schema_error_unless_new_table(); }
    bool set_link_type(size_t, LinkType) { return schema_error_unless_new_table(); }
    bool move_column(size_t, size_t) { return schema_error_unless_new_table(); }
    bool move_group_level_table(size_t, size_t) { return schema_error_unless_new_table(); }

    // Removing or renaming things while a Realm is open is never supported
    bool erase_group_level_table(size_t, size_t) { schema_error(); }
    bool rename_group_level_table(size_t, StringData) { schema_error(); }
    bool erase_column(size_t) { schema_error(); }
    bool erase_link_column(size_t, size_t, size_t) { schema_error(); }
    bool rename_column(size_t, StringData) { schema_error(); }

    bool select_descriptor(int levels, const size_t*)
    {
        // subtables not supported
        return levels == 0;
    }

    bool select_table(size_t group_level_ndx, int, const size_t*) noexcept
    {
        m_current_table = group_level_ndx;
        return true;
    }

    bool select_link_list(size_t, size_t, size_t) { return true; }

    // Non-schema changes are all allowed
    void parse_complete() { }
    bool insert_empty_rows(size_t, size_t, size_t, bool) { return true; }
    bool erase_rows(size_t, size_t, size_t, bool) { return true; }
    bool swap_rows(size_t, size_t) { return true; }
    bool clear_table() noexcept { return true; }
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
    TransactLogValidator(SchemaMode schema_mode) { this->schema_mode = schema_mode; }
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
        std::rotate(begin(container) + from, begin(container) + to, begin(container) + to + 1);
    else
        std::rotate(begin(container) + to, begin(container) + from, begin(container) + from + 1);
}

// Insert a default-initialized value at pos if there is anything after pos in the container.
template<typename Container>
void insert_empty_at(Container& container, size_t pos)
{
    if (pos < container.size())
        container.insert(container.begin() + pos, typename Container::value_type{});
}

// Shift `value` to reflect a move from `from` to `to`
void adjust_for_move(size_t& value, size_t from, size_t to)
{
    if (value == from)
        value = to;
    else if (value > from && value < to)
        --value;
    else if (value < from && value >= to)
        ++value;
}

// Extends TransactLogValidator to also track changes and report it to the
// binding context if any properties are being observed
class TransactLogObserver : public TransactLogValidationMixin, public MarkDirtyMixin<TransactLogObserver> {
    using ColumnInfo = BindingContext::ColumnInfo;
    using ObserverState = BindingContext::ObserverState;

    // Observed table rows which need change information
    std::vector<ObserverState> m_observers;
    // Userdata pointers for rows which have been deleted
    std::vector<void *> invalidated;
    // Delegate to send change information to
    BindingContext* m_context;

    // Change information for the currently selected LinkList, if any
    ColumnInfo* m_active_linklist = nullptr;

    // Get the change info for the given column, creating it if needed
    static ColumnInfo& get_change(ObserverState& state, size_t i)
    {
        expand_to(state, i);
        return state.changes[i];
    }

    static void expand_to(ObserverState& state, size_t i)
    {
        auto old_size = state.changes.size();
        if (old_size <= i) {
            auto new_size = std::max(state.changes.size() * 2, i + 1);
            state.changes.resize(new_size);
            size_t base = old_size == 0 ? 0 : state.changes[old_size - 1].initial_column_index + 1;
            for (size_t i = old_size; i < new_size; ++i)
                state.changes[i].initial_column_index = i - old_size + base;
        }
    }

    // Remove the given observer from the list of observed objects and add it
    // to the listed of invalidated objects
    void invalidate(ObserverState *o)
    {
        invalidated.push_back(o->info);
        m_observers.erase(m_observers.begin() + (o - &m_observers[0]));
    }

public:
    template<typename Func>
    TransactLogObserver(BindingContext* context, SharedGroup& sg, Func&& func, util::Optional<SchemaMode> schema_mode)
    : m_context(context)
    {
        auto old_version = sg.get_version_of_current_transaction();
        if (context) {
            m_observers = context->get_observed_rows();
        }
        if (m_observers.empty()) {
            if (schema_mode) {
                func(TransactLogValidator(*schema_mode));
            }
            else {
                func();
            }
            if (context && old_version != sg.get_version_of_current_transaction()) {
                context->did_change({}, {});
            }
            return;
        }

        std::sort(begin(m_observers), end(m_observers));
        func(*this);
        context->did_change(m_observers, invalidated);
    }

    // Mark the given row/col as needing notifications sent
    void mark_dirty(size_t row_ndx, size_t col_ndx)
    {
        auto it = lower_bound(begin(m_observers), end(m_observers), ObserverState{current_table(), row_ndx, nullptr});
        if (it != end(m_observers) && it->table_ndx == current_table() && it->row_ndx == row_ndx) {
            get_change(*it, col_ndx).kind = ColumnInfo::Kind::Set;
        }
    }

    // Called at the end of the transaction log immediately before the version
    // is advanced
    void parse_complete()
    {
        m_context->will_change(m_observers, invalidated);
    }

    bool insert_group_level_table(size_t table_ndx, size_t prior_size, StringData name)
    {
        for (auto& observer : m_observers) {
            if (observer.table_ndx >= table_ndx)
                ++observer.table_ndx;
        }
        TransactLogValidationMixin::insert_group_level_table(table_ndx, prior_size, name);
        return true;
    }

    bool insert_empty_rows(size_t row_ndx, size_t num_rows, size_t prior_size, bool)
    {
        if (row_ndx != prior_size) {
            for (auto& observer : m_observers) {
                if (observer.table_ndx == current_table() && observer.row_ndx >= row_ndx)
                    observer.row_ndx += num_rows;
            }
        }
        return true;
    }

    bool erase_rows(size_t row_ndx, size_t rows_to_erase, size_t prior_size, bool unordered)
    {
        REALM_ASSERT(unordered || rows_to_erase == 1);
        size_t last_row_ndx = prior_size - 1;

        if (unordered) {
            auto end = m_observers.end();
            auto row_it = lower_bound(begin(m_observers), end, ObserverState{current_table(), row_ndx, nullptr});
            auto last_it = lower_bound(row_it, end, ObserverState{current_table(), last_row_ndx, nullptr});
            bool have_row = row_it != end && row_it->table_ndx == current_table() && row_it->row_ndx == row_ndx;
            bool have_last = last_it != end && last_it->table_ndx == current_table() && last_it->row_ndx == last_row_ndx;
            if (have_row && have_last) {
                invalidated.push_back(row_it->info);
                row_it->info = last_it->info;
                row_it->changes = std::move(last_it->changes);
                m_observers.erase(last_it);
            }
            else if (have_row) {
                invalidated.push_back(row_it->info);
                m_observers.erase(row_it);
            }
            else if (have_last) {
                last_it->row_ndx = row_ndx;
                std::rotate(row_it, last_it, end);
            }
        }
        else {
            for (size_t i = 0; i < m_observers.size(); ++i) {
                auto& o = m_observers[i];
                if (o.table_ndx == current_table()) {
                    if (o.row_ndx == row_ndx) {
                        invalidate(&o);
                        --i;
                    }
                    else if (o.row_ndx > row_ndx) {
                        o.row_ndx -= rows_to_erase;
                    }
                }
            }
        }
        return true;
    }

    bool swap_rows(size_t row_ndx_1, size_t row_ndx_2)
    {
        REALM_ASSERT(row_ndx_1 < row_ndx_2); // this is enforced by core

        auto end = m_observers.end();
        auto it_1 = lower_bound(begin(m_observers), end, ObserverState{current_table(), row_ndx_1, nullptr});
        auto it_2 = lower_bound(it_1, end, ObserverState{current_table(), row_ndx_2, nullptr});
        bool have_row_1 = it_1 != end && it_1->table_ndx == current_table() && it_1->row_ndx == row_ndx_1;
        bool have_row_2 = it_2 != end && it_2->table_ndx == current_table() && it_2->row_ndx == row_ndx_2;

        if (have_row_1 && have_row_2) {
            std::swap(it_1->info, it_2->info);
            std::swap(it_1->changes, it_2->changes);
        }
        else if (have_row_1) {
            it_1->row_ndx = row_ndx_2;
            std::rotate(it_1, it_1 + 1, it_2);
        }
        else if (have_row_2) {
            it_2->row_ndx = row_ndx_1;
            std::rotate(it_1, it_2, end);
        }

        return true;
    }

    bool merge_rows(size_t from, size_t to)
    {
        REALM_ASSERT(from != to);

        auto end = m_observers.end();
        auto from_it = lower_bound(begin(m_observers), end, ObserverState{current_table(), from, nullptr});
        if (from_it == end || from_it->table_ndx != current_table() || from_it->row_ndx != from)
            return true;

        auto to_it = lower_bound(begin(m_observers), end, ObserverState{current_table(), to, nullptr});
        // an observer for the subsuming row should not already exist
        REALM_ASSERT_DEBUG(to_it == end || (ObserverState{current_table(), to, nullptr}) < *to_it);

        from_it->row_ndx = to;
        if (from < to)
            std::rotate(from_it, from_it + 1, to_it);
        else
            std::rotate(to_it, from_it, from_it + 1);
        return true;
    }

    bool clear_table()
    {
        for (size_t i = 0; i < m_observers.size(); ) {
            auto& o = m_observers[i];
            if (o.table_ndx == current_table()) {
                invalidate(&o);
            }
            else {
                ++i;
            }
        }
        return true;
    }

    bool select_link_list(size_t col, size_t row, size_t)
    {
        m_active_linklist = nullptr;
        for (auto& o : m_observers) {
            if (o.table_ndx == current_table() && o.row_ndx == row) {
                m_active_linklist = &get_change(o, col);
                break;
            }
        }
        return true;
    }

    void append_link_list_change(ColumnInfo::Kind kind, size_t index) {
        ColumnInfo *o = m_active_linklist;
        if (!o || o->kind == ColumnInfo::Kind::SetAll) {
            // Active LinkList isn't observed or already has multiple kinds of changes
            return;
        }

        if (o->kind == ColumnInfo::Kind::None) {
            o->kind = kind;
            o->indices.add(index);
        }
        else if (o->kind == kind) {
            if (kind == ColumnInfo::Kind::Remove) {
                o->indices.add_shifted(index);
            }
            else if (kind == ColumnInfo::Kind::Insert) {
                o->indices.insert_at(index);
            }
            else {
                o->indices.add(index);
            }
        }
        else {
            // Array KVO can only send a single kind of change at a time, so
            // if there are multiple just give up and send "Set"
            o->indices.set(0);
            o->kind = ColumnInfo::Kind::SetAll;
        }
    }

    bool link_list_set(size_t index, size_t, size_t)
    {
        append_link_list_change(ColumnInfo::Kind::Set, index);
        return true;
    }

    bool link_list_insert(size_t index, size_t, size_t)
    {
        append_link_list_change(ColumnInfo::Kind::Insert, index);
        return true;
    }

    bool link_list_erase(size_t index, size_t)
    {
        append_link_list_change(ColumnInfo::Kind::Remove, index);
        return true;
    }

    bool link_list_nullify(size_t index, size_t)
    {
        append_link_list_change(ColumnInfo::Kind::Remove, index);
        return true;
    }

    bool link_list_swap(size_t index1, size_t index2)
    {
        append_link_list_change(ColumnInfo::Kind::Set, index1);
        append_link_list_change(ColumnInfo::Kind::Set, index2);
        return true;
    }

    bool link_list_clear(size_t old_size)
    {
        ColumnInfo *o = m_active_linklist;
        if (!o || o->kind == ColumnInfo::Kind::SetAll) {
            return true;
        }

        if (o->kind == ColumnInfo::Kind::Remove)
            old_size += o->indices.count();
        else if (o->kind == ColumnInfo::Kind::Insert)
            old_size -= o->indices.count();

        o->indices.set(old_size);

        o->kind = ColumnInfo::Kind::Remove;
        return true;
    }

    bool link_list_move(size_t from, size_t to)
    {
        ColumnInfo *o = m_active_linklist;
        if (!o || o->kind == ColumnInfo::Kind::SetAll) {
            return true;
        }
        if (from > to) {
            std::swap(from, to);
        }

        if (o->kind == ColumnInfo::Kind::None) {
            o->kind = ColumnInfo::Kind::Set;
        }
        if (o->kind == ColumnInfo::Kind::Set) {
            for (size_t i = from; i <= to; ++i)
                o->indices.add(i);
        }
        else {
            o->indices.set(0);
            o->kind = ColumnInfo::Kind::SetAll;
        }
        return true;
    }

    bool insert_column(size_t ndx, DataType, StringData, bool)
    {
        for (auto& observer : m_observers) {
            if (observer.table_ndx == current_table()) {
                expand_to(observer, ndx);
                insert_empty_at(observer.changes, ndx);
            }
        }
        return true;
    }

    bool move_column(size_t from, size_t to)
    {
        for (auto& observer : m_observers) {
            if (observer.table_ndx == current_table()) {
                // have to initialize the columns one past the moved one so that
                // we can later initialize any more columns after that
                expand_to(observer, std::max(from, to) + 1);
                rotate(observer.changes, from, to);
            }
        }
        return true;
    }

    bool move_group_level_table(size_t from, size_t to)
    {
        for (auto& observer : m_observers)
            adjust_for_move(observer.table_ndx, from, to);
        return true;
    }

    bool insert_link_column(size_t ndx, DataType type, StringData name, size_t, size_t) { return insert_column(ndx, type, name, false); }

};

// Extends TransactLogValidator to track changes made to LinkViews
class LinkViewObserver : public TransactLogValidationMixin, public MarkDirtyMixin<LinkViewObserver> {
    _impl::TransactionChangeInfo& m_info;
    _impl::CollectionChangeBuilder* m_active = nullptr;

    _impl::CollectionChangeBuilder* get_change()
    {
        auto tbl_ndx = current_table();
        if (!m_info.track_all && (tbl_ndx >= m_info.table_modifications_needed.size() || !m_info.table_modifications_needed[tbl_ndx]))
            return nullptr;
        if (m_info.tables.size() <= tbl_ndx) {
            m_info.tables.resize(std::max(m_info.tables.size() * 2, tbl_ndx + 1));
        }
        return &m_info.tables[tbl_ndx];
    }

    bool need_move_info() const
    {
        auto tbl_ndx = current_table();
        return m_info.track_all || (tbl_ndx < m_info.table_moves_needed.size() && m_info.table_moves_needed[tbl_ndx]);
    }

public:
    LinkViewObserver(_impl::TransactionChangeInfo& info)
    : m_info(info) { }

    void mark_dirty(size_t row, size_t)
    {
        if (auto change = get_change())
            change->modify(row);
    }

    void parse_complete()
    {
        for (auto& table : m_info.tables) {
            table.parse_complete();
        }
        for (auto& list : m_info.lists) {
            list.changes->clean_up_stale_moves();
        }
    }

    bool select_link_list(size_t col, size_t row, size_t)
    {
        mark_dirty(row, col);

        m_active = nullptr;
        // When there are multiple source versions there could be multiple
        // change objects for a single LinkView, in which case we need to use
        // the last one
        for (auto it = m_info.lists.rbegin(), end = m_info.lists.rend(); it != end; ++it) {
            if (it->table_ndx == current_table() && it->row_ndx == row && it->col_ndx == col) {
                m_active = it->changes;
                break;
            }
        }
        return true;
    }

    bool link_list_set(size_t index, size_t, size_t)
    {
        if (m_active)
            m_active->modify(index);
        return true;
    }

    bool link_list_insert(size_t index, size_t, size_t)
    {
        if (m_active)
            m_active->insert(index);
        return true;
    }

    bool link_list_erase(size_t index, size_t)
    {
        if (m_active)
            m_active->erase(index);
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
        if (m_active)
            m_active->clear(old_size);
        return true;
    }

    bool link_list_move(size_t from, size_t to)
    {
        if (m_active)
            m_active->move(from, to);
        return true;
    }

    bool insert_empty_rows(size_t row_ndx, size_t num_rows_to_insert, size_t, bool unordered)
    {
        REALM_ASSERT(!unordered);
        if (auto change = get_change())
            change->insert(row_ndx, num_rows_to_insert, need_move_info());
        for (auto& list : m_info.lists) {
            if (list.table_ndx == current_table() && list.row_ndx >= row_ndx)
                list.row_ndx += num_rows_to_insert;
        }

        return true;
    }

    bool erase_rows(size_t row_ndx, size_t, size_t prior_num_rows, bool unordered)
    {
        REALM_ASSERT(unordered);
        size_t last_row = prior_num_rows - 1;

        for (auto it = begin(m_info.lists); it != end(m_info.lists); ) {
            if (it->table_ndx == current_table()) {
                if (it->row_ndx == row_ndx) {
                    *it = std::move(m_info.lists.back());
                    m_info.lists.pop_back();
                    continue;
                }
                if (it->row_ndx == last_row)
                    it->row_ndx = row_ndx;
            }
            ++it;
        }

        if (auto change = get_change())
            change->move_over(row_ndx, last_row, need_move_info());
        return true;
    }

    bool swap_rows(size_t row_ndx_1, size_t row_ndx_2) {
        REALM_ASSERT(row_ndx_1 < row_ndx_2);
        for (auto& list : m_info.lists) {
            if (list.table_ndx == current_table()) {
                if (list.row_ndx == row_ndx_1)
                    list.row_ndx = row_ndx_2;
                else if (list.row_ndx == row_ndx_2)
                    list.row_ndx = row_ndx_1;
            }
        }
        if (auto change = get_change())
            change->swap(row_ndx_1, row_ndx_2, need_move_info());
        return true;
    }

    bool merge_rows(size_t from, size_t to)
    {
        for (auto& list : m_info.lists) {
            if (list.table_ndx == current_table() && list.row_ndx == from)
                list.row_ndx = to;
        }
        if (auto change = get_change())
            change->subsume(from, to, need_move_info());
        return true;
    }

    bool clear_table()
    {
        auto tbl_ndx = current_table();
        auto it = remove_if(begin(m_info.lists), end(m_info.lists),
                            [&](auto const& lv) { return lv.table_ndx == tbl_ndx; });
        m_info.lists.erase(it, end(m_info.lists));
        if (auto change = get_change())
            change->clear(std::numeric_limits<size_t>::max());
        return true;
    }

    bool insert_column(size_t ndx, DataType, StringData, bool)
    {
        for (auto& list : m_info.lists) {
            if (list.table_ndx == current_table() && list.col_ndx >= ndx)
                ++list.col_ndx;
        }
        return true;
    }

    bool insert_group_level_table(size_t ndx, size_t, StringData)
    {
        for (auto& list : m_info.lists) {
            if (list.table_ndx >= ndx)
                ++list.table_ndx;
        }
        insert_empty_at(m_info.tables, ndx);
        insert_empty_at(m_info.table_moves_needed, ndx);
        insert_empty_at(m_info.table_modifications_needed, ndx);
        return true;
    }

    bool move_column(size_t from, size_t to)
    {
        for (auto& list : m_info.lists) {
            if (list.table_ndx == current_table())
                adjust_for_move(list.col_ndx, from, to);
        }
        return true;
    }

    bool move_group_level_table(size_t from, size_t to)
    {
        for (auto& list : m_info.lists)
            adjust_for_move(list.table_ndx, from, to);
        rotate(m_info.tables, from, to);
        rotate(m_info.table_modifications_needed, from, to);
        rotate(m_info.table_moves_needed, from, to);
        return true;
    }

    bool insert_link_column(size_t ndx, DataType type, StringData name, size_t, size_t) { return insert_column(ndx, type, name, false); }

};
} // anonymous namespace

namespace realm {
namespace _impl {
namespace transaction {
void advance(SharedGroup& sg, BindingContext* context, SchemaMode schema_mode, VersionID version)
{
    TransactLogObserver(context, sg, [&](auto&&... args) {
        LangBindHelper::advance_read(sg, std::move(args)..., version);
    }, schema_mode);
}

void begin_without_validation(SharedGroup& sg)
{
    LangBindHelper::promote_to_write(sg);
}

void begin(SharedGroup& sg, BindingContext* context, SchemaMode schema_mode)
{
    TransactLogObserver(context, sg, [&](auto&&... args) {
        LangBindHelper::promote_to_write(sg, std::move(args)...);
    }, schema_mode);
}

void commit(SharedGroup& sg, BindingContext* context)
{
    LangBindHelper::commit_and_continue_as_read(sg);

    if (context) {
        context->did_change({}, {});
    }
}

void cancel(SharedGroup& sg, BindingContext* context)
{
    TransactLogObserver(context, sg, [&](auto&&... args) {
        LangBindHelper::rollback_and_continue_as_read(sg, std::move(args)...);
    }, util::none);
}

void advance(SharedGroup& sg,
             TransactionChangeInfo& info,
             VersionID version)
{
    if (!info.track_all && info.table_modifications_needed.empty() && info.lists.empty()) {
        LangBindHelper::advance_read(sg, version);
    }
    else {
        LangBindHelper::advance_read(sg, LinkViewObserver(info), version);
    }

}

} // namespace transaction
} // namespace _impl
} // namespace realm
