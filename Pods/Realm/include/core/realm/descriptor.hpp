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

#ifndef REALM_DESCRIPTOR_HPP
#define REALM_DESCRIPTOR_HPP

#include <cstddef>

#include <realm/util/assert.hpp>
#include <realm/descriptor_fwd.hpp>
#include <realm/table.hpp>


namespace realm {

namespace _impl {
class DescriptorFriend;
}


/// Accessor for table type descriptors.
///
/// A table type descriptor is an entity that specifies the dynamic
/// type of a Realm table. Objects of this class are accessors
/// through which the descriptor can be inspected and
/// changed. Accessors can become detached, see is_attached() for more
/// on this. The descriptor itself is stored inside the database file,
/// or elsewhere in case of a free-standing table or a table in a
/// free-standing group.
///
/// The dynamic type consists first, and foremost of an ordered list
/// of column descriptors. Each column descriptor specifies the name
/// and type of the column.
///
/// When a table has a subtable column, every cell in than column
/// contains a subtable. All those subtables have the same dynamic
/// type, and therefore have a shared descriptor. See is_root() for
/// more on this.
///
/// The Table class contains convenience methods, such as
/// Table::get_column_count() and Table::add_column(), that allow you
/// to inspect and change the dynamic type of simple tables without
/// resorting to use of descriptors. For example, the following two
/// statements have the same effect:
///
///     table->add_column(type, name);
///     table->get_descriptor()->add_column(type, name);
///
/// Note, however, that this equivalence holds only as long as no
/// shared subtable descriptors are involved.
///
/// \sa Table::get_descriptor()
class Descriptor : public std::enable_shared_from_this<Descriptor> {
public:
    /// Get the number of columns in the associated tables.
    size_t get_column_count() const noexcept;

    /// Get the type of the column at the specified index.
    ///
    /// The consequences of specifying a column index that is out of
    /// range, are undefined.
    DataType get_column_type(size_t column_ndx) const noexcept;

    /// Get the name of the column at the specified index.
    ///
    /// The consequences of specifying a column index that is out of
    /// range, are undefined.
    StringData get_column_name(size_t column_ndx) const noexcept;

    /// Search for a column with the specified name.
    ///
    /// This function finds the first column with the specified name,
    /// and returns its index. If there are no such columns, it
    /// returns `not_found`.
    size_t get_column_index(StringData name) const noexcept;

    /// Get the index of the column to which links in the column at the
    /// specified index refer.
    ///
    /// The consequences of specifying a column index that is out of
    /// range, are undefined.
    ///
    /// The consequences of specifying a column index that does not refer
    /// to a link column, are undefined.
    size_t get_column_link_target(size_t column_ndx) const noexcept;

    /// Get whether or not the specified column is nullable.
    ///
    /// The consequences of specifying a column index that is out of
    /// range, are undefined.
    bool is_nullable(size_t column_ndx) const noexcept;

    /// \defgroup descriptor_column_accessors Accessing Columns Via A Descriptor
    ///
    /// add_column() and add_column_link() are a shorthands for calling
    /// insert_column() and insert_column_link(), respectively, with a column
    /// index equal to the original number of columns. The returned value is
    /// that column index.
    ///
    /// insert_column() inserts a new column into all the tables associated with
    /// this descriptor. If any of the tables are not empty, the new column will
    /// be filled with the default value associated with the specified data
    /// type. This function cannot be used to insert link-type columns. For
    /// that, you have to use insert_column_link() instead.
    ///
    /// This function modifies the dynamic type of all the tables that
    /// share this descriptor. It does this by inserting a new column
    /// with the specified name and type into the descriptor at the
    /// specified index, and into each of the tables that share this
    /// descriptor.
    ///
    /// insert_column_link() is like insert_column(), but inserts a link-type
    /// column to a group-level table. It is not possible to add link-type
    /// columns to tables that are not group-level tables. This functions must
    /// be used in place of insert_column() when the column type is `type_Link`
    /// or `type_LinkList`. A link-type column is associated with a particular
    /// target table. All links in a link-type column refer to rows in the
    /// target table of that column. The target table must also be a group-level
    /// table, and it must belong to the same group as the origin table.
    ///
    /// \param name Name of new column. All strings are valid column names as
    /// long as they are valid UTF-8 encodings and the number of bytes does not
    /// exceed `max_column_name_length`. An attempt to add a column with a name
    /// that is longer than `max_column_name_length` will cause an exception to
    /// be thrown.
    ///
    /// \param subdesc If a non-null pointer is passed, and the
    /// specified type is `type_Table`, then this function
    /// automatically retrieves the descriptor associated with the new
    /// subtable column, and stores a reference to its accessor in
    /// `*subdesc`.
    ///
    /// \param col_ndx Insert the new column at this index. Preexisting columns
    /// at indexes equal to, or greater than `col_ndx` will be shifted to the
    /// next higher index. It is an error to specify an index that is greater
    /// than the number of columns prior to the insertion.
    ///
    /// \param link_type See set_link_type().
    ///
    /// \sa Table::add_column()
    /// \sa Table::insert_column()
    /// \sa Table::add_column_link()
    /// \sa Table::insert_column_link()
    /// \sa is_root()
    //@{

