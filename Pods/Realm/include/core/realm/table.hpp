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
#include <realm/util/function_ref.hpp>
#include <realm/util/thread.hpp>
#include <realm/table_ref.hpp>
#include <realm/list.hpp>
#include <realm/spec.hpp>
#include <realm/query.hpp>
#include <realm/cluster_tree.hpp>
#include <realm/keys.hpp>
#include <realm/global_key.hpp>

// Only set this to one when testing the code paths that exercise object ID
// hash collisions. It artificially limits the "optimistic" local ID to use
// only the lower 15 bits of the ID rather than the lower 63 bits, making it
// feasible to generate collisions within reasonable time.
#define REALM_EXERCISE_OBJECT_ID_COLLISION 0

namespace realm {

class BacklinkColumn;
template <class>
class BacklinkCount;
class BinaryColumy;
class ConstTableView;
class Group;
class SortDescriptor;
class StringIndex;
class TableView;
template <class>
class Columns;
template <class>
class SubQuery;
struct LinkTargetInfo;
class ColKeys;
struct GlobalKey;
class LinkChain;

struct Link {
};
typedef Link BackLink;


namespace _impl {
class TableFriend;
}
namespace metrics {
class QueryInfo;
}

class Table {
public:
    /// Construct a new freestanding top-level table with static
    /// lifetime.
    ///
    /// This constructor should be used only when placing a table
    /// instance on the stack, and it is then the responsibility of
    /// the application that there are no objects of type TableRef or
    /// ConstTableRef that refer to it, or to any of its subtables,
    /// when it goes out of scope.
    Table(Allocator& = Allocator::get_default());

    /// Construct a copy of the specified table as a new freestanding
    /// top-level table with static lifetime.
    ///
    /// This constructor should be used only when placing a table
    /// instance on the stack, and it is then the responsibility of
    /// the application that there are no objects of type TableRef or
    /// ConstTableRef that refer to it, or to any of its subtables,
    /// when it goes out of scope.
    Table(const Table&, Allocator& = Allocator::get_default());

    ~Table() noexcept;

    Allocator& get_alloc() const;

    /// Construct a copy of the specified table as a new freestanding top-level
    /// table with dynamic lifetime. This method is deprecated.
    TableRef copy(Allocator& = Allocator::get_default()) const;

    /// Get the name of this table, if it has one. Only group-level tables have
    /// names. For a table of any other kind, this function returns the empty
    /// string.
    StringData get_name() const noexcept;

    // Whether or not elements can be null.
    bool is_nullable(ColKey col_key) const;

    // Whether or not the column is a list.
    bool is_list(ColKey col_key) const;

    //@{
    /// Conventience functions for inspecting the dynamic table type.
    ///
    size_t get_column_count() const noexcept;
    DataType get_column_type(ColKey column_key) const;
    StringData get_column_name(ColKey column_key) const;
    ColumnAttrMask get_column_attr(ColKey column_key) const noexcept;
    ColKey get_column_key(StringData name) const noexcept;
    ColKeys get_column_keys() const;
    typedef util::Optional<std::pair<ConstTableRef, ColKey>> BacklinkOrigin;
    BacklinkOrigin find_backlink_origin(StringData origin_table_name, StringData origin_col_name) const noexcept;
    BacklinkOrigin find_backlink_origin(ColKey backlink_col) const noexcept;
    //@}

    // Primary key columns
    ColKey get_primary_key_column() const;
    void set_primary_key_column(ColKey col);
    void validate_primary_column();

    //@{
    /// Convenience functions for manipulating the dynamic table type.
    ///
    static const size_t max_column_name_length = 63;
    static const uint64_t max_num_columns = 0xFFFFUL; // <-- must be power of two -1
    ColKey add_column(DataType type, StringData name, bool nullable = false);
    ColKey add_column_list(DataType type, StringData name, bool nullable = false);
    ColKey add_column_link(DataType type, StringData name, Table& target, LinkType link_type = link_Weak);

    // Pass a ColKey() as first argument to have a new colkey generated
    // Requesting a specific ColKey may fail with invalidkey exception, if the key is already in use
    // We recommend allowing Core to choose the ColKey.
    ColKey insert_column(ColKey col_key, DataType type, StringData name, bool nullable = false);
    ColKey insert_column_link(ColKey col_key, DataType type, StringData name, Table& target,
                              LinkType link_type = link_Weak);
    void remove_column(ColKey col_key);
    void rename_column(ColKey col_key, StringData new_name);
    bool valid_column(ColKey col_key) const noexcept;
    void check_column(ColKey col_key) const;
    //@}

