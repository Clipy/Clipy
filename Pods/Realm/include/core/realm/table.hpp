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

#ifndef REALM_TABLE_HPP
#define REALM_TABLE_HPP

#include <algorithm>
#include <map>
#include <utility>
#include <typeinfo>
#include <memory>
#include <mutex>

#include <realm/util/features.h>
#include <realm/util/thread.hpp>
#include <realm/table_ref.hpp>
#include <realm/link_view_fwd.hpp>
#include <realm/row.hpp>
#include <realm/descriptor_fwd.hpp>
#include <realm/spec.hpp>
#include <realm/mixed.hpp>
#include <realm/query.hpp>
#include <realm/column.hpp>
#include <realm/column_binary.hpp>

namespace realm {

class BacklinkColumn;
template <class>
class BacklinkCount;
class BinaryColumy;
class ConstTableView;
class Group;
class LinkColumn;
class LinkColumnBase;
class LinkListColumn;
class LinkView;
class SortDescriptor;
class StringIndex;
class TableView;
class TableViewBase;
class TimestampColumn;
template <class>
class Columns;
template <class>
class SubQuery;
struct LinkTargetInfo;

struct SubTable {
};
struct Link {
};
typedef Link LinkList;
typedef Link BackLink;

namespace _impl {
class TableFriend;
}
namespace metrics {
class QueryInfo;
}

class Replication;


/// FIXME: Table assignment (from any group to any group) could be made aliasing
/// safe as follows: Start by cloning source table into target allocator. On
/// success, assign, and then deallocate any previous structure at the target.
///
/// FIXME: It might be desirable to have a 'table move' feature between two
/// places inside the same group (say from a subtable or a mixed column to group
/// level). This could be done in a very efficient manner.
///
/// FIXME: When compiling in debug mode, all public non-static table functions
/// should REALM_ASSERT(is_attached()).
class Table {
public:
    /// Construct a new freestanding top-level table with static
    /// lifetime.
    ///
    /// This constructor should be used only when placing a table
    /// instance on the stack, and it is then the responsibility of
    /// the application that there are no objects of type TableRef or
    /// ConstTableRef that refer to it, or to any of its subtables,
    /// when it goes out of scope. To create a top-level table with
    /// dynamic lifetime, use Table::create() instead.
    Table(Allocator& = Allocator::get_default());

    /// Construct a copy of the specified table as a new freestanding
    /// top-level table with static lifetime.
    ///
    /// This constructor should be used only when placing a table
    /// instance on the stack, and it is then the responsibility of
    /// the application that there are no objects of type TableRef or
    /// ConstTableRef that refer to it, or to any of its subtables,
    /// when it goes out of scope. To create a top-level table with
    /// dynamic lifetime, use Table::copy() instead.
    Table(const Table&, Allocator& = Allocator::get_default());

    ~Table() noexcept;

    Allocator& get_alloc() const;

    /// Construct a new freestanding top-level table with dynamic lifetime.
    static TableRef create(Allocator& = Allocator::get_default());

    /// Construct a copy of the specified table as a new freestanding top-level
    /// table with dynamic lifetime.
    TableRef copy(Allocator& = Allocator::get_default()) const;

    /// Returns true if, and only if this accessor is currently attached to an
    /// underlying table.
    ///
    /// A table accessor may get detached from the underlying row for various
    /// reasons (see below). When it does, it no longer refers to anything, and
    /// can no longer be used, except for calling is_attached(). The
    /// consequences of calling other non-static functions on a detached table
    /// accessor are unspecified. Table accessors obtained by calling functions in
    /// the Realm API are always in the 'attached' state immediately upon
    /// return from those functions.
    ///
    /// A table accessor of a free-standing table never becomes detached (except
    /// during its eventual destruction). A group-level table accessor becomes
    /// detached if the underlying table is removed from the group, or when the
    /// group accessor is destroyed. A subtable accessor becomes detached if the
    /// underlying subtable is removed, or if the parent table accessor is
    /// detached. A table accessor does not become detached for any other reason
    /// than those mentioned here.
    ///
    /// FIXME: High level language bindings will probably want to be able to
    /// explicitely detach a group and all tables of that group if any modifying
    /// operation fails (e.g. memory allocation failure) (and something similar
    /// for freestanding tables) since that leaves the group in state where any
    /// further access is disallowed. This way they will be able to reliably
    /// intercept any attempt at accessing such a failed group.
    ///
    /// FIXME: The C++ documentation must state that if any modifying operation
    /// on a group (incl. tables, subtables, and specs) or on a free standing
    /// table (incl. subtables and specs) fails, then any further access to that
    /// group (except ~Group()) or freestanding table (except ~Table()) has
    /// undefined behaviour and is considered an error on behalf of the
    /// application. Note that even Table::is_attached() is disallowed in this
    /// case.
    bool is_attached() const noexcept;

    /// Get the name of this table, if it has one. Only group-level tables have
    /// names. For a table of any other kind, this function returns the empty
    /// string.
    StringData get_name() const noexcept;

    // Whether or not elements can be null.
    bool is_nullable(size_t col_ndx) const;

    // Returns the link type for the given column.
    // Throws an LogicError if target column is not a link column.
    LinkType get_link_type(size_t col_ndx) const;

    //@{
    /// Conventience functions for inspecting the dynamic table type.
    ///
    /// These functions behave as if they were called on the descriptor returned
    /// by get_descriptor().
    size_t get_column_count() const noexcept;
    DataType get_column_type(size_t column_ndx) const noexcept;
    StringData get_column_name(size_t column_ndx) const noexcept;
    size_t get_column_index(StringData name) const noexcept;
    typedef util::Optional<std::pair<ConstTableRef, size_t>> BacklinkOrigin;
    BacklinkOrigin find_backlink_origin(StringData origin_table_name, StringData origin_col_name) const noexcept;
    //@}

    //@{
    /// Convenience functions for manipulating the dynamic table type.
    ///
    /// These function must be called only for tables with independent dynamic
    /// type. A table has independent dynamic type if the function
    /// has_shared_type() returns false. A table that is a direct member of a
    /// group has independent dynamic type. So does a free-standing table, and a
    /// subtable in a column of type 'mixed'. All other tables have shared
    /// dynamic type. The consequences of calling any of these functions for a
    /// table with shared dynamic type are undefined.
    ///
    /// Apart from that, these functions behave as if they were called on the
    /// descriptor returned by get_descriptor(). Note especially that the
    /// `_link` suffixed functions must be used when inserting link-type
    /// columns.
    ///
    /// If you need to change the shared dynamic type of the subtables in a
    /// subtable column, consider using the API offered by the Descriptor class.
    ///
    /// \sa has_shared_type()
    /// \sa get_descriptor()

    size_t add_column(DataType type, StringData name, bool nullable = false, DescriptorRef* subdesc = nullptr);
    void insert_column(size_t column_ndx, DataType type, StringData name, bool nullable = false,
                       DescriptorRef* subdesc = nullptr);

    // Todo, these prototypes only exist for backwards compatibility. We should remove them because they are error
    // prone (optional arguments and implicit bool to null-ptr conversion)
    size_t add_column(DataType type, StringData name, DescriptorRef* subdesc)
    {
        return add_column(type, name, false, subdesc);
    }
    void insert_column(size_t column_ndx, DataType type, StringData name, DescriptorRef* subdesc)
    {
        insert_column(column_ndx, type, name, false, subdesc);
    }

    size_t add_column_link(DataType type, StringData name, Table& target, LinkType link_type = link_Weak);
    void insert_column_link(size_t column_ndx, DataType type, StringData name, Table& target,
                            LinkType link_type = link_Weak);
    void remove_column(size_t column_ndx);
    void rename_column(size_t column_ndx, StringData new_name);
    //@}
    //@{

    /// has_search_index() returns true if, and only if a search index has been
    /// added to the specified column. Rather than throwing, it returns false if
    /// the table accessor is detached or the specified index is out of range.
    ///
    /// add_search_index() adds a search index to the specified column of the
    /// table. It has no effect if a search index has already been added to the
    /// specified column (idempotency).
    ///
    /// remove_search_index() removes the search index from the specified column
    /// of the table. It has no effect if the specified column has no search
    /// index. The search index cannot be removed from the primary key of a
    /// table.
    ///
    /// This table must be a root table; that is, it must have an independent
    /// descriptor. Freestanding tables, group-level tables, and subtables in a
    /// column of type 'mixed' are all examples of root tables. See add_column()
    /// for more on this. If you want to manipulate subtable indexes, you must use
    /// the Descriptor interface.
    ///
    /// \param column_ndx The index of a column of the table.

    bool has_search_index(size_t column_ndx) const noexcept;
    void add_search_index(size_t column_ndx);
    void remove_search_index(size_t column_ndx);

    //@}

    //@{
    /// Get the dynamic type descriptor for this table.
    ///
    /// Every table has an associated descriptor that specifies its dynamic
    /// type. For simple tables, that is, tables without subtable columns, the
    /// dynamic type can be inspected and modified directly using member
    /// functions such as get_column_count() and add_column(). For more complex
    /// tables, the type is best managed through the associated descriptor
    /// object which is returned by this function.
    ///
    /// \sa has_shared_type()
    DescriptorRef get_descriptor();
    ConstDescriptorRef get_descriptor() const;
    //@}

    //@{
    /// Get the dynamic type descriptor for the column with the
    /// specified index. That column must have type 'table'.
    ///
    /// This is merely a shorthand for calling `get_subdescriptor(column_ndx)`
    /// on the descriptor returned by `get_descriptor()`.
    DescriptorRef get_subdescriptor(size_t column_ndx);
    ConstDescriptorRef get_subdescriptor(size_t column_ndx) const;
    //@}

    //@{
    /// Get access to an arbitrarily nested dynamic type descriptor.
    ///
    /// The returned descriptor is the one you would get by calling
    /// Descriptor::get_subdescriptor() once for each entry in the specified
    /// path, starting with the descriptor returned by get_descriptor(). The
    /// path is allowed to be empty.
    typedef std::vector<size_t> path_vec;
    DescriptorRef get_subdescriptor(const path_vec& path);
    ConstDescriptorRef get_subdescriptor(const path_vec& path) const;
    //@}

    //@{
    /// Convenience functions for manipulating nested table types.
    ///
    /// These functions behave as if they were called on the descriptor returned
    /// by `get_subdescriptor(path)`. These function must be called only on
    /// tables with independent dynamic type.
    ///
    /// \return The value returned by add_subcolumn(), is the index of
    /// the added column within the descriptor referenced by the
    /// specified path.
    ///
    /// \sa Descriptor::add_column()
    /// \sa has_shared_type()
    size_t add_subcolumn(const path_vec& path, DataType type, StringData name);
    void insert_subcolumn(const path_vec& path, size_t column_ndx, DataType type, StringData name);
    void remove_subcolumn(const path_vec& path, size_t column_ndx);
    void rename_subcolumn(const path_vec& path, size_t column_ndx, StringData new_name);
    //@}

    /// Does this table share its type with other tables?
    ///
    /// Tables that are direct members of groups have independent
    /// dynamic types. The same is true for free-standing tables and
    /// subtables in coulmns of type 'mixed'. For such tables, this
    /// function returns false.
    ///
    /// When a table has a column of type 'table', the cells in that
    /// column contain subtables. All those subtables have the same
    /// dynamic type, and they share a single type descriptor. For all
    /// such subtables, this function returns true. See
    /// Descriptor::is_root() for more on this.
    ///
    /// Please note that Table functions that modify the dynamic type
    /// directly, such as add_column(), are only allowed to be used on
    /// tables with non-shared type. If you need to modify a shared
    /// type, you will have to do that through the descriptor returned
    /// by get_descriptor(), but note that it will then affect all the
    /// tables sharing that descriptor.
    ///
    /// \sa get_descriptor()
    /// \sa Descriptor::is_root()
    bool has_shared_type() const noexcept;


    template <class T>
    Columns<T> column(size_t column); // FIXME: Should this one have been declared noexcept?
    template <class T>
    Columns<T> column(const Table& origin, size_t origin_column_ndx);
    // BacklinkCount is a total count per row and therefore not attached to a specific column
    template <class T>
    BacklinkCount<T> get_backlink_count();

    template <class T>
    SubQuery<T> column(size_t column, Query subquery);
    template <class T>
    SubQuery<T> column(const Table& origin, size_t origin_column_ndx, Query subquery);

    // Table size and deletion
    bool is_empty() const noexcept;
    size_t size() const noexcept;

    typedef BasicRowExpr<Table> RowExpr;
    typedef BasicRowExpr<const Table> ConstRowExpr;

    RowExpr get(size_t row_ndx) noexcept;
    ConstRowExpr get(size_t row_ndx) const noexcept;

    RowExpr front() noexcept;
    ConstRowExpr front() const noexcept;

    RowExpr back() noexcept;
    ConstRowExpr back() const noexcept;

    RowExpr operator[](size_t row_ndx) noexcept;
    ConstRowExpr operator[](size_t row_ndx) const noexcept;


    //@{