    static const size_t max_column_name_length = 63;

    size_t add_column(DataType type, StringData name, DescriptorRef* subdesc = nullptr, bool nullable = false);

    void insert_column(size_t col_ndx, DataType type, StringData name, DescriptorRef* subdesc = nullptr,
                       bool nullable = false);

    size_t add_column_link(DataType type, StringData name, Table& target, LinkType = link_Weak);
    void insert_column_link(size_t col_ndx, DataType type, StringData name, Table& target, LinkType = link_Weak);
    //@}

    /// Remove the specified column from each of the associated
    /// tables. If the removed column is the only column in the
    /// descriptor, then the table size will drop to zero for all
    /// tables that were not already empty.
    ///
    /// This function modifies the dynamic type of all the tables that
    /// share this descriptor. It does this by removing the column at
    /// the specified index from the descriptor, and from each of the
    /// tables that share this descriptor. The consequences of
    /// specifying a column index that is out of range, are undefined.
    ///
    /// If the removed column was a subtable column, then the
    /// associated descriptor accessor will be detached, if it
    /// exists. This function will also detach all accessors of
    /// subtables of the root table. Only the accessor of the root
    /// table will remain attached. The root table is the table
    /// associated with the root descriptor.
    ///
    /// \param col_ndx The index of the column to be removed. It is an error to
    /// specify an index that is greater than, or equal to the number of
    /// columns.
    ///
    /// \sa is_root()
    /// \sa Table::remove_column()
    void remove_column(size_t col_ndx);

    /// Rename the specified column.
    ///
    /// This function modifies the dynamic type of all the tables that
    /// share this descriptor. The consequences of specifying a column
    /// index that is out of range, are undefined.
    ///
    /// This function will detach all accessors of subtables of the
    /// root table. Only the accessor of the root table will remain
    /// attached. The root table is the table associated with the root
    /// descriptor.
    ///
    /// \param col_ndx The index of the column to be renamed. It is an error to
    /// specify an index that is greater than, or equal to the number of
    /// columns.
    ///
    /// \param new_name The new name of the column.
    ///
    /// \sa is_root()
    /// \sa Table::rename_column()
    void rename_column(size_t col_ndx, StringData new_name);

