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
#ifndef REALM_TABLE_VIEW_BASIC_HPP
#define REALM_TABLE_VIEW_BASIC_HPP

#include <realm/util/type_traits.hpp>
#include <realm/table_view.hpp>
#include <realm/table_accessors.hpp>

namespace realm {


/// Common base class for BasicTableView<Tab> and BasicTableView<const
/// Tab>.
///
/// \tparam Impl Is either TableView or ConstTableView.
template<class Tab, class View, class Impl>
class BasicTableViewBase {
public:
    typedef typename Tab::spec_type spec_type;
    typedef Tab table_type;

    bool is_empty() const noexcept { return m_impl.is_empty(); }
    bool is_attached() const noexcept { return m_impl.is_attached(); }
    size_t size() const noexcept { return m_impl.size(); }

    // Get row index in the source table this view is "looking" at.
    size_t get_source_ndx(size_t row_ndx) const noexcept
    {
        return m_impl.get_source_ndx(row_ndx);
    }

    void to_json(std::ostream& out) const { m_impl.to_json(out); };
    void to_string(std::ostream& out, size_t limit=500) const
    {
        m_impl.to_string(out, limit);
    }
    void row_to_string(size_t row_ndx, std::ostream& out) const
    {
        m_impl.row_to_string(row_ndx, out);
    }

private:
    typedef typename Tab::spec_type Spec;

    template<int col_idx>
    struct Col {
        typedef typename util::TypeAt<typename Spec::Columns, col_idx>::type value_type;
        typedef _impl::ColumnAccessor<View, col_idx, value_type> type;
    };
    typedef typename Spec::template ColNames<Col, View*> ColsAccessor;

    template<int col_idx>
    struct ConstCol {
        typedef typename util::TypeAt<typename Spec::Columns, col_idx>::type value_type;
        typedef _impl::ColumnAccessor<const View, col_idx, value_type> type;
    };
    typedef typename Spec::template ColNames<ConstCol, const View*> ConstColsAccessor;

public:
    ColsAccessor column() noexcept
    {
        return ColsAccessor(static_cast<View*>(this));
    }

    ConstColsAccessor column() const noexcept
    {
        return ConstColsAccessor(static_cast<const View*>(this));
    }

private:
    template<int col_idx>
    struct Field {
        typedef typename util::TypeAt<typename Spec::Columns, col_idx>::type value_type;
        typedef _impl::FieldAccessor<View, col_idx, value_type, std::is_const<Tab>::value> type;
    };
    typedef std::pair<View*, size_t> FieldInit;
    typedef typename Spec::template ColNames<Field, FieldInit> RowAccessor;

    template<int col_idx>
    struct ConstField {
        typedef typename util::TypeAt<typename Spec::Columns, col_idx>::type value_type;
        typedef _impl::FieldAccessor<const View, col_idx, value_type, true> type;
    };
    typedef std::pair<const View*, size_t> ConstFieldInit;
    typedef typename Spec::template ColNames<ConstField, ConstFieldInit> ConstRowAccessor;

public:
    RowAccessor operator[](size_t row_idx) noexcept
    {
        return RowAccessor(std::make_pair(static_cast<View*>(this), row_idx));
    }

    ConstRowAccessor operator[](size_t row_idx) const noexcept
    {
        return ConstRowAccessor(std::make_pair(static_cast<const View*>(this), row_idx));
    }

protected:
    template<class, int, class, bool>
    friend class _impl::FieldAccessor;

    template<class, int, class>
    friend class _impl::MixedFieldAccessorBase;

    template<class Spec>
    friend class BasicTable;

    Impl m_impl;

    BasicTableViewBase() {}
    BasicTableViewBase(const BasicTableViewBase& tv, typename Impl::HandoverPatch& patch,
                       ConstSourcePayload mode)
        : m_impl(tv.m_impl, patch, mode) { }
    BasicTableViewBase(BasicTableViewBase& tv, typename Impl::HandoverPatch& patch,
                       MutableSourcePayload mode)
        : m_impl(tv.m_impl, patch, mode) { }
    BasicTableViewBase(Impl i): m_impl(std::move(i)) {}

