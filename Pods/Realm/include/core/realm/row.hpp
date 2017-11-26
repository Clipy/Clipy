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

#ifndef REALM_ROW_HPP
#define REALM_ROW_HPP

#include <cstdint>

#include <realm/util/type_traits.hpp>
#include <realm/mixed.hpp>
#include <realm/table_ref.hpp>
#include <realm/link_view_fwd.hpp>
#include <realm/handover_defs.hpp>

namespace realm {

template <class>
class BasicRow;


/// This class is a "mixin" and contains the common set of functions for several
/// distinct row-like classes.
///
/// There is a direct and natural correspondance between the functions in this
/// class and functions in Table of the same name. For example:
///
///     table[i].get_int(j) == table.get_int(i,j)
///
/// The effect of calling most of the row accessor functions on a detached
/// accessor is unspecified and may lead to general corruption, and/or a
/// crash. The exceptions are is_attached(), detach(), get_table(), get_index(),
/// and the destructor. Note however, that get_index() will still return an
/// unspecified value for a deatched accessor.
///
/// When a row accessor is evaluated in a boolean context, it evaluates to true
/// if, and only if it is attached.
///
/// \tparam T A const or non-const table type (currently either `Table` or
/// `const Table`).
///
/// \tparam R A specific row accessor class (BasicRow or BasicRowExpr) providing
/// members `T* impl_get_table() const`, `size_t impl_get_row_ndx()
/// const`, and `void impl_detach()`. Neither are allowed to throw.
///
/// \sa Table
/// \sa BasicRow
template <class T, class R>
class RowFuncs {
public:
    typedef T table_type;

    typedef BasicTableRef<const T> ConstTableRef;
    typedef BasicTableRef<T> TableRef; // Same as ConstTableRef if `T` is 'const'

    typedef typename util::CopyConst<T, LinkView>::type L;
    using ConstLinkViewRef = std::shared_ptr<const L>;
    using LinkViewRef = std::shared_ptr<L>; // Same as ConstLinkViewRef if `T` is 'const'

    int_fast64_t get_int(size_t col_ndx) const noexcept;
    bool get_bool(size_t col_ndx) const noexcept;
    float get_float(size_t col_ndx) const noexcept;
    double get_double(size_t col_ndx) const noexcept;
    StringData get_string(size_t col_ndx) const noexcept;
    BinaryData get_binary(size_t col_ndx) const noexcept;
    OldDateTime get_olddatetime(size_t col_ndx) const noexcept;
    Timestamp get_timestamp(size_t col_ndx) const noexcept;
    ConstTableRef get_subtable(size_t col_ndx) const;
    TableRef get_subtable(size_t col_ndx);
    size_t get_subtable_size(size_t col_ndx) const noexcept;
    size_t get_link(size_t col_ndx) const noexcept;
    bool is_null_link(size_t col_ndx) const noexcept;
    bool is_null(size_t col_ndx) const noexcept;
    ConstLinkViewRef get_linklist(size_t col_ndx) const;
    LinkViewRef get_linklist(size_t col_ndx);
    bool linklist_is_empty(size_t col_ndx) const noexcept;
    size_t get_link_count(size_t col_ndx) const noexcept;
    Mixed get_mixed(size_t col_ndx) const noexcept;
    DataType get_mixed_type(size_t col_ndx) const noexcept;

    template <typename U>
    U get(size_t col_ndx) const noexcept;

    void set_int(size_t col_ndx, int_fast64_t value);
    void set_int_unique(size_t col_ndx, int_fast64_t value);
    void add_int(size_t col_ndx, int_fast64_t value);
    void set_bool(size_t col_ndx, bool value);
    void set_float(size_t col_ndx, float value);
    void set_double(size_t col_ndx, double value);
    void set_string(size_t col_ndx, StringData value);
    void set_string_unique(size_t col_ndx, StringData value);
    void set_binary(size_t col_ndx, BinaryData value);
    void set_olddatetime(size_t col_ndx, OldDateTime value);
    void set_timestamp(size_t col_ndx, Timestamp value);
    void set_subtable(size_t col_ndx, const Table* value);
    void set_link(size_t col_ndx, size_t value);
    void nullify_link(size_t col_ndx);
    void set_mixed(size_t col_ndx, Mixed value);
    void set_mixed_subtable(size_t col_ndx, const Table* value);
    void set_null(size_t col_ndx);
    void set_null_unique(size_t col_ndx);