    /// There are two kinds of links, 'weak' and 'strong'. A strong link is one
    /// that implies ownership, i.e., that the origin row (parent) owns the
    /// target row (child). Simply stated, this means that when the origin row
    /// (parent) is removed, so is the target row (child). If there are multiple
    /// strong links to a target row, the origin rows share ownership, and the
    /// target row is removed when the last owner disappears. Weak links do not
    /// imply ownership, and will be nullified or removed when the target row
    /// disappears.
    ///
    /// To put this in precise terms; when a strong link is broken, and the
    /// target row has no other strong links to it, the target row is removed. A
    /// row that is implicitly removed in this way, is said to be
    /// *cascade-removed*. When a weak link is broken, nothing is
    /// cascade-removed.
    ///
    /// A link is considered broken if
    ///
    ///  - the link is nullified, removed, or replaced by a different link
    ///    (Row::nullify_link(), Row::set_link(), LinkView::remove_link(),
    ///    LinkView::set_link(), LinkView::clear()), or if
    ///
    ///  - the origin row is explicitly removed (Row::move_last_over(),
    ///    Table::clear()), or if
    ///
    ///  - the origin row is cascade-removed, or if
    ///
    ///  - the origin column is removed from the table (Table::remove_column()),
    ///    or if
    ///
    ///  - the origin table is removed from the group.
    ///
    /// Note that a link is *not* considered broken when it is replaced by a
    /// link to the same target row. I.e., no no rows will be cascade-removed
    /// due to such an operation.
    ///
    /// When a row is explicitly removed (such as by Table::move_last_over()),
    /// all links to it are automatically removed or nullified. For single link
    /// columns (type_Link), links to the removed row are nullified. For link
    /// list columns (type_LinkList), links to the removed row are removed from
    /// the list.
    ///
    /// When a row is cascade-removed there can no longer be any strong links to
    /// it, but if there are any weak links, they will be removed or nullified.
    ///
    /// It is important to understand that this cascade-removal scheme is too
    /// simplistic to enable detection and removal of orphaned link-cycles. In
    /// this respect, it suffers from the same limitations as a reference
    /// counting scheme generally does.
    ///
    /// It is also important to understand, that the possible presence of a link
    /// cycle can cause a row to be cascade-removed as a consequence of being
    /// modified. This happens, for example, if two rows, A and B, have strong
    /// links to each other, and there are no other strong links to either of
    /// them. In this case, if A->B is changed to A->C, then both A and B will
    /// be cascade-removed. This can lead to obscure bugs in some applications,
    /// such as in the following case:
    ///
    ///     table.set_link(col_ndx_1, row_ndx, ...);
    ///     table.set_int(col_ndx_2, row_ndx, ...); // Oops, `row_ndx` may no longer refer to the same row
    ///
    /// To be safe, applications, that may encounter cycles, are advised to
    /// adopt the following pattern:
    ///
    ///     Row row = table[row_ndx];
    ///     row.set_link(col_ndx_1, ...);
    ///     if (row)
    ///         row.set_int(col_ndx_2, ...); // Ok, because we check whether the row has disappeared
    ///
    /// \param col_ndx The index of the link column (`type_Link` or
    /// `type_LinkList`) to be modified. It is an error to specify an index that
    /// is greater than, or equal to the number of columns, or to specify the
    /// index of a non-link column.
    ///
    /// \param link_type The type of links the column should store.
    void set_link_type(size_t col_ndx, LinkType link_type);

    //@{
    /// Get the descriptor for the specified subtable column.
    ///
    /// This function provides access to the shared subtable
    /// descriptor for the subtables in the specified column. The
    /// specified column must be a column whose type is 'table'. The
    /// consequences of specifying a column of a different type, or
    /// specifying an index that is out of range, are undefined.
    ///
    /// Note that this function cannot be used with 'mixed' columns,
    /// since subtables of that kind have independent dynamic types,
    /// and therefore, have independent descriptors. You can only get
    /// access to the descriptor of a subtable in a mixed column by
    /// first getting access to the subtable itself.
    ///
    /// \sa is_root()
    DescriptorRef get_subdescriptor(size_t column_ndx);
    ConstDescriptorRef get_subdescriptor(size_t column_ndx) const;
    //@}

    //@{
    /// Returns the parent table descriptor, if any.
    ///
    /// If this descriptor is the *root descriptor*, then this
    /// function returns null. Otherwise it returns the accessor of
    /// the parent descriptor.
    ///
    /// \sa is_root()
    DescriptorRef get_parent() noexcept;
    ConstDescriptorRef get_parent() const noexcept;
    //@}

    //@{
    /// Get the table associated with the root descriptor.
    ///
    /// \sa get_parent()
    /// \sa is_root()
    TableRef get_root_table() noexcept;
    ConstTableRef get_root_table() const noexcept;
    //@}

    //@{
    /// Get the target table associated with the specified link column. This
    /// descriptor must be a root descriptor (is_root()), and the specified
    /// column must be a link column (`type_Link` or `type_LinkList`).
    TableRef get_link_target(size_t col_ndx) noexcept;
    ConstTableRef get_link_target(size_t col_ndx) const noexcept;
    //@}