    /// There are two kinds of links, 'weak' and 'strong'. A strong link is one
    /// that implies ownership, i.e., that the origin object (parent) owns the
    /// target parent (child). Simply stated, this means that when the origin object
    /// (parent) is removed, so is the target object (child). If there are multiple
    /// strong links to an object, the origin objects share ownership, and the
    /// target object is removed when the last owner disappears. Weak links do not
    /// imply ownership, and will be nullified or removed when the target object
    /// disappears.
    ///
    /// To put this in precise terms; when a strong link is broken, and the
    /// target object has no other strong links to it, the target object is removed. A
    /// object that is implicitly removed in this way, is said to be
    /// *cascade-removed*. When a weak link is broken, nothing is
    /// cascade-removed.
    ///
    /// A link is considered broken if
    ///
    ///  - the link is nullified, removed, or replaced by a different link
    ///
    ///  - the origin object is explicitly removed
    ///
    ///  - the origin object is cascade-removed, or if
    ///
    ///  - the origin field is removed from the table (Table::remove_column()),
    ///    or if
    ///
    ///  - the origin table is removed from the group.
    ///
    /// Note that a link is *not* considered broken when it is replaced by a
    /// link to the same target object. I.e., no objects will be cascade-removed
    /// due to such an operation.
    ///
    /// When a object is explicitly removed (such as by Table::move_last_over()),
    /// all links to it are automatically removed or nullified. For single link
    /// columns (type_Link), links to the removed object are nullified. For link
    /// list columns (type_LinkList), links to the removed object are removed from
    /// the list.
    ///
    /// When a object is cascade-removed there can no longer be any strong links to
    /// it, but if there are any weak links, they will be removed or nullified.
    ///
    /// It is important to understand that this cascade-removal scheme is too
    /// simplistic to enable detection and removal of orphaned link-cycles. In
    /// this respect, it suffers from the same limitations as a reference
    /// counting scheme generally does.
    ///
    /// It is also important to understand, that the possible presence of a link
    /// cycle can cause a object to be cascade-removed as a consequence of being
    /// modified. This happens, for example, if two objects, A and B, have strong
    /// links to each other, and there are no other strong links to either of
    /// them. In this case, if A->B is changed to A->C, then both A and B will
    /// be cascade-removed. This can lead to obscure bugs in some applications.
    ///
    /// The link type must be specified at column creation and cannot be changed
    // Returns the link type for the given column.
    // Throws an LogicError if target column is not a link column.
    LinkType get_link_type(ColKey col_key) const;

    /// True for `col_type_Link` and `col_type_LinkList`.
    static bool is_link_type(ColumnType) noexcept;

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
    /// \param col_key The key of a column of the table.

    bool has_search_index(ColKey col_key) const noexcept;
    void add_search_index(ColKey col_key);
    void remove_search_index(ColKey col_key);

    void enumerate_string_column(ColKey col_key);
    bool is_enumerated(ColKey col_key) const noexcept;
    bool contains_unique_values(ColKey col_key) const;

    //@}

    /// If the specified column is optimized to store only unique values, then
    /// this function returns the number of unique values currently
    /// stored. Otherwise it returns zero. This function is mainly intended for
    /// debugging purposes.
    size_t get_num_unique_values(ColKey col_key) const;

    template <class T>
    Columns<T> column(ColKey col_key) const; // FIXME: Should this one have been declared noexcept?
    template <class T>
    Columns<T> column(const Table& origin, ColKey origin_col_key) const;

    // BacklinkCount is a total count per row and therefore not attached to a specific column
    template <class T>
    BacklinkCount<T> get_backlink_count() const;

    template <class T>
    SubQuery<T> column(ColKey col_key, Query subquery) const;
    template <class T>
    SubQuery<T> column(const Table& origin, ColKey origin_col_key, Query subquery) const;

    // Table size and deletion
    bool is_empty() const noexcept;
    size_t size() const noexcept;

    //@{

    /// Object handling.

    // Create an object with key. If the key is omitted, a key will be generated by the system
    Obj create_object(ObjKey key = {}, const FieldValues& = {});
    // Create an object with specific GlobalKey.
    Obj create_object(GlobalKey object_id, const FieldValues& = {});
    // Create an object with primary key. If an object with the given primary key already exists, it
    // will be returned and did_create (if supplied) will be set to false.
    Obj create_object_with_primary_key(const Mixed& primary_key, bool* did_create = nullptr);
    /// Create a number of objects and add corresponding keys to a vector
    void create_objects(size_t number, std::vector<ObjKey>& keys);
    /// Create a number of objects with keys supplied
    void create_objects(const std::vector<ObjKey>& keys);
    /// Does the key refer to an object within the table?
    bool is_valid(ObjKey key) const
    {
        return m_clusters.is_valid(key);
    }
    ObjKey get_obj_key(GlobalKey id) const;
    GlobalKey get_object_id(ObjKey key) const;
    Obj get_object(ObjKey key)
    {
        return m_clusters.get(key);
    }
    ConstObj get_object(ObjKey key) const
    {
        return m_clusters.get(key);
    }
    Obj get_object(size_t ndx)
    {
        return m_clusters.get(ndx);
    }
    ConstObj get_object(size_t ndx) const
    {
        return m_clusters.get(ndx);
    }
    // Get logical index for object. This function is not very efficient
    size_t get_object_ndx(ObjKey key) const
    {
        return m_clusters.get_ndx(key);
    }

    void dump_objects();

    bool traverse_clusters(ClusterTree::TraverseFunction func) const
    {
        return m_clusters.traverse(func);
    }

    /// remove_object() removes the specified object from the table.
    /// The removal of an object a table may cause other linked objects to be
    /// cascade-removed. The clearing of a table may also cause linked objects
    /// to be cascade-removed, but in this respect, the effect is exactly as if
    /// each object had been removed individually. See set_link_type() for details.
    void remove_object(ObjKey key);
    /// remove_object_recursive() will delete linked rows if the removed link was the
    /// last one holding on to the row in question. This will be done recursively.
    void remove_object_recursive(ObjKey key);
    void clear();
    using Iterator = ClusterTree::Iterator;
    using ConstIterator = ClusterTree::ConstIterator;
    ConstIterator begin() const;
    ConstIterator end() const;
    Iterator begin();
    Iterator end();
    void remove_object(const ConstIterator& it)
    {
        remove_object(it->get_key());
    }
    //@}


    TableRef get_link_target(ColKey column_key) noexcept;
    ConstTableRef get_link_target(ColKey column_key) const noexcept;

    static const size_t max_string_size = 0xFFFFF8 - Array::header_size - 1;
    static const size_t max_binary_size = 0xFFFFF8 - Array::header_size;