    /// Row handling.
    ///
    /// remove() removes the specified row from the table and shifts all rows at
    /// higher index to fill the vacated slot. This operation assumes that the
    /// table is ordered, and it is therefore allowed only on tables **without**
    /// link columns, as link columns are only allowed in unordered tables.
    ///
    /// move_last_over() removes the specified row from the table, and if it is
    /// not the last row in the table, it then moves the last row into the
    /// vacated slot. This operation assumes that the table is unordered, and it
    /// may therfore be used on tables with link columns.
    ///
    /// remove_recursive() will delete linked rows if the removed link was the
    /// last one holding on to the row in question. This will be done recursively.
    ///
    /// The removal of a row from an unordered table (move_last_over()) may
    /// cause other linked rows to be cascade-removed. The clearing of a table
    /// may also cause linked rows to be cascade-removed, but in this respect,
    /// the effect is exactly as if each row had been removed individually. See
    /// Descriptor::set_link_type() for details.

    size_t add_empty_row(size_t num_rows = 1);
    void insert_empty_row(size_t row_ndx, size_t num_rows = 1);
    size_t add_row_with_key(size_t col_ndx, util::Optional<int64_t> key);
    size_t add_row_with_keys(size_t col_1_ndx, int64_t key1, size_t col_2_ndx, StringData key2);
    void remove(size_t row_ndx);
    void remove_recursive(size_t row_ndx);
    void remove_last();
    void move_last_over(size_t row_ndx);
    void clear();
    void swap_rows(size_t row_ndx_1, size_t row_ndx_2);
    void move_row(size_t from_ndx, size_t to_ndx);
    //@}

    /// Replaces all links to \a row_ndx with links to \a new_row_ndx.
    ///
    /// This operation is usually followed by Table::move_last_over()
    /// as part of Table::set_int_unique() or Table::set_string_unique()
    /// or Table::set_null_unique() detecting a collision.
    ///
    /// \sa Table::move_last_over()
    /// \sa Table::set_int_unique()
    /// \sa Table::set_string_unique()
    /// \sa Table::set_null_unique()
    void merge_rows(size_t row_ndx, size_t new_row_ndx);

    //@{

    /// Get cell values.
    /// Will assert if the requested type does not match the column type.
    ///
    /// When fetching from a nullable column and the value is null, a default
    /// value will be returned, except for object like types (StringData,
    /// BinaryData, Timestamp) which have support for storing nulls. In that
    /// case, call the `is_null()` method on the returned object to check
    /// whether the stored value was null. If nullability matters and returning
    /// a default value is unacceptable, check Table::is_null() before getting a
    /// cell value.
    ///
    /// \sa Table::is_nullable(size_t col_ndx)
    /// \sa Table::is_null(size_t col_ndx, size_t row_ndx)
    /// \sa StringData::is_null()
    int64_t get_int(size_t column_ndx, size_t row_ndx) const noexcept;
    bool get_bool(size_t column_ndx, size_t row_ndx) const noexcept;
    OldDateTime get_olddatetime(size_t column_ndx, size_t row_ndx) const noexcept;
    float get_float(size_t column_ndx, size_t row_ndx) const noexcept;
    double get_double(size_t column_ndx, size_t row_ndx) const noexcept;
    StringData get_string(size_t column_ndx, size_t row_ndx) const noexcept;
    BinaryData get_binary(size_t column_ndx, size_t row_ndx) const noexcept;
    BinaryIterator get_binary_iterator(size_t column_ndx, size_t row_ndx) const noexcept;
    Mixed get_mixed(size_t column_ndx, size_t row_ndx) const noexcept;
    DataType get_mixed_type(size_t column_ndx, size_t row_ndx) const noexcept;
    Timestamp get_timestamp(size_t column_ndx, size_t row_ndx) const noexcept;

    //@}

    /// Return data from position 'pos' and onwards. If the blob is distributed
    /// across multiple arrays, you will only get data from one array. 'pos'
    /// will be updated to be an index to next available data. It will be 0
    /// if no more data.
    BinaryData get_binary_at(size_t col_ndx, size_t ndx, size_t& pos) const noexcept;

    template <class T>
    T get(size_t c, size_t r) const noexcept;

    size_t get_link(size_t column_ndx, size_t row_ndx) const noexcept;
    bool is_null_link(size_t column_ndx, size_t row_ndx) const noexcept;
    LinkViewRef get_linklist(size_t column_ndx, size_t row_ndx);
    ConstLinkViewRef get_linklist(size_t column_ndx, size_t row_ndx) const;
    size_t get_link_count(size_t column_ndx, size_t row_ndx) const noexcept;
    bool linklist_is_empty(size_t column_ndx, size_t row_ndx) const noexcept;
    bool is_null(size_t column_ndx, size_t row_ndx) const noexcept;

    TableRef get_link_target(size_t column_ndx) noexcept;
    ConstTableRef get_link_target(size_t column_ndx) const noexcept;

    //@{

    /// Set cell values.
    ///
    /// It is an error to specify a column index, row index, or string position
    /// that is out of range.
    ///
    /// The number of bytes in a string value must not exceed `max_string_size`,
    /// and the number of bytes in a binary data value must not exceed
    /// `max_binary_size`. String must also contain valid UTF-8 encodings. These
    /// requirements also apply when modifying a string with insert_substring()
    /// and remove_substring(), and for strings in a mixed columnt. Passing, or
    /// producing an oversized string or binary data value will cause an
    /// exception to be thrown.
    ///
    /// The "unique" variants (set_int_unique(), set_string_unique(), set_null_unique())
    /// are intended to be used in the implementation of primary key support. They
    /// check if the given column already contains one or more values that are
    /// equal to \a value, and if there are conflicts, it calls
    /// Table::merge_rows() for the row_ndx to be replaced by the
    /// existing row, followed by a Table::move_last_over() of row_ndx. The
    /// return value is always a row index of a row that contains \a value in
    /// the specified column, possibly different from \a row_ndx if a conflict
    /// occurred.  Users intending to implement primary keys must therefore
    /// manually check for duplicates if they want to raise an error instead.
    ///
    /// NOTE:  It is an error to call either function after adding elements to a
    /// linklist in the object. In general, calling set_int_unique() or
    /// set_string_unique() or set_null_unique() should be the first thing that
    /// happens after creating a row. These limitations are imposed by limitations
    /// in the Realm Object Server and may be relaxed in the future. A violation of
    /// these rules results in a LogicError being thrown.
    ///
    /// add_int() adds a 64-bit signed integer to the current value of the
    /// cell.  If the addition would cause signed integer overflow or
    /// underflow, the addition "wraps around" with semantics similar to
    /// unsigned integer arithmetic, such that Table::max_integer + 1 ==
    /// Table::min_integer and Table::min_integer - 1 == Table::max_integer.
    /// Note that the wrapping is platform-independent (all platforms wrap in
    /// the same way regardless of integer representation). If the existing
    /// value in the cell is null, a LogicError exception is thrown.
    ///
    /// insert_substring() inserts the specified string into the currently
    /// stored string at the specified position. The position must be less than
    /// or equal to the size of the currently stored string.
    ///
    /// remove_substring() removes the specified byte range from the currently
    /// stored string. The beginning of the range (\a pos) must be less than or
    /// equal to the size of the currently stored string. If the specified range
    /// extends beyond the end of the currently stored string, it will be
    /// silently clamped.
    ///
    /// String level modifications performed via insert_substring() and
    /// remove_substring() are mergable and subject to operational
    /// transformation. That is, the effect of two causally unrelated
    /// modifications will in general both be retained during synchronization.

    static const size_t max_string_size = 0xFFFFF8 - Array::header_size - 1;
    static const size_t max_binary_size = 0xFFFFF8 - Array::header_size;

    // FIXME: These limits should be chosen independently of the underlying
    // platform's choice to define int64_t and independent of the integer
    // representation. The current values only work for 2's complement, which is
    // not guaranteed by the standard.
    static constexpr int_fast64_t max_integer = std::numeric_limits<int64_t>::max();
    static constexpr int_fast64_t min_integer = std::numeric_limits<int64_t>::min();

    template <class T>
    void set(size_t c, size_t r, T value, bool is_default = false);

    template <class T>
    size_t set_unique(size_t c, size_t r, T value);

    void set_int(size_t column_ndx, size_t row_ndx, int_fast64_t value, bool is_default = false);
    size_t set_int_unique(size_t column_ndx, size_t row_ndx, int_fast64_t value);
    void set_bool(size_t column_ndx, size_t row_ndx, bool value, bool is_default = false);
    void set_olddatetime(size_t column_ndx, size_t row_ndx, OldDateTime value, bool is_default = false);
    void set_timestamp(size_t column_ndx, size_t row_ndx, Timestamp value, bool is_default = false);
    template <class E>
    void set_enum(size_t column_ndx, size_t row_ndx, E value);
    void set_float(size_t column_ndx, size_t row_ndx, float value, bool is_default = false);
    void set_double(size_t column_ndx, size_t row_ndx, double value, bool is_default = false);
    void set_string(size_t column_ndx, size_t row_ndx, StringData value, bool is_default = false);
    size_t set_string_unique(size_t column_ndx, size_t row_ndx, StringData value);
    void set_binary(size_t column_ndx, size_t row_ndx, BinaryData value, bool is_default = false);
    void set_mixed(size_t column_ndx, size_t row_ndx, Mixed value, bool is_default = false);
    void set_link(size_t column_ndx, size_t row_ndx, size_t target_row_ndx, bool is_default = false);
    void nullify_link(size_t column_ndx, size_t row_ndx);
    void set_null(size_t column_ndx, size_t row_ndx, bool is_default = false);
    void set_null_unique(size_t col_ndx, size_t row_ndx);

    // Sync needs to store blobs bigger than 16 M. This function can be used for that. Data should be read
    // out again using the get_binary_at() function. Should not be used for user data as normal get_binary()
    // will just return null if the data is bigger than the limit.
    void set_binary_big(size_t column_ndx, size_t row_ndx, BinaryData value, bool is_default = false);

    void add_int(size_t column_ndx, size_t row_ndx, int_fast64_t value);

    void insert_substring(size_t col_ndx, size_t row_ndx, size_t pos, StringData);
    void remove_substring(size_t col_ndx, size_t row_ndx, size_t pos, size_t substring_size = realm::npos);

    //@}

    /// Assumes that the specified column is a subtable column (in
    /// particular, not a mixed column) and that the specified table
    /// has a spec that is compatible with that column, that is, the
    /// number of columns must be the same, and corresponding columns
    /// must have identical data types (as returned by
    /// get_column_type()).
    void set_subtable(size_t col_ndx, size_t row_ndx, const Table*);
    void set_mixed_subtable(size_t col_ndx, size_t row_ndx, const Table*);


    // Sub-tables (works on columns whose type is either 'subtable' or
    // 'mixed', for a value in a mixed column that is not a subtable,
    // get_subtable() returns null, get_subtable_size() returns zero,
    // and clear_subtable() replaces the value with an empty table.)
    // Currently, subtables of subtables are not supported.
    TableRef get_subtable(size_t column_ndx, size_t row_ndx);
    ConstTableRef get_subtable(size_t column_ndx, size_t row_ndx) const;
    size_t get_subtable_size(size_t column_ndx, size_t row_ndx) const noexcept;
    void clear_subtable(size_t column_ndx, size_t row_ndx);

    // Backlinks
    size_t get_backlink_count(size_t row_ndx, bool only_strong_links = false) const noexcept;
    size_t get_backlink_count(size_t row_ndx, const Table& origin, size_t origin_col_ndx) const noexcept;
    size_t get_backlink(size_t row_ndx, const Table& origin, size_t origin_col_ndx, size_t backlink_ndx) const
        noexcept;


    //@{

    /// If this accessor is attached to a subtable, then that subtable has a
    /// parent table, and the subtable either resides in a column of type
    /// `table` or of type `mixed` in that parent. In that case
    /// get_parent_table() returns a reference to the accessor associated with
    /// the parent, and get_parent_row_index() returns the index of the row in
    /// which the subtable resides. In all other cases (free-standing and
    /// group-level tables), get_parent_table() returns null and
    /// get_parent_row_index() returns realm::npos.
    ///
    /// If this accessor is attached to a subtable, and \a column_ndx_out is
    /// specified, then `*column_ndx_out` is set to the index of the column of
    /// the parent table in which the subtable resides. If this accessor is not
    /// attached to a subtable, then `*column_ndx_out` will retain its original
    /// value upon return.

    TableRef get_parent_table(size_t* column_ndx_out = nullptr) noexcept;
    ConstTableRef get_parent_table(size_t* column_ndx_out = nullptr) const noexcept;
    size_t get_parent_row_index() const noexcept;

    //@}


    /// Only group-level unordered tables can be used as origins or targets of
    /// links.
    bool is_group_level() const noexcept;

    /// If this table is a group-level table, then this function returns the
    /// index of this table within the group. Otherwise it returns realm::npos.
    size_t get_index_in_group() const noexcept;

    // Aggregate functions
    size_t count_int(size_t column_ndx, int64_t value) const;
    size_t count_string(size_t column_ndx, StringData value) const;
    size_t count_float(size_t column_ndx, float value) const;
    size_t count_double(size_t column_ndx, double value) const;

