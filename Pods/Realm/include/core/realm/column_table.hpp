/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2015] Realm Inc
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of Realm Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Realm Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Realm Incorporated.
 *
 **************************************************************************/
#ifndef REALM_COLUMN_TABLE_HPP
#define REALM_COLUMN_TABLE_HPP

#include <vector>

#include <realm/util/features.h>
#include <memory>
#include <realm/column.hpp>
#include <realm/table.hpp>

namespace realm {


/// Base class for any type of column that can contain subtables.
// FIXME: Don't derive from IntegerColumn, but define a BpTree<ref_type> specialization.
class SubtableColumnBase: public IntegerColumn, public Table::Parent {
public:
    void discard_child_accessors() noexcept;

    ~SubtableColumnBase() noexcept override;

    static ref_type create(Allocator&, size_t size = 0);

    Table* get_subtable_accessor(size_t) const noexcept override;

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
    void mark(int) noexcept override;
    void refresh_accessor_tree(size_t, const Spec&) override;
    bool supports_search_index() const noexcept override { return false; }
    StringIndex* create_search_index() override { return nullptr; }

#ifdef REALM_DEBUG
    void verify() const override;
    void verify(const Table&, size_t) const override;
#endif

protected:
    /// A pointer to the table that this column is part of. For a free-standing
    /// column, this pointer is null.
    Table* const m_table;

    /// The index of this column within m_table.m_cols. For a free-standing
    /// column, this index is zero.
    size_t m_column_ndx;

    struct SubtableMap {
        ~SubtableMap() noexcept {}
        bool empty() const noexcept { return m_entries.empty(); }
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
        template<bool fix_ndx_in_parent>
        void adj_insert_rows(size_t row_ndx, size_t num_rows_inserted) noexcept;
        // Returns true if, and only if an entry was found and removed, and it
        // was the last entry in the map.
        template<bool fix_ndx_in_parent>
        bool adj_erase_rows(size_t row_ndx, size_t num_rows_erased) noexcept;
        // Returns true if, and only if an entry was found and removed, and it
        // was the last entry in the map.
        template<bool fix_ndx_in_parent>
        bool adj_move_over(size_t from_row_ndx, size_t to_row_ndx) noexcept;
        template<bool fix_ndx_in_parent>
        void adj_swap_rows(size_t row_ndx_1, size_t row_ndx_2) noexcept;

        void update_accessors(const size_t* col_path_begin, const size_t* col_path_end,
                              _impl::TableFriend::AccessorUpdater&);
        void recursive_mark() noexcept;
        void refresh_accessor_tree(size_t spec_ndx_in_parent);
    private:
        struct entry {
            size_t m_subtable_ndx;
            Table* m_table;
        };
        typedef std::vector<entry> entries;
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

    SubtableColumnBase(Allocator&, ref_type, Table*, size_t column_ndx);

    /// Get a pointer to the accessor of the specified subtable. The
    /// accessor will be created if it does not already exist.
    ///
    /// The returned table pointer must **always** end up being
    /// wrapped in some instantiation of BasicTableRef<>.
    ///
    /// NOTE: This method must be used only for subtables with
    /// independent specs, i.e. for elements of a MixedColumn.
    Table* get_subtable_ptr(size_t subtable_ndx);

    // Overriding method in ArrayParent
    void update_child_ref(size_t, ref_type) override;

    // Overriding method in ArrayParent
    ref_type get_child_ref(size_t) const noexcept override;

    // Overriding method in Table::Parent
    Table* get_parent_table(size_t*) noexcept override;

    // Overriding method in Table::Parent
    void child_accessor_destroyed(Table*) noexcept override;

    /// Assumes that the two tables have the same spec.
    static bool compare_subtable_rows(const Table&, const Table&);

    /// Construct a copy of the columns array of the specified table
    /// and return just the ref to that array.
    ///
    /// In the clone, no string column will be of the enumeration
    /// type.
    ref_type clone_table_columns(const Table*);

    size_t* record_subtable_path(size_t* begin,
                                      size_t* end) noexcept override;

    void update_table_accessors(const size_t* col_path_begin, const size_t* col_path_end,
                                _impl::TableFriend::AccessorUpdater&);

    /// \param row_ndx Must be `realm::npos` if appending.
    void do_insert(size_t row_ndx, int_fast64_t value, size_t num_rows);

#ifdef REALM_DEBUG
    std::pair<ref_type, size_t>
    get_to_dot_parent(size_t ndx_in_parent) const override;
#endif

    friend class Table;
};



class SubtableColumn: public SubtableColumnBase {
public:
    /// Create a subtable column accessor and attach it to a
    /// preexisting underlying structure of arrays.
    ///
    /// \param table If this column is used as part of a table you must
    /// pass a pointer to that table. Otherwise you must pass null.
    ///
    /// \param column_ndx If this column is used as part of a table
    /// you must pass the logical index of the column within that
    /// table. Otherwise you should pass zero.
    SubtableColumn(Allocator&, ref_type, Table* table, size_t column_ndx);