    /// Is this a root descriptor?
    ///
    /// Descriptors of tables with independent dynamic type are root
    /// descriptors. Root descriptors are never shared. Tables that
    /// are direct members of groups have independent dynamic
    /// types. The same is true for free-standing tables and subtables
    /// in columns of type 'mixed'.
    ///
    /// When a table has a column of type 'table', the cells in that
    /// column contain subtables. All those subtables have the same
    /// dynamic type, and they share a single dynamic type
    /// descriptor. Such shared descriptors are never root
    /// descriptors.
    ///
    /// A type descriptor can even be shared by subtables with
    /// different parent tables, but only if the parent tables
    /// themselves have a shared type descriptor. For example, if a
    /// table has a column `foo` of type 'table', and each of the
    /// subtables in `foo` has a column `bar` of type 'table', then
    /// all the subtables in all the `bar` columns share the same
    /// dynamic type descriptor.
    ///
    /// \sa Table::has_shared_type()
    bool is_root() const noexcept;

    /// Determine whether this accessor is still attached.
    ///
    /// A table descriptor accessor may get detached from the
    /// underlying descriptor for various reasons (see below). When it
    /// does, it no longer refers to that descriptor, and can no
    /// longer be used, except for calling is_attached(). The
    /// consequences of calling other methods on a detached accessor
    /// are undefined. Descriptor accessors obtained by calling
    /// functions in the Realm API are always in the 'attached'
    /// state immediately upon return from those functions.
    ///
    /// A descriptor accessor that is obtained directly from a table
    /// becomes detached if the table becomes detached. A shared
    /// subtable descriptor accessor that is obtained by a call to
    /// get_subdescriptor() becomes detached if the parent descriptor
    /// accessor becomes detached, or if the corresponding subtable
    /// column is removed. A descriptor accessor does not get detached
    /// under any other circumstances.
    bool is_attached() const noexcept;

    //@{
    /// \brief Compare two table descriptors.
    ///
    /// Two table descriptors are equal if they have the same number of columns,
    /// and for each column index, the two columns have the same name, data
    /// type, and set of attributes.
    ///
    /// For link columns (`type_Link` and `type_LinkList`), the target table
    /// (get_link_target()) of the two columns must be the same.
    ///
    /// For subtable columns (`type_Table`), the two corresponding
    /// subdescriptors must themselves be equal, as if by a recursive call to
    /// operator==().
    ///
    /// The consequences of comparing a detached descriptor are
    /// undefined.
    bool operator==(const Descriptor&) const noexcept;
    bool operator!=(const Descriptor&) const noexcept;
    //@}

    /// If the specified column is optimized to store only unique values, then
    /// this function returns the number of unique values currently
    /// stored. Otherwise it returns zero. This function is mainly intended for
    /// debugging purposes.
    size_t get_num_unique_values(size_t column_ndx) const;

    ~Descriptor() noexcept;

private:
    // for initialization through make_shared
    struct PrivateTag {
    };

public:
    Descriptor(const PrivateTag&)
        : Descriptor()
    {
    }

private:
    // Table associated with root descriptor. Detached iff null.
    TableRef m_root_table;
    DescriptorRef m_parent; // Null iff detached or root descriptor.
    Spec* m_spec;           // Valid if attached. Owned iff valid and `m_parent`.

    // Whenever a subtable descriptor accessor is created, it is
    // stored in this map. This ensures that when get_subdescriptor()
    // is called to created multiple DescriptorRef objects that
    // overlap in time, then they will all refer to the same
    // descriptor object.
    //
    // It also enables the necessary recursive detaching of descriptor
    // objects.
    struct subdesc_entry {
        size_t m_column_ndx;
        std::weak_ptr<Descriptor> m_subdesc;
        subdesc_entry(size_t column_ndx, DescriptorRef);
    };
    typedef std::vector<subdesc_entry> subdesc_map;
    mutable subdesc_map m_subdesc_map;

    Descriptor() noexcept;