    int64_t sum_int(size_t column_ndx) const;
    double sum_float(size_t column_ndx) const;
    double sum_double(size_t column_ndx) const;
    int64_t maximum_int(size_t column_ndx, size_t* return_ndx = nullptr) const;
    float maximum_float(size_t column_ndx, size_t* return_ndx = nullptr) const;
    double maximum_double(size_t column_ndx, size_t* return_ndx = nullptr) const;
    OldDateTime maximum_olddatetime(size_t column_ndx, size_t* return_ndx = nullptr) const;
    Timestamp maximum_timestamp(size_t column_ndx, size_t* return_ndx = nullptr) const;
    int64_t minimum_int(size_t column_ndx, size_t* return_ndx = nullptr) const;
    float minimum_float(size_t column_ndx, size_t* return_ndx = nullptr) const;
    double minimum_double(size_t column_ndx, size_t* return_ndx = nullptr) const;
    OldDateTime minimum_olddatetime(size_t column_ndx, size_t* return_ndx = nullptr) const;
    Timestamp minimum_timestamp(size_t column_ndx, size_t* return_ndx = nullptr) const;
    double average_int(size_t column_ndx, size_t* value_count = nullptr) const;
    double average_float(size_t column_ndx, size_t* value_count = nullptr) const;
    double average_double(size_t column_ndx, size_t* value_count = nullptr) const;

    // Searching
    template <class T>
    size_t find_first(size_t column_ndx, T value) const;

    size_t find_first_link(size_t target_row_index) const;
    size_t find_first_int(size_t column_ndx, int64_t value) const;
    size_t find_first_bool(size_t column_ndx, bool value) const;
    size_t find_first_olddatetime(size_t column_ndx, OldDateTime value) const;
    size_t find_first_timestamp(size_t column_ndx, Timestamp value) const;
    size_t find_first_float(size_t column_ndx, float value) const;
    size_t find_first_double(size_t column_ndx, double value) const;
    size_t find_first_string(size_t column_ndx, StringData value) const;
    size_t find_first_binary(size_t column_ndx, BinaryData value) const;
    size_t find_first_null(size_t column_ndx) const;

    TableView find_all_link(size_t target_row_index);
    ConstTableView find_all_link(size_t target_row_index) const;
    TableView find_all_int(size_t column_ndx, int64_t value);
    ConstTableView find_all_int(size_t column_ndx, int64_t value) const;
    TableView find_all_bool(size_t column_ndx, bool value);
    ConstTableView find_all_bool(size_t column_ndx, bool value) const;
    TableView find_all_olddatetime(size_t column_ndx, OldDateTime value);
    ConstTableView find_all_olddatetime(size_t column_ndx, OldDateTime value) const;
    TableView find_all_float(size_t column_ndx, float value);
    ConstTableView find_all_float(size_t column_ndx, float value) const;
    TableView find_all_double(size_t column_ndx, double value);
    ConstTableView find_all_double(size_t column_ndx, double value) const;
    TableView find_all_string(size_t column_ndx, StringData value);
    ConstTableView find_all_string(size_t column_ndx, StringData value) const;
    TableView find_all_binary(size_t column_ndx, BinaryData value);
    ConstTableView find_all_binary(size_t column_ndx, BinaryData value) const;
    TableView find_all_null(size_t column_ndx);
    ConstTableView find_all_null(size_t column_ndx) const;

    /// The following column types are supported: String, Integer, OldDateTime, Bool
    TableView get_distinct_view(size_t column_ndx);
    ConstTableView get_distinct_view(size_t column_ndx) const;

    TableView get_sorted_view(size_t column_ndx, bool ascending = true);
    ConstTableView get_sorted_view(size_t column_ndx, bool ascending = true) const;

    TableView get_sorted_view(SortDescriptor order);
    ConstTableView get_sorted_view(SortDescriptor order) const;

    TableView get_range_view(size_t begin, size_t end);
    ConstTableView get_range_view(size_t begin, size_t end) const;

    TableView get_backlink_view(size_t row_ndx, Table* src_table, size_t src_col_ndx);


    // Pivot / aggregate operation types. Experimental! Please do not document method publicly.
    enum AggrType {
        aggr_count,
        aggr_sum,
        aggr_avg,
        aggr_min,
        aggr_max,
    };

    // Simple pivot aggregate method. Experimental! Please do not document method publicly.
    void aggregate(size_t group_by_column, size_t aggr_column, AggrType op, Table& result,
                   const IntegerColumn* viewrefs = nullptr) const;

    /// Report the current versioning counter for the table. The versioning counter is guaranteed to
    /// change when the contents of the table changes after advance_read() or promote_to_write(), or
    /// immediately after calls to methods which change the table. The term "change" means "change of
    /// value": The storage layout of the table may change, for example due to optimization, but this
    /// is not considered a change of a value. This means that you *cannot* use a non-changing version
    /// count to indicate that object addresses (e.g. strings, binary data) remain the same.
    /// The versioning counter *may* change (but is not required to do so) when another table linked
    /// from this table, or linking to this table, is changed. The version counter *may* also change
    /// without any apparent reason.
    uint_fast64_t get_version_counter() const noexcept;

private:
    template <class T>
    TableView find_all(size_t column_ndx, T value);

public:
    //@{
    /// Find the lower/upper bound according to a column that is
    /// already sorted in ascending order.
    ///
    /// For an integer column at index 0, and an integer value '`v`',
    /// lower_bound_int(0,v) returns the index '`l`' of the first row
    /// such that `get_int(0,l) &ge; v`, and upper_bound_int(0,v)
    /// returns the index '`u`' of the first row such that
    /// `get_int(0,u) &gt; v`. In both cases, if no such row is found,
    /// the returned value is the number of rows in the table.
    ///
    ///     3 3 3 4 4 4 5 6 7 9 9 9
    ///     ^     ^     ^     ^     ^
    ///     |     |     |     |     |
    ///     |     |     |     |      -- Lower and upper bound of 15
    ///     |     |     |     |
    ///     |     |     |      -- Lower and upper bound of 8
    ///     |     |     |
    ///     |     |      -- Upper bound of 4
    ///     |     |
    ///     |      -- Lower bound of 4
    ///     |
    ///      -- Lower and upper bound of 1
    ///
    /// These functions are similar to std::lower_bound() and
    /// std::upper_bound().
    ///
    /// The string versions assume that the column is sorted according
    /// to StringData::operator<().
    size_t lower_bound_int(size_t column_ndx, int64_t value) const noexcept;
    size_t upper_bound_int(size_t column_ndx, int64_t value) const noexcept;
    size_t lower_bound_bool(size_t column_ndx, bool value) const noexcept;
    size_t upper_bound_bool(size_t column_ndx, bool value) const noexcept;
    size_t lower_bound_float(size_t column_ndx, float value) const noexcept;
    size_t upper_bound_float(size_t column_ndx, float value) const noexcept;
    size_t lower_bound_double(size_t column_ndx, double value) const noexcept;
    size_t upper_bound_double(size_t column_ndx, double value) const noexcept;
    size_t lower_bound_string(size_t column_ndx, StringData value) const noexcept;
    size_t upper_bound_string(size_t column_ndx, StringData value) const noexcept;
    //@}

    // Queries
    // Using where(tv) is the new method to perform queries on TableView. The 'tv' can have any order; it does not
    // need to be sorted, and, resulting view retains its order.
    Query where(TableViewBase* tv = nullptr)
    {
        return Query(*this, tv);
    }

    // FIXME: We need a ConstQuery class or runtime check against modifications in read transaction.
    Query where(TableViewBase* tv = nullptr) const
    {
        return Query(*this, tv);
    }

    // Perform queries on a LinkView. The returned Query holds a reference to lv.
    Query where(const LinkViewRef& lv)
    {
        return Query(*this, lv);
    }

    //@{
    /// WARNING: The link() and backlink() methods will alter a state on the Table object and return a reference to itself.
    /// Be aware if assigning the return value of link() to a variable; this might be an error!

    /// This is an error:

    /// Table& cats = owners->link(1);
    /// auto& dogs = owners->link(2);

    /// Query q = person_table->where()
    /// .and_query(cats.column<String>(5).equal("Fido"))
    /// .Or()
    /// .and_query(dogs.column<String>(6).equal("Meowth"));

    /// Instead, do this:

    /// Query q = owners->where()
    /// .and_query(person_table->link(1).column<String>(5).equal("Fido"))
    /// .Or()
    /// .and_query(person_table->link(2).column<String>(6).equal("Meowth"));

    /// The two calls to link() in the errorneous example will append the two values 0 and 1 to an internal vector in the
    /// owners table, and we end up with three references to that same table: owners, cats and dogs. They are all the same
    /// table, its vector has the values {0, 1}, so a query would not make any sense.
    Table& link(size_t link_column);    
    Table& backlink(const Table& origin, size_t origin_col_ndx);
    //@}

    // Optimizing. enforce == true will enforce enumeration of all string columns;
    // enforce == false will auto-evaluate if they should be enumerated or not
    void optimize(bool enforce = false);

    /// Write this table (or a slice of this table) to the specified
    /// output stream.
    ///
    /// The output will have the same format as any other Realm
    /// database file, such as those produced by Group::write(). In
    /// this case, however, the resulting database file will contain
    /// exactly one table, and that table will contain only the
    /// specified slice of the source table (this table).
    ///
    /// The new table will always have the same dynamic type (see
    /// Descriptor) as the source table (this table), and unless it is
    /// overridden (\a override_table_name), the new table will have
    /// the same name as the source table (see get_name()). Indexes
    /// (see add_search_index()) will not be carried over to the new
    /// table.
    ///
    /// \param out The destination output stream buffer.
    ///
    /// \param offset Index of first row to include (if `slice_size >
    /// 0`). Must be less than, or equal to size().
    ///
    /// \param slice_size Number of rows to include. May be zero. If
    /// `slice_size > size() - offset`, then the effective size of
    /// the written slice will be `size() - offset`.
    ///
    /// \param override_table_name Custom name to write out instead of
    /// the actual table name.
    ///
    /// \throw std::out_of_range If `offset > size()`.
    ///
    /// FIXME: While this function does provided a maximally efficient
    /// way of serializing part of a table, it offers little in terms
    /// of general utility. This is unfortunate, because it pulls
    /// quite a large amount of code into the core library to support
    /// it.
    void write(std::ostream& out, size_t offset = 0, size_t slice_size = npos,
               StringData override_table_name = StringData()) const;

    // Conversion
    void to_json(std::ostream& out, size_t link_depth = 0,
                 std::map<std::string, std::string>* renames = nullptr) const;
    void to_string(std::ostream& out, size_t limit = 500) const;
    void row_to_string(size_t row_ndx, std::ostream& out) const;

    // Get a reference to this table
    TableRef get_table_ref()
    {
        return TableRef(this);
    }
    ConstTableRef get_table_ref() const
    {
        return ConstTableRef(this);
    }

    /// \brief Compare two tables for equality.
    ///
    /// Two tables are equal if they have equal descriptors
    /// (`Descriptor::operator==()`) and equal contents. Equal descriptors imply
    /// that the two tables have the same columns in the same order. Equal
    /// contents means that the two tables must have the same number of rows,
    /// and that for each row index, the two rows must have the same values in
    /// each column.
    ///
    /// In mixed columns, both the value types and the values are required to be
    /// equal.
    ///
    /// For a particular row and column, if the two values are themselves tables
    /// (subtable and mixed columns) value equality implies a recursive
    /// invocation of `Table::operator==()`.
    bool operator==(const Table&) const;

    /// \brief Compare two tables for inequality.
    ///
    /// See operator==().
    bool operator!=(const Table& t) const;

    /// A subtable in a column of type 'table' (which shares descriptor with
    /// other subtables in the same column) is initially in a degenerate state
    /// where it takes up a minimal amout of space. This function returns true
    /// if, and only if the table accessor is attached to such a subtable. This
    /// function is mainly intended for debugging purposes.
    bool is_degenerate() const;

    /// Compute the sum of the sizes in number of bytes of all the array nodes
    /// that currently make up this table. See also
    /// Group::compute_aggregate_byte_size().
    ///
    /// If this table accessor is the detached state, this function returns
    /// zero.
    size_t compute_aggregated_byte_size() const noexcept;

    // Debug
    void verify() const;
#ifdef REALM_DEBUG
    void to_dot(std::ostream&, StringData title = StringData()) const;
    void print() const;
    MemStats stats() const;
    void dump_node_structure() const; // To std::cerr (for GDB)
    void dump_node_structure(std::ostream&, int level) const;
#endif

    class Parent;
    using HandoverPatch = TableHandoverPatch;
    static void generate_patch(const Table* ref, std::unique_ptr<HandoverPatch>& patch);
    static TableRef create_from_and_consume_patch(std::unique_ptr<HandoverPatch>& patch, Group& group);

protected:
    /// Get a pointer to the accessor of the specified subtable. The
    /// accessor will be created if it does not already exist.
    ///
    /// The returned table pointer must **always** end up being
    /// wrapped in some instantiation of BasicTableRef<>.
    TableRef get_subtable_tableref(size_t col_ndx, size_t row_ndx);

    /// See non-const get_subtable_tableref().
    ConstTableRef get_subtable_tableref(size_t col_ndx, size_t row_ndx) const;

    /// Compare the rows of two tables under the assumption that the two tables
    /// have the same number of columns, and the same data type at each column
    /// index (as expressed through the DataType enum).
    bool compare_rows(const Table&) const;

