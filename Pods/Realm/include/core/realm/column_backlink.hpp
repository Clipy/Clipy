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

#ifndef REALM_COLUMN_BACKLINK_HPP
#define REALM_COLUMN_BACKLINK_HPP

#include <vector>

#include <realm/column.hpp>
#include <realm/column_linkbase.hpp>
#include <realm/table.hpp>

namespace realm {

/// A column of backlinks (BacklinkColumn) is a single B+-tree, and the root of
/// the column is the root of the B+-tree. All leaf nodes are single arrays of
/// type Array with the hasRefs bit set.
///
/// The individual values in the column are either refs to Columns containing
/// the row indexes in the origin table that links to it, or in the case where
/// there is a single link, a tagged ref encoding the origin row position.
class BacklinkColumn : public IntegerColumn, public ArrayParent {
public:
    BacklinkColumn(Allocator&, ref_type, size_t col_ndx = npos);
    ~BacklinkColumn() noexcept override
    {
    }

    static ref_type create(Allocator&, size_t size = 0);

    bool has_backlinks(size_t row_ndx) const noexcept;
    size_t get_backlink_count(size_t row_ndx) const noexcept;
    size_t get_backlink(size_t row_ndx, size_t backlink_ndx) const noexcept;

    void add_backlink(size_t row_ndx, size_t origin_row_ndx);
    void remove_one_backlink(size_t row_ndx, size_t origin_row_ndx);
    void remove_all_backlinks(size_t num_rows);
    void update_backlink(size_t row_ndx, size_t old_origin_row_ndx, size_t new_origin_row_ndx);
    void swap_backlinks(size_t row_ndx, size_t origin_row_ndx_1, size_t origin_row_ndx_2);

    void add_row();

    // Link origination info
    Table& get_origin_table() const noexcept;
    void set_origin_table(Table&) noexcept;
    LinkColumnBase& get_origin_column() const noexcept;
    size_t get_origin_column_index() const noexcept;
    void set_origin_column(LinkColumnBase& column) noexcept;

    void insert_rows(size_t, size_t, size_t, bool) override;
    void erase_rows(size_t, size_t, size_t, bool) override;
    void move_last_row_over(size_t, size_t, bool) override;
    void swap_rows(size_t, size_t) override;
    void clear(size_t, bool) override;
    void adj_acc_insert_rows(size_t, size_t) noexcept override;
    void adj_acc_erase_row(size_t) noexcept override;
    void adj_acc_move_over(size_t, size_t) noexcept override;
    void adj_acc_swap_rows(size_t, size_t) noexcept override;
    void adj_acc_move_row(size_t, size_t) noexcept override;
    void adj_acc_merge_rows(size_t, size_t) noexcept override;
    void adj_acc_clear_root_table() noexcept override;
    void mark(int) noexcept override;

    void bump_link_origin_table_version() noexcept override;

    void cascade_break_backlinks_to(size_t row_ndx, CascadeState& state) override;
    void cascade_break_backlinks_to_all_rows(size_t num_rows, CascadeState&) override;

    int compare_values(size_t, size_t) const noexcept override;

    void verify() const override;
    void verify(const Table&, size_t) const override;
#ifdef REALM_DEBUG
    struct VerifyPair {
        size_t origin_row_ndx, target_row_ndx;
        bool operator<(const VerifyPair&) const noexcept;
    };
    void get_backlinks(std::vector<VerifyPair>&); // Sorts
#endif

protected:
    // ArrayParent overrides
    void update_child_ref(size_t child_ndx, ref_type new_ref) override;
    ref_type get_child_ref(size_t child_ndx) const noexcept override;

    std::pair<ref_type, size_t> get_to_dot_parent(size_t) const override;

private:
    TableRef m_origin_table;
    LinkColumnBase* m_origin_column = nullptr;