    // Called by the root table if this becomes the root
    // descriptor. Otherwise it is called by the descriptor that
    // becomes its parent.
    //
    // Puts this descriptor accessor into the attached state. This
    // attaches it to the underlying structure of array nodes. It does
    // not establish the parents reference to this descriptor, that is
    // the job of the parent. When this function returns,
    // is_attached() will return true.
    //
    // Not idempotent.
    //
    // The specified table is not allowed to be a subtable with a
    // shareable spec. That is, Table::has_shared_spec() must return
    // false.
    //
    // The specified spec must be the spec of the specified table or
    // of one of its direct or indirect subtable columns.
    //
    // When the specified spec is the spec of the root table, the
    // parent must be specified as null. When the specified spec is
    // not the root spec, a proper parent must be specified.
    void attach(Table*, DescriptorRef parent, Spec*) noexcept;

    // Detach accessor from underlying descriptor. Caller must ensure
    // that a reference count exists upon return, for example by
    // obtaining an extra reference count before the call.
    //
    // This function is called either by the root table if this is the
    // root descriptor, or by the parent descriptor, if it is not.
    //
    // Puts this descriptor accessor into the detached state. This
    // detaches it from the underlying structure of array nodes. It
    // also calls detach_subdesc_accessors(). When this function
    // returns, is_attached() will return false.
    //
    // Not idempotent.
    void detach() noexcept;

    // Recursively detach all subtable descriptor accessors that
    // exist, that is, all subtable descriptor accessors that have
    // this descriptor as ancestor.
    void detach_subdesc_accessors() noexcept;

    // Record the path in terms of subtable column indexes from the
    // root descriptor to this descriptor. If this descriptor is a
    // root descriptor, the path is empty. Returns zero if the path is
    // too long to fit in the specified buffer. Otherwise the path
    // indexes will be stored between `begin_2`and `end`, where
    // `begin_2` is the returned pointer.
    size_t* record_subdesc_path(size_t* begin, size_t* end) const noexcept;

    // Returns a pointer to the accessor of the specified
    // subdescriptor if that accessor exists, otherwise this function
    // return null.
    DescriptorRef get_subdesc_accessor(size_t column_ndx) noexcept;

    void move_column(size_t from_ndx, size_t to_ndx);

    void adj_insert_column(size_t col_ndx) noexcept;
    void adj_erase_column(size_t col_ndx) noexcept;
    void adj_move_column(size_t col_ndx_1, size_t col_ndx_2) noexcept;