    void set_into_mixed(Table* parent, size_t col_ndx, size_t row_ndx) const;

    void check_lists_are_empty(size_t row_ndx) const;

private:
    class SliceWriter;

    // Number of rows in this table
    size_t m_size;

    // Underlying array structure. `m_top` is in use only for root tables; that
    // is, for tables with independent descriptor. `m_columns` contains a ref
    // for each column and search index in order of the columns. A search index
    // ref always occurs immediately after the ref of the column to which the
    // search index belongs.
    //
    // A subtable column (a column of type `type_table`) is essentially just a
    // column of 'refs' pointing to the root node of each subtable.
    //
    // To save space in the database file, a subtable in such a column always
    // starts out in a degenerate form where nothing is allocated on its behalf,
    // and a null 'ref' is stored in the corresponding slot of the column. A
    // subtable remains in this degenerate state until the first row is added to
    // the subtable.
    //
    // For this scheme to work, it must be (and is) possible to create a table
    // accessor that refers to a degenerate subtable. A table accessor (instance
    // of `Table`) refers to a degenerate subtable if, and only if `m_columns`
    // is unattached.
    //
    // FIXME: The fact that `m_columns` may be detached means that many
    // functions (even non-modifying functions) need to check for that before
    // accessing the contents of the table. This incurs a runtime
    // overhead. Consider whether this overhead can be eliminated by having
    // `Table::m_columns` always attached to something, and then detect the
    // degenerate state in a different way.
    Array m_top;
    Array m_columns; // 2nd slot in m_top (for root tables)

    // Management class for the spec object. Only if the table has an independent
    // spec, the spec object should be deleted when the table object is deleted.
    // If the table has a shared spec, the spec object is managed by the spec object
    // of the containing table.
    class SpecPtr {
    public:
        ~SpecPtr()
         {
            optionally_delete();
         }
        void manage(Spec* ptr)
        {
            optionally_delete();
            m_p = ptr;
            m_is_managed = true;
        }
        void detach()
        {
            if (m_is_managed) {
                m_p->detach();
            }
        }
        SpecPtr& operator=(Spec* ptr)
        {
            optionally_delete();
            m_p = ptr;
            m_is_managed = false;
            return *this;
        }
        Spec* operator->() const
        {
            return m_p;
        }
        Spec* get() const
        {
            return m_p;
        }
        Spec& operator*() const
        {
            return *m_p;
        }
        operator bool() const
        {
            return m_p != nullptr;
        }
        bool is_managed() const
        {
            return m_is_managed;
        }

    private:
        Spec* m_p = nullptr;
        bool m_is_managed = false;

        void optionally_delete()
        {
            if (m_is_managed) {
                delete m_p;
            }
        }
    };

    SpecPtr m_spec; // 1st slot in m_top (for root tables)

    // Is guaranteed to be empty for a detached accessor. Otherwise it is empty
    // when the table accessor is attached to a degenerate subtable (unattached
    // `m_columns`), otherwise it contains precisely one column accessor for
    // each column in the table, in order.
    //
    // In some cases an entry may be null. This is currently possible only in
    // connection with Group::advance_transact(), but it means that several
    // member functions must be prepared to handle these null entries; in
    // particular, detach(), ~Table(), functions called on behalf of detach()
    // and ~Table(), and functiones called on behalf of
    // Group::advance_transact().
    typedef std::vector<ColumnBase*> column_accessors;
    column_accessors m_cols;

    mutable std::atomic<size_t> m_ref_count;

    // If this table is a root table (has independent descriptor),
    // then Table::m_descriptor refers to the accessor of its
    // descriptor when, and only when the descriptor accessor
    // exists. This is used to ensure that at most one descriptor
    // accessor exists for each underlying descriptor at any given
    // point in time. Subdescriptors are kept unique by means of a
    // registry in the parent descriptor. Table::m_descriptor is
    // always null for tables with shared descriptor.
    mutable std::weak_ptr<Descriptor> m_descriptor;

    // Table view instances
    // Access needs to be protected by m_accessor_mutex
    typedef std::vector<TableViewBase*> views;
    mutable views m_views;

    // Points to first bound row accessor, or is null if there are none.
    mutable RowBase* m_row_accessors = nullptr;

    // Mutex which must be locked any time the row accessor chain or m_views is used
    mutable util::Mutex m_accessor_mutex;

    // Used for queries: Items are added with link() method during buildup of query
    mutable std::vector<size_t> m_link_chain;

    /// Used only in connection with Group::advance_transact() and
    /// Table::refresh_accessor_tree().
    mutable bool m_mark;

    mutable uint_fast64_t m_version;

    void erase_row(size_t row_ndx, bool is_move_last_over);
    void batch_erase_rows(const IntegerColumn& row_indexes, bool is_move_last_over);
    void do_remove(size_t row_ndx, bool broken_reciprocal_backlinks);
    void do_move_last_over(size_t row_ndx, bool broken_reciprocal_backlinks);
    void do_swap_rows(size_t row_ndx_1, size_t row_ndx_2);
    void do_move_row(size_t from_ndx, size_t to_ndx);
    void do_merge_rows(size_t row_ndx, size_t new_row_ndx);
    void do_clear(bool broken_reciprocal_backlinks);
    size_t do_set_link(size_t col_ndx, size_t row_ndx, size_t target_row_ndx);
    template <class ColType, class T>
    size_t do_find_unique(ColType& col, size_t ndx, T&& value, bool& conflict);
    template <class ColType>
    size_t do_set_unique_null(ColType& col, size_t ndx, bool& conflict);
    template <class ColType, class T>
    size_t do_set_unique(ColType& column, size_t row_ndx, T&& value, bool& conflict);

    void _add_search_index(size_t column_ndx);
    void _remove_search_index(size_t column_ndx);

    void rebuild_search_index(size_t current_file_format_version);

    // Upgrades OldDateTime columns to Timestamp columns
    void upgrade_olddatetime();

    // Indicate that the current global state version has been "observed". Until this
    // happens, bumping of the global version counter can be bypassed, as any query
    // checking for a version change will see the older version change anyways.
    // Also returns the table-local version.
    uint64_t observe_version() const noexcept;

    /// Update the version of this table and all tables which have links to it.
    /// This causes all views referring to those tables to go out of sync, so that
    /// calls to sync_if_needed() will bring the view up to date by reexecuting the
    /// query.
    ///
    /// \param bump_global chooses whether the global versioning counter must be
    /// bumped first as part of the update. This is the normal mode of operation,
    /// when a change is made to the table. When calling recursively (following links
    /// or going to the parent table), the parameter should be set to false to correctly
    /// prune traversal.
    void bump_version(bool bump_global = true) const noexcept;

    /// Disable copying assignment.
    ///
    /// It could easily be implemented by calling assign(), but the
    /// non-checking nature of the low-level dynamically typed API
    /// makes it too risky to offer this feature as an
    /// operator.
    ///
    /// FIXME: assign() has not yet been implemented, but the
    /// intention is that it will copy the rows of the argument table
    /// into this table after clearing the original contents, and for
    /// target tables without a shared spec, it would also copy the
    /// spec. For target tables with shared spec, it would be an error
    /// to pass an argument table with an incompatible spec, but
    /// assign() would not check for spec compatibility. This would
    /// make it ideal as a basis for implementing operator=() for
    /// typed tables.
    Table& operator=(const Table&) = delete;

    /// Used when constructing an accessor whose lifetime is going to be managed
    /// by reference counting. The lifetime of accessors of free-standing tables
    /// allocated on the stack by the application is not managed by reference
    /// counting, so that is a case where this tag must **not** be specified.
    class ref_count_tag {
    };

    /// Create an uninitialized accessor whose lifetime is managed by reference
    /// counting.
    Table(ref_count_tag, Allocator&);

    void init(ref_type top_ref, ArrayParent*, size_t ndx_in_parent, bool skip_create_column_accessors = false);
    void init(Spec* shared_spec, ArrayParent* parent_column, size_t parent_row_ndx);

    static void do_insert_column(Descriptor&, size_t col_ndx, DataType type, StringData name,
                                 LinkTargetInfo& link_target_info, bool nullable = false);
    static void do_insert_column_unless_exists(Descriptor&, size_t col_ndx, DataType type, StringData name,
                                               LinkTargetInfo& link, bool nullable = false,
                                               bool* was_inserted = nullptr);
    static void do_erase_column(Descriptor&, size_t col_ndx);
    static void do_rename_column(Descriptor&, size_t col_ndx, StringData name);

    static void do_add_search_index(Descriptor&, size_t col_ndx);
    static void do_remove_search_index(Descriptor&, size_t col_ndx);

    struct InsertSubtableColumns;
    struct EraseSubtableColumns;
    struct RenameSubtableColumns;

    void insert_root_column(size_t col_ndx, DataType type, StringData name, LinkTargetInfo& link_target,
                            bool nullable = false);
    void erase_root_column(size_t col_ndx);
    void do_insert_root_column(size_t col_ndx, ColumnType, StringData name, bool nullable = false);
    void do_erase_root_column(size_t col_ndx);
    void do_set_link_type(size_t col_ndx, LinkType);
    void insert_backlink_column(size_t origin_table_ndx, size_t origin_col_ndx, size_t backlink_col_ndx);
    void erase_backlink_column(size_t origin_table_ndx, size_t origin_col_ndx);
    void update_link_target_tables(size_t old_col_ndx_begin, size_t new_col_ndx_begin);
    void update_link_target_tables_after_column_move(size_t moved_from, size_t moved_to);

    struct SubtableUpdater {
        virtual void update(const SubtableColumn&, Array& subcolumns) = 0;
        virtual void update_accessor(Table&) = 0;
        virtual ~SubtableUpdater()
        {
        }
    };
    static void update_subtables(Descriptor&, SubtableUpdater*);
    void update_subtables(const size_t* col_path_begin, const size_t* col_path_end, SubtableUpdater*);

    struct AccessorUpdater {
        virtual void update(Table&) = 0;
        virtual void update_parent(Table&) = 0;
        virtual ~AccessorUpdater()
        {
        }
    };
    void update_accessors(const size_t* col_path_begin, const size_t* col_path_end, AccessorUpdater&);

    void create_degen_subtab_columns();
    ColumnBase* create_column_accessor(ColumnType, size_t col_ndx, size_t ndx_in_parent);
    void destroy_column_accessors() noexcept;

    /// Called in the context of Group::commit() to ensure that
    /// attached table accessors stay valid across a commit. Please
    /// note that this works only for non-transactional commits. Table
    /// accessors obtained during a transaction are always detached
    /// when the transaction ends.
    void update_from_parent(size_t old_baseline) noexcept;

    // Support function for conversions
    void to_string_header(std::ostream& out, std::vector<size_t>& widths) const;
    void to_string_row(size_t row_ndx, std::ostream& out, const std::vector<size_t>& widths) const;

    // recursive methods called by to_json, to follow links
    void to_json(std::ostream& out, size_t link_depth, std::map<std::string, std::string>& renames,
                 std::vector<ref_type>& followed) const;
    void to_json_row(size_t row_ndx, std::ostream& out, size_t link_depth,
                     std::map<std::string, std::string>& renames, std::vector<ref_type>& followed) const;
    void to_json_row(size_t row_ndx, std::ostream& out, size_t link_depth = 0,
                     std::map<std::string, std::string>* renames = nullptr) const;

    // Detach accessor from underlying table. Caller must ensure that
    // a reference count exists upon return, for example by obtaining
    // an extra reference count before the call.
    //
    // This function puts this table accessor into the detached
    // state. This detaches it from the underlying structure of array
    // nodes. It also recursively detaches accessors for subtables,
    // and the type descriptor accessor. When this function returns,
    // is_attached() will return false.
    //
    // This function may be called for a table accessor that is
    // already in the detached state (idempotency).
    //
    // It is also valid to call this function for a table accessor
    // that has not yet been detached, but whose underlying structure
    // of arrays have changed in an unpredictable/unknown way. This
    // kind of change generally happens when a modifying table
    // operation fails, and also when one transaction is ended and a
    // new one is started.
    void detach() noexcept;

    /// Detach and remove all attached row, link list, and subtable
    /// accessors. This function does not discard the descriptor accessor, if
    /// any, and it does not discard column accessors either.
    void discard_child_accessors() noexcept;

    void discard_row_accessors() noexcept;

    // Detach the type descriptor accessor if it exists.
    void discard_desc_accessor() noexcept;

    void bind_ptr() const noexcept;
    void unbind_ptr() const noexcept;

    void register_view(const TableViewBase* view);
    void unregister_view(const TableViewBase* view) noexcept;
    void move_registered_view(const TableViewBase* old_addr, const TableViewBase* new_addr) noexcept;
    void discard_views() noexcept;

    void register_row_accessor(RowBase*) const noexcept;
    void unregister_row_accessor(RowBase*) const noexcept;
    void do_unregister_row_accessor(RowBase*) const noexcept;

    class UnbindGuard;

    ColumnType get_real_column_type(size_t column_ndx) const noexcept;

    /// If this table is a group-level table, the parent group is returned,
    /// otherwise null is returned.
    Group* get_parent_group() const noexcept;