    // FIXME: These limits should be chosen independently of the underlying
    // platform's choice to define int64_t and independent of the integer
    // representation. The current values only work for 2's complement, which is
    // not guaranteed by the standard.
    static constexpr int_fast64_t max_integer = std::numeric_limits<int64_t>::max();
    static constexpr int_fast64_t min_integer = std::numeric_limits<int64_t>::min();

    /// Only group-level unordered tables can be used as origins or targets of
    /// links.
    bool is_group_level() const noexcept;

    /// A Table accessor obtained from a frozen transaction is also frozen.
    bool is_frozen() const noexcept { return m_is_frozen; }

    /// If this table is a group-level table, then this function returns the
    /// index of this table within the group. Otherwise it returns realm::npos.
    size_t get_index_in_group() const noexcept;
    TableKey get_key() const noexcept;

    uint64_t allocate_sequence_number();
    // Used by upgrade
    void set_sequence_number(uint64_t seq);
    void set_collision_map(ref_type ref);

    // Get the key of this table directly, without needing a Table accessor.
    static TableKey get_key_direct(Allocator& alloc, ref_type top_ref);

    // Aggregate functions
    size_t count_int(ColKey col_key, int64_t value) const;
    size_t count_string(ColKey col_key, StringData value) const;
    size_t count_float(ColKey col_key, float value) const;
    size_t count_double(ColKey col_key, double value) const;

    int64_t sum_int(ColKey col_key) const;
    double sum_float(ColKey col_key) const;
    double sum_double(ColKey col_key) const;
    int64_t maximum_int(ColKey col_key, ObjKey* return_ndx = nullptr) const;
    float maximum_float(ColKey col_key, ObjKey* return_ndx = nullptr) const;
    double maximum_double(ColKey col_key, ObjKey* return_ndx = nullptr) const;
    Timestamp maximum_timestamp(ColKey col_key, ObjKey* return_ndx = nullptr) const;
    int64_t minimum_int(ColKey col_key, ObjKey* return_ndx = nullptr) const;
    float minimum_float(ColKey col_key, ObjKey* return_ndx = nullptr) const;
    double minimum_double(ColKey col_key, ObjKey* return_ndx = nullptr) const;
    Timestamp minimum_timestamp(ColKey col_key, ObjKey* return_ndx = nullptr) const;
    double average_int(ColKey col_key, size_t* value_count = nullptr) const;
    double average_float(ColKey col_key, size_t* value_count = nullptr) const;
    double average_double(ColKey col_key, size_t* value_count = nullptr) const;

    // Will return pointer to search index accessor. Will return nullptr if no index
    StringIndex* get_search_index(ColKey col) const noexcept
    {
        report_invalid_key(col);
        if (!has_search_index(col))
            return nullptr;
        return m_index_accessors[col.get_index().val];
    }
    template <class T>
    ObjKey find_first(ColKey col_key, T value) const;

    ObjKey find_first_int(ColKey col_key, int64_t value) const;
    ObjKey find_first_bool(ColKey col_key, bool value) const;
    ObjKey find_first_timestamp(ColKey col_key, Timestamp value) const;
    ObjKey find_first_float(ColKey col_key, float value) const;
    ObjKey find_first_double(ColKey col_key, double value) const;
    ObjKey find_first_string(ColKey col_key, StringData value) const;
    ObjKey find_first_binary(ColKey col_key, BinaryData value) const;
    ObjKey find_first_null(ColKey col_key) const;

    //    TableView find_all_link(Key target_key);
    //    ConstTableView find_all_link(Key target_key) const;
    TableView find_all_int(ColKey col_key, int64_t value);
    ConstTableView find_all_int(ColKey col_key, int64_t value) const;
    TableView find_all_bool(ColKey col_key, bool value);
    ConstTableView find_all_bool(ColKey col_key, bool value) const;
    TableView find_all_float(ColKey col_key, float value);
    ConstTableView find_all_float(ColKey col_key, float value) const;
    TableView find_all_double(ColKey col_key, double value);
    ConstTableView find_all_double(ColKey col_key, double value) const;
    TableView find_all_string(ColKey col_key, StringData value);
    ConstTableView find_all_string(ColKey col_key, StringData value) const;
    TableView find_all_binary(ColKey col_key, BinaryData value);
    ConstTableView find_all_binary(ColKey col_key, BinaryData value) const;
    TableView find_all_null(ColKey col_key);
    ConstTableView find_all_null(ColKey col_key) const;

    /// The following column types are supported: String, Integer, OldDateTime, Bool
    TableView get_distinct_view(ColKey col_key);
    ConstTableView get_distinct_view(ColKey col_key) const;

    TableView get_sorted_view(ColKey col_key, bool ascending = true);
    ConstTableView get_sorted_view(ColKey col_key, bool ascending = true) const;

    TableView get_sorted_view(SortDescriptor order);
    ConstTableView get_sorted_view(SortDescriptor order) const;

    // Report the current content version. This is a 64-bit value which is bumped whenever
    // the content in the table changes.
    uint_fast64_t get_content_version() const noexcept;

    // Report the current instance version. This is a 64-bit value which is bumped
    // whenever the table accessor is recycled.
    uint_fast64_t get_instance_version() const noexcept;

    // Report the current storage version. This is a 64-bit value which is bumped
    // whenever the location in memory of any part of the table changes.
    uint_fast64_t get_storage_version(uint64_t instance_version) const;
    uint_fast64_t get_storage_version() const;
    void bump_storage_version() const noexcept;
    void bump_content_version() const noexcept;

    // Change the nullability of the column identified by col_key.
    // This might result in the creation of a new column and deletion of the old.
    // The column key to use going forward is returned.
    // If the conversion is from nullable to non-nullable, throw_on_null determines
    // the reaction to encountering a null value: If clear, null values will be
    // converted to default values. If set, a 'column_not_nullable' is thrown and the
    // table is unchanged.
    ColKey set_nullability(ColKey col_key, bool nullable, bool throw_on_null);