    friend class util::bind_ptr<Descriptor>;
    friend class util::bind_ptr<const Descriptor>;
    friend class _impl::DescriptorFriend;
};


// Implementation:

inline size_t Descriptor::get_column_count() const noexcept
{
    REALM_ASSERT(is_attached());
    return m_spec->get_public_column_count();
}

inline StringData Descriptor::get_column_name(size_t ndx) const noexcept
{
    REALM_ASSERT(is_attached());
    return m_spec->get_column_name(ndx);
}

inline DataType Descriptor::get_column_type(size_t ndx) const noexcept
{
    REALM_ASSERT(is_attached());
    return m_spec->get_public_column_type(ndx);
}

inline bool Descriptor::is_nullable(size_t ndx) const noexcept
{
    REALM_ASSERT(is_attached());
    return m_spec->get_column_attr(ndx) & col_attr_Nullable;
}

inline size_t Descriptor::get_column_index(StringData name) const noexcept
{
    REALM_ASSERT(is_attached());
    return m_spec->get_column_index(name);
}

inline size_t Descriptor::get_column_link_target(size_t column_ndx) const noexcept
{
    REALM_ASSERT(is_attached());
    return m_spec->get_opposite_link_table_ndx(column_ndx);
}

inline size_t Descriptor::add_column(DataType type, StringData name, DescriptorRef* subdesc, bool nullable)
{
    size_t col_ndx = m_spec->get_public_column_count();
    insert_column(col_ndx, type, name, subdesc, nullable); // Throws
    return col_ndx;
}

inline void Descriptor::insert_column(size_t col_ndx, DataType type, StringData name, DescriptorRef* subdesc,
                                      bool nullable)
{
    typedef _impl::TableFriend tf;

    if (REALM_UNLIKELY(!is_attached()))
        throw LogicError(LogicError::detached_accessor);
    if (REALM_UNLIKELY(col_ndx > get_column_count()))
        throw LogicError(LogicError::column_index_out_of_range);
    if (REALM_UNLIKELY(tf::is_link_type(ColumnType(type))))
        throw LogicError(LogicError::illegal_type);

    LinkTargetInfo invalid_link;
    tf::insert_column(*this, col_ndx, type, name, invalid_link, nullable); // Throws
    adj_insert_column(col_ndx);
    if (subdesc && type == type_Table)
        *subdesc = get_subdescriptor(col_ndx);
}

inline size_t Descriptor::add_column_link(DataType type, StringData name, Table& target, LinkType link_type)
{
    size_t col_ndx = m_spec->get_public_column_count();
    insert_column_link(col_ndx, type, name, target, link_type); // Throws
    return col_ndx;
}

inline void Descriptor::insert_column_link(size_t col_ndx, DataType type, StringData name, Table& target,
                                           LinkType link_type)
{
    typedef _impl::TableFriend tf;

    if (REALM_UNLIKELY(!is_attached() || !target.is_attached()))
        throw LogicError(LogicError::detached_accessor);
    if (REALM_UNLIKELY(col_ndx > get_column_count()))
        throw LogicError(LogicError::column_index_out_of_range);
    if (REALM_UNLIKELY(!tf::is_link_type(ColumnType(type))))
        throw LogicError(LogicError::illegal_type);
    if (REALM_UNLIKELY(!is_root()))
        throw LogicError(LogicError::wrong_kind_of_descriptor);
    // Both origin and target must be group-level tables, and in the same group.
    Group* origin_group = tf::get_parent_group(*get_root_table());
    Group* target_group = tf::get_parent_group(target);
    if (!origin_group || !target_group)
        throw LogicError(LogicError::wrong_kind_of_table);
    if (origin_group != target_group)
        throw LogicError(LogicError::group_mismatch);

    LinkTargetInfo link(&target);
    tf::insert_column(*this, col_ndx, type, name, link); // Throws
    adj_insert_column(col_ndx);

    tf::set_link_type(*get_root_table(), col_ndx, link_type); // Throws
}

inline void Descriptor::remove_column(size_t col_ndx)
{
    typedef _impl::TableFriend tf;

    if (REALM_UNLIKELY(!is_attached()))
        throw LogicError(LogicError::detached_accessor);
    if (REALM_UNLIKELY(col_ndx >= get_column_count()))
        throw LogicError(LogicError::column_index_out_of_range);

    tf::erase_column(*this, col_ndx); // Throws
    adj_erase_column(col_ndx);
}

inline void Descriptor::rename_column(size_t col_ndx, StringData name)
{
    typedef _impl::TableFriend tf;

    if (REALM_UNLIKELY(!is_attached()))
        throw LogicError(LogicError::detached_accessor);
    if (REALM_UNLIKELY(col_ndx >= get_column_count()))
        throw LogicError(LogicError::column_index_out_of_range);

    tf::rename_column(*this, col_ndx, name); // Throws
}

inline void Descriptor::move_column(size_t from_ndx, size_t to_ndx)
{
    REALM_ASSERT(is_attached());
    typedef _impl::TableFriend tf;
    tf::move_column(*this, from_ndx, to_ndx); // Throws
    adj_move_column(from_ndx, to_ndx);
}

inline void Descriptor::set_link_type(size_t col_ndx, LinkType link_type)
{
    typedef _impl::TableFriend tf;

    if (REALM_UNLIKELY(!is_attached()))
        throw LogicError(LogicError::detached_accessor);
    if (REALM_UNLIKELY(col_ndx >= get_column_count()))
        throw LogicError(LogicError::column_index_out_of_range);
    if (REALM_UNLIKELY(!tf::is_link_type(ColumnType(get_column_type(col_ndx)))))
        throw LogicError(LogicError::illegal_type);

    tf::set_link_type(*get_root_table(), col_ndx, link_type); // Throws
}

inline ConstDescriptorRef Descriptor::get_subdescriptor(size_t column_ndx) const
{
    return const_cast<Descriptor*>(this)->get_subdescriptor(column_ndx);
}

inline DescriptorRef Descriptor::get_parent() noexcept
{
    return m_parent;
}

inline ConstDescriptorRef Descriptor::get_parent() const noexcept
{
    return const_cast<Descriptor*>(this)->get_parent();
}

inline TableRef Descriptor::get_root_table() noexcept
{
    return m_root_table;
}

inline ConstTableRef Descriptor::get_root_table() const noexcept
{
    return const_cast<Descriptor*>(this)->get_root_table();
}

inline TableRef Descriptor::get_link_target(size_t col_ndx) noexcept
{
    REALM_ASSERT(is_attached());
    REALM_ASSERT(is_root());
    return get_root_table()->get_link_target(col_ndx);
}

inline ConstTableRef Descriptor::get_link_target(size_t col_ndx) const noexcept
{
    REALM_ASSERT(is_attached());
    REALM_ASSERT(is_root());
    return get_root_table()->get_link_target(col_ndx);
}

inline bool Descriptor::is_root() const noexcept
{
    return !m_parent;
}

inline Descriptor::Descriptor() noexcept
{
}

inline void Descriptor::attach(Table* table, DescriptorRef parent, Spec* spec) noexcept
{
    REALM_ASSERT(!is_attached());
    REALM_ASSERT(!table->has_shared_type());
    m_root_table.reset(table);
    m_parent = parent;
    m_spec = spec;
}

inline bool Descriptor::is_attached() const noexcept
{
    return bool(m_root_table);
}

inline Descriptor::subdesc_entry::subdesc_entry(size_t n, DescriptorRef d)
    : m_column_ndx(n)
    , m_subdesc(d)
{
}

inline bool Descriptor::operator==(const Descriptor& d) const noexcept
{
    REALM_ASSERT(is_attached());
    REALM_ASSERT(d.is_attached());
    return *m_spec == *d.m_spec;
}

inline bool Descriptor::operator!=(const Descriptor& d) const noexcept
{
    return !(*this == d);
}

// The purpose of this class is to give internal access to some, but
// not all of the non-public parts of the Descriptor class.
class _impl::DescriptorFriend {
public:
    static DescriptorRef create()
    {
        return std::make_shared<Descriptor>(Descriptor::PrivateTag()); // Throws
    }

