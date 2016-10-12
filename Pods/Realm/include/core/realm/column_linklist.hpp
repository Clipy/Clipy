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
#ifndef REALM_COLUMN_LINKLIST_HPP
#define REALM_COLUMN_LINKLIST_HPP

#include <algorithm>
#include <vector>

#include <realm/column.hpp>
#include <realm/column_linkbase.hpp>
#include <realm/table.hpp>
#include <realm/column_backlink.hpp>
#include <realm/link_view_fwd.hpp>
#include <iostream>

namespace realm {

namespace _impl {
class TransactLogConvenientEncoder;
}


/// A column of link lists (LinkListColumn) is a single B+-tree, and the root of
/// the column is the root of the B+-tree. All leaf nodes are single arrays of
/// type Array with the hasRefs bit set.
///
/// The individual values in the column are either refs to Columns containing the
/// row positions in the target table, or in the case where they are empty, a zero
/// ref.
class LinkListColumn: public LinkColumnBase, public ArrayParent {
public:
    using LinkColumnBase::LinkColumnBase;
    LinkListColumn(Allocator& alloc, ref_type ref, Table* table, size_t column_ndx);
    ~LinkListColumn() noexcept override;

    static ref_type create(Allocator&, size_t size = 0);

    bool is_nullable() const noexcept final;

    bool has_links(size_t row_ndx) const noexcept;
    size_t get_link_count(size_t row_ndx) const noexcept;

    ConstLinkViewRef get(size_t row_ndx) const;
    LinkViewRef get(size_t row_ndx);

    bool is_null(size_t row_ndx) const noexcept final;
    void set_null(size_t row_ndx) final;

    /// Compare two columns for equality.
    bool compare_link_list(const LinkListColumn&) const;

    void to_json_row(size_t row_ndx, std::ostream& out) const;

    void insert_rows(size_t, size_t, size_t, bool) override;
    void erase_rows(size_t, size_t, size_t, bool) override;
    void move_last_row_over(size_t, size_t, bool) override;
    void swap_rows(size_t, size_t) override;
    void clear(size_t, bool) override;
    void cascade_break_backlinks_to(size_t, CascadeState&) override;
    void cascade_break_backlinks_to_all_rows(size_t, CascadeState&) override;
    void update_from_parent(size_t) noexcept override;
    void adj_acc_clear_root_table() noexcept override;
    void adj_acc_insert_rows(size_t, size_t) noexcept override;
    void adj_acc_erase_row(size_t) noexcept override;
    void adj_acc_move_over(size_t, size_t) noexcept override;
    void adj_acc_swap_rows(size_t, size_t) noexcept override;
    void refresh_accessor_tree(size_t, const Spec&) override;

#ifdef REALM_DEBUG
    void verify() const override;
    void verify(const Table&, size_t) const override;
#endif

protected:
    void do_discard_child_accessors() noexcept override;

private:
    struct list_entry {
        size_t m_row_ndx;
        std::weak_ptr<LinkView> m_list;
        bool operator<(const list_entry& other) const { return m_row_ndx < other.m_row_ndx; }
    };

    // The accessors stored in `m_list_accessors` are sorted by their row index.
    // When a LinkList accessor is destroyed because the last shared_ptr pointing
    // to it dies, its entry is implicitly replaced by a tombstone (an entry with 
    // an empty `m_list`). These tombstones are pruned at a later time by 
    // `prune_list_accessor_tombstones`. This is done to amortize the O(n) cost 
    // of `std::vector::erase` that would otherwise be incurred each time an 
    // accessor is removed.
    mutable std::vector<list_entry> m_list_accessors;
    mutable std::atomic<bool> m_list_accessors_contains_tombstones;

    std::shared_ptr<LinkView> get_ptr(size_t row_ndx) const;

    void do_nullify_link(size_t row_ndx, size_t old_target_row_ndx) override;
    void do_update_link(size_t row_ndx, size_t old_target_row_ndx,
                        size_t new_target_row_ndx) override;
    void do_swap_link(size_t row_ndx, size_t target_row_ndx_1,
                      size_t target_row_ndx_2) override;