    const ColumnBase& get_column_base(size_t column_ndx) const noexcept;
    ColumnBase& get_column_base(size_t column_ndx);

    const ColumnBaseWithIndex& get_column_base_indexed(size_t ndx) const noexcept;
    ColumnBaseWithIndex& get_column_base_indexed(size_t ndx);

    template <class T, ColumnType col_type>
    T& get_column(size_t ndx);

    template <class T, ColumnType col_type>
    const T& get_column(size_t ndx) const noexcept;

    IntegerColumn& get_column(size_t column_ndx);
    const IntegerColumn& get_column(size_t column_ndx) const noexcept;
    IntNullColumn& get_column_int_null(size_t column_ndx);
    const IntNullColumn& get_column_int_null(size_t column_ndx) const noexcept;
    FloatColumn& get_column_float(size_t column_ndx);
    const FloatColumn& get_column_float(size_t column_ndx) const noexcept;
    DoubleColumn& get_column_double(size_t column_ndx);
    const DoubleColumn& get_column_double(size_t column_ndx) const noexcept;
    StringColumn& get_column_string(size_t column_ndx);
    const StringColumn& get_column_string(size_t column_ndx) const noexcept;
    BinaryColumn& get_column_binary(size_t column_ndx);
    const BinaryColumn& get_column_binary(size_t column_ndx) const noexcept;
    StringEnumColumn& get_column_string_enum(size_t column_ndx);
    const StringEnumColumn& get_column_string_enum(size_t column_ndx) const noexcept;
    SubtableColumn& get_column_table(size_t column_ndx);
    const SubtableColumn& get_column_table(size_t column_ndx) const noexcept;
    MixedColumn& get_column_mixed(size_t column_ndx);
    const MixedColumn& get_column_mixed(size_t column_ndx) const noexcept;
    TimestampColumn& get_column_timestamp(size_t column_ndx);
    const TimestampColumn& get_column_timestamp(size_t column_ndx) const noexcept;
    const LinkColumnBase& get_column_link_base(size_t ndx) const noexcept;
    LinkColumnBase& get_column_link_base(size_t ndx);
    const LinkColumn& get_column_link(size_t ndx) const noexcept;
    LinkColumn& get_column_link(size_t ndx);
    const LinkListColumn& get_column_link_list(size_t ndx) const noexcept;
    LinkListColumn& get_column_link_list(size_t ndx);
    const BacklinkColumn& get_column_backlink(size_t ndx) const noexcept;
    BacklinkColumn& get_column_backlink(size_t ndx);

    void verify_column(size_t col_ndx, const ColumnBase* col) const;

    void instantiate_before_change();
    void validate_column_type(const ColumnBase& col, ColumnType expected_type, size_t ndx) const;

    static size_t get_size_from_ref(ref_type top_ref, Allocator&) noexcept;
    static size_t get_size_from_ref(ref_type spec_ref, ref_type columns_ref, Allocator&) noexcept;

    const Table* get_parent_table_ptr(size_t* column_ndx_out = nullptr) const noexcept;
    Table* get_parent_table_ptr(size_t* column_ndx_out = nullptr) noexcept;

    /// Create an empty table with independent spec and return just
    /// the reference to the underlying memory.
    static ref_type create_empty_table(Allocator&);

    /// Create a column of the specified type, fill it with the
    /// specified number of default values, and return just the
    /// reference to the underlying memory.
    static ref_type create_column(ColumnType column_type, size_t num_default_values, bool nullable, Allocator&);

    /// Construct a copy of the columns array of this table using the
    /// specified allocator and return just the ref to that array.
    ///
    /// In the clone, no string column will be of the enumeration
    /// type.
    ref_type clone_columns(Allocator&) const;

    /// Construct a complete copy of this table (including its spec)
    /// using the specified allocator and return just the ref to the
    /// new top array.
    ref_type clone(Allocator&) const;

    /// True for `col_type_Link` and `col_type_LinkList`.
    static bool is_link_type(ColumnType) noexcept;

    void connect_opposite_link_columns(size_t link_col_ndx, Table& target_table, size_t backlink_col_ndx) noexcept;

    //@{

    /// Cascading removal of strong links.
    ///
    /// cascade_break_backlinks_to() removes all backlinks pointing to the row
    /// at \a row_ndx. Additionally, if this causes the number of **strong**
    /// backlinks originating from a particular opposite row (target row of
    /// corresponding forward link) to drop to zero, and that row is not already
    /// in \a state.rows, then that row is added to \a state.rows, and
    /// cascade_break_backlinks_to() is called recursively for it. This
    /// operation is the first half of the cascading row removal operation. The
    /// second half is performed by passing the resulting contents of \a
    /// state.rows to remove_backlink_broken_rows().
    ///
    /// Operations that trigger cascading row removal due to explicit removal of
    /// one or more rows (the *initiating rows*), should add those rows to \a
    /// rows initially, and then call cascade_break_backlinks_to() once for each
    /// of them in turn. This is opposed to carrying out the explicit row
    /// removals independently, which is also possible, but does require that
    /// any initiating rows, that end up in \a state.rows due to link cycles,
    /// are removed before passing \a state.rows to
    /// remove_backlink_broken_rows(). In the case of clear(), where all rows of
    /// a table are explicitly removed, it is better to use
    /// cascade_break_backlinks_to_all_rows(), and then carry out the table
    /// clearing as an independent step. For operations that trigger cascading
    /// row removal for other reasons than explicit row removal, \a state.rows
    /// must be empty initially, but cascade_break_backlinks_to() must still be
    /// called for each of the initiating rows.
    ///
    /// When the last non-recursive invocation of cascade_break_backlinks_to()
    /// returns, all forward links originating from a row in \a state.rows have
    /// had their reciprocal backlinks removed, so remove_backlink_broken_rows()
    /// does not perform reciprocal backlink removal at all. Additionally, all
    /// remaining backlinks originating from rows in \a state.rows are
    /// guaranteed to point to rows that are **not** in \a state.rows. This is
    /// true because any backlink that was pointing to a row in \a state.rows
    /// has been removed by one of the invocations of
    /// cascade_break_backlinks_to(). The set of forward links, that correspond
    /// to these remaining backlinks, is precisely the set of forward links that
    /// need to be removed/nullified by remove_backlink_broken_rows(), which it
    /// does by way of reciprocal forward link removal. Note also, that while
    /// all the rows in \a state.rows can have remaining **weak** backlinks
    /// originating from them, only the initiating rows in \a state.rows can
    /// have remaining **strong** backlinks originating from them. This is true
    /// because a non-initiating row is added to \a state.rows only when the
    /// last backlink originating from it is lost.
    ///
    /// Each row removal is replicated individually (as opposed to one
    /// replication instruction for the entire cascading operation). This is
    /// done because it provides an easy way for Group::advance_transact() to
    /// know which tables are affected by the cascade. Note that this has
    /// several important consequences: First of all, the replication log
    /// receiver must execute the row removal instructions in a non-cascading
    /// fashion, meaning that there will be an asymmetry between the two sides
    /// in how the effect of the cascade is brought about. While this is fine
    /// for simple 1-to-1 replication, it may end up interfering badly with
    /// *transaction merging*, when that feature is introduced. Imagine for
    /// example that the cascade initiating operation gets canceled during
    /// conflict resolution, but some, or all of the induced row removals get to
    /// stay. That would break causal consistency. It is important, however, for
    /// transaction merging that the cascaded row removals are explicitly
    /// mentioned in the replication log, such that they can be used to adjust
    /// row indexes during the *operational transform*.
    ///
    /// cascade_break_backlinks_to_all_rows() has the same affect as calling
    /// cascade_break_backlinks_to() once for each row in the table. When
    /// calling this function, \a state.stop_on_table must be set to the origin
    /// table (origin table of corresponding forward links), and \a
    /// state.stop_on_link_list_column must be null.
    ///
    /// It is immaterial which table remove_backlink_broken_rows() is called on,
    /// as long it that table is in the same group as the removed rows.

    void cascade_break_backlinks_to(size_t row_ndx, CascadeState& state);
    void cascade_break_backlinks_to_all_rows(CascadeState& state);
    void remove_backlink_broken_rows(const CascadeState&);

    //@}

    /// Used by query. Follows chain of link columns and returns final target table
    const Table* get_link_chain_target(const std::vector<size_t>& link_chain) const;

    /// Remove the specified row by the 'move last over' method.
    void do_move_last_over(size_t row_ndx);

    // Precondition: 1 <= end - begin
    size_t* record_subtable_path(size_t* begin, size_t* end) const noexcept;

    /// Check if an accessor exists for the specified subtable. If it does,
    /// return a pointer to it, otherwise return null. This function assumes
    /// that the specified column index in a valid index into `m_cols` but does
    /// not otherwise assume more than minimal accessor consistency (see
    /// AccessorConsistencyLevels.)
    TableRef get_subtable_accessor(size_t col_ndx, size_t row_ndx) noexcept;

    /// Unless the column accessor is missing, this function returns the
    /// accessor for the target table of the specified link-type column. The
    /// column accessor is said to be missing if `m_cols[col_ndx]` is null, and
    /// this can happen only during certain operations such as the updating of
    /// the accessor tree when a read transaction is advanced. Note that for
    /// link type columns, the target table accessor exists when, and only when
    /// the origin table accessor exists. This function assumes that the
    /// specified column index in a valid index into `m_cols` and that the
    /// column is a link-type column. Beyond that, it assume nothing more than
    /// minimal accessor consistency (see AccessorConsistencyLevels.)
    Table* get_link_target_table_accessor(size_t col_ndx) noexcept;

    void discard_subtable_accessor(size_t col_ndx, size_t row_ndx) noexcept;

    void adj_acc_insert_rows(size_t row_ndx, size_t num_rows) noexcept;
    void adj_acc_erase_row(size_t row_ndx) noexcept;
    void adj_acc_swap_rows(size_t row_ndx_1, size_t row_ndx_2) noexcept;
    void adj_acc_move_row(size_t from_ndx, size_t to_ndx) noexcept;
    void adj_acc_merge_rows(size_t old_row_ndx, size_t new_row_ndx) noexcept;

    /// Adjust this table accessor and its subordinates after move_last_over()
    /// (or its inverse).
    ///
    /// First, any row, subtable, or link list accessors registered as being at
    /// \a to_row_ndx will be detached, as that row is assumed to have been
    /// replaced. Next, any row, subtable, or link list accessors registered as
    /// being at \a from_row_ndx, will be reregistered as being at \a
    /// to_row_ndx, as the row at \a from_row_ndx is assumed to have been moved
    /// to \a to_row_ndx.
    ///
    /// Crucially, if \a to_row_ndx is equal to \a from_row_ndx, then row,
    /// subtable, or link list accessors at that row are **still detached**.
    ///
    /// Additionally, this function causes all link-adjacent tables to be marked
    /// (dirty). Two tables are link-adjacent if one is the target table of a
    /// link column of the other table. Note that this marking follows these
    /// relations in both directions, but only to a depth of one.
    ///
    /// When this function is used in connection with move_last_over(), set \a
    /// to_row_ndx to the index of the row to be removed, and set \a
    /// from_row_ndx to the index of the last row in the table. As mentioned
    /// earlier, this function can also be used in connection with the **inverse
    /// of** move_last_over(), which is an operation that vacates a row by
    /// moving its contents into a new last row of the table. In that case, set
    /// \a to_row_ndx to one plus the index of the last row in the table, and
    /// set \a from_row_ndx to the index of the row to be vacated.
    ///
    /// This function is used as part of Table::refresh_accessor_tree() to
    /// promote the state of the accessors from Minimal Consistency into
    /// Structural Correspondence, so it must be able to execute without
    /// accessing the underlying array nodes.
    void adj_acc_move_over(size_t from_row_ndx, size_t to_row_ndx) noexcept;

    void adj_acc_clear_root_table() noexcept;
    void adj_acc_clear_nonroot_table() noexcept;
    void adj_row_acc_insert_rows(size_t row_ndx, size_t num_rows) noexcept;
    void adj_row_acc_erase_row(size_t row_ndx) noexcept;
    void adj_row_acc_swap_rows(size_t row_ndx_1, size_t row_ndx_2) noexcept;
    void adj_row_acc_move_row(size_t from_ndx, size_t to_ndx) noexcept;
    void adj_row_acc_merge_rows(size_t old_row_ndx, size_t new_row_ndx) noexcept;

    /// Called by adj_acc_move_over() to adjust row accessors.
    void adj_row_acc_move_over(size_t from_row_ndx, size_t to_row_ndx) noexcept;

    void adj_insert_column(size_t col_ndx);
    void adj_erase_column(size_t col_ndx) noexcept;

    bool is_marked() const noexcept;
    void mark() noexcept;
    void unmark() noexcept;
    void recursive_mark() noexcept;
    void mark_link_target_tables(size_t col_ndx_begin) noexcept;
    void mark_opposite_link_tables() noexcept;

    Replication* get_repl() noexcept;

    void set_ndx_in_parent(size_t ndx_in_parent) noexcept;