    template <typename U>
    void set(size_t col_ndx, U&& value, bool is_default = false);

    template <typename U>
    void set_unique(size_t col_ndx, U&& value);

    void insert_substring(size_t col_ndx, size_t pos, StringData);
    void remove_substring(size_t col_ndx, size_t pos, size_t size);

    //@{
    /// Note that these operations will cause the row accessor to be detached.
    void remove();
    void move_last_over();
    //@}

    size_t get_backlink_count() const noexcept;
    size_t get_backlink_count(const Table& src_table, size_t src_col_ndx) const noexcept;
    size_t get_backlink(const Table& src_table, size_t src_col_ndx, size_t backlink_ndx) const noexcept;

    size_t get_column_count() const noexcept;
    DataType get_column_type(size_t col_ndx) const noexcept;
    StringData get_column_name(size_t col_ndx) const noexcept;
    size_t get_column_index(StringData name) const noexcept;

    /// Returns true if, and only if this accessor is currently attached to a
    /// row.
    ///
    /// A row accesor may get detached from the underlying row for various
    /// reasons (see below). When it does, it no longer refers to anything, and
    /// can no longer be used, except for calling is_attached(), detach(),
    /// get_table(), get_index(), and the destructor. The consequences of
    /// calling other methods on a detached row accessor are unspecified. There
    /// are a few Realm functions (Table::find_pkey_int()) that return a
    /// detached row accessor to indicate a 'null' result. In all other cases,
    /// however, row accessors obtained by calling functions in the Realm API
    /// are always in the 'attached' state immediately upon return from those
    /// functions.
    ///
    /// A row accessor becomes detached if the underlying row is removed, if the
    /// associated table accessor becomes detached, or if the detach() method is
    /// called. A row accessor does not become detached for any other reason.
    bool is_attached() const noexcept;

    /// Detach this accessor from the row it was attached to. This function has
    /// no effect if the accessor was already detached (idempotency).
    void detach() noexcept;

    /// The table containing the row to which this accessor is currently
    /// bound. For a detached accessor, the returned value is null.
    const table_type* get_table() const noexcept;
    table_type* get_table() noexcept;

    /// The index of the row to which this accessor is currently bound. For a
    /// detached accessor, the returned value is unspecified.
    size_t get_index() const noexcept;

    explicit operator bool() const noexcept;

private:
    const T* table() const noexcept;
    T* table() noexcept;
    size_t row_ndx() const noexcept;
};


/// This class is a special kind of row accessor. It differes from a real row
/// accessor (BasicRow) by having a trivial and fast copy constructor and
/// descructor. It is supposed to be used as the return type of functions such
/// as Table::operator[](), and then to be used as a basis for constructing a
/// real row accessor. Objects of this class are intended to only ever exist as
/// temporaries.
///
/// In contrast to a real row accessor (`BasicRow`), objects of this class do
/// not keep the parent table "alive", nor are they maintained (adjusted) across
/// row insertions and row removals like real row accessors are.
///
/// \sa BasicRow
template <class T>
class BasicRowExpr : public RowFuncs<T, BasicRowExpr<T>> {
public:
    BasicRowExpr() noexcept = default;

    template <class U>
    BasicRowExpr(const BasicRowExpr<U>&) noexcept;

    template <class U>
    BasicRowExpr(const BasicRow<U>&) noexcept;

private:
    T* m_table = nullptr;       // nullptr if detached.
    size_t m_row_ndx = 0; // Undefined if detached.

    BasicRowExpr(T*, size_t init_row_ndx) noexcept;

    T* impl_get_table() const noexcept;
    size_t impl_get_row_ndx() const noexcept;
    void impl_detach() noexcept;

    // Make impl_get_table(), impl_get_row_ndx(), and impl_detach() accessible
    // from RowFuncs.
    friend class RowFuncs<T, BasicRowExpr<T>>;

    // Make m_table and m_row_ndx accessible from BasicRowExpr(const
    // BasicRowExpr<U>&) for any U.
    template <class>
    friend class BasicRowExpr;

    // Make m_table and m_row_ndx accessible from
    // BasicRow::BaicRow(BasicRowExpr<U>) for any U.
    template <class>
    friend class BasicRow;

    // Make BasicRowExpr(T*, size_t) accessible from Table.
    friend class Table;
};

// fwd decl
class Group;

class RowBase {
protected:
    TableRef m_table; // nullptr if detached.
    size_t m_row_ndx; // Undefined if detached.