    Impl* get_impl() noexcept { return &m_impl; }
    const Impl* get_impl() const noexcept { return &m_impl; }
};




/// A BasicTableView wraps a TableView and provides a type and
/// structure safe set of access methods. The TableView methods are
/// not visible through a BasicTableView. A BasicTableView is used
/// essentially the same way as a BasicTable.
///
/// Note that this class is specialized for const-qualified parent
/// tables.
///
/// There are three levels of consteness to consider. A 'const
/// BasicTableView<Tab>' prohibits any modification of the table as
/// well as any modification of the table view, regardless of whether
/// Tab is const-qualified or not.
///
/// A non-const 'BasicTableView<Tab>' where Tab is const-qualified,
/// still does not allow any modification of the parent
/// table. However, the view itself may be modified, for example, by
/// reordering its rows.
///
/// A non-const 'BasicTableView<Tab>' where Tab is not
/// const-qualified, gives full modification access to both the parent
/// table and the view.
///
/// Just like TableView, a BasicTableView has both copy and move
/// semantics. See TableView for more on this.
///
/// \tparam Tab The possibly const-qualified parent table type. This
/// must always be an instance of the BasicTable template.
///
template<class Tab>
class BasicTableView: public BasicTableViewBase<Tab, BasicTableView<Tab>, TableView> {
private:
    typedef BasicTableViewBase<Tab, BasicTableView<Tab>, TableView> Base;

public:
    BasicTableView() {}
    BasicTableView& operator=(BasicTableView);
    friend BasicTableView move(BasicTableView& tv) { return BasicTableView(&tv); }

    // Deleting
    void remove(size_t ndx, RemoveMode underlying_mode = RemoveMode::ordered);
    void remove_last(RemoveMode underlying_mode = RemoveMode::ordered);
    void clear(RemoveMode underlying_mode = RemoveMode::ordered);

    // Resort after requery
    void apply_same_order(BasicTableView& order) { Base::m_impl.apply_same_order(order.m_impl); }

    Tab& get_parent() noexcept
    {
        return static_cast<Tab&>(Base::m_impl.get_parent());
    }

    const Tab& get_parent() const noexcept
    {
        return static_cast<const Tab&>(Base::m_impl.get_parent());
    }


public:
    void move_assign(BasicTableView<Tab>& tv)
    {
        Base::m_impl.move_assign(tv.m_impl);
    }
    using HandoverPatch = TableViewHandoverPatch;

    std::unique_ptr<BasicTableView<Tab>>
    clone_for_handover(std::unique_ptr<HandoverPatch>& patch, ConstSourcePayload mode) const
    {
        patch.reset(new HandoverPatch);
        std::unique_ptr<BasicTableView<Tab>> retval(new BasicTableView<Tab>(*this, *patch, mode));
        return retval;
    }

    std::unique_ptr<BasicTableView<Tab>>
    clone_for_handover(std::unique_ptr<HandoverPatch>& patch, MutableSourcePayload mode)
    {
        patch.reset(new HandoverPatch);
        std::unique_ptr<BasicTableView<Tab>> retval(new BasicTableView<Tab>(*this, *patch, mode));
        return retval;
    }

    void apply_and_consume_patch(std::unique_ptr<HandoverPatch>& patch, Group& group)
    {
        apply_patch(*patch, group);
        patch.reset();
    }

    BasicTableView(const BasicTableView<Tab>& source, HandoverPatch& patch,
                   ConstSourcePayload mode):
        Base(source, patch, mode)
    {
    }

    BasicTableView(BasicTableView<Tab>& source, HandoverPatch& patch,
                   MutableSourcePayload mode):
        Base(source, patch, mode)
    {
    }

