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

#ifndef REALM_LINK_VIEW_HPP
#define REALM_LINK_VIEW_HPP

#include <realm/column.hpp>
#include <realm/column_linklist.hpp>
#include <realm/link_view_fwd.hpp>
#include <realm/table.hpp>

namespace realm {

class LinkListColumn;

namespace _impl {
class LinkListFriend;
class TransactLogConvenientEncoder;
}


/// The effect of calling most of the link list functions on a detached accessor
/// is unspecified and may lead to general corruption, or even a crash. The
/// exceptions are is_attached() and the destructor.
///
/// FIXME: Rename this class to `LinkList`.
class LinkView : public RowIndexes, public std::enable_shared_from_this<LinkView> {
public:
    ~LinkView() noexcept;
    bool is_attached() const noexcept;

    /// This method will return true if the LinkView is detached (no assert).
    bool is_empty() const noexcept;

    /// This method will return 0 if the LinkView is detached (no assert).
    size_t size() const noexcept override;

    bool operator==(const LinkView&) const noexcept;
    bool operator!=(const LinkView&) const noexcept;

    // Getting links
    Table::ConstRowExpr operator[](size_t link_ndx) const noexcept;
    Table::RowExpr operator[](size_t link_ndx) noexcept;
    Table::ConstRowExpr get(size_t link_ndx) const noexcept;
    Table::RowExpr get(size_t link_ndx) noexcept;

    // Modifiers
    void add(size_t target_row_ndx);
    void insert(size_t link_ndx, size_t target_row_ndx);
    void set(size_t link_ndx, size_t target_row_ndx);
    /// Move the link at \a from_ndx such that it ends up at \a to_ndx. Other
    /// links are shifted as necessary in such a way that their order is
    /// preserved.
    ///
    /// Note that \a to_ndx is the desired final index of the moved link,
    /// therefore, `move(1,1)` is a no-op, while `move(1,2)` moves the link at
    /// index 1 by one position, such that it ends up at index 2. A side-effect
    /// of that, is that the link, that was originally at index 2, is moved to
    /// index 1.
    void move(size_t from_ndx, size_t to_ndx);
    void swap(size_t link1_ndx, size_t link2_ndx);
    void remove(size_t link_ndx);
    void clear();

    void sort(size_t column, bool ascending = true);
    void sort(const SortDescriptor& order);

    TableView get_sorted_view(size_t column_index, bool ascending = true) const;
    TableView get_sorted_view(SortDescriptor order) const;

    /// Remove the target row of the specified link from the target table. This
    /// also removes the specified link from this link list, and any other link
    /// pointing to that row. This is merely a shorthand for
    /// `get_target_table.move_last_over(get(link_ndx))`.
    void remove_target_row(size_t link_ndx);

    /// Remove all target rows pointed to by links in this link list, and clear
    /// this link list.
    void remove_all_target_rows();

    /// Search this list for a link to the specified target table row (specified
    /// by its index in the target table). If found, the index of the link to
    /// that row within this list is returned, otherwise `realm::not_found` is
    /// returned.
    size_t find(size_t target_row_ndx, size_t start = 0) const noexcept;

    const ColumnBase& get_column_base(size_t index)
        const override; // FIXME: `ColumnBase` is not part of the public API, so this function must be made private.
    const Table& get_origin_table() const noexcept;
    Table& get_origin_table() noexcept;

    size_t get_origin_row_index() const noexcept;

    const Table& get_target_table() const noexcept;
    Table& get_target_table() noexcept;

    // No-op because LinkViews are always kept in sync.
    uint_fast64_t sync_if_needed() const override;
    bool is_in_sync() const override
    {
        return true;
    }

private:
    struct ctor_cookie {
    };

    TableRef m_origin_table;
    LinkListColumn& m_origin_column;

    using HandoverPatch = LinkViewHandoverPatch;
    static void generate_patch(const ConstLinkViewRef& ref, std::unique_ptr<HandoverPatch>& patch);
    static LinkViewRef create_from_and_consume_patch(std::unique_ptr<HandoverPatch>& patch, Group& group);