    void unregister_linkview();
    ref_type get_row_ref(size_t row_ndx) const noexcept;
    void set_row_ref(size_t row_ndx, ref_type new_ref);
    void add_backlink(size_t target_row, size_t source_row);
    void remove_backlink(size_t target_row, size_t source_row);

    // ArrayParent overrides
    void update_child_ref(size_t child_ndx, ref_type new_ref) override;
    ref_type get_child_ref(size_t child_ndx) const noexcept override;

    // These helpers are needed because of the way the B+-tree of links is
    // traversed in cascade_break_backlinks_to() and
    // cascade_break_backlinks_to_all_rows().
    void cascade_break_backlinks_to__leaf(size_t row_ndx, const Array& link_list_leaf,
                                          CascadeState&);
    void cascade_break_backlinks_to_all_rows__leaf(const Array& link_list_leaf, CascadeState&);

    void discard_child_accessors() noexcept;

    template<bool fix_ndx_in_parent>
    void adj_insert_rows(size_t row_ndx, size_t num_rows_inserted) noexcept;

    template<bool fix_ndx_in_parent>
    void adj_erase_rows(size_t row_ndx, size_t num_rows_erased) noexcept;

    template<bool fix_ndx_in_parent>
    void adj_move_over(size_t from_row_ndx, size_t to_row_ndx) noexcept;

    template<bool fix_ndx_in_parent>
    void adj_swap(size_t row_ndx_1, size_t row_ndx_2) noexcept;

    void prune_list_accessor_tombstones() noexcept;
    void validate_list_accessors() const noexcept;

#ifdef REALM_DEBUG
    std::pair<ref_type, size_t> get_to_dot_parent(size_t) const override;
#endif

    friend class BacklinkColumn;
    friend class LinkView;
    friend class _impl::TransactLogConvenientEncoder;
};





// Implementation

inline LinkListColumn::LinkListColumn(Allocator& alloc, ref_type ref, Table* table, size_t column_ndx):
    LinkColumnBase(alloc, ref, table, column_ndx)
{ 
    m_list_accessors_contains_tombstones.store(false); 
}

inline LinkListColumn::~LinkListColumn() noexcept
{
    discard_child_accessors();
}

inline ref_type LinkListColumn::create(Allocator& alloc, size_t size)
{
    return IntegerColumn::create(alloc, Array::type_HasRefs, size); // Throws
}

inline bool LinkListColumn::is_nullable() const noexcept
{
    return false;
}

inline bool LinkListColumn::has_links(size_t row_ndx) const noexcept
{
    ref_type ref = LinkColumnBase::get_as_ref(row_ndx);
    return (ref != 0);
}

inline size_t LinkListColumn::get_link_count(size_t row_ndx) const noexcept
{
    ref_type ref = LinkColumnBase::get_as_ref(row_ndx);
    if (ref == 0)
        return 0;
    return ColumnBase::get_size_from_ref(ref, get_alloc());
}

inline ConstLinkViewRef LinkListColumn::get(size_t row_ndx) const
{
    return get_ptr(row_ndx);
}

inline LinkViewRef LinkListColumn::get(size_t row_ndx)
{
    return get_ptr(row_ndx);
}

inline bool LinkListColumn::is_null(size_t) const noexcept
{
    return false;
}

inline void LinkListColumn::set_null(size_t)
{
    throw LogicError{LogicError::column_not_nullable};
}

inline void LinkListColumn::do_discard_child_accessors() noexcept
{
    discard_child_accessors();
}

inline ref_type LinkListColumn::get_row_ref(size_t row_ndx) const noexcept
{
    return LinkColumnBase::get_as_ref(row_ndx);
}

inline void LinkListColumn::set_row_ref(size_t row_ndx, ref_type new_ref)
{
    LinkColumnBase::set(row_ndx, new_ref); // Throws
}

inline void LinkListColumn::add_backlink(size_t target_row, size_t source_row)
{
    m_backlink_column->add_backlink(target_row, source_row); // Throws
}

inline void LinkListColumn::remove_backlink(size_t target_row, size_t source_row)
{
    m_backlink_column->remove_one_backlink(target_row, source_row); // Throws
}


} //namespace realm

#endif //REALM_COLUMN_LINKLIST_HPP