    void apply_patch(TableView::HandoverPatch& patch, Group& group)
    {
        Base::m_impl.apply_patch(patch, group);
    }

private:
    BasicTableView(BasicTableView* tv): Base(move(tv->m_impl)) {}
    BasicTableView(TableView tv): Base(std::move(tv)) {}

    template<class Subtab>
    Subtab* get_subtable_ptr(size_t column_ndx, size_t ndx)
    {
        return get_parent().template
            get_subtable_ptr<Subtab>(column_ndx, Base::m_impl.get_source_ndx(ndx));
    }

    template<class Subtab>
    const Subtab* get_subtable_ptr(size_t column_ndx, size_t ndx) const
    {
        return get_parent().template
            get_subtable_ptr<Subtab>(column_ndx, Base::m_impl.get_source_ndx(ndx));
    }

    friend class BasicTableView<const Tab>;

    template<class, int, class, bool>
    friend class _impl::FieldAccessor;

    template<class, int, class>
    friend class _impl::MixedFieldAccessorBase;

    template<class, int, class>
    friend class _impl::ColumnAccessorBase;

    template<class, int, class>
    friend class _impl::ColumnAccessor;

    friend class Tab::Query;
};




/// Specialization for 'const' access to parent table.
///
template<class Tab>
class BasicTableView<const Tab>:
    public BasicTableViewBase<const Tab, BasicTableView<const Tab>, ConstTableView> {
private:
    typedef BasicTableViewBase<const Tab, BasicTableView<const Tab>, ConstTableView> Base;

public:
    BasicTableView() {}
    BasicTableView& operator=(BasicTableView tv) { Base::m_impl = move(tv.m_impl); return *this; }
    friend BasicTableView move(BasicTableView& tv) { return BasicTableView(&tv); }

    /// Construct BasicTableView<const Tab> from BasicTableView<Tab>.
    ///
        BasicTableView(BasicTableView<Tab> tv): Base(std::move(tv.m_impl)) {}

    /// Assign BasicTableView<Tab> to BasicTableView<const Tab>.
    ///
    BasicTableView& operator=(BasicTableView<Tab> tv)
    {
        Base::m_impl = std::move(tv.m_impl);
        return *this;
    }

    const Tab& get_parent() const noexcept
    {
        return static_cast<const Tab&>(Base::m_impl.get_parent());
    }

private:
    BasicTableView(BasicTableView* tv): Base(move(tv->m_impl)) {}
    BasicTableView(ConstTableView tv): Base(std::move(tv)) {}

    template<class Subtab>
    const Subtab* get_subtable_ptr(size_t column_ndx, size_t ndx) const
    {
        return get_parent().template
            get_subtable_ptr<Subtab>(column_ndx, Base::m_impl.get_source_ndx(ndx));
    }

    template<class, int, class, bool>
    friend class _impl::FieldAccessor;

    template<class, int, class>
    friend class _impl::MixedFieldAccessorBase;

    template<class, int, class>
    friend class _impl::ColumnAccessorBase;

    template<class, int, class>
    friend class _impl::ColumnAccessor;

    friend class Tab::Query;
};




// Implementation

template<class Tab>
inline BasicTableView<Tab>& BasicTableView<Tab>::operator=(BasicTableView tv)
{
    Base::m_impl = std::move(tv.m_impl);
    return *this;
}

template<class Tab>
inline void BasicTableView<Tab>::remove(size_t ndx, RemoveMode underlying_mode)
{
    Base::m_impl.remove(ndx, underlying_mode);
}

template<class Tab>
inline void BasicTableView<Tab>::remove_last(RemoveMode underlying_mode)
{
    Base::m_impl.remove_last(underlying_mode);
}

template<class Tab>
inline void BasicTableView<Tab>::clear(RemoveMode underlying_mode)
{
    Base::m_impl.clear(underlying_mode);
}

} // namespace realm

#endif // REALM_TABLE_VIEW_BASIC_HPP