    // Iterate through (subset of) columns. The supplied function may abort iteration
    // by returning 'true' (early out).
    template <typename Func>
    bool for_each_and_every_column(Func func) const
    {
        for (auto col_key : m_leaf_ndx2colkey) {
            if (!col_key)
                continue;
            if (func(col_key))
                return true;
        }
        return false;
    }
    template <typename Func>
    bool for_each_public_column(Func func) const
    {
        for (auto col_key : m_leaf_ndx2colkey) {
            if (!col_key)
                continue;
            if (col_key.get_type() == col_type_BackLink)
                continue;
            if (func(col_key))
                return true;
        }
        return false;
    }
    template <typename Func>
    bool for_each_backlink_column(Func func) const
    {
        // FIXME: Optimize later - to not iterate through all non-backlink columns:
        for (auto col_key : m_leaf_ndx2colkey) {
            if (!col_key)
                continue;
            if (col_key.get_type() != col_type_BackLink)
                continue;
            if (func(col_key))
                return true;
        }
        return false;
    }

private:
    template <class T>
    TableView find_all(ColKey col_key, T value);
    void build_column_mapping();
    ColKey generate_col_key(ColumnType ct, ColumnAttrMask attrs);
    void convert_column(ColKey from, ColKey to, bool throw_on_null);
    template <class F, class T>
    void change_nullability(ColKey from, ColKey to, bool throw_on_null);
    template <class F, class T>
    void change_nullability_list(ColKey from, ColKey to, bool throw_on_null);

public:
    // mapping between index used in leaf nodes (leaf_ndx) and index used in spec (spec_ndx)
    // as well as the full column key. A leaf_ndx can be obtained directly from the column key
    size_t colkey2spec_ndx(ColKey key) const;
    size_t leaf_ndx2spec_ndx(ColKey::Idx idx) const;
    ColKey::Idx spec_ndx2leaf_ndx(size_t idx) const;
    ColKey leaf_ndx2colkey(ColKey::Idx idx) const;
    ColKey spec_ndx2colkey(size_t ndx) const;
    void report_invalid_key(ColKey col_key) const;
    size_t num_leaf_cols() const;
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
    ///
    /// FIXME: Deprecate or change to return ObjKey.
    size_t lower_bound_int(ColKey col_key, int64_t value) const noexcept;
    size_t upper_bound_int(ColKey col_key, int64_t value) const noexcept;
    size_t lower_bound_bool(ColKey col_key, bool value) const noexcept;
    size_t upper_bound_bool(ColKey col_key, bool value) const noexcept;
    size_t lower_bound_float(ColKey col_key, float value) const noexcept;
    size_t upper_bound_float(ColKey col_key, float value) const noexcept;
    size_t lower_bound_double(ColKey col_key, double value) const noexcept;
    size_t upper_bound_double(ColKey col_key, double value) const noexcept;
    size_t lower_bound_string(ColKey col_key, StringData value) const noexcept;
    size_t upper_bound_string(ColKey col_key, StringData value) const noexcept;
    //@}

    // Queries
    // Using where(tv) is the new method to perform queries on TableView. The 'tv' can have any order; it does not
    // need to be sorted, and, resulting view retains its order.
    Query where(ConstTableView* tv = nullptr)
    {
        return Query(m_own_ref, tv);
    }

    // FIXME: We need a ConstQuery class or runtime check against modifications in read transaction.
    Query where(ConstTableView* tv = nullptr) const
    {
        return Query(m_own_ref, tv);
    }

    // Perform queries on a LinkView. The returned Query holds a reference to list.
    Query where(const LnkLst& list) const
    {
        return Query(m_own_ref, list);
    }

    //@{
    /// WARNING: The link() and backlink() methods will alter a state on the Table object and return a reference
    /// to itself. Be aware if assigning the return value of link() to a variable; this might be an error!

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

    /// The two calls to link() in the erroneous example will append the two values 0 and 1 to an internal vector in
    /// the owners table, and we end up with three references to that same table: owners, cats and dogs. They are all
    /// the same table, its vector has the values {0, 1}, so a query would not make any sense.
    LinkChain link(ColKey link_column) const;
    LinkChain backlink(const Table& origin, ColKey origin_col_key) const;

    // Conversion
    void to_json(std::ostream& out, size_t link_depth = 0,
                 std::map<std::string, std::string>* renames = nullptr) const;

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
    MemStats stats() const;
#endif
    TableRef get_opposite_table(ColKey col_key) const;
    TableKey get_opposite_table_key(ColKey col_key) const;
    bool links_to_self(ColKey col_key) const;
    ColKey get_opposite_column(ColKey col_key) const;
    ColKey find_opposite_column(ColKey col_key) const;

protected:
    /// Compare the objects of two tables under the assumption that the two tables
    /// have the same number of columns, and the same data type at each column
    /// index (as expressed through the DataType enum).
    bool compare_objects(const Table&) const;

    void check_lists_are_empty(size_t row_ndx) const;

private:
    mutable WrappedAllocator m_alloc;
    Array m_top;
    void update_allocator_wrapper(bool writable)
    {
        m_alloc.update_from_underlying_allocator(writable);
    }
    Spec m_spec;            // 1st slot in m_top
    ClusterTree m_clusters; // 3rd slot in m_top
    TableKey m_key;     // 4th slot in m_top
    Array m_index_refs; // 5th slot in m_top
    Array m_opposite_table;  // 7th slot in m_top
    Array m_opposite_column; // 8th slot in m_top
    std::vector<StringIndex*> m_index_accessors;
    ColKey m_primary_key_col;
    Replication* const* m_repl;
    static Replication* g_dummy_replication;
    bool m_is_frozen = false;
    TableRef m_own_ref;