    /// Refresh the part of the accessor tree that is rooted at this
    /// table. Subtable accessors will be refreshed only if they are marked
    /// (Table::m_mark), and this applies recursively to subtables of
    /// subtables. All refreshed table accessors (including this one) will be
    /// unmarked upon return.
    ///
    /// The following conditions are necessary and sufficient for the proper
    /// operation of this function:
    ///
    ///  - This table must be a group-level table, or a subtable. It must not be
    ///    a free-standing table (because a free-standing table has no parent).
    ///
    ///  - The `index in parent` property is correct. The `index in parent`
    ///    property of the table is the `index in parent` property of
    ///    `m_columns` for subtables with shared descriptor, and the `index in
    ///    parent` property of `m_top` for all other tables.
    ///
    ///  - If this table has shared descriptor, then the `index in parent`
    ///    property of the contained spec accessor is correct.
    ///
    ///  - The parent accessor is in a valid state (already refreshed). If the
    ///    parent is a group, then the group accessor (excluding its table
    ///    accessors) must be in a valid state. If the parent is a table, then
    ///    the table accessor (excluding its subtable accessors) must be in a
    ///    valid state.
    ///
    ///  - Every descendant subtable accessor is marked if it needs to be
    ///    refreshed, or if it has a descendant accessor that needs to be
    ///    refreshed.
    ///
    ///  - This table accessor, as well as all its descendant accessors, are in
    ///    structural correspondence with the underlying node hierarchy whose
    ///    root ref is stored in the parent (see AccessorConsistencyLevels).
    void refresh_accessor_tree();

    void refresh_spec_accessor();

    void refresh_column_accessors(size_t col_ndx_begin = 0);

    // Look for link columns starting from col_ndx_begin.
    // If a link column is found, follow the link and update it's
    // backlink column accessor if it is in different table.
    void refresh_link_target_accessors(size_t col_ndx_begin = 0);

    bool is_cross_table_link_target() const noexcept;
    std::recursive_mutex* get_parent_accessor_management_lock() const;
#ifdef REALM_DEBUG
    void to_dot_internal(std::ostream&) const;
#endif

    friend class SubtableNode;
    friend class _impl::TableFriend;
    friend class Query;
    friend class metrics::QueryInfo;
    template <class>
    friend class util::bind_ptr;
    template <class>
    friend class SimpleQuerySupport;
    friend class LangBindHelper;
    friend class TableViewBase;
    template <class T>
    friend class Columns;
    friend class Columns<StringData>;
    friend class ParentNode;
    template <class>
    friend class SequentialGetter;
    friend struct util::serializer::SerialisationState;
    friend class RowBase;
    friend class LinksToNode;
    friend class LinkMap;
    friend class LinkView;
    friend class Group;
};

class Table::Parent : public ArrayParent {
public:
    ~Parent() noexcept override
    {
    }

protected:
    virtual StringData get_child_name(size_t child_ndx) const noexcept;

    /// If children are group-level tables, then this function returns the
    /// group. Otherwise it returns null.
    virtual Group* get_parent_group() noexcept;

    /// If children are subtables, then this function returns the
    /// parent table. Otherwise it returns null.
    ///
    /// If \a column_ndx_out is not null, this function must assign the index of
    /// the column within the parent table to `*column_ndx_out` when , and only
    /// when this table parent is a column in a parent table.
    virtual Table* get_parent_table(size_t* column_ndx_out = nullptr) noexcept;

    virtual Spec* get_subtable_spec() noexcept;

    /// Must be called whenever a child table accessor is about to be destroyed.
    ///
    /// Note that the argument is a pointer to the child Table rather than its
    /// `ndx_in_parent` property. This is because only minimal accessor
    /// consistency can be assumed by this function.
    virtual void child_accessor_destroyed(Table* child) noexcept = 0;


    virtual size_t* record_subtable_path(size_t* begin, size_t* end) noexcept;
    virtual std::recursive_mutex* get_accessor_management_lock() noexcept = 0;

    friend class Table;
};


// Implementation:


inline uint_fast64_t Table::get_version_counter() const noexcept
{
    return observe_version();
}

inline uint64_t Table::observe_version() const noexcept
{
    m_top.get_alloc().observe_version();
    return m_version;
}

inline void Table::bump_version(bool bump_global) const noexcept
{
    if (bump_global) {
        // This is only set on initial entry through an operation on the same
        // table.  recursive calls (via parent or via backlinks) must be done
        // with bump_global=false.
        m_top.get_alloc().bump_global_version();
    }
    if (m_top.get_alloc().should_propagate_version(m_version)) {
        if (const Table* parent = get_parent_table_ptr())
            parent->bump_version(false);
        // Recurse through linked tables, use m_mark to avoid infinite recursion
        for (auto& column_ptr : m_cols) {
            // We may meet a null pointer in place of a backlink column, pending
            // replacement with a new one. This can happen ONLY when creation of
            // the corresponding forward link column in the origin table is
            // pending as well. In this case it is ok to just ignore the zeroed
            // backlink column, because the origin table is guaranteed to also
            // be refreshed/marked dirty and hence have it's version bumped.
            if (column_ptr != nullptr)
                column_ptr->bump_link_origin_table_version();
        }
    }
}

inline void Table::remove(size_t row_ndx)
{
    bool is_move_last_over = false;
    erase_row(row_ndx, is_move_last_over); // Throws
}

inline void Table::move_last_over(size_t row_ndx)
{
    bool is_move_last_over = true;
    erase_row(row_ndx, is_move_last_over); // Throws
}

inline void Table::remove_last()
{
    if (!is_empty())
        remove(size() - 1);
}

// A good place to start if you want to understand the memory ordering
// chosen for the operations below is http://preshing.com/20130922/acquire-and-release-fences/
inline void Table::bind_ptr() const noexcept
{
    m_ref_count.fetch_add(1, std::memory_order_relaxed);
}

inline void Table::unbind_ptr() const noexcept
{
    // The delete operation runs the destructor, and the destructor
    // must always see all changes to the object being deleted.
    // Within each thread, we know that unbind_ptr will always happen after
    // any changes, so it is a convenient place to do a release.
    // The release will then be observed by the acquire fence in
    // the case where delete is actually called (the count reaches 0)
    if (m_ref_count.fetch_sub(1, std::memory_order_release) != 1) {
        return;
    }

    std::atomic_thread_fence(std::memory_order_acquire);

    std::recursive_mutex* lock = get_parent_accessor_management_lock();
    if (lock) {
        std::lock_guard<std::recursive_mutex> lg(*lock);
        if (m_ref_count == 0)
            delete this;
    }
    else {
        delete this;
    }
}

inline void Table::register_view(const TableViewBase* view)
{
    util::LockGuard lock(m_accessor_mutex);
    // Casting away constness here - operations done on tableviews
    // through m_views are all internal and preserving "some" kind
    // of logical constness.
    m_views.push_back(const_cast<TableViewBase*>(view));
}

inline bool Table::is_attached() const noexcept
{
    // Note that it is not possible to tie the state of attachment of a table to
    // the state of attachment of m_top, because tables with shared spec do not
    // have a 'top' array. Neither is it possible to tie it to the state of
    // attachment of m_columns, because subtables with shared spec start out in
    // a degenerate form where they do not have a 'columns' array. For these
    // reasons, it is neccessary to define the notion of attachment for a table
    // as follows: A table is attached if, and ony if m_column stores a non-null
    // parent pointer. This works because even for degenerate subtables,
    // m_columns is initialized with the correct parent pointer.
    return m_columns.has_parent();
}

inline StringData Table::get_name() const noexcept
{
    REALM_ASSERT(is_attached());
    const Array& real_top = m_top.is_attached() ? m_top : m_columns;
    ArrayParent* parent = real_top.get_parent();
    if (!parent)
        return StringData("");
    size_t index_in_parent = real_top.get_ndx_in_parent();
    REALM_ASSERT(dynamic_cast<Parent*>(parent));
    return static_cast<Parent*>(parent)->get_child_name(index_in_parent);
}

inline size_t Table::get_column_count() const noexcept
{
    REALM_ASSERT(is_attached());
    return m_spec->get_public_column_count();
}

inline StringData Table::get_column_name(size_t ndx) const noexcept
{
    REALM_ASSERT_3(ndx, <, get_column_count());
    return m_spec->get_column_name(ndx);
}

inline size_t Table::get_column_index(StringData name) const noexcept
{
    REALM_ASSERT(is_attached());
    return m_spec->get_column_index(name);
}

inline ColumnType Table::get_real_column_type(size_t ndx) const noexcept
{
    REALM_ASSERT_3(ndx, <, m_spec->get_column_count());
    return m_spec->get_column_type(ndx);
}

inline DataType Table::get_column_type(size_t ndx) const noexcept
{
    REALM_ASSERT_3(ndx, <, m_spec->get_column_count());
    return m_spec->get_public_column_type(ndx);
}

template <class Col, ColumnType col_type>
inline Col& Table::get_column(size_t ndx)
{
    ColumnBase& col = get_column_base(ndx);
#ifdef REALM_DEBUG
    validate_column_type(col, col_type, ndx);
#endif
    REALM_ASSERT(typeid(Col) == typeid(col));
    return static_cast<Col&>(col);
}

template <class Col, ColumnType col_type>
inline const Col& Table::get_column(size_t ndx) const noexcept
{
    const ColumnBase& col = get_column_base(ndx);
#ifdef REALM_DEBUG
    validate_column_type(col, col_type, ndx);
#endif
    REALM_ASSERT(typeid(Col) == typeid(col));
    return static_cast<const Col&>(col);
}

inline bool Table::has_shared_type() const noexcept
{
    REALM_ASSERT(is_attached());
    return !m_top.is_attached();
}

inline void Table::verify_column(size_t col_ndx, const ColumnBase* col) const
{
    // Check if the column exists at the expected location
    if (REALM_LIKELY(col_ndx < m_cols.size() && m_cols[col_ndx] == col))
        return;
    // The column might be elsewhere in the list
    for (auto c : m_cols) {
        if (c == col)
            return;
    }
    throw LogicError(LogicError::column_does_not_exist);
}

class Table::UnbindGuard {
public:
    UnbindGuard(Table* table) noexcept
        : m_table(table)
    {
    }

    ~UnbindGuard() noexcept
    {
        if (m_table)
            m_table->unbind_ptr();
    }

    Table& operator*() const noexcept
    {
        return *m_table;
    }

    Table* operator->() const noexcept
    {
        return m_table;
    }

    Table* get() const noexcept
    {
        return m_table;
    }