    template <typename Func>
    size_t for_each_link(size_t row_ndx, bool do_destroy, Func&& f);
};


// Implementation

inline BacklinkColumn::BacklinkColumn(Allocator& alloc, ref_type ref, size_t col_ndx)
    : IntegerColumn(alloc, ref, col_ndx) // Throws
{
}

inline ref_type BacklinkColumn::create(Allocator& alloc, size_t size)
{
    return IntegerColumn::create(alloc, Array::type_HasRefs, size); // Throws
}

inline bool BacklinkColumn::has_backlinks(size_t ndx) const noexcept
{
    return IntegerColumn::get(ndx) != 0;
}

inline Table& BacklinkColumn::get_origin_table() const noexcept
{
    return *m_origin_table;
}

inline void BacklinkColumn::set_origin_table(Table& table) noexcept
{
    REALM_ASSERT(!m_origin_table);
    m_origin_table = table.get_table_ref();
}

inline LinkColumnBase& BacklinkColumn::get_origin_column() const noexcept
{
    return *m_origin_column;
}

inline size_t BacklinkColumn::get_origin_column_index() const noexcept
{
    return m_origin_column ? m_origin_column->get_column_index() : npos;
}

inline void BacklinkColumn::set_origin_column(LinkColumnBase& column) noexcept
{
    m_origin_column = &column;
}

inline void BacklinkColumn::add_row()
{
    IntegerColumn::add(0);
}

inline void BacklinkColumn::adj_acc_insert_rows(size_t row_ndx, size_t num_rows) noexcept
{
    IntegerColumn::adj_acc_insert_rows(row_ndx, num_rows);

    typedef _impl::TableFriend tf;
    tf::mark(*m_origin_table);
}

inline void BacklinkColumn::adj_acc_erase_row(size_t row_ndx) noexcept
{
    IntegerColumn::adj_acc_erase_row(row_ndx);

    typedef _impl::TableFriend tf;
    tf::mark(*m_origin_table);
}

inline void BacklinkColumn::adj_acc_move_over(size_t from_row_ndx, size_t to_row_ndx) noexcept
{
    IntegerColumn::adj_acc_move_over(from_row_ndx, to_row_ndx);

    typedef _impl::TableFriend tf;
    tf::mark(*m_origin_table);
}

inline void BacklinkColumn::adj_acc_swap_rows(size_t row_ndx_1, size_t row_ndx_2) noexcept
{
    Column::adj_acc_swap_rows(row_ndx_1, row_ndx_2);

    using tf = _impl::TableFriend;
    tf::mark(*m_origin_table);
}

inline void BacklinkColumn::adj_acc_move_row(size_t from_ndx, size_t to_ndx) noexcept
{
    Column::adj_acc_move_row(from_ndx, to_ndx);

    using tf = _impl::TableFriend;
    tf::mark(*m_origin_table);
}

inline void BacklinkColumn::adj_acc_merge_rows(size_t old_row_ndx, size_t new_row_ndx) noexcept
{
    Column::adj_acc_merge_rows(old_row_ndx, new_row_ndx);

    using tf = _impl::TableFriend;
    tf::mark(*m_origin_table);
}

inline void BacklinkColumn::adj_acc_clear_root_table() noexcept
{
    IntegerColumn::adj_acc_clear_root_table();

    typedef _impl::TableFriend tf;
    tf::mark(*m_origin_table);
}

inline void BacklinkColumn::mark(int type) noexcept
{
    if (type & mark_LinkOrigins) {
        typedef _impl::TableFriend tf;
        tf::mark(*m_origin_table);
    }
}

inline void BacklinkColumn::bump_link_origin_table_version() noexcept
{
    // It is important to mark connected tables as modified.
    // Also see LinkColumnBase::bump_link_origin_table_version().
    typedef _impl::TableFriend tf;
    if (m_origin_table) {
        bool bump_global = false;
        tf::bump_version(*m_origin_table, bump_global);
    }
}

#ifdef REALM_DEBUG

inline bool BacklinkColumn::VerifyPair::operator<(const VerifyPair& p) const noexcept
{
    return origin_row_ndx < p.origin_row_ndx;
}

#endif // REALM_DEBUG

} // namespace realm

#endif // REALM_COLUMN_BACKLINK_HPP