    static void attach(Descriptor& desc, Table* table, DescriptorRef parent, Spec* spec) noexcept
    {
        desc.attach(table, parent, spec);
    }

    static void detach(Descriptor& desc) noexcept
    {
        desc.detach();
    }

    static Table& get_root_table(Descriptor& desc) noexcept
    {
        return *desc.m_root_table;
    }

    static const Table& get_root_table(const Descriptor& desc) noexcept
    {
        return *desc.m_root_table;
    }

    static Spec& get_spec(Descriptor& desc) noexcept
    {
        return *desc.m_spec;
    }

    static const Spec& get_spec(const Descriptor& desc) noexcept
    {
        return *desc.m_spec;
    }

    static size_t* record_subdesc_path(const Descriptor& desc, size_t* begin, size_t* end) noexcept
    {
        return desc.record_subdesc_path(begin, end);
    }

    static DescriptorRef get_subdesc_accessor(Descriptor& desc, size_t column_ndx) noexcept
    {
        return desc.get_subdesc_accessor(column_ndx);
    }

    static void move_column(Descriptor& desc, size_t from_ndx, size_t to_ndx)
    {
        return desc.move_column(from_ndx, to_ndx);
    }

    static void adj_insert_column(Descriptor& desc, size_t col_ndx) noexcept
    {
        desc.adj_insert_column(col_ndx);
    }

    static void adj_erase_column(Descriptor& desc, size_t col_ndx) noexcept
    {
        desc.adj_erase_column(col_ndx);
    }

    static void adj_move_column(Descriptor& desc, size_t col_ndx_1, size_t col_ndx_2) noexcept
    {
        desc.adj_move_column(col_ndx_1, col_ndx_2);
    }
};

} // namespace realm

#endif // REALM_DESCRIPTOR_HPP