    Table* release() noexcept
    {
        Table* table = m_table;
        m_table = nullptr;
        return table;
    }

private:
    Table* m_table;
};


inline Table::Table(Allocator& alloc)
    : m_top(alloc)
    , m_columns(alloc)
{
    m_ref_count = 1; // Explicitly managed lifetime

    ref_type ref = create_empty_table(alloc); // Throws
    Parent* parent = nullptr;
    size_t ndx_in_parent = 0;
    init(ref, parent, ndx_in_parent);
}

inline Table::Table(const Table& t, Allocator& alloc)
    : m_top(alloc)
    , m_columns(alloc)
{
    m_ref_count = 1; // Explicitly managed lifetime

    ref_type ref = t.clone(alloc); // Throws
    Parent* parent = nullptr;
    size_t ndx_in_parent = 0;
    init(ref, parent, ndx_in_parent);
}

inline Table::Table(ref_count_tag, Allocator& alloc)
    : m_top(alloc)
    , m_columns(alloc)
{
    m_ref_count = 0; // Lifetime managed by reference counting
}

inline Allocator& Table::get_alloc() const
{
    return m_top.get_alloc();
}

inline TableRef Table::create(Allocator& alloc)
{
    std::unique_ptr<Table> table(new Table(ref_count_tag(), alloc)); // Throws
    ref_type ref = create_empty_table(alloc);                        // Throws
    Parent* parent = nullptr;
    size_t ndx_in_parent = 0;
    table->init(ref, parent, ndx_in_parent); // Throws
    return table.release()->get_table_ref();
}

inline TableRef Table::copy(Allocator& alloc) const
{
    std::unique_ptr<Table> table(new Table(ref_count_tag(), alloc)); // Throws
    ref_type ref = clone(alloc);                                     // Throws
    Parent* parent = nullptr;
    size_t ndx_in_parent = 0;
    table->init(ref, parent, ndx_in_parent); // Throws
    return table.release()->get_table_ref();
}

// For use by queries
template <class T>
inline Columns<T> Table::column(size_t column_ndx)
{
    std::vector<size_t> link_chain = std::move(m_link_chain);
    m_link_chain.clear();

    // Check if user-given template type equals Realm type. Todo, we should clean up and reuse all our
    // type traits (all the is_same() cases below).
    const Table* table = get_link_chain_target(link_chain);

    realm::DataType ct = table->get_column_type(column_ndx);
    if (std::is_same<T, int64_t>::value && ct != type_Int)
        throw LogicError(LogicError::type_mismatch);
    else if (std::is_same<T, bool>::value && ct != type_Bool)
        throw LogicError(LogicError::type_mismatch);
    else if (std::is_same<T, realm::OldDateTime>::value && ct != type_OldDateTime)
        throw LogicError(LogicError::type_mismatch);
    else if (std::is_same<T, float>::value && ct != type_Float)
        throw LogicError(LogicError::type_mismatch);
    else if (std::is_same<T, double>::value && ct != type_Double)
        throw LogicError(LogicError::type_mismatch);

    if (std::is_same<T, Link>::value || std::is_same<T, LinkList>::value || std::is_same<T, BackLink>::value) {
        link_chain.push_back(column_ndx);
    }

    return Columns<T>(column_ndx, this, std::move(link_chain));
}

template <class T>
inline Columns<T> Table::column(const Table& origin, size_t origin_col_ndx)
{
    static_assert(std::is_same<T, BackLink>::value, "");

    size_t origin_table_ndx = origin.get_index_in_group();
    const Table& current_target_table = *get_link_chain_target(m_link_chain);
    size_t backlink_col_ndx = current_target_table.m_spec->find_backlink_column(origin_table_ndx, origin_col_ndx);

    std::vector<size_t> link_chain = std::move(m_link_chain);
    m_link_chain.clear();
    link_chain.push_back(backlink_col_ndx);

    return Columns<T>(backlink_col_ndx, this, std::move(link_chain));
}

template <class T>
inline BacklinkCount<T> Table::get_backlink_count()
{
    std::vector<size_t> link_chain = std::move(m_link_chain);
    m_link_chain.clear();
    return BacklinkCount<T>(this, std::move(link_chain));
}

template <class T>
SubQuery<T> Table::column(size_t column_ndx, Query subquery)
{
    static_assert(std::is_same<T, LinkList>::value, "A subquery must involve a link list or backlink column");
    return SubQuery<T>(column<T>(column_ndx), std::move(subquery));
}

template <class T>
SubQuery<T> Table::column(const Table& origin, size_t origin_col_ndx, Query subquery)
{
    static_assert(std::is_same<T, BackLink>::value, "A subquery must involve a link list or backlink column");
    return SubQuery<T>(column<T>(origin, origin_col_ndx), std::move(subquery));
}

inline Table& Table::link(size_t link_column)
{
    m_link_chain.push_back(link_column);
    return *this;
}

inline Table& Table::backlink(const Table& origin, size_t origin_col_ndx)
{
    size_t origin_table_ndx = origin.get_index_in_group();
    const Table& current_target_table = *get_link_chain_target(m_link_chain);
    size_t backlink_col_ndx = current_target_table.m_spec->find_backlink_column(origin_table_ndx, origin_col_ndx);
    return link(backlink_col_ndx);
}

inline bool Table::is_empty() const noexcept
{
    return m_size == 0;
}

inline size_t Table::size() const noexcept
{
    return m_size;
}

inline Table::RowExpr Table::get(size_t row_ndx) noexcept
{
    REALM_ASSERT_3(row_ndx, <, size());
    return RowExpr(this, row_ndx);
}

inline Table::ConstRowExpr Table::get(size_t row_ndx) const noexcept
{
    REALM_ASSERT_3(row_ndx, <, size());
    return ConstRowExpr(this, row_ndx);
}

inline Table::RowExpr Table::front() noexcept
{
    return get(0);
}

inline Table::ConstRowExpr Table::front() const noexcept
{
    return get(0);
}

inline Table::RowExpr Table::back() noexcept
{
    return get(m_size - 1);
}

inline Table::ConstRowExpr Table::back() const noexcept
{
    return get(m_size - 1);
}

inline Table::RowExpr Table::operator[](size_t row_ndx) noexcept
{
    return get(row_ndx);
}

inline Table::ConstRowExpr Table::operator[](size_t row_ndx) const noexcept
{
    return get(row_ndx);
}

inline size_t Table::add_empty_row(size_t num_rows)
{
    size_t row_ndx = m_size;
    insert_empty_row(row_ndx, num_rows); // Throws
    return row_ndx;                      // Return index of first new row
}

inline ConstTableRef Table::get_subtable_tableref(size_t col_ndx, size_t row_ndx) const
{
    return const_cast<Table*>(this)->get_subtable_tableref(col_ndx, row_ndx); // Throws
}

inline bool Table::is_null_link(size_t col_ndx, size_t row_ndx) const noexcept
{
    return get_link(col_ndx, row_ndx) == realm::npos;
}

inline ConstTableRef Table::get_link_target(size_t col_ndx) const noexcept
{
    return const_cast<Table*>(this)->get_link_target(col_ndx);
}

template <class E>
inline void Table::set_enum(size_t column_ndx, size_t row_ndx, E value)
{
    set_int(column_ndx, row_ndx, value);
}

inline void Table::nullify_link(size_t col_ndx, size_t row_ndx)
{
    set_link(col_ndx, row_ndx, realm::npos);
}

inline TableRef Table::get_subtable(size_t column_ndx, size_t row_ndx)
{
    return get_subtable_tableref(column_ndx, row_ndx);
}

inline ConstTableRef Table::get_subtable(size_t column_ndx, size_t row_ndx) const
{
    return get_subtable_tableref(column_ndx, row_ndx);
}

inline ConstTableRef Table::get_parent_table(size_t* column_ndx_out) const noexcept
{
    return ConstTableRef(get_parent_table_ptr(column_ndx_out));
}

inline TableRef Table::get_parent_table(size_t* column_ndx_out) noexcept
{
    return TableRef(get_parent_table_ptr(column_ndx_out));
}

inline bool Table::is_group_level() const noexcept
{
    return bool(get_parent_group());
}

inline bool Table::operator==(const Table& t) const
{
    return *m_spec == *t.m_spec && compare_rows(t); // Throws
}

inline bool Table::operator!=(const Table& t) const
{
    return !(*this == t); // Throws
}

inline bool Table::is_degenerate() const
{
    if (!is_attached()) {
        throw LogicError{LogicError::detached_accessor};
    }

    return !m_columns.is_attached();
}

inline void Table::set_into_mixed(Table* parent, size_t col_ndx, size_t row_ndx) const
{
    parent->set_mixed_subtable(col_ndx, row_ndx, this);
}

inline size_t Table::get_size_from_ref(ref_type top_ref, Allocator& alloc) noexcept
{
    const char* top_header = alloc.translate(top_ref);
    std::pair<int_least64_t, int_least64_t> p = Array::get_two(top_header, 0);
    ref_type spec_ref = to_ref(p.first), columns_ref = to_ref(p.second);
    return get_size_from_ref(spec_ref, columns_ref, alloc);
}

inline Table* Table::get_parent_table_ptr(size_t* column_ndx_out) noexcept
{
    const Table* parent = const_cast<const Table*>(this)->get_parent_table_ptr(column_ndx_out);
    return const_cast<Table*>(parent);
}

inline bool Table::is_link_type(ColumnType col_type) noexcept
{
    return col_type == col_type_Link || col_type == col_type_LinkList;
}

inline size_t* Table::record_subtable_path(size_t* begin, size_t* end) const noexcept
{
    const Array& real_top = m_top.is_attached() ? m_top : m_columns;
    size_t index_in_parent = real_top.get_ndx_in_parent();
    REALM_ASSERT_3(begin, <, end);
    *begin++ = index_in_parent;
    ArrayParent* parent = real_top.get_parent();
    REALM_ASSERT(parent);
    REALM_ASSERT(dynamic_cast<Parent*>(parent));
    return static_cast<Parent*>(parent)->record_subtable_path(begin, end);
}

inline size_t* Table::Parent::record_subtable_path(size_t* begin, size_t*) noexcept
{
    return begin;
}

inline bool Table::is_marked() const noexcept
{
    return m_mark;
}

inline void Table::mark() noexcept
{
    m_mark = true;
}

inline void Table::unmark() noexcept
{
    m_mark = false;
}

inline Replication* Table::get_repl() noexcept
{
    return m_top.get_alloc().get_replication();
}

inline void Table::set_ndx_in_parent(size_t ndx_in_parent) noexcept
{
    if (m_top.is_attached()) {
        // Root table (independent descriptor)
        m_top.set_ndx_in_parent(ndx_in_parent);
    }
    else {
        // Subtable with shared descriptor
        m_columns.set_ndx_in_parent(ndx_in_parent);
    }
}

// Declare our explicit specializations so that the inline wrappers don't try
// to instantiate them
template<> int64_t Table::get<int64_t>(size_t, size_t) const noexcept;
template<> util::Optional<int64_t> Table::get<util::Optional<int64_t>>(size_t, size_t) const noexcept;
template<> bool Table::get<bool>(size_t, size_t) const noexcept;
template<> Optional<bool> Table::get<Optional<bool>>(size_t, size_t) const noexcept;
template<> float Table::get<float>(size_t, size_t) const noexcept;
template<> util::Optional<float> Table::get<util::Optional<float>>(size_t, size_t) const noexcept;
template<> double Table::get<double>(size_t, size_t) const noexcept;
template<> util::Optional<double> Table::get<util::Optional<double>>(size_t, size_t) const noexcept;
template<> OldDateTime Table::get<OldDateTime>(size_t, size_t) const noexcept;
template<> Timestamp Table::get<Timestamp>(size_t, size_t) const noexcept;
template<> StringData Table::get<StringData>(size_t, size_t) const noexcept;
template<> BinaryData Table::get<BinaryData>(size_t, size_t) const noexcept;
template<> BinaryIterator Table::get<BinaryIterator>(size_t, size_t) const noexcept;
template<> Mixed Table::get<Mixed>(size_t, size_t) const noexcept;

template<> void Table::set<int64_t>(size_t, size_t, int64_t, bool);
template<> void Table::set<bool>(size_t, size_t, bool, bool);
template<> void Table::set<float>(size_t, size_t, float, bool);
template<> void Table::set<double>(size_t, size_t, double, bool);
template<> void Table::set<OldDateTime>(size_t, size_t, OldDateTime, bool);
template<> void Table::set<Timestamp>(size_t, size_t, Timestamp, bool);
template<> void Table::set<StringData>(size_t, size_t, StringData, bool);
template<> void Table::set<BinaryData>(size_t, size_t, BinaryData, bool);
template<> void Table::set<Mixed>(size_t, size_t, Mixed, bool);
template<> void Table::set<null>(size_t, size_t, null, bool);

template<> size_t Table::set_unique<int64_t>(size_t, size_t, int64_t);
template<> size_t Table::set_unique<StringData>(size_t, size_t, StringData);
template<> size_t Table::set_unique<null>(size_t, size_t, null);


inline int64_t Table::get_int(size_t col_ndx, size_t ndx) const noexcept
{
    if (is_nullable(col_ndx))
        return get<util::Optional<int64_t>>(col_ndx, ndx).value_or(0);
    else
        return get<int64_t>(col_ndx, ndx);
}

inline size_t Table::set_int_unique(size_t col_ndx, size_t ndx, int_fast64_t value)
{
    return set_unique(col_ndx, ndx, value);
}

inline void Table::set_int(size_t col_ndx, size_t ndx, int_fast64_t value, bool is_default)
{
    return set(col_ndx, ndx, value, is_default);
}

inline Timestamp Table::get_timestamp(size_t col_ndx, size_t ndx) const noexcept
{
    return get<Timestamp>(col_ndx, ndx);
}

inline void Table::set_timestamp(size_t col_ndx, size_t ndx, Timestamp value, bool is_default)
{
    return set(col_ndx, ndx, value, is_default);
}

inline bool Table::get_bool(size_t col_ndx, size_t ndx) const noexcept
{
    if (is_nullable(col_ndx))
        return get<util::Optional<bool>>(col_ndx, ndx).value_or(false);
    else
        return get<bool>(col_ndx, ndx);
}

inline void Table::set_bool(size_t col_ndx, size_t ndx, bool value, bool is_default)
{
    return set(col_ndx, ndx, value, is_default);
}

inline OldDateTime Table::get_olddatetime(size_t col_ndx, size_t ndx) const noexcept
{
    return get<OldDateTime>(col_ndx, ndx);
}

inline void Table::set_olddatetime(size_t col_ndx, size_t ndx, OldDateTime value, bool is_default)
{
    return set(col_ndx, ndx, value, is_default);
}

inline float Table::get_float(size_t col_ndx, size_t ndx) const noexcept
{
    float f = get<float>(col_ndx, ndx);
    return null::is_null_float(f) ? 0.0f : f;
}

inline void Table::set_float(size_t col_ndx, size_t ndx, float value, bool is_default)
{
    return set(col_ndx, ndx, value, is_default);
}

inline double Table::get_double(size_t col_ndx, size_t ndx) const noexcept
{
    double d = get<double>(col_ndx, ndx);
    return null::is_null_float(d) ? 0.0 : d;
}

inline void Table::set_double(size_t col_ndx, size_t ndx, double value, bool is_default)
{
    return set(col_ndx, ndx, value, is_default);
}

inline StringData Table::get_string(size_t col_ndx, size_t ndx) const noexcept
{
    return get<StringData>(col_ndx, ndx);
}

inline void Table::set_string(size_t col_ndx, size_t ndx, StringData value, bool is_default)
{
    return set(col_ndx, ndx, value, is_default);
}

inline size_t Table::set_string_unique(size_t col_ndx, size_t ndx, StringData value)
{
    return set_unique(col_ndx, ndx, value);
}

inline BinaryData Table::get_binary(size_t col_ndx, size_t ndx) const noexcept
{
    return get<BinaryData>(col_ndx, ndx);
}

inline BinaryIterator Table::get_binary_iterator(size_t col_ndx, size_t ndx) const noexcept
{
    return get<BinaryIterator>(col_ndx, ndx);
}

inline void Table::set_binary(size_t col_ndx, size_t ndx, BinaryData value, bool is_default)
{
    set(col_ndx, ndx, value, is_default);
}

inline Mixed Table::get_mixed(size_t col_ndx, size_t ndx) const noexcept
{
    return get<Mixed>(col_ndx, ndx);
}

inline void Table::set_mixed(size_t col_ndx, size_t ndx, Mixed value, bool is_default)
{
    set(col_ndx, ndx, value, is_default);
}

inline void Table::set_null(size_t col_ndx, size_t ndx, bool is_default)
{
    set(col_ndx, ndx, null(), is_default);
}

inline void Table::set_null_unique(size_t col_ndx, size_t ndx)
{
    set_unique(col_ndx, ndx, null());
}


// This class groups together information about the target of a link column
// This is not a valid link if the target table == nullptr
struct LinkTargetInfo {
    LinkTargetInfo(Table* target = nullptr, size_t backlink_ndx = realm::npos)
        : m_target_table(target)
        , m_backlink_col_ndx(backlink_ndx)
    {
    }
    bool is_valid() const
    {
        return (m_target_table != nullptr);
    }
    Table* m_target_table;
    size_t m_backlink_col_ndx; // a value of npos indicates the backlink should be appended
};

// The purpose of this class is to give internal access to some, but
// not all of the non-public parts of the Table class.
class _impl::TableFriend {
public:
    typedef Table::UnbindGuard UnbindGuard;