    void batch_erase_rows(const KeyColumn& keys);
    size_t do_set_link(ColKey col_key, size_t row_ndx, size_t target_row_ndx);

    void populate_search_index(ColKey col_key);

    // Migration support
    void migrate_column_info();
    bool verify_column_keys();
    void migrate_indexes();
    void migrate_subspec();
    void create_columns();
    bool migrate_objects(ColKey pk_col_key); // Returns true if there are no links to migrate
    void migrate_links();
    void finalize_migration(ColKey pk_col_key);

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

    /// Create an uninitialized accessor whose lifetime is managed by Group
    Table(Replication* const* repl, Allocator&);
    void revive(Replication* const* repl, Allocator& new_allocator, bool writable);

    void init(ref_type top_ref, ArrayParent*, size_t ndx_in_parent, bool is_writable, bool is_frozen);

    void set_key(TableKey key);

    ColKey do_insert_column(ColKey col_key, DataType type, StringData name, LinkTargetInfo& link_target_info,
                            bool nullable = false, bool listtype = false, LinkType link_type = link_Weak);

    struct InsertSubtableColumns;
    struct EraseSubtableColumns;
    struct RenameSubtableColumns;

    ColKey insert_root_column(ColKey col_key, DataType type, StringData name, LinkTargetInfo& link_target,
                              bool nullable = false, bool linktype = false, LinkType link_type = link_Weak);
    void erase_root_column(ColKey col_key);
    ColKey do_insert_root_column(ColKey col_key, ColumnType, StringData name, bool nullable = false,
                                 bool listtype = false, LinkType link_type = link_Weak);
    void do_erase_root_column(ColKey col_key);
    ColKey insert_backlink_column(TableKey origin_table_key, ColKey origin_col_key, ColKey backlink_col_key);
    void erase_backlink_column(ColKey backlink_col_key);

    void set_opposite_column(ColKey col_key, TableKey opposite_table, ColKey opposite_column);
    void do_set_primary_key_column(ColKey col_key);
    void validate_column_is_unique(ColKey col_key) const;
    void rebuild_table_with_pk_column();

    ObjKey get_next_key();
    /// Some Object IDs are generated as a tuple of the client_file_ident and a
    /// local sequence number. This function takes the next number in the
    /// sequence for the given table and returns an appropriate globally unique
    /// GlobalKey.
    GlobalKey allocate_object_id_squeezed();

    /// Find the local 64-bit object ID for the provided global 128-bit ID.
    ObjKey global_to_local_object_id_hashed(GlobalKey global_id) const;

    /// After a local ObjKey collision has been detected, this function may be
    /// called to obtain a non-colliding local ObjKey in such a way that subsequent
    /// calls to global_to_local_object_id() will return the correct local ObjKey
    /// for both \a incoming_id and \a colliding_id.
    ObjKey allocate_local_id_after_hash_collision(GlobalKey incoming_id, GlobalKey colliding_id,
                                                  ObjKey colliding_local_id);
    /// Should be called when an object is deleted
    void free_local_id_after_hash_collision(ObjKey key);

    /// Called in the context of Group::commit() to ensure that
    /// attached table accessors stay valid across a commit. Please
    /// note that this works only for non-transactional commits. Table
    /// accessors obtained during a transaction are always detached
    /// when the transaction ends.
    void update_from_parent(size_t old_baseline) noexcept;

    // Detach accessor. This recycles the Table accessor and all subordinate
    // accessors become invalid.
    void detach() noexcept;
    void fully_detach() noexcept;

    ColumnType get_real_column_type(ColKey col_key) const noexcept;

    /// If this table is a group-level table, the parent group is returned,
    /// otherwise null is returned.
    Group* get_parent_group() const noexcept;
    uint64_t get_sync_file_id() const noexcept;

    static size_t get_size_from_ref(ref_type top_ref, Allocator&) noexcept;
    static size_t get_size_from_ref(ref_type spec_ref, ref_type columns_ref, Allocator&) noexcept;

    /// Create an empty table with independent spec and return just
    /// the reference to the underlying memory.
    static ref_type create_empty_table(Allocator&, TableKey = TableKey());

    void nullify_links(CascadeState&);
    void remove_recursive(CascadeState&);
    //@{

    /// Cascading removal of strong links.
    ///
    /// FIXME: Update this explanation
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

    void cascade_break_backlinks_to(size_t, CascadeState&)
    {
        REALM_ASSERT(false); // unimplemented
    }

    void cascade_break_backlinks_to_all_rows(CascadeState&)
    {
        REALM_ASSERT(false); // unimplemented
    }

    void remove_backlink_broken_rows(const CascadeState&)
    {
        REALM_ASSERT(false); // unimplemented
    }

    //@}

    /// Used by query. Follows chain of link columns and returns final target table
    const Table* get_link_chain_target(const std::vector<ColKey>&) const;

    Replication* get_repl() const noexcept;

    void set_ndx_in_parent(size_t ndx_in_parent) noexcept;

    /// Refresh the part of the accessor tree that is rooted at this
    /// table.
    void refresh_accessor_tree();
    void refresh_index_accessors();
    void refresh_content_version();
    void flush_for_commit();

    bool is_cross_table_link_target() const noexcept;
    template <Action action, typename T, typename R>
    R aggregate(ColKey col_key, T value = {}, size_t* resultcount = nullptr, ObjKey* return_ndx = nullptr) const;
    template <typename T>
    double average(ColKey col_key, size_t* resultcount) const;

    std::vector<ColKey> m_leaf_ndx2colkey;
    std::vector<ColKey::Idx> m_spec_ndx2leaf_ndx;
    std::vector<size_t> m_leaf_ndx2spec_ndx;