    void attach(Table*, size_t row_ndx) noexcept;
    void reattach(Table*, size_t row_ndx) noexcept;
    void impl_detach() noexcept;

    RowBase()
    {
    }

    RowBase(const RowBase&) = delete;
    using HandoverPatch = RowBaseHandoverPatch;

    RowBase(const RowBase& source, HandoverPatch& patch);

public:
    static void generate_patch(const RowBase& source, HandoverPatch& patch);
    void apply_patch(HandoverPatch& patch, Group& group);

private:
    RowBase* m_prev = nullptr; // nullptr if first, undefined if detached.
    RowBase* m_next = nullptr; // nullptr if last, undefined if detached.

    // Table needs to be able to modify m_table and m_row_ndx.
    friend class Table;
};


/// An accessor class for table rows (a.k.a. a "row accessor").
///
/// For as long as it remains attached, a row accessor will keep the parent
/// table accessor alive. In case the lifetime of the parent table is not
/// managed by reference counting (such as when the table is an automatic
/// variable on the stack), the destruction of the table will cause all
/// remaining row accessors to be detached.
///
/// While attached, a row accessor is bound to a particular row of the parent
/// table. If that row is removed, the accesssor becomes detached. If rows are
/// inserted or removed before it (at lower row index), then the accessor is
/// automatically adjusted to account for the change in index of the row to
/// which the accessor is bound. In other words, a row accessor is bound to the
/// contents of a row, not to a row index. See also is_attached().
///
/// Row accessors are created and used as follows:
///
///     Row row       = table[7];  // 8th row of `table`
///     ConstRow crow = ctable[2]; // 3rd row of const `ctable`
///     Row first_row = table.front();
///     Row last_row  = table.back();
///
///     float v = row.get_float(1); // Get the float in the 2nd column
///     row.set_string(0, "foo");   // Update the string in the 1st column
///
///     Table* t = row.get_table();      // The parent table
///     size_t i = row.get_index(); // The current row index
///
/// \sa RowFuncs
template <class T>
class BasicRow : private RowBase, public RowFuncs<T, BasicRow<T>> {
public:
    BasicRow() noexcept;

    template <class U>
    BasicRow(BasicRowExpr<U>) noexcept;

    BasicRow(const BasicRow<T>&) noexcept;

    template <class U>
    BasicRow(const BasicRow<U>&) noexcept;

    template <class U>
    BasicRow& operator=(BasicRowExpr<U>) noexcept;

    template <class U>
    BasicRow& operator=(BasicRow<U>) noexcept;

    BasicRow& operator=(const BasicRow<T>&) noexcept;

    ~BasicRow() noexcept;

private:
    T* impl_get_table() const noexcept;
    size_t impl_get_row_ndx() const noexcept;

    // Make impl_get_table(), impl_get_row_ndx(), and impl_detach() accessible
    // from RowFuncs.
    friend class RowFuncs<T, BasicRow<T>>;

    // Make m_table and m_row_ndx accessible from BasicRow(const BasicRow<U>&)
    // for any U.
    template <class>
    friend class BasicRow;

    // Make m_table and m_row_ndx accessible from BasicRowExpr(const
    // BasicRow<U>&) for any U.
    template <class>
    friend class BasicRowExpr;

public:
    std::unique_ptr<BasicRow<T>> clone_for_handover(std::unique_ptr<HandoverPatch>& patch) const
    {
        patch.reset(new HandoverPatch);
        std::unique_ptr<BasicRow<T>> retval(new BasicRow<T>(*this, *patch));
        return retval;
    }

    static void generate_patch(const BasicRow& row, std::unique_ptr<HandoverPatch>& patch)
    {
        patch.reset(new HandoverPatch);
        RowBase::generate_patch(row, *patch);
    }

    void apply_and_consume_patch(std::unique_ptr<HandoverPatch>& patch, Group& group)
    {
        apply_patch(*patch, group);
        patch.reset();
    }