    static ref_type create_empty_table(Allocator& alloc)
    {
        return Table::create_empty_table(alloc); // Throws
    }

    static ref_type clone(const Table& table, Allocator& alloc)
    {
        return table.clone(alloc); // Throws
    }

    static ref_type clone_columns(const Table& table, Allocator& alloc)
    {
        return table.clone_columns(alloc); // Throws
    }

    static Table* create_accessor(Allocator& alloc, ref_type top_ref, Table::Parent* parent, size_t ndx_in_parent)
    {
        std::unique_ptr<Table> table(new Table(Table::ref_count_tag(), alloc)); // Throws
        table->init(top_ref, parent, ndx_in_parent);                            // Throws
        return table.release();
    }

    static Table* create_accessor(Spec* shared_spec, Table::Parent* parent_column, size_t parent_row_ndx)
    {
        Allocator& alloc = shared_spec->get_alloc();
        std::unique_ptr<Table> table(new Table(Table::ref_count_tag(), alloc)); // Throws
        table->init(shared_spec, parent_column, parent_row_ndx);                // Throws
        return table.release();
    }

    // Intended to be used only by Group::create_table_accessor()
    static Table* create_incomplete_accessor(Allocator& alloc, ref_type top_ref, Table::Parent* parent,
                                             size_t ndx_in_parent)
    {
        std::unique_ptr<Table> table(new Table(Table::ref_count_tag(), alloc)); // Throws
        bool skip_create_column_accessors = true;
        table->init(top_ref, parent, ndx_in_parent, skip_create_column_accessors); // Throws
        return table.release();
    }

    // Intended to be used only by Group::create_table_accessor()
    static void complete_accessor(Table& table)
    {
        table.refresh_column_accessors(); // Throws
    }

    static void set_top_parent(Table& table, ArrayParent* parent, size_t ndx_in_parent) noexcept
    {
        table.m_top.set_parent(parent, ndx_in_parent);
    }

    static void update_from_parent(Table& table, size_t old_baseline) noexcept
    {
        table.update_from_parent(old_baseline);
    }

    static void detach(Table& table) noexcept
    {
        table.detach();
    }

    static void discard_row_accessors(Table& table) noexcept
    {
        table.discard_row_accessors();
    }

    static void discard_child_accessors(Table& table) noexcept
    {
        table.discard_child_accessors();
    }

    static void discard_subtable_accessor(Table& table, size_t col_ndx, size_t row_ndx) noexcept
    {
        table.discard_subtable_accessor(col_ndx, row_ndx);
    }

    static void bind_ptr(Table& table) noexcept
    {
        table.bind_ptr();
    }

    static void unbind_ptr(Table& table) noexcept
    {
        table.unbind_ptr();
    }

    static bool compare_rows(const Table& a, const Table& b)
    {
        return a.compare_rows(b); // Throws
    }

    static size_t get_size_from_ref(ref_type ref, Allocator& alloc) noexcept
    {
        return Table::get_size_from_ref(ref, alloc);
    }

    static size_t get_size_from_ref(ref_type spec_ref, ref_type columns_ref, Allocator& alloc) noexcept
    {
        return Table::get_size_from_ref(spec_ref, columns_ref, alloc);
    }

    static Spec& get_spec(Table& table) noexcept
    {
        return *table.m_spec;
    }

    static const Spec& get_spec(const Table& table) noexcept
    {
        return *table.m_spec;
    }

    static ColumnBase& get_column(const Table& table, size_t col_ndx)
    {
        return *table.m_cols[col_ndx];
    }

    static void do_remove(Table& table, size_t row_ndx)
    {
        bool broken_reciprocal_backlinks = false;
        table.do_remove(row_ndx, broken_reciprocal_backlinks); // Throws
    }

    static void do_move_last_over(Table& table, size_t row_ndx)
    {
        bool broken_reciprocal_backlinks = false;
        table.do_move_last_over(row_ndx, broken_reciprocal_backlinks); // Throws
    }

    static void do_swap_rows(Table& table, size_t row_ndx_1, size_t row_ndx_2)
    {
        table.do_swap_rows(row_ndx_1, row_ndx_2); // Throws
    }

    static void do_move_row(Table& table, size_t from_ndx, size_t to_ndx)
    {
        table.do_move_row(from_ndx, to_ndx); // Throws
    }

    static void do_merge_rows(Table& table, size_t row_ndx, size_t new_row_ndx)
    {
        table.do_merge_rows(row_ndx, new_row_ndx); // Throws
    }

    static void do_clear(Table& table)
    {
        bool broken_reciprocal_backlinks = false;
        table.do_clear(broken_reciprocal_backlinks); // Throws
    }

    static void do_set_link(Table& table, size_t col_ndx, size_t row_ndx, size_t target_row_ndx)
    {
        table.do_set_link(col_ndx, row_ndx, target_row_ndx); // Throws
    }

    static size_t get_backlink_count(const Table& table, size_t row_ndx, bool only_strong_links) noexcept
    {
        return table.get_backlink_count(row_ndx, only_strong_links);
    }

    static void cascade_break_backlinks_to(Table& table, size_t row_ndx, CascadeState& state)
    {
        table.cascade_break_backlinks_to(row_ndx, state); // Throws
    }

    static void remove_backlink_broken_rows(Table& table, const CascadeState& rows)
    {
        table.remove_backlink_broken_rows(rows); // Throws
    }

    static size_t* record_subtable_path(const Table& table, size_t* begin, size_t* end) noexcept
    {
        return table.record_subtable_path(begin, end);
    }

    static void insert_column(Descriptor& desc, size_t column_ndx, DataType type, StringData name,
                              LinkTargetInfo& link, bool nullable = false)
    {
        Table::do_insert_column(desc, column_ndx, type, name, link, nullable); // Throws
    }

    static void insert_column_unless_exists(Descriptor& desc, size_t column_ndx, DataType type, StringData name,
                                            LinkTargetInfo link, bool nullable = false, bool* was_inserted = nullptr)
    {
        Table::do_insert_column_unless_exists(desc, column_ndx, type, name, link, nullable, was_inserted); // Throws
    }

    static void erase_column(Descriptor& desc, size_t column_ndx)
    {
        Table::do_erase_column(desc, column_ndx); // Throws
    }

    static void rename_column(Descriptor& desc, size_t column_ndx, StringData name)
    {
        Table::do_rename_column(desc, column_ndx, name); // Throws
    }

    static void add_search_index(Descriptor& desc, size_t column_ndx)
    {
        Table::do_add_search_index(desc, column_ndx); // Throws
    }

    static void remove_search_index(Descriptor& desc, size_t column_ndx)
    {
        Table::do_remove_search_index(desc, column_ndx); // Throws
    }

    static void set_link_type(Table& table, size_t column_ndx, LinkType link_type)
    {
        table.do_set_link_type(column_ndx, link_type); // Throws
    }

    static void erase_row(Table& table, size_t row_ndx, bool is_move_last_over)
    {
        table.erase_row(row_ndx, is_move_last_over); // Throws
    }

    static void batch_erase_rows(Table& table, const IntegerColumn& row_indexes, bool is_move_last_over)
    {
        table.batch_erase_rows(row_indexes, is_move_last_over); // Throws
    }

    static TableRef get_subtable_accessor(Table& table, size_t col_ndx, size_t row_ndx) noexcept
    {
        return table.get_subtable_accessor(col_ndx, row_ndx);
    }

    static const Table* get_link_target_table_accessor(const Table& table, size_t col_ndx) noexcept
    {
        return const_cast<Table&>(table).get_link_target_table_accessor(col_ndx);
    }

    static Table* get_link_target_table_accessor(Table& table, size_t col_ndx) noexcept
    {
        return table.get_link_target_table_accessor(col_ndx);
    }

    static void adj_acc_insert_rows(Table& table, size_t row_ndx, size_t num_rows) noexcept
    {
        table.adj_acc_insert_rows(row_ndx, num_rows);
    }

    static void adj_acc_erase_row(Table& table, size_t row_ndx) noexcept
    {
        table.adj_acc_erase_row(row_ndx);
    }

    static void adj_acc_swap_rows(Table& table, size_t row_ndx_1, size_t row_ndx_2) noexcept
    {
        table.adj_acc_swap_rows(row_ndx_1, row_ndx_2);
    }

    static void adj_acc_move_row(Table& table, size_t from_ndx, size_t to_ndx) noexcept
    {
        table.adj_acc_move_row(from_ndx, to_ndx);
    }

    static void adj_acc_merge_rows(Table& table, size_t row_ndx_1, size_t row_ndx_2) noexcept
    {
        table.adj_acc_merge_rows(row_ndx_1, row_ndx_2);
    }

    static void adj_acc_move_over(Table& table, size_t from_row_ndx, size_t to_row_ndx) noexcept
    {
        table.adj_acc_move_over(from_row_ndx, to_row_ndx);
    }

    static void adj_acc_clear_root_table(Table& table) noexcept
    {
        table.adj_acc_clear_root_table();
    }

    static void adj_acc_clear_nonroot_table(Table& table) noexcept
    {
        table.adj_acc_clear_nonroot_table();
    }

    static void adj_insert_column(Table& table, size_t col_ndx)
    {
        table.adj_insert_column(col_ndx); // Throws
    }

    static void adj_add_column(Table& table)
    {
        size_t num_cols = table.m_cols.size();
        table.adj_insert_column(num_cols); // Throws
    }

    static void adj_erase_column(Table& table, size_t col_ndx) noexcept
    {
        table.adj_erase_column(col_ndx);
    }

    static bool is_marked(const Table& table) noexcept
    {
        return table.is_marked();
    }

    static void mark(Table& table) noexcept
    {
        table.mark();
    }

    static void unmark(Table& table) noexcept
    {
        table.unmark();
    }

    static void recursive_mark(Table& table) noexcept
    {
        table.recursive_mark();
    }

    static void mark_link_target_tables(Table& table, size_t col_ndx_begin) noexcept
    {
        table.mark_link_target_tables(col_ndx_begin);
    }

    static void mark_opposite_link_tables(Table& table) noexcept
    {
        table.mark_opposite_link_tables();
    }

    static DescriptorRef get_root_table_desc_accessor(Table& root_table) noexcept
    {
        return root_table.m_descriptor.lock();
    }

    typedef Table::AccessorUpdater AccessorUpdater;
    static void update_accessors(Table& table, const size_t* col_path_begin, const size_t* col_path_end,
                                 AccessorUpdater& updater)
    {
        table.update_accessors(col_path_begin, col_path_end, updater); // Throws
    }

    static void refresh_accessor_tree(Table& table)
    {
        table.refresh_accessor_tree(); // Throws
    }

    static void refresh_spec_accessor(Table& table)
    {
        table.refresh_spec_accessor(); // Throws
    }

    static void set_ndx_in_parent(Table& table, size_t ndx_in_parent) noexcept
    {
        table.set_ndx_in_parent(ndx_in_parent);
    }

    static bool is_link_type(ColumnType type) noexcept
    {
        return Table::is_link_type(type);
    }

    static void bump_version(Table& table, bool bump_global = true) noexcept
    {
        table.bump_version(bump_global);
    }

    static bool is_cross_table_link_target(const Table& table)
    {
        return table.is_cross_table_link_target();
    }

    static Group* get_parent_group(const Table& table) noexcept
    {
        return table.get_parent_group();
    }

    static Replication* get_repl(Table& table) noexcept
    {
        return table.get_repl();
    }

    static void register_view(Table& table, const TableViewBase* view)
    {
        table.register_view(view); // Throws
    }

    static void unregister_view(Table& table, const TableViewBase* view) noexcept
    {
        table.unregister_view(view);
    }
};


} // namespace realm

#endif // REALM_TABLE_HPP