    void detach();
    void set_origin_row_index(size_t row_ndx) noexcept;

    void do_insert(size_t link_ndx, size_t target_row_ndx);
    size_t do_set(size_t link_ndx, size_t target_row_ndx);
    size_t do_remove(size_t link_ndx);
    void do_clear(bool broken_reciprocal_backlinks);

    void do_nullify_link(size_t old_target_row_ndx);
    void do_update_link(size_t old_target_row_ndx, size_t new_target_row_ndx);
    void do_swap_link(size_t target_row_ndx_1, size_t target_row_ndx_2);

    void refresh_accessor_tree(size_t new_row_ndx) noexcept;

    void update_from_parent(size_t old_baseline) noexcept;

    Replication* get_repl() noexcept;
    void repl_unselect() noexcept;
    friend class _impl::TransactLogConvenientEncoder;

#ifdef REALM_DEBUG
    void verify(size_t row_ndx) const;
#endif
    // allocate using make_shared:
    static std::shared_ptr<LinkView> create(Table* origin_table, LinkListColumn&, size_t row_ndx);

    friend class _impl::LinkListFriend;
    friend class LinkListColumn;
    friend class LangBindHelper;
    friend class SharedGroup;
    friend class Query;
    friend class TableViewBase;

    // must be public for use by make_shared, but cannot be called from outside,
    // because ctor_cookie is private
public:
    LinkView(const ctor_cookie&, Table* origin_table, LinkListColumn&, size_t row_ndx);
};


// Implementation

inline LinkView::LinkView(const ctor_cookie&, Table* origin_table, LinkListColumn& column, size_t row_ndx)
    : RowIndexes(IntegerColumn::unattached_root_tag(), column.get_alloc()) // Throws
    , m_origin_table(origin_table->get_table_ref())
    , m_origin_column(column)
{
    Array& root = *m_row_indexes.get_root_array();
    root.set_parent(&column, row_ndx);
    if (ref_type ref = root.get_ref_from_parent())
        root.init_from_ref(ref);
}

inline std::shared_ptr<LinkView> LinkView::create(Table* origin_table, LinkListColumn& column, size_t row_ndx)
{
    return std::make_shared<LinkView>(ctor_cookie(), origin_table, column, row_ndx);
}

inline LinkView::~LinkView() noexcept
{
    if (is_attached()) {
        repl_unselect();
        m_origin_column.unregister_linkview();
    }
}

inline void LinkView::detach()
{
    REALM_ASSERT(is_attached());
    repl_unselect();
    m_origin_table.reset();
    m_row_indexes.detach();
}

inline bool LinkView::is_attached() const noexcept
{
    return static_cast<bool>(m_origin_table);
}

inline bool LinkView::is_empty() const noexcept
{
    if (!is_attached())
        return true;

    if (!m_row_indexes.is_attached())
        return true;

    return m_row_indexes.is_empty();
}

inline size_t LinkView::size() const noexcept
{
    if (!is_attached())
        return 0;

    if (!m_row_indexes.is_attached())
        return 0;

    return m_row_indexes.size();
}

inline bool LinkView::operator==(const LinkView& link_list) const noexcept
{
    Table& target_table_1 = m_origin_column.get_target_table();
    Table& target_table_2 = link_list.m_origin_column.get_target_table();
    if (target_table_1.get_index_in_group() != target_table_2.get_index_in_group())
        return false;
    if (!m_row_indexes.is_attached() || m_row_indexes.is_empty()) {
        return !link_list.m_row_indexes.is_attached() || link_list.m_row_indexes.is_empty();
    }
    return link_list.m_row_indexes.is_attached() && m_row_indexes.compare(link_list.m_row_indexes);
}

inline bool LinkView::operator!=(const LinkView& link_list) const noexcept
{
    return !(*this == link_list);
}

inline Table::ConstRowExpr LinkView::get(size_t link_ndx) const noexcept
{
    return const_cast<LinkView*>(this)->get(link_ndx);
}

inline Table::RowExpr LinkView::get(size_t link_ndx) noexcept
{
    REALM_ASSERT(is_attached());
    REALM_ASSERT(m_row_indexes.is_attached());
    REALM_ASSERT_3(link_ndx, <, m_row_indexes.size());

    Table& target_table = m_origin_column.get_target_table();
    size_t target_row_ndx = to_size_t(m_row_indexes.get(link_ndx));
    return target_table[target_row_ndx];
}

inline Table::ConstRowExpr LinkView::operator[](size_t link_ndx) const noexcept
{
    return get(link_ndx);
}

inline Table::RowExpr LinkView::operator[](size_t link_ndx) noexcept
{
    return get(link_ndx);
}

inline void LinkView::add(size_t target_row_ndx)
{
    REALM_ASSERT(is_attached());
    size_t ins_pos = (m_row_indexes.is_attached()) ? m_row_indexes.size() : 0;
    insert(ins_pos, target_row_ndx);
}

inline size_t LinkView::find(size_t target_row_ndx, size_t start) const noexcept
{
    REALM_ASSERT(is_attached());
    REALM_ASSERT_3(target_row_ndx, <, m_origin_column.get_target_table().size());
    REALM_ASSERT_3(start, <=, size());

    if (!m_row_indexes.is_attached())
        return not_found;

    return m_row_indexes.find_first(target_row_ndx, start);
}

inline const ColumnBase& LinkView::get_column_base(size_t index) const
{
    return get_target_table().get_column_base(index);
}

inline const Table& LinkView::get_origin_table() const noexcept
{
    return *m_origin_table;
}

inline Table& LinkView::get_origin_table() noexcept
{
    return *m_origin_table;
}

inline size_t LinkView::get_origin_row_index() const noexcept
{
    REALM_ASSERT(is_attached());
    return m_row_indexes.get_root_array()->get_ndx_in_parent();
}

inline void LinkView::set_origin_row_index(size_t row_ndx) noexcept
{
    REALM_ASSERT(is_attached());
    m_row_indexes.get_root_array()->set_ndx_in_parent(row_ndx);
}

inline const Table& LinkView::get_target_table() const noexcept
{
    return m_origin_column.get_target_table();
}

inline Table& LinkView::get_target_table() noexcept
{
    return m_origin_column.get_target_table();
}

inline void LinkView::refresh_accessor_tree(size_t new_row_ndx) noexcept
{
    Array& root = *m_row_indexes.get_root_array();
    root.set_ndx_in_parent(new_row_ndx);
    if (ref_type ref = root.get_ref_from_parent()) {
        root.init_from_ref(ref);
    }
    else {
        root.detach();
    }
}

inline void LinkView::update_from_parent(size_t old_baseline) noexcept
{
    if (m_row_indexes.is_attached())
        m_row_indexes.update_from_parent(old_baseline);
}

inline Replication* LinkView::get_repl() noexcept
{
    typedef _impl::TableFriend tf;
    return tf::get_repl(*m_origin_table);
}


// The purpose of this class is to give internal access to some, but not all of
// the non-public parts of LinkView.
class _impl::LinkListFriend {
public:
    static void do_set(LinkView& list, size_t link_ndx, size_t target_row_ndx)
    {
        list.do_set(link_ndx, target_row_ndx);
    }

    static void do_remove(LinkView& list, size_t link_ndx)
    {
        list.do_remove(link_ndx);
    }

    static void do_clear(LinkView& list)
    {
        bool broken_reciprocal_backlinks = false;
        list.do_clear(broken_reciprocal_backlinks);
    }

    static void do_insert(LinkView& list, size_t link_ndx, size_t target_row_ndx)
    {
        list.do_insert(link_ndx, target_row_ndx);
    }
};

} // namespace realm

#endif // REALM_LINK_VIEW_HPP