    uint64_t m_in_file_version_at_transaction_boundary = 0;

    static constexpr int top_position_for_spec = 0;
    static constexpr int top_position_for_columns = 1;
    static constexpr int top_position_for_cluster_tree = 2;
    static constexpr int top_position_for_key = 3;
    static constexpr int top_position_for_search_indexes = 4;
    static constexpr int top_position_for_column_key = 5;
    static constexpr int top_position_for_version = 6;
    static constexpr int top_position_for_opposite_table = 7;
    static constexpr int top_position_for_opposite_column = 8;
    static constexpr int top_position_for_sequence_number = 9;
    static constexpr int top_position_for_collision_map = 10;
    static constexpr int top_position_for_pk_col = 11;
    static constexpr int top_array_size = 12;

    enum { s_collision_map_lo = 0, s_collision_map_hi = 1, s_collision_map_local_id = 2, s_collision_map_num_slots };

    friend class SubtableNode;
    friend class _impl::TableFriend;
    friend class Query;
    friend class metrics::QueryInfo;
    template <class>
    friend class SimpleQuerySupport;
    friend class LangBindHelper;
    friend class ConstTableView;
    template <class T>
    friend class Columns;
    friend class Columns<StringData>;
    friend class ParentNode;
    friend struct util::serializer::SerialisationState;
    friend class LinksToNode;
    friend class LinkMap;
    friend class LinkView;
    friend class Group;
    friend class Transaction;
    friend class Cluster;
    friend class ClusterTree;
    friend class ColKeyIterator;
    friend class ConstObj;
    friend class Obj;
    friend class IncludeDescriptor;
};

class ColKeyIterator {
public:
    bool operator!=(const ColKeyIterator& other)
    {
        return m_pos != other.m_pos;
    }
    ColKeyIterator& operator++()
    {
        ++m_pos;
        return *this;
    }
    ColKeyIterator operator++(int)
    {
        ColKeyIterator tmp(m_table, m_pos);
        ++m_pos;
        return tmp;
    }
    ColKey operator*()
    {
        if (m_pos < m_table->get_column_count()) {
            REALM_ASSERT(m_table->m_spec.get_key(m_pos) == m_table->spec_ndx2colkey(m_pos));
            return m_table->m_spec.get_key(m_pos);
        }
        return {};
    }

private:
    friend class ColKeys;
    const Table* m_table;
    size_t m_pos;

    ColKeyIterator(const Table* t, size_t p)
        : m_table(t)
        , m_pos(p)
    {
    }
};

class ColKeys {
public:
    ColKeys(const Table* t)
        : m_table(t)
    {
    }

    ColKeys()
        : m_table(nullptr)
    {
    }

    size_t size() const
    {
        return m_table->get_column_count();
    }
    bool empty() const
    {
        return size() == 0;
    }
    ColKey operator[](size_t p) const
    {
        return ColKeyIterator(m_table, p).operator*();
    }
    ColKeyIterator begin() const
    {
        return ColKeyIterator(m_table, 0);
    }
    ColKeyIterator end() const
    {
        return ColKeyIterator(m_table, size());
    }

private:
    const Table* m_table;
};

// Class used to collect a chain of links when building up a Query following links.
// It has member functions corresponding to the ones defined on Table.
class LinkChain {
public:
    LinkChain(ConstTableRef t)
        : m_current_table(t.unchecked_ptr())
        , m_base_table(t)
    {
    }
    const Table* get_base_table()
    {
        return m_base_table.unchecked_ptr();
    }

    LinkChain& link(ColKey link_column)
    {
        add(link_column);
        return *this;
    }

    LinkChain& backlink(const Table& origin, ColKey origin_col_key)
    {
        auto backlink_col_key = origin.get_opposite_column(origin_col_key);
        return link(backlink_col_key);
    }


    template <class T>
    inline Columns<T> column(ColKey col_key)
    {
        m_current_table->report_invalid_key(col_key);

        // Check if user-given template type equals Realm type.
        auto ct = col_key.get_type();
        if (ct == col_type_LinkList)
            ct = col_type_Link;
        if (ct != ColumnTypeTraits<T>::column_id)
            throw LogicError(LogicError::type_mismatch);

        if (std::is_same<T, Link>::value || std::is_same<T, LnkLst>::value || std::is_same<T, BackLink>::value) {
            m_link_cols.push_back(col_key);
        }

        return Columns<T>(col_key, m_base_table, m_link_cols);
    }
    template <class T>
    Columns<T> column(const Table& origin, ColKey origin_col_key)
    {
        static_assert(std::is_same<T, BackLink>::value, "");

        auto backlink_col_key = origin.get_opposite_column(origin_col_key);
        m_link_cols.push_back(backlink_col_key);

        return Columns<T>(backlink_col_key, m_base_table, std::move(m_link_cols));
    }
    template <class T>
    SubQuery<T> column(ColKey col_key, Query subquery)
    {
        static_assert(std::is_same<T, Link>::value, "A subquery must involve a link list or backlink column");
        return SubQuery<T>(column<T>(col_key), std::move(subquery));
    }

    template <class T>
    SubQuery<T> column(const Table& origin, ColKey origin_col_key, Query subquery)
    {
        static_assert(std::is_same<T, BackLink>::value, "A subquery must involve a link list or backlink column");
        return SubQuery<T>(column<T>(origin, origin_col_key), std::move(subquery));
    }


    template <class T>
    BacklinkCount<T> get_backlink_count()
    {
        return BacklinkCount<T>(m_base_table, std::move(m_link_cols));
    }

private:
    friend class Table;

    std::vector<ColKey> m_link_cols;
    const Table* m_current_table;
    ConstTableRef m_base_table;