    ~SubtableColumn() noexcept override {}

    size_t get_subtable_size(size_t ndx) const noexcept;

    /// Get a pointer to the accessor of the specified subtable. The
    /// accessor will be created if it does not already exist.
    ///
    /// The returned table pointer must **always** end up being
    /// wrapped in some instantiation of BasicTableRef<>.
    Table* get_subtable_ptr(size_t subtable_ndx);

    const Table* get_subtable_ptr(size_t subtable_ndx) const;

    // When passing a table to add() or insert() it is assumed that
    // the table spec is compatible with this column. The number of
    // columns must be the same, and the corresponding columns must
    // have the same data type (as returned by
    // Table::get_column_type()).

    void add(const Table* value = nullptr);
    void insert(size_t ndx, const Table* value = nullptr);
    void set(size_t ndx, const Table*);
    void clear_table(size_t ndx);

    using SubtableColumnBase::insert;

    void erase_rows(size_t, size_t, size_t, bool) override;
    void move_last_row_over(size_t, size_t, bool) override;

    /// Compare two subtable columns for equality.
    bool compare_table(const SubtableColumn&) const;

    void refresh_accessor_tree(size_t, const Spec&) override;

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
inline void SubtableColumnBase::insert_rows(size_t row_ndx, size_t num_rows_to_insert,
                                            size_t prior_num_rows, bool insert_nulls)
{
    REALM_ASSERT_DEBUG(prior_num_rows == size());
    REALM_ASSERT(row_ndx <= prior_num_rows);
    REALM_ASSERT(!insert_nulls);

    size_t row_ndx_2 = (row_ndx == prior_num_rows ? realm::npos : row_ndx);
    int_fast64_t value = 0;
    do_insert(row_ndx_2, value, num_rows_to_insert); // Throws
}

// Overriding virtual method of Column.
inline void SubtableColumnBase::erase_rows(size_t row_ndx, size_t num_rows_to_erase,
                                           size_t prior_num_rows,
                                           bool broken_reciprocal_backlinks)
{
    IntegerColumn::erase_rows(row_ndx, num_rows_to_erase, prior_num_rows,
                       broken_reciprocal_backlinks); // Throws

    const bool fix_ndx_in_parent = true;
    bool last_entry_removed =
        m_subtable_map.adj_erase_rows<fix_ndx_in_parent>(row_ndx, num_rows_to_erase);
    typedef _impl::TableFriend tf;
    if (last_entry_removed)
        tf::unbind_ptr(*m_table);
}

// Overriding virtual method of Column.
inline void SubtableColumnBase::move_last_row_over(size_t row_ndx, size_t prior_num_rows,
                                                   bool broken_reciprocal_backlinks)
{
    IntegerColumn::move_last_row_over(row_ndx, prior_num_rows, broken_reciprocal_backlinks); // Throws

    const bool fix_ndx_in_parent = true;
    size_t last_row_ndx = prior_num_rows - 1;
    bool last_entry_removed =
        m_subtable_map.adj_move_over<fix_ndx_in_parent>(last_row_ndx, row_ndx);
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

    const bool fix_ndx_in_parent = true;
    m_subtable_map.adj_swap_rows<fix_ndx_in_parent>(row_ndx_1, row_ndx_2);
}

inline void SubtableColumnBase::mark(int type) noexcept
{
    if (type & mark_Recursive)
        m_subtable_map.recursive_mark();
}

inline void SubtableColumnBase::refresh_accessor_tree(size_t col_ndx, const Spec& spec)
{
    IntegerColumn::refresh_accessor_tree(col_ndx, spec); // Throws
    m_column_ndx = col_ndx;
}

inline void SubtableColumnBase::adj_acc_insert_rows(size_t row_ndx,
                                                    size_t num_rows) noexcept
{
    // This function must assume no more than minimal consistency of the
    // accessor hierarchy. This means in particular that it cannot access the
    // underlying node structure. See AccessorConsistencyLevels.

    const bool fix_ndx_in_parent = false;
    m_subtable_map.adj_insert_rows<fix_ndx_in_parent>(row_ndx, num_rows);
}

inline void SubtableColumnBase::adj_acc_erase_row(size_t row_ndx) noexcept
{
    // This function must assume no more than minimal consistency of the
    // accessor hierarchy. This means in particular that it cannot access the
    // underlying node structure. See AccessorConsistencyLevels.

    const bool fix_ndx_in_parent = false;
    size_t num_rows_erased = 1;
    bool last_entry_removed =
        m_subtable_map.adj_erase_rows<fix_ndx_in_parent>(row_ndx, num_rows_erased);
    typedef _impl::TableFriend tf;
    if (last_entry_removed)
        tf::unbind_ptr(*m_table);
}

inline void SubtableColumnBase::adj_acc_move_over(size_t from_row_ndx,
                                                  size_t to_row_ndx) noexcept
{
    // This function must assume no more than minimal consistency of the
    // accessor hierarchy. This means in particular that it cannot access the
    // underlying node structure. See AccessorConsistencyLevels.

    const bool fix_ndx_in_parent = false;
    bool last_entry_removed =
        m_subtable_map.adj_move_over<fix_ndx_in_parent>(from_row_ndx, to_row_ndx);
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
    const bool fix_ndx_in_parent = false;
    m_subtable_map.adj_swap_rows<fix_ndx_in_parent>(row_ndx_1, row_ndx_2);
}

inline Table* SubtableColumnBase::get_subtable_accessor(size_t row_ndx) const noexcept
{
    // This function must assume no more than minimal consistency of the
    // accessor hierarchy. This means in particular that it cannot access the
    // underlying node structure. See AccessorConsistencyLevels.

    Table* subtable = m_subtable_map.find(row_ndx);
    return subtable;
}

inline void SubtableColumnBase::discard_subtable_accessor(size_t row_ndx) noexcept
{
    // This function must assume no more than minimal consistency of the
    // accessor hierarchy. This means in particular that it cannot access the
    // underlying node structure. See AccessorConsistencyLevels.

    bool last_entry_removed = m_subtable_map.detach_and_remove(row_ndx);
    typedef _impl::TableFriend tf;
    if (last_entry_removed)
        tf::unbind_ptr(*m_table);
}

inline void SubtableColumnBase::SubtableMap::add(size_t subtable_ndx, Table* table)
{
    entry e;
    e.m_subtable_ndx = subtable_ndx;
    e.m_table        = table;
    m_entries.push_back(e);
}

template<bool fix_ndx_in_parent>
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

template<bool fix_ndx_in_parent>
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
    }
    while (i != end);
    m_entries.erase(end, m_entries.end());
    return m_entries.empty();
}


template<bool fix_ndx_in_parent>
bool SubtableColumnBase::SubtableMap::adj_move_over(size_t from_row_ndx,
                                                    size_t to_row_ndx) noexcept
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
        entry& e = m_entries[i];
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

template<bool fix_ndx_in_parent>
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

inline SubtableColumnBase::SubtableColumnBase(Allocator& alloc, ref_type ref,
                                              Table* table, size_t column_ndx):
    IntegerColumn(alloc, ref), // Throws
    m_table(table),
    m_column_ndx(column_ndx)
{
}

inline void SubtableColumnBase::update_child_ref(size_t child_ndx, ref_type new_ref)
{
    set(child_ndx, new_ref);
}

inline ref_type SubtableColumnBase::get_child_ref(size_t child_ndx) const noexcept
{
    return get_as_ref(child_ndx);
}

inline void SubtableColumnBase::discard_child_accessors() noexcept
{
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
    return _impl::TableFriend::compare_rows(a,b);
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
    *begin++ = m_column_ndx;
    if (end == begin)
        return 0; // Error, not enough space in buffer
    return _impl::TableFriend::record_subtable_path(*m_table, begin, end);
}

inline void SubtableColumnBase::update_table_accessors(const size_t* col_path_begin,
                                                       const size_t* col_path_end,
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


inline SubtableColumn::SubtableColumn(Allocator& alloc, ref_type ref,
                                      Table* table, size_t column_ndx):
    SubtableColumnBase(alloc, ref, table, column_ndx),
    m_subspec_ndx(realm::npos)
{
}

inline const Table* SubtableColumn::get_subtable_ptr(size_t subtable_ndx) const
{
    return const_cast<SubtableColumn*>(this)->get_subtable_ptr(subtable_ndx);
}

inline void SubtableColumn::refresh_accessor_tree(size_t col_ndx, const Spec& spec)
{
    SubtableColumnBase::refresh_accessor_tree(col_ndx, spec); // Throws
    m_subspec_ndx = spec.get_subspec_ndx(col_ndx);
    m_subtable_map.refresh_accessor_tree(m_subspec_ndx); // Throws
}

inline size_t SubtableColumn::get_subspec_ndx() const noexcept
{
    if (REALM_UNLIKELY(m_subspec_ndx == realm::npos)) {
        typedef _impl::TableFriend tf;
        const Spec& spec = tf::get_spec(*m_table);
        m_subspec_ndx = spec.get_subspec_ndx(m_column_ndx);
    }
    return m_subspec_ndx;
}


} // namespace realm

#endif // REALM_COLUMN_TABLE_HPP
