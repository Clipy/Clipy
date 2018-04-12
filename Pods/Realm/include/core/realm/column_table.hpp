/*************************************************************************
 *
 * Copyright 2016 Realm Inc.
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

#ifndef REALM_COLUMN_TABLE_HPP
#define REALM_COLUMN_TABLE_HPP

#include <vector>
#include <mutex>

#include <realm/util/features.h>
#include <memory>
#include <realm/column.hpp>
#include <realm/table.hpp>

namespace realm {


/// Base class for any type of column that can contain subtables.
// FIXME: Don't derive from IntegerColumn, but define a BpTree<ref_type> specialization.
class SubtableColumnBase : public IntegerColumn, public Table::Parent {
public:
    void discard_child_accessors() noexcept;

    ~SubtableColumnBase() noexcept override;

    static ref_type create(Allocator&, size_t size = 0);

    TableRef get_subtable_accessor(size_t) const noexcept override;

    void insert_rows(size_t, size_t, size_t, bool) override;
    void erase_rows(size_t, size_t, size_t, bool) override;
    void move_last_row_over(size_t, size_t, bool) override;
    void clear(size_t, bool) override;
    void swap_rows(size_t, size_t) override;
    void discard_subtable_accessor(size_t) noexcept override;
    void update_from_parent(size_t) noexcept override;
    void adj_acc_insert_rows(size_t, size_t) noexcept override;
    void adj_acc_erase_row(size_t) noexcept override;
    void adj_acc_move_over(size_t, size_t) noexcept override;
    void adj_acc_clear_root_table() noexcept override;
    void adj_acc_swap_rows(size_t, size_t) noexcept override;
    void adj_acc_move_row(size_t, size_t) noexcept override;
    void mark(int) noexcept override;
    bool supports_search_index() const noexcept override
    {
        return false;
    }
    StringIndex* create_search_index() override
    {
        return nullptr;
    }
    bool is_null(size_t ndx) const noexcept override
    {
        return get_as_ref(ndx) == 0;
    }

    void verify() const override;
    void verify(const Table&, size_t) const override;

protected:
    /// A pointer to the table that this column is part of. For a free-standing
    /// column, this pointer is null.
    Table* const m_table;

    struct SubtableMap {
        bool empty() const noexcept
        {
            return m_entries.empty();
        }
        Table* find(size_t subtable_ndx) const noexcept;
        void add(size_t subtable_ndx, Table*);
        // Returns true if, and only if at least one entry was detached and
        // removed from the map.
        bool detach_and_remove_all() noexcept;
        // Returns true if, and only if the entry was found and removed, and it
        // was the last entry in the map.
        bool detach_and_remove(size_t subtable_ndx) noexcept;
        // Returns true if, and only if the entry was found and removed, and it
        // was the last entry in the map.
        bool remove(Table*) noexcept;
        void update_from_parent(size_t old_baseline) const noexcept;
        template <bool fix_ndx_in_parent>
        void adj_insert_rows(size_t row_ndx, size_t num_rows_inserted) noexcept;
        // Returns true if, and only if an entry was found and removed, and it
        // was the last entry in the map.
        template <bool fix_ndx_in_parent>
        bool adj_erase_rows(size_t row_ndx, size_t num_rows_erased) noexcept;
        // Returns true if, and only if an entry was found and removed, and it
        // was the last entry in the map.
        template <bool fix_ndx_in_parent>
        bool adj_move_over(size_t from_row_ndx, size_t to_row_ndx) noexcept;
        template <bool fix_ndx_in_parent>
        void adj_swap_rows(size_t row_ndx_1, size_t row_ndx_2) noexcept;
        template <bool fix_ndx_in_parent>
        void adj_move_row(size_t from_ndx, size_t to_ndx) noexcept;
        void adj_set_null(size_t row_ndx) noexcept;

        void update_accessors(const size_t* col_path_begin, const size_t* col_path_end,
                              _impl::TableFriend::AccessorUpdater&);
        void recursive_mark() noexcept;
        void refresh_accessor_tree();
        void verify(const SubtableColumn& parent);

    private:
        struct SubtableEntry {
            size_t m_subtable_ndx;
            Table* m_table;
        };
        typedef std::vector<SubtableEntry> entries;
        entries m_entries;
    };

    /// Contains all existing accessors that are attached to a subtable in this
    /// column. It can map a row index into a pointer to the corresponding
    /// accessor when it exists.
    ///
    /// There is an invariant in force: Either `m_table` is null, or there is an
    /// additional referece count on `*m_table` when, and only when the map is
    /// non-empty.
    mutable SubtableMap m_subtable_map;
    mutable std::recursive_mutex m_subtable_map_lock;

    SubtableColumnBase(Allocator&, ref_type, Table*, size_t column_ndx);

    /// Get a TableRef to the accessor of the specified subtable. The
    /// accessor will be created if it does not already exist.
    ///
    /// NOTE: This method must be used only for subtables with
    /// independent specs, i.e. for elements of a MixedColumn.
    TableRef get_subtable_tableref(size_t subtable_ndx);

    // Overriding method in ArrayParent
    void update_child_ref(size_t, ref_type) override;

    // Overriding method in ArrayParent
    ref_type get_child_ref(size_t) const noexcept override;

    // Overriding method in Table::Parent
    Table* get_parent_table(size_t*) noexcept override;

    // Overriding method in Table::Parent
    void child_accessor_destroyed(Table*) noexcept override;

    // Overriding method in Table::Parent
    std::recursive_mutex* get_accessor_management_lock() noexcept override
    { return &m_subtable_map_lock; }

    /// Assumes that the two tables have the same spec.
    static bool compare_subtable_rows(const Table&, const Table&);

    /// Construct a copy of the columns array of the specified table
    /// and return just the ref to that array.
    ///
    /// In the clone, no string column will be of the enumeration
    /// type.
    ref_type clone_table_columns(const Table*);

    size_t* record_subtable_path(size_t* begin, size_t* end) noexcept override;

    void update_table_accessors(const size_t* col_path_begin, const size_t* col_path_end,
                                _impl::TableFriend::AccessorUpdater&);

    /// \param row_ndx Must be `realm::npos` if appending.
    /// \param value The value to place in any newly created rows.
    /// \param num_rows The number of rows to insert.
    void do_insert(size_t row_ndx, int_fast64_t value, size_t num_rows);

    std::pair<ref_type, size_t> get_to_dot_parent(size_t ndx_in_parent) const override;

    friend class Table;
};


class SubtableColumn : public SubtableColumnBase {
public:
    using value_type = ConstTableRef;
    /// Create a subtable column accessor and attach it to a
    /// preexisting underlying structure of arrays.
    ///
    /// \param alloc The allocator to provide new memory.
    ///
    /// \param ref The memory reference of the underlying subtable that
    /// we are creating an accessor for.
    ///
    /// \param table If this column is used as part of a table you must
    /// pass a pointer to that table. Otherwise you must pass null.
    ///
    /// \param column_ndx If this column is used as part of a table
    /// you must pass the logical index of the column within that
    /// table. Otherwise you should pass zero.
    SubtableColumn(Allocator& alloc, ref_type ref, Table* table, size_t column_ndx);

    ~SubtableColumn() noexcept override
    {
    }

    // Overriding method in Table::Parent
    Spec* get_subtable_spec() noexcept override;

    size_t get_subtable_size(size_t ndx) const noexcept;

    /// Get a TableRef to the accessor of the specified subtable. The
    /// accessor will be created if it does not already exist.
    TableRef get_subtable_tableref(size_t subtable_ndx);

    ConstTableRef get_subtable_tableref(size_t subtable_ndx) const;

    /// This is to be used by the query system that does not need to
    /// modify the subtable. Will return a ref object containing a
    /// nullptr if there is no table object yet.
    ConstTableRef get(size_t subtable_ndx) const
    {
        int64_t ref = IntegerColumn::get(subtable_ndx);
        if (ref)
            return get_subtable_tableref(subtable_ndx);
        else
            return {};
    }

    // When passing a table to add() or insert() it is assumed that
    // the table spec is compatible with this column. The number of
    // columns must be the same, and the corresponding columns must
    // have the same data type (as returned by
    // Table::get_column_type()).

    void add(const Table* value = nullptr);
    void insert(size_t ndx, const Table* value = nullptr);
    void set(size_t ndx, const Table*);
    void clear_table(size_t ndx);
    void set_null(size_t ndx) override;

    using SubtableColumnBase::insert;

    void erase_rows(size_t, size_t, size_t, bool) override;
    void move_last_row_over(size_t, size_t, bool) override;

    /// Compare two subtable columns for equality.
    bool compare_table(const SubtableColumn&) const;

    void refresh_accessor_tree(size_t, const Spec&) override;
    void refresh_subtable_map();

#ifdef REALM_DEBUG
    void verify(const Table&, size_t) const override;
    void do_dump_node_structure(std::ostream&, int) const override;
    void to_dot(std::ostream&, StringData title) const override;
#endif

private:
    mutable size_t m_subspec_ndx; // Unknown if equal to `npos`

    size_t get_subspec_ndx() const noexcept;

    void destroy_subtable(size_t ndx) noexcept;

    void do_discard_child_accessors() noexcept override;
};


// Implementation

// Overriding virtual method of Column.
inline void SubtableColumnBase::insert_rows(size_t row_ndx, size_t num_rows_to_insert, size_t prior_num_rows, bool)
{
    REALM_ASSERT_DEBUG(prior_num_rows == size());
    REALM_ASSERT(row_ndx <= prior_num_rows);

    size_t row_ndx_2 = (row_ndx == prior_num_rows ? realm::npos : row_ndx);
    int_fast64_t value = 0;
    do_insert(row_ndx_2, value, num_rows_to_insert); // Throws
}

// Overriding virtual method of Column.
inline void SubtableColumnBase::erase_rows(size_t row_ndx, size_t num_rows_to_erase, size_t prior_num_rows,
                                           bool broken_reciprocal_backlinks)
{
    IntegerColumn::erase_rows(row_ndx, num_rows_to_erase, prior_num_rows, broken_reciprocal_backlinks); // Throws

    std::lock_guard<std::recursive_mutex> lg(m_subtable_map_lock);
    const bool fix_ndx_in_parent = true;
    bool last_entry_removed = m_subtable_map.adj_erase_rows<fix_ndx_in_parent>(row_ndx, num_rows_to_erase);
    typedef _impl::TableFriend tf;
    if (last_entry_removed)
        tf::unbind_ptr(*m_table);
}

// Overriding virtual method of Column.
inline void SubtableColumnBase::move_last_row_over(size_t row_ndx, size_t prior_num_rows,
                                                   bool broken_reciprocal_backlinks)
{
    IntegerColumn::move_last_row_over(row_ndx, prior_num_rows, broken_reciprocal_backlinks); // Throws

    std::lock_guard<std::recursive_mutex> lg(m_subtable_map_lock);
    const bool fix_ndx_in_parent = true;
    size_t last_row_ndx = prior_num_rows - 1;
    bool last_entry_removed = m_subtable_map.adj_move_over<fix_ndx_in_parent>(last_row_ndx, row_ndx);
    typedef _impl::TableFriend tf;
    if (last_entry_removed)
        tf::unbind_ptr(*m_table);
}

inline void SubtableColumnBase::clear(size_t, bool)
{
    discard_child_accessors();
    clear_without_updating_index(); // Throws
    // FIXME: This one is needed because
    // IntegerColumn::clear_without_updating_index() forgets about the
    // leaf type. A better solution should probably be sought after.
    get_root_array()->set_type(Array::type_HasRefs);
}

inline void SubtableColumnBase::swap_rows(size_t row_ndx_1, size_t row_ndx_2)
{
    IntegerColumn::swap_rows(row_ndx_1, row_ndx_2); // Throws

    std::lock_guard<std::recursive_mutex> lg(m_subtable_map_lock);
    const bool fix_ndx_in_parent = true;
    m_subtable_map.adj_swap_rows<fix_ndx_in_parent>(row_ndx_1, row_ndx_2);
}

inline void SubtableColumnBase::mark(int type) noexcept
{
    if (type & mark_Recursive) {
        std::lock_guard<std::recursive_mutex> lg(m_subtable_map_lock);
        m_subtable_map.recursive_mark();
    }
}

inline void SubtableColumnBase::adj_acc_insert_rows(size_t row_ndx, size_t num_rows) noexcept
{
    // This function must assume no more than minimal consistency of the
    // accessor hierarchy. This means in particular that it cannot access the
    // underlying node structure. See AccessorConsistencyLevels.

    std::lock_guard<std::recursive_mutex> lg(m_subtable_map_lock);
    const bool fix_ndx_in_parent = false;
    m_subtable_map.adj_insert_rows<fix_ndx_in_parent>(row_ndx, num_rows);
}

inline void SubtableColumnBase::adj_acc_erase_row(size_t row_ndx) noexcept
{
    // This function must assume no more than minimal consistency of the
    // accessor hierarchy. This means in particular that it cannot access the
    // underlying node structure. See AccessorConsistencyLevels.

    std::lock_guard<std::recursive_mutex> lg(m_subtable_map_lock);
    const bool fix_ndx_in_parent = false;
    size_t num_rows_erased = 1;
    bool last_entry_removed = m_subtable_map.adj_erase_rows<fix_ndx_in_parent>(row_ndx, num_rows_erased);
    typedef _impl::TableFriend tf;
    if (last_entry_removed)
        tf::unbind_ptr(*m_table);
}

inline void SubtableColumnBase::adj_acc_move_over(size_t from_row_ndx, size_t to_row_ndx) noexcept
{
    // This function must assume no more than minimal consistency of the
    // accessor hierarchy. This means in particular that it cannot access the
    // underlying node structure. See AccessorConsistencyLevels.

    std::lock_guard<std::recursive_mutex> lg(m_subtable_map_lock);
    const bool fix_ndx_in_parent = false;
    bool last_entry_removed = m_subtable_map.adj_move_over<fix_ndx_in_parent>(from_row_ndx, to_row_ndx);
    typedef _impl::TableFriend tf;
    if (last_entry_removed)
        tf::unbind_ptr(*m_table);
}

inline void SubtableColumnBase::adj_acc_clear_root_table() noexcept
{
    // This function must assume no more than minimal consistency of the
    // accessor hierarchy. This means in particular that it cannot access the
    // underlying node structure. See AccessorConsistencyLevels.

    IntegerColumn::adj_acc_clear_root_table();
    discard_child_accessors();
}

inline void SubtableColumnBase::adj_acc_swap_rows(size_t row_ndx_1, size_t row_ndx_2) noexcept
{
    std::lock_guard<std::recursive_mutex> lg(m_subtable_map_lock);
    const bool fix_ndx_in_parent = false;
    m_subtable_map.adj_swap_rows<fix_ndx_in_parent>(row_ndx_1, row_ndx_2);
}

inline void SubtableColumnBase::adj_acc_move_row(size_t from_ndx, size_t to_ndx) noexcept
{
    std::lock_guard<std::recursive_mutex> lg(m_subtable_map_lock);
    const bool fix_ndx_in_parent = false;
    m_subtable_map.adj_move_row<fix_ndx_in_parent>(from_ndx, to_ndx);
}

inline TableRef SubtableColumnBase::get_subtable_accessor(size_t row_ndx) const noexcept
{
    // This function must assume no more than minimal consistency of the
    // accessor hierarchy. This means in particular that it cannot access the
    // underlying node structure. See AccessorConsistencyLevels.
    std::lock_guard<std::recursive_mutex> lg(m_subtable_map_lock);
    TableRef subtable(m_subtable_map.find(row_ndx));
    return subtable;
}

inline void SubtableColumnBase::discard_subtable_accessor(size_t row_ndx) noexcept
{
    // This function must assume no more than minimal consistency of the
    // accessor hierarchy. This means in particular that it cannot access the
    // underlying node structure. See AccessorConsistencyLevels.

    std::lock_guard<std::recursive_mutex> lg(m_subtable_map_lock);
    bool last_entry_removed = m_subtable_map.detach_and_remove(row_ndx);
    typedef _impl::TableFriend tf;
    if (last_entry_removed)
        tf::unbind_ptr(*m_table);
}

inline void SubtableColumnBase::SubtableMap::add(size_t subtable_ndx, Table* table)
{
    SubtableEntry e;
    e.m_subtable_ndx = subtable_ndx;
    e.m_table = table;
    m_entries.push_back(e);
}

template <bool fix_ndx_in_parent>
void SubtableColumnBase::SubtableMap::adj_insert_rows(size_t row_ndx, size_t num_rows_inserted) noexcept
{
    for (auto& entry : m_entries) {
        if (entry.m_subtable_ndx >= row_ndx) {
            entry.m_subtable_ndx += num_rows_inserted;
            typedef _impl::TableFriend tf;
            if (fix_ndx_in_parent)
                tf::set_ndx_in_parent(*(entry.m_table), entry.m_subtable_ndx);
        }
    }
}

template <bool fix_ndx_in_parent>
bool SubtableColumnBase::SubtableMap::adj_erase_rows(size_t row_ndx, size_t num_rows_erased) noexcept
{
    if (m_entries.empty())
        return false;
    typedef _impl::TableFriend tf;
    auto end = m_entries.end();
    auto i = m_entries.begin();
    do {
        if (i->m_subtable_ndx >= row_ndx + num_rows_erased) {
            i->m_subtable_ndx -= num_rows_erased;
            if (fix_ndx_in_parent)
                tf::set_ndx_in_parent(*(i->m_table), i->m_subtable_ndx);
        }
        else if (i->m_subtable_ndx >= row_ndx) {
            // Must hold a counted reference while detaching
            TableRef table(i->m_table);
            tf::detach(*table);
            // Move last over
            *i = *--end;
            continue;
        }
        ++i;
    } while (i != end);
    m_entries.erase(end, m_entries.end());
    return m_entries.empty();
}


template <bool fix_ndx_in_parent>
bool SubtableColumnBase::SubtableMap::adj_move_over(size_t from_row_ndx, size_t to_row_ndx) noexcept
{
    typedef _impl::TableFriend tf;

    size_t i = 0, n = m_entries.size();
    // We return true if, and only if we remove the last entry in the map.  We
    // need special handling for the case, where the set of entries are already
    // empty, otherwise the final return statement would return true in this
    // case, even though we didn't actually remove an entry.
    if (n == 0)
        return false;

    while (i < n) {
        SubtableEntry& e = m_entries[i];
        if (REALM_UNLIKELY(e.m_subtable_ndx == to_row_ndx)) {
            // Must hold a counted reference while detaching
            TableRef table(e.m_table);
            tf::detach(*table);
            // Delete entry by moving last over (faster and avoids invalidating
            // iterators)
            e = m_entries[--n];
            m_entries.pop_back();
        }
        else {
            if (REALM_UNLIKELY(e.m_subtable_ndx == from_row_ndx)) {
                e.m_subtable_ndx = to_row_ndx;
                if (fix_ndx_in_parent)
                    tf::set_ndx_in_parent(*(e.m_table), e.m_subtable_ndx);
            }
            ++i;
        }
    }
    return m_entries.empty();
}

template <bool fix_ndx_in_parent>
void SubtableColumnBase::SubtableMap::adj_swap_rows(size_t row_ndx_1, size_t row_ndx_2) noexcept
{
    using tf = _impl::TableFriend;
    for (auto& entry : m_entries) {
        if (REALM_UNLIKELY(entry.m_subtable_ndx == row_ndx_1)) {
            entry.m_subtable_ndx = row_ndx_2;
            if (fix_ndx_in_parent)
                tf::set_ndx_in_parent(*(entry.m_table), entry.m_subtable_ndx);
        }
        else if (REALM_UNLIKELY(entry.m_subtable_ndx == row_ndx_2)) {
            entry.m_subtable_ndx = row_ndx_1;
            if (fix_ndx_in_parent)
                tf::set_ndx_in_parent(*(entry.m_table), entry.m_subtable_ndx);
        }
    }
}


template <bool fix_ndx_in_parent>
void SubtableColumnBase::SubtableMap::adj_move_row(size_t from_ndx, size_t to_ndx) noexcept
{
    using tf = _impl::TableFriend;
    for (auto& entry : m_entries) {
        if (entry.m_subtable_ndx == from_ndx) {
            entry.m_subtable_ndx = to_ndx;
            if (fix_ndx_in_parent)
                tf::set_ndx_in_parent(*(entry.m_table), entry.m_subtable_ndx);
        }
        else {
            if (from_ndx < to_ndx) {
                // shift the range (from, to] down one
                if (entry.m_subtable_ndx <= to_ndx && entry.m_subtable_ndx > from_ndx) {
                    entry.m_subtable_ndx--;
                    if (fix_ndx_in_parent) {
                        tf::set_ndx_in_parent(*(entry.m_table), entry.m_subtable_ndx);
                    }
                }
            } else if (from_ndx > to_ndx) {
                // shift the range (from, to] up one
                if (entry.m_subtable_ndx >= to_ndx && entry.m_subtable_ndx < from_ndx) {
                    entry.m_subtable_ndx++;
                    if (fix_ndx_in_parent) {
                        tf::set_ndx_in_parent(*(entry.m_table), entry.m_subtable_ndx);
                    }
                }
            }
        }
    }
}

inline void SubtableColumnBase::SubtableMap::adj_set_null(size_t row_ndx) noexcept
{
    Table* table = find(row_ndx);
    if (table)
        _impl::TableFriend::refresh_accessor_tree(*table);
}

inline SubtableColumnBase::SubtableColumnBase(Allocator& alloc, ref_type ref, Table* table, size_t column_ndx)
    : IntegerColumn(alloc, ref, column_ndx) // Throws
    , m_table(table)
{
}

inline void SubtableColumnBase::update_child_ref(size_t child_ndx, ref_type new_ref)
{
    set_as_ref(child_ndx, new_ref);
}

inline ref_type SubtableColumnBase::get_child_ref(size_t child_ndx) const noexcept
{
    return get_as_ref(child_ndx);
}

inline void SubtableColumnBase::discard_child_accessors() noexcept
{
    std::lock_guard<std::recursive_mutex> lg(m_subtable_map_lock);
    bool last_entry_removed = m_subtable_map.detach_and_remove_all();
    if (last_entry_removed && m_table)
        _impl::TableFriend::unbind_ptr(*m_table);
}

inline SubtableColumnBase::~SubtableColumnBase() noexcept
{
    discard_child_accessors();
}

inline bool SubtableColumnBase::compare_subtable_rows(const Table& a, const Table& b)
{
    return _impl::TableFriend::compare_rows(a, b);
}

inline ref_type SubtableColumnBase::clone_table_columns(const Table* t)
{
    return _impl::TableFriend::clone_columns(*t, get_root_array()->get_alloc());
}

inline ref_type SubtableColumnBase::create(Allocator& alloc, size_t size)
{
    return IntegerColumn::create(alloc, Array::type_HasRefs, size); // Throws
}

inline size_t* SubtableColumnBase::record_subtable_path(size_t* begin, size_t* end) noexcept
{
    if (end == begin)
        return 0; // Error, not enough space in buffer
    *begin++ = get_column_index();
    if (end == begin)
        return 0; // Error, not enough space in buffer
    return _impl::TableFriend::record_subtable_path(*m_table, begin, end);
}

inline void SubtableColumnBase::update_table_accessors(const size_t* col_path_begin, const size_t* col_path_end,
                                                       _impl::TableFriend::AccessorUpdater& updater)
{
    // This function must assume no more than minimal consistency of the
    // accessor hierarchy. This means in particular that it cannot access the
    // underlying node structure. See AccessorConsistencyLevels.

    m_subtable_map.update_accessors(col_path_begin, col_path_end, updater); // Throws
}

inline void SubtableColumnBase::do_insert(size_t row_ndx, int_fast64_t value, size_t num_rows)
{
    IntegerColumn::insert_without_updating_index(row_ndx, value, num_rows); // Throws
    bool is_append = row_ndx == realm::npos;
    if (!is_append) {
        const bool fix_ndx_in_parent = true;
        m_subtable_map.adj_insert_rows<fix_ndx_in_parent>(row_ndx, num_rows);
    }
}


inline SubtableColumn::SubtableColumn(Allocator& alloc, ref_type ref, Table* table, size_t column_ndx)
    : SubtableColumnBase(alloc, ref, table, column_ndx)
    , m_subspec_ndx(realm::npos)
{
}

inline ConstTableRef SubtableColumn::get_subtable_tableref(size_t subtable_ndx) const
{
    return const_cast<SubtableColumn*>(this)->get_subtable_tableref(subtable_ndx);
}

inline void SubtableColumn::refresh_accessor_tree(size_t col_ndx, const Spec& spec)
{
    SubtableColumnBase::refresh_accessor_tree(col_ndx, spec); // Throws
    m_subspec_ndx = spec.get_subspec_ndx(col_ndx);
    std::lock_guard<std::recursive_mutex> lg(m_subtable_map_lock);
    m_subtable_map.refresh_accessor_tree(); // Throws
}

inline void SubtableColumn::refresh_subtable_map()
{
    std::lock_guard<std::recursive_mutex> lg(m_subtable_map_lock);
    m_subtable_map.refresh_accessor_tree(); // Throws
}

inline size_t SubtableColumn::get_subspec_ndx() const noexcept
{
    if (REALM_UNLIKELY(m_subspec_ndx == realm::npos)) {
        typedef _impl::TableFriend tf;
        m_subspec_ndx = tf::get_spec(*m_table).get_subspec_ndx(get_column_index());
    }
    return m_subspec_ndx;
}

inline Spec* SubtableColumn::get_subtable_spec() noexcept
{
    typedef _impl::TableFriend tf;
    return tf::get_spec(*m_table).get_subtable_spec(get_column_index());
}


} // namespace realm

#endif // REALM_COLUMN_TABLE_HPP
