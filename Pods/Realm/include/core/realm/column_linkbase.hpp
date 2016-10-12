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
#ifndef REALM_COLUMN_LINKBASE_HPP
#define REALM_COLUMN_LINKBASE_HPP

#include <realm/table.hpp>

namespace realm {

class BacklinkColumn;
class Table;

// Abstract base class for columns containing links
class LinkColumnBase: public IntegerColumn {
public:
    // Create unattached root array aaccessor.
    LinkColumnBase(Allocator& alloc, ref_type ref, Table* table, size_t column_ndx);
    ~LinkColumnBase() noexcept override;

    bool is_nullable() const noexcept override = 0;
    void set_null(size_t) override = 0;
    bool is_null(size_t) const noexcept override = 0;

    bool supports_search_index() const noexcept final { return false; }
    StringIndex* create_search_index() override;

    bool get_weak_links() const noexcept;
    void set_weak_links(bool) noexcept;

    Table& get_target_table() const noexcept;
    void set_target_table(Table&) noexcept;
    BacklinkColumn& get_backlink_column() const noexcept;
    void set_backlink_column(BacklinkColumn&) noexcept;

    void swap_rows(size_t, size_t) override = 0;

    virtual void do_nullify_link(size_t row_ndx, size_t old_target_row_ndx) = 0;
    virtual void do_update_link(size_t row_ndx, size_t old_target_row_ndx,
                                size_t new_target_row_ndx) = 0;
    virtual void do_swap_link(size_t row_ndx, size_t target_row_ndx_1,
                              size_t target_row_ndx_2) = 0;

    void adj_acc_insert_rows(size_t, size_t) noexcept override;
    void adj_acc_erase_row(size_t) noexcept override;
    void adj_acc_move_over(size_t, size_t) noexcept override;
    void adj_acc_swap_rows(size_t, size_t) noexcept override;
    void adj_acc_clear_root_table() noexcept override;
    void mark(int) noexcept override;
    void refresh_accessor_tree(size_t, const Spec&) override;
    void bump_link_origin_table_version() noexcept override;

#ifdef REALM_DEBUG
    void verify(const Table&, size_t) const override;
    using IntegerColumn::verify;
#endif

protected:
    // A pointer to the table that this column is part of.
    Table* const m_table;

    // The index of this column within m_table.m_cols.
    size_t m_column_ndx;

    TableRef m_target_table;
    BacklinkColumn* m_backlink_column = nullptr;
    bool m_weak_links = false; // True if these links are weak (not strong)

    /// Call Table::cascade_break_backlinks_to() for the specified target row if
    /// it is not already in \a state.rows, and the number of strong links to it
    /// has dropped to zero.
    void check_cascade_break_backlinks_to(size_t target_table_ndx, size_t target_row_ndx,
                                          CascadeState& state);
};




// Implementation

inline LinkColumnBase::LinkColumnBase(Allocator& alloc, ref_type ref, Table* table, size_t column_ndx):
    IntegerColumn(alloc, ref), // Throws
    m_table(table),
    m_column_ndx(column_ndx)
{
}

inline LinkColumnBase::~LinkColumnBase() noexcept
{
}

inline StringIndex* LinkColumnBase::create_search_index()
{
    return nullptr;
}

inline bool LinkColumnBase::get_weak_links() const noexcept
{
    return m_weak_links;
}

inline void LinkColumnBase::set_weak_links(bool value) noexcept
{
    m_weak_links = value;
}

inline Table& LinkColumnBase::get_target_table() const noexcept
{
    return *m_target_table;
}

inline void LinkColumnBase::set_target_table(Table& table) noexcept
{
    REALM_ASSERT(!m_target_table);
    m_target_table = table.get_table_ref();
}

inline BacklinkColumn& LinkColumnBase::get_backlink_column() const noexcept
{
    return *m_backlink_column;
}

inline void LinkColumnBase::set_backlink_column(BacklinkColumn& column) noexcept
{
    m_backlink_column = &column;
}

inline void LinkColumnBase::adj_acc_insert_rows(size_t row_ndx,
                                                size_t num_rows) noexcept
{
    IntegerColumn::adj_acc_insert_rows(row_ndx, num_rows);

    typedef _impl::TableFriend tf;
    tf::mark(*m_target_table);
}

inline void LinkColumnBase::adj_acc_erase_row(size_t row_ndx) noexcept
{
    IntegerColumn::adj_acc_erase_row(row_ndx);

    typedef _impl::TableFriend tf;
    tf::mark(*m_target_table);
}

inline void LinkColumnBase::adj_acc_move_over(size_t from_row_ndx,
                                              size_t to_row_ndx) noexcept
{
    IntegerColumn::adj_acc_move_over(from_row_ndx, to_row_ndx);

    typedef _impl::TableFriend tf;
    tf::mark(*m_target_table);
}

inline void LinkColumnBase::adj_acc_swap_rows(size_t row_ndx_1, size_t row_ndx_2) noexcept
{
    IntegerColumn::adj_acc_swap_rows(row_ndx_1, row_ndx_2);

    typedef _impl::TableFriend tf;
    tf::mark(*m_target_table);
}

inline void LinkColumnBase::adj_acc_clear_root_table() noexcept
{
    IntegerColumn::adj_acc_clear_root_table();

    typedef _impl::TableFriend tf;
    tf::mark(*m_target_table);
}

inline void LinkColumnBase::mark(int type) noexcept
{
    if (type & mark_LinkTargets) {
        typedef _impl::TableFriend tf;
        tf::mark(*m_target_table);
    }
}

inline void LinkColumnBase::bump_link_origin_table_version() noexcept
{
    // It is important to mark connected tables as modified.
    // Also see BacklinkColumn::bump_link_origin_table_version().
    typedef _impl::TableFriend tf;
    if (m_target_table) {
        bool bump_global = false;
        tf::bump_version(*m_target_table, bump_global);
    }
}



} // namespace realm

#endif // REALM_COLUMN_LINKBASE_HPP