    void add(ColKey ck)
    {
        // Link column can be a single Link, LinkList, or BackLink.
        REALM_ASSERT(m_current_table->valid_column(ck));
        ColumnType type = ck.get_type();
        if (type == col_type_LinkList || type == col_type_Link || type == col_type_BackLink) {
            m_current_table = m_current_table->get_opposite_table(ck).unchecked_ptr();
        }
        else {
            // Only last column in link chain is allowed to be non-link
            throw(LogicError::type_mismatch);
        }
        m_link_cols.push_back(ck);
    }
};

// Implementation:

inline ColKeys Table::get_column_keys() const
{
    return ColKeys(this);
}

inline uint_fast64_t Table::get_content_version() const noexcept
{
    return m_alloc.get_content_version();
}

inline uint_fast64_t Table::get_instance_version() const noexcept
{
    return m_alloc.get_instance_version();
}


inline uint_fast64_t Table::get_storage_version(uint64_t instance_version) const
{
    return m_alloc.get_storage_version(instance_version);
}

inline uint_fast64_t Table::get_storage_version() const
{
    return m_alloc.get_storage_version();
}


inline TableKey Table::get_key() const noexcept
{
    return m_key;
}

inline void Table::bump_storage_version() const noexcept
{
    return m_alloc.bump_storage_version();
}

inline void Table::bump_content_version() const noexcept
{
    m_alloc.bump_content_version();
}



inline size_t Table::get_column_count() const noexcept
{
    return m_spec.get_public_column_count();
}

inline StringData Table::get_column_name(ColKey column_key) const
{
    auto spec_ndx = colkey2spec_ndx(column_key);
    REALM_ASSERT_3(spec_ndx, <, get_column_count());
    return m_spec.get_column_name(spec_ndx);
}

inline ColKey Table::get_column_key(StringData name) const noexcept
{
    size_t spec_ndx = m_spec.get_column_index(name);
    if (spec_ndx == npos)
        return ColKey();
    return spec_ndx2colkey(spec_ndx);
}

inline ColumnType Table::get_real_column_type(ColKey col_key) const noexcept
{
    return col_key.get_type();
}

inline DataType Table::get_column_type(ColKey column_key) const
{
    return DataType(column_key.get_type());
}

inline ColumnAttrMask Table::get_column_attr(ColKey column_key) const noexcept
{
    return column_key.get_attrs();
}


inline Table::Table(Allocator& alloc)
    : m_alloc(alloc)
    , m_top(m_alloc)
    , m_spec(m_alloc)
    , m_clusters(this, m_alloc)
    , m_index_refs(m_alloc)
    , m_opposite_table(m_alloc)
    , m_opposite_column(m_alloc)
    , m_repl(&g_dummy_replication)
    , m_own_ref(this, alloc.get_instance_version())
{
    m_spec.set_parent(&m_top, top_position_for_spec);
    m_index_refs.set_parent(&m_top, top_position_for_search_indexes);
    m_opposite_table.set_parent(&m_top, top_position_for_opposite_table);
    m_opposite_column.set_parent(&m_top, top_position_for_opposite_column);

    ref_type ref = create_empty_table(m_alloc); // Throws
    ArrayParent* parent = nullptr;
    size_t ndx_in_parent = 0;
    init(ref, parent, ndx_in_parent, true, false);
}

inline Table::Table(Replication* const* repl, Allocator& alloc)
    : m_alloc(alloc)
    , m_top(m_alloc)
    , m_spec(m_alloc)
    , m_clusters(this, m_alloc)
    , m_index_refs(m_alloc)
    , m_opposite_table(m_alloc)
    , m_opposite_column(m_alloc)
    , m_repl(repl)
    , m_own_ref(this, alloc.get_instance_version())
{
    m_spec.set_parent(&m_top, top_position_for_spec);
    m_index_refs.set_parent(&m_top, top_position_for_search_indexes);
    m_opposite_table.set_parent(&m_top, top_position_for_opposite_table);
    m_opposite_column.set_parent(&m_top, top_position_for_opposite_column);
}

inline void Table::revive(Replication* const* repl, Allocator& alloc, bool writable)
{
    m_alloc.switch_underlying_allocator(alloc);
    m_alloc.update_from_underlying_allocator(writable);
    m_repl = repl;
    m_own_ref = TableRef(this, m_alloc.get_instance_version());

    // since we're rebinding to a new table, we'll bump version counters
    // FIXME
    // this can be optimized if version counters are saved along with the
    // table data.
    bump_content_version();
    bump_storage_version();
    // we assume all other accessors are detached, so we're done.
}

inline Allocator& Table::get_alloc() const
{
    return m_alloc;
}

// For use by queries
template <class T>
inline Columns<T> Table::column(ColKey col_key) const
{
    LinkChain lc(m_own_ref);
    return lc.column<T>(col_key);
}

template <class T>
inline Columns<T> Table::column(const Table& origin, ColKey origin_col_key) const
{
    LinkChain lc(m_own_ref);
    return lc.column<T>(origin, origin_col_key);
}

template <class T>
inline BacklinkCount<T> Table::get_backlink_count() const
{
    return BacklinkCount<T>(this, {});
}

template <class T>
SubQuery<T> Table::column(ColKey col_key, Query subquery) const
{
    LinkChain lc(m_own_ref);
    return lc.column<T>(col_key, subquery);
}

template <class T>
SubQuery<T> Table::column(const Table& origin, ColKey origin_col_key, Query subquery) const
{
    LinkChain lc(m_own_ref);
    return lc.column<T>(origin, origin_col_key, subquery);
}

inline LinkChain Table::link(ColKey link_column) const
{
    LinkChain lc(m_own_ref);
    lc.add(link_column);

    return lc;
}

inline LinkChain Table::backlink(const Table& origin, ColKey origin_col_key) const
{
    auto backlink_col_key = origin.get_opposite_column(origin_col_key);
    return link(backlink_col_key);
}

inline bool Table::is_empty() const noexcept
{
    return size() == 0;
}

inline size_t Table::size() const noexcept
{
    return m_clusters.size();
}


inline ConstTableRef Table::get_link_target(ColKey col_key) const noexcept
{
    return const_cast<Table*>(this)->get_link_target(col_key);
}

inline bool Table::is_group_level() const noexcept
{
    return bool(get_parent_group());
}

inline bool Table::operator==(const Table& t) const
{
    return m_spec == t.m_spec && compare_objects(t); // Throws
}

inline bool Table::operator!=(const Table& t) const
{
    return !(*this == t); // Throws
}

inline size_t Table::get_size_from_ref(ref_type top_ref, Allocator& alloc) noexcept
{
    const char* top_header = alloc.translate(top_ref);
    std::pair<int_least64_t, int_least64_t> p = Array::get_two(top_header, 0);
    ref_type spec_ref = to_ref(p.first), columns_ref = to_ref(p.second);
    return get_size_from_ref(spec_ref, columns_ref, alloc);
}

inline bool Table::is_link_type(ColumnType col_type) noexcept
{
    return col_type == col_type_Link || col_type == col_type_LinkList;
}

inline Replication* Table::get_repl() const noexcept
{
    return *m_repl;
}

inline void Table::set_ndx_in_parent(size_t ndx_in_parent) noexcept
{
    REALM_ASSERT(m_top.is_attached());
    m_top.set_ndx_in_parent(ndx_in_parent);
}

inline size_t Table::colkey2spec_ndx(ColKey key) const
{
    auto leaf_idx = key.get_index();
    REALM_ASSERT(leaf_idx.val < m_leaf_ndx2spec_ndx.size());
    return m_leaf_ndx2spec_ndx[leaf_idx.val];
}

inline size_t Table::num_leaf_cols() const
{
    return m_leaf_ndx2spec_ndx.size();
}

inline ColKey Table::spec_ndx2colkey(size_t spec_ndx) const
{
    REALM_ASSERT(spec_ndx < m_spec_ndx2leaf_ndx.size());
    return m_leaf_ndx2colkey[m_spec_ndx2leaf_ndx[spec_ndx].val];
}

inline void Table::report_invalid_key(ColKey col_key) const
{
    if (col_key == ColKey())
        throw LogicError(LogicError::column_does_not_exist);
    auto idx = col_key.get_index();
    if (idx.val >= m_leaf_ndx2colkey.size() || m_leaf_ndx2colkey[idx.val] != col_key)
        throw LogicError(LogicError::column_does_not_exist);
}

inline size_t Table::leaf_ndx2spec_ndx(ColKey::Idx leaf_ndx) const
{
    REALM_ASSERT(leaf_ndx.val < m_leaf_ndx2colkey.size());
    return m_leaf_ndx2spec_ndx[leaf_ndx.val];
}

inline ColKey::Idx Table::spec_ndx2leaf_ndx(size_t spec_ndx) const
{
    REALM_ASSERT(spec_ndx < m_spec_ndx2leaf_ndx.size());
    return m_spec_ndx2leaf_ndx[spec_ndx];
}

inline ColKey Table::leaf_ndx2colkey(ColKey::Idx leaf_ndx) const
{
    // this may be called with leaf indicies outside of the table. This can happen
    // when a column is removed from the mapping, but space for it is still reserved
    // at leaf level. Operations on Cluster and ClusterTree which walks the columns
    // based on leaf indicies may ask for colkeys which are no longer valid.
    if (leaf_ndx.val < m_leaf_ndx2spec_ndx.size())
        return m_leaf_ndx2colkey[leaf_ndx.val];
    else
        return ColKey();
}

bool inline Table::valid_column(ColKey col_key) const noexcept
{
    if (col_key == ColKey())
        return false;
    ColKey::Idx leaf_idx = col_key.get_index();
    if (leaf_idx.val >= m_leaf_ndx2colkey.size())
        return false;
    return col_key == m_leaf_ndx2colkey[leaf_idx.val];
}

inline void Table::check_column(ColKey col_key) const
{
    if (REALM_UNLIKELY(!valid_column(col_key)))
        throw InvalidKey("No such column");
}

// This class groups together information about the target of a link column
// This is not a valid link if the target table == nullptr
struct LinkTargetInfo {
    LinkTargetInfo(Table* target = nullptr, ColKey backlink_key = ColKey())
        : m_target_table(target)
        , m_backlink_col_key(backlink_key)
    {
        if (backlink_key)
            m_target_table->report_invalid_key(backlink_key);
    }
    bool is_valid() const
    {
        return (m_target_table != nullptr);
    }
    Table* m_target_table;
    ColKey m_backlink_col_key; // a value of ColKey() indicates the backlink should be appended
};

// The purpose of this class is to give internal access to some, but
// not all of the non-public parts of the Table class.
class _impl::TableFriend {
public:
    static Spec& get_spec(Table& table) noexcept
    {
        return table.m_spec;
    }

    static const Spec& get_spec(const Table& table) noexcept
    {
        return table.m_spec;
    }

    static TableRef get_opposite_link_table(const Table& table, ColKey col_key);

    static Group* get_parent_group(const Table& table) noexcept
    {
        return table.get_parent_group();
    }

    static void remove_recursive(Table& table, CascadeState& rows)
    {
        table.remove_recursive(rows); // Throws
    }

    static void batch_erase_rows(Table& table, const KeyColumn& keys)
    {
        table.batch_erase_rows(keys); // Throws
    }
};

} // namespace realm

#endif // REALM_TABLE_HPP