    void apply_patch(HandoverPatch& patch, Group& group)
    {
        RowBase::apply_patch(patch, group);
    }

private:
    BasicRow(const BasicRow<T>& source, HandoverPatch& patch)
        : RowBase(source, patch)
    {
    }
    friend class SharedGroup;
};

typedef BasicRow<Table> Row;
typedef BasicRow<const Table> ConstRow;


// Implementation

template <class T, class R>
inline int_fast64_t RowFuncs<T, R>::get_int(size_t col_ndx) const noexcept
{
    return table()->get_int(col_ndx, row_ndx());
}

template <class T, class R>
inline bool RowFuncs<T, R>::get_bool(size_t col_ndx) const noexcept
{
    return table()->get_bool(col_ndx, row_ndx());
}

template <class T, class R>
inline float RowFuncs<T, R>::get_float(size_t col_ndx) const noexcept
{
    return table()->get_float(col_ndx, row_ndx());
}

template <class T, class R>
inline double RowFuncs<T, R>::get_double(size_t col_ndx) const noexcept
{
    return table()->get_double(col_ndx, row_ndx());
}

template <class T, class R>
inline StringData RowFuncs<T, R>::get_string(size_t col_ndx) const noexcept
{
    return table()->get_string(col_ndx, row_ndx());
}

template <class T, class R>
inline BinaryData RowFuncs<T, R>::get_binary(size_t col_ndx) const noexcept
{
    return table()->get_binary(col_ndx, row_ndx());
}

template <class T, class R>
inline OldDateTime RowFuncs<T, R>::get_olddatetime(size_t col_ndx) const noexcept
{
    return table()->get_olddatetime(col_ndx, row_ndx());
}

template <class T, class R>
inline Timestamp RowFuncs<T, R>::get_timestamp(size_t col_ndx) const noexcept
{
    return table()->get_timestamp(col_ndx, row_ndx());
}

template <class T, class R>
inline typename RowFuncs<T, R>::ConstTableRef RowFuncs<T, R>::get_subtable(size_t col_ndx) const
{
    return table()->get_subtable(col_ndx, row_ndx()); // Throws
}

template <class T, class R>
inline typename RowFuncs<T, R>::TableRef RowFuncs<T, R>::get_subtable(size_t col_ndx)
{
    return table()->get_subtable(col_ndx, row_ndx()); // Throws
}

template <class T, class R>
inline size_t RowFuncs<T, R>::get_subtable_size(size_t col_ndx) const noexcept
{
    return table()->get_subtable_size(col_ndx, row_ndx());
}

template <class T, class R>
inline size_t RowFuncs<T, R>::get_link(size_t col_ndx) const noexcept
{
    return table()->get_link(col_ndx, row_ndx());
}

template <class T, class R>
inline bool RowFuncs<T, R>::is_null_link(size_t col_ndx) const noexcept
{
    return table()->is_null_link(col_ndx, row_ndx());
}

template <class T, class R>
inline bool RowFuncs<T, R>::is_null(size_t col_ndx) const noexcept
{
    return table()->is_null(col_ndx, row_ndx());
}

template <class T, class R>
inline typename RowFuncs<T, R>::ConstLinkViewRef RowFuncs<T, R>::get_linklist(size_t col_ndx) const
{
    return table()->get_linklist(col_ndx, row_ndx()); // Throws
}

template <class T, class R>
inline typename RowFuncs<T, R>::LinkViewRef RowFuncs<T, R>::get_linklist(size_t col_ndx)
{
    return table()->get_linklist(col_ndx, row_ndx()); // Throws
}

template <class T, class R>
inline bool RowFuncs<T, R>::linklist_is_empty(size_t col_ndx) const noexcept
{
    return table()->linklist_is_empty(col_ndx, row_ndx());
}

template <class T, class R>
inline size_t RowFuncs<T, R>::get_link_count(size_t col_ndx) const noexcept
{
    return table()->get_link_count(col_ndx, row_ndx());
}

template <class T, class R>
inline Mixed RowFuncs<T, R>::get_mixed(size_t col_ndx) const noexcept
{
    return table()->get_mixed(col_ndx, row_ndx());
}

template <class T, class R>
inline DataType RowFuncs<T, R>::get_mixed_type(size_t col_ndx) const noexcept
{
    return table()->get_mixed_type(col_ndx, row_ndx());
}

template <class T, class R>
template <class U>
inline U RowFuncs<T, R>::get(size_t col_ndx) const noexcept
{
    return table()->template get<U>(col_ndx, row_ndx());
}

template <class T, class R>
inline void RowFuncs<T, R>::set_int(size_t col_ndx, int_fast64_t value)
{
    table()->set_int(col_ndx, row_ndx(), value); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::set_int_unique(size_t col_ndx, int_fast64_t value)
{
    table()->set_int_unique(col_ndx, row_ndx(), value); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::add_int(size_t col_ndx, int_fast64_t value)
{
    table()->add_int(col_ndx, row_ndx(), value); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::set_bool(size_t col_ndx, bool value)
{
    table()->set_bool(col_ndx, row_ndx(), value); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::set_float(size_t col_ndx, float value)
{
    table()->set_float(col_ndx, row_ndx(), value); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::set_double(size_t col_ndx, double value)
{
    table()->set_double(col_ndx, row_ndx(), value); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::set_string(size_t col_ndx, StringData value)
{
    table()->set_string(col_ndx, row_ndx(), value); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::set_string_unique(size_t col_ndx, StringData value)
{
    table()->set_string_unique(col_ndx, row_ndx(), value); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::set_binary(size_t col_ndx, BinaryData value)
{
    table()->set_binary(col_ndx, row_ndx(), value); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::set_olddatetime(size_t col_ndx, OldDateTime value)
{
    table()->set_olddatetime(col_ndx, row_ndx(), value); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::set_timestamp(size_t col_ndx, Timestamp value)
{
    table()->set_timestamp(col_ndx, row_ndx(), value); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::set_subtable(size_t col_ndx, const Table* value)
{
    table()->set_subtable(col_ndx, row_ndx(), value); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::set_link(size_t col_ndx, size_t value)
{
    table()->set_link(col_ndx, row_ndx(), value); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::nullify_link(size_t col_ndx)
{
    table()->nullify_link(col_ndx, row_ndx()); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::set_mixed(size_t col_ndx, Mixed value)
{
    table()->set_mixed(col_ndx, row_ndx(), value); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::set_mixed_subtable(size_t col_ndx, const Table* value)
{
    table()->set_mixed_subtable(col_ndx, row_ndx(), value); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::set_null(size_t col_ndx)
{
    table()->set_null(col_ndx, row_ndx()); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::set_null_unique(size_t col_ndx)
{
    table()->set_null_unique(col_ndx, row_ndx()); // Throws
}

template <class T, class R>
template <class U>
inline void RowFuncs<T, R>::set(size_t col_ndx, U&& value, bool is_default)
{
    table()->set(col_ndx, row_ndx(), std::forward<U>(value), is_default); // Throws
}

template <class T, class R>
template <class U>
inline void RowFuncs<T, R>::set_unique(size_t col_ndx, U&& value)
{
    table()->set_unique(col_ndx, row_ndx(), std::forward<U>(value)); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::insert_substring(size_t col_ndx, size_t pos, StringData value)
{
    table()->insert_substring(col_ndx, row_ndx(), pos, value); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::remove_substring(size_t col_ndx, size_t pos, size_t size)
{
    table()->remove_substring(col_ndx, row_ndx(), pos, size); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::remove()
{
    table()->remove(row_ndx()); // Throws
}

template <class T, class R>
inline void RowFuncs<T, R>::move_last_over()
{
    table()->move_last_over(row_ndx()); // Throws
}

template <class T, class R>
inline size_t RowFuncs<T, R>::get_backlink_count() const noexcept
{
    return table()->get_backlink_count(row_ndx());
}

template <class T, class R>
inline size_t RowFuncs<T, R>::get_backlink_count(const Table& src_table, size_t src_col_ndx) const noexcept
{
    return table()->get_backlink_count(row_ndx(), src_table, src_col_ndx);
}

template <class T, class R>
inline size_t RowFuncs<T, R>::get_backlink(const Table& src_table, size_t src_col_ndx, size_t backlink_ndx) const
    noexcept
{
    return table()->get_backlink(row_ndx(), src_table, src_col_ndx, backlink_ndx);
}

template <class T, class R>
inline size_t RowFuncs<T, R>::get_column_count() const noexcept
{
    return table()->get_column_count();
}

template <class T, class R>
inline DataType RowFuncs<T, R>::get_column_type(size_t col_ndx) const noexcept
{
    return table()->get_column_type(col_ndx);
}

template <class T, class R>
inline StringData RowFuncs<T, R>::get_column_name(size_t col_ndx) const noexcept
{
    return table()->get_column_name(col_ndx);
}

template <class T, class R>
inline size_t RowFuncs<T, R>::get_column_index(StringData name) const noexcept
{
    return table()->get_column_index(name);
}

template <class T, class R>
inline bool RowFuncs<T, R>::is_attached() const noexcept
{
    return static_cast<const R*>(this)->impl_get_table();
}

template <class T, class R>
inline void RowFuncs<T, R>::detach() noexcept
{
    static_cast<R*>(this)->impl_detach();
}

template <class T, class R>
inline const T* RowFuncs<T, R>::get_table() const noexcept
{
    return table();
}

template <class T, class R>
inline T* RowFuncs<T, R>::get_table() noexcept
{
    return table();
}

template <class T, class R>
inline size_t RowFuncs<T, R>::get_index() const noexcept
{
    return row_ndx();
}

template <class T, class R>
inline RowFuncs<T, R>::operator bool() const noexcept
{
    return is_attached();
}

template <class T, class R>
inline const T* RowFuncs<T, R>::table() const noexcept
{
    return static_cast<const R*>(this)->impl_get_table();
}

template <class T, class R>
inline T* RowFuncs<T, R>::table() noexcept
{
    return static_cast<R*>(this)->impl_get_table();
}

template <class T, class R>
inline size_t RowFuncs<T, R>::row_ndx() const noexcept
{
    return static_cast<const R*>(this)->impl_get_row_ndx();
}


template <class T>
template <class U>
inline BasicRowExpr<T>::BasicRowExpr(const BasicRowExpr<U>& expr) noexcept
    : m_table(expr.m_table)
    , m_row_ndx(expr.m_row_ndx)
{
}

template <class T>
template <class U>
inline BasicRowExpr<T>::BasicRowExpr(const BasicRow<U>& row) noexcept
    : m_table(row.m_table.get())
    , m_row_ndx(row.m_row_ndx)
{
}

template <class T>
inline BasicRowExpr<T>::BasicRowExpr(T* init_table, size_t init_row_ndx) noexcept
    : m_table(init_table)
    , m_row_ndx(init_row_ndx)
{
}

template <class T>
inline T* BasicRowExpr<T>::impl_get_table() const noexcept
{
    return m_table;
}

template <class T>
inline size_t BasicRowExpr<T>::impl_get_row_ndx() const noexcept
{
    return m_row_ndx;
}

template <class T>
inline void BasicRowExpr<T>::impl_detach() noexcept
{
    m_table = nullptr;
}


template <class T>
inline BasicRow<T>::BasicRow() noexcept
{
}

template <class T>
inline BasicRow<T>::BasicRow(const BasicRow<T>& row) noexcept
    : RowBase()
{
    attach(const_cast<Table*>(row.m_table.get()), row.m_row_ndx);
}

template <class T>
template <class U>
inline BasicRow<T>::BasicRow(BasicRowExpr<U> expr) noexcept
{
    T* expr_table = expr.m_table; // Check that pointer types are compatible
    attach(const_cast<Table*>(expr_table), expr.m_row_ndx);
}

template <class T>
template <class U>
inline BasicRow<T>::BasicRow(const BasicRow<U>& row) noexcept
{
    T* row_table = row.m_table.get(); // Check that pointer types are compatible
    attach(const_cast<Table*>(row_table), row.m_row_ndx);
}

template <class T>
template <class U>
inline BasicRow<T>& BasicRow<T>::operator=(BasicRowExpr<U> expr) noexcept
{
    T* expr_table = expr.m_table; // Check that pointer types are compatible
    reattach(const_cast<Table*>(expr_table), expr.m_row_ndx);
    return *this;
}

template <class T>
template <class U>
inline BasicRow<T>& BasicRow<T>::operator=(BasicRow<U> row) noexcept
{
    T* row_table = row.m_table.get(); // Check that pointer types are compatible
    reattach(const_cast<Table*>(row_table), row.m_row_ndx);
    return *this;
}

template <class T>
inline BasicRow<T>& BasicRow<T>::operator=(const BasicRow<T>& row) noexcept
{
    reattach(const_cast<Table*>(row.m_table.get()), row.m_row_ndx);
    return *this;
}

template <class T>
inline BasicRow<T>::~BasicRow() noexcept
{
    RowBase::impl_detach();
}

template <class T>
inline T* BasicRow<T>::impl_get_table() const noexcept
{
    return m_table.get();
}

template <class T>
inline size_t BasicRow<T>::impl_get_row_ndx() const noexcept
{
    return m_row_ndx;
}

} // namespace realm

#endif // REALM_ROW_HPP
