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

#ifndef REALM_LANG_BIND_HELPER_HPP
#define REALM_LANG_BIND_HELPER_HPP

#include <cstddef>

#include <realm/table.hpp>
#include <realm/table_view.hpp>
#include <realm/link_view.hpp>
#include <realm/group.hpp>
#include <realm/group_shared.hpp>

#include <realm/replication.hpp>

namespace realm {


/// These functions are only to be used by language bindings to gain
/// access to certain memebers that are othewise private.
///
/// \note Applications are not supposed to call any of these functions
/// directly.
///
/// All of the get_subtable_ptr() functions bind the table accessor pointer
/// before it is returned (bind_table_ptr()). The caller is then responsible for
/// making the corresponding call to unbind_table_ptr().
class LangBindHelper {
public:
    /// Increment the reference counter of the specified table accessor. This is
    /// done automatically by all of the functions in this class that return
    /// table accessor pointers, but if the binding/application makes a copy of
    /// such a pointer, and the copy needs to have an "independent life", then
    /// the binding/application must bind that copy using this function.
    static void bind_table_ptr(const Table*) noexcept;

    /// Decrement the reference counter of the specified table accessor. The
    /// binding/application must call this function for every bound table
    /// accessor pointer object, when that pointer object ends its life.
    static void unbind_table_ptr(const Table*) noexcept;

    /// Construct a new freestanding table. The table accessor pointer is bound
    /// by the callee before it is returned (bind_table_ptr()).
    static Table* new_table();

    /// Construct a new freestanding table as a copy of the specified one. The
    /// table accessor pointer is bound by the callee before it is returned
    /// (bind_table_ptr()).
    static Table* copy_table(const Table&);

    //@{

    /// These functions are like their namesakes in Group, but these bypass the
    /// construction of a smart-pointer object (TableRef). The table accessor
    /// pointer is bound by the callee before it is returned (bind_table_ptr()).

    static Table* get_table(Group&, size_t index_in_group);
    static const Table* get_table(const Group&, size_t index_in_group);

    static Table* get_table(Group&, StringData name);
    static const Table* get_table(const Group&, StringData name);

    static Table* add_table(Group&, StringData name, bool require_unique_name = true);
    static Table* get_or_add_table(Group&, StringData name, bool* was_added = nullptr);

    //@}

    static Table* get_subtable_ptr(Table*, size_t column_ndx, size_t row_ndx);
    static const Table* get_subtable_ptr(const Table*, size_t column_ndx, size_t row_ndx);

    // FIXME: This is an 'oddball', do we really need it? If we do,
    // please provide a comment that explains why it is needed!
    static Table* get_subtable_ptr_during_insert(Table*, size_t col_ndx, size_t row_ndx);

    static Table* get_subtable_ptr(TableView*, size_t column_ndx, size_t row_ndx);
    static const Table* get_subtable_ptr(const TableView*, size_t column_ndx, size_t row_ndx);
    static const Table* get_subtable_ptr(const ConstTableView*, size_t column_ndx, size_t row_ndx);

    /// Calls parent.set_mixed_subtable(col_ndx, row_ndx, &source). Note
    /// that the source table must have a descriptor that is
    /// compatible with the target subtable column.
    static void set_mixed_subtable(Table& parent, size_t col_ndx, size_t row_ndx, const Table& source);

    static const LinkViewRef& get_linklist_ptr(Row&, size_t col_ndx);
    static void unbind_linklist_ptr(const LinkViewRef&);

    using VersionID = SharedGroup::VersionID;

    /// \defgroup lang_bind_helper_transactions Continuous Transactions
    ///
    /// advance_read() is equivalent to terminating the current read transaction
    /// (SharedGroup::end_read()), and initiating a new one
    /// (SharedGroup::begin_read()), except that all subordinate accessors
    /// (Table, Row, Descriptor) will remain attached to the underlying objects,
    /// unless those objects were removed in the target snapshot. By default,
    /// the read transaction is advanced to the latest available snapshot, but
    /// see SharedGroup::begin_read() for information about \a version.
    ///
    /// promote_to_write() is equivalent to terminating the current read
    /// transaction (SharedGroup::end_read()), and initiating a new write
    /// transaction (SharedGroup::begin_write()), except that all subordinate
    /// accessors (Table, Row, Descriptor) will remain attached to the
    /// underlying objects, unless those objects were removed in the target
    /// snapshot.
    ///
    /// commit_and_continue_as_read() is equivalent to committing the current
    /// write transaction (SharedGroup::commit()) and initiating a new read
    /// transaction, which is bound to the snapshot produced by the write
    /// transaction (SharedGroup::begin_read()), except that all subordinate
    /// accessors (Table, Row, Descriptor) will remain attached to the
    /// underlying objects. commit_and_continue_as_read() returns the version
    /// produced by the committed transaction.
    ///
    /// rollback_and_continue_as_read() is equivalent to rolling back the
    /// current write transaction (SharedGroup::rollback()) and initiating a new
    /// read transaction, which is bound to the snapshot, that the write
    /// transaction was based on (SharedGroup::begin_read()), except that all
    /// subordinate accessors (Table, Row, Descriptor) will remain attached to
    /// the underlying objects, unless they were attached to object that were
    /// added during the rolled back transaction.
    ///
    /// If advance_read(), promote_to_write(), commit_and_continue_as_read(), or
    /// rollback_and_continue_as_read() throws, the associated group accessor
    /// and all of its subordinate accessors are left in a state that may not be
    /// fully consistent. Only minimal consistency is guaranteed (see
    /// AccessorConsistencyLevels). In this case, the application is required to
    /// either destroy the SharedGroup object, forcing all associated accessors
    /// to become detached, or take some other equivalent action that involves a
    /// complete accessor detachment, such as terminating the transaction in
    /// progress. Until then it is an error, and unsafe if the application
    /// attempts to access any of those accessors.
    ///
    /// The application must use SharedGroup::end_read() if it wants to
    /// terminate the transaction after advance_read() or promote_to_write() has
    /// thrown an exception. Likewise, it must use SharedGroup::rollback() if it
    /// wants to terminate the transaction after commit_and_continue_as_read()
    /// or rollback_and_continue_as_read() has thrown an exception.
    ///
    /// \param observer An optional custom replication instruction handler. The
    /// application may pass such a handler to observe the sequence of
    /// modifications that advances (or rolls back) the state of the Realm.
    ///
    /// \throw SharedGroup::BadVersion Thrown by advance_read() if the specified
    /// version does not correspond to a bound (or tethered) snapshot.
    ///
    //@{

    static void advance_read(SharedGroup&, VersionID = VersionID());
    template <class O>
    static void advance_read(SharedGroup&, O&& observer, VersionID = VersionID());
    static void promote_to_write(SharedGroup&);
    template <class O>
    static void promote_to_write(SharedGroup&, O&& observer);
    static SharedGroup::version_type commit_and_continue_as_read(SharedGroup&);
    static void rollback_and_continue_as_read(SharedGroup&);
    template <class O>
    static void rollback_and_continue_as_read(SharedGroup&, O&& observer);

    //@}

    /// Returns the name of the specified data type. Examples:
    ///
    /// <pre>
    ///
    ///   type_Int          ->  "int"
    ///   type_Bool         ->  "bool"
    ///   type_Float        ->  "float"
    ///   ...
    ///
    /// </pre>
    static const char* get_data_type_name(DataType) noexcept;

    static SharedGroup::version_type get_version_of_latest_snapshot(SharedGroup&);
};


// Implementation:

inline Table* LangBindHelper::new_table()
{
    typedef _impl::TableFriend tf;
    Allocator& alloc = Allocator::get_default();
    size_t ref = tf::create_empty_table(alloc); // Throws
    Table::Parent* parent = nullptr;
    size_t ndx_in_parent = 0;
    Table* table = tf::create_accessor(alloc, ref, parent, ndx_in_parent); // Throws
    bind_table_ptr(table);
    return table;
}

inline Table* LangBindHelper::copy_table(const Table& table)
{
    typedef _impl::TableFriend tf;
    Allocator& alloc = Allocator::get_default();
    size_t ref = tf::clone(table, alloc); // Throws
    Table::Parent* parent = nullptr;
    size_t ndx_in_parent = 0;
    Table* copy_of_table = tf::create_accessor(alloc, ref, parent, ndx_in_parent); // Throws
    bind_table_ptr(copy_of_table);
    return copy_of_table;
}

inline Table* LangBindHelper::get_subtable_ptr(Table* t, size_t column_ndx, size_t row_ndx)
{
    Table* subtab = t->get_subtable_ptr(column_ndx, row_ndx); // Throws
    subtab->bind_ptr();
    return subtab;
}

inline const Table* LangBindHelper::get_subtable_ptr(const Table* t, size_t column_ndx, size_t row_ndx)
{
    const Table* subtab = t->get_subtable_ptr(column_ndx, row_ndx); // Throws
    subtab->bind_ptr();
    return subtab;
}

inline Table* LangBindHelper::get_subtable_ptr(TableView* tv, size_t column_ndx, size_t row_ndx)
{
    return get_subtable_ptr(&tv->get_parent(), column_ndx, tv->get_source_ndx(row_ndx));
}

inline const Table* LangBindHelper::get_subtable_ptr(const TableView* tv, size_t column_ndx, size_t row_ndx)
{
    return get_subtable_ptr(&tv->get_parent(), column_ndx, tv->get_source_ndx(row_ndx));
}

inline const Table* LangBindHelper::get_subtable_ptr(const ConstTableView* tv, size_t column_ndx, size_t row_ndx)
{
    return get_subtable_ptr(&tv->get_parent(), column_ndx, tv->get_source_ndx(row_ndx));
}

inline Table* LangBindHelper::get_table(Group& group, size_t index_in_group)
{
    typedef _impl::GroupFriend gf;
    Table* table = &gf::get_table(group, index_in_group); // Throws
    table->bind_ptr();
    return table;
}

inline const Table* LangBindHelper::get_table(const Group& group, size_t index_in_group)
{
    typedef _impl::GroupFriend gf;
    const Table* table = &gf::get_table(group, index_in_group); // Throws
    table->bind_ptr();
    return table;
}

inline Table* LangBindHelper::get_table(Group& group, StringData name)
{
    typedef _impl::GroupFriend gf;
    Table* table = gf::get_table(group, name); // Throws
    if (table)
        table->bind_ptr();
    return table;
}

inline const Table* LangBindHelper::get_table(const Group& group, StringData name)
{
    typedef _impl::GroupFriend gf;
    const Table* table = gf::get_table(group, name); // Throws
    if (table)
        table->bind_ptr();
    return table;
}

inline Table* LangBindHelper::add_table(Group& group, StringData name, bool require_unique_name)
{
    typedef _impl::GroupFriend gf;
    Table* table = &gf::add_table(group, name, require_unique_name); // Throws
    table->bind_ptr();
    return table;
}

inline Table* LangBindHelper::get_or_add_table(Group& group, StringData name, bool* was_added)
{
    typedef _impl::GroupFriend gf;
    Table* table = &gf::get_or_add_table(group, name, was_added); // Throws
    table->bind_ptr();
    return table;
}

inline void LangBindHelper::unbind_table_ptr(const Table* t) noexcept
{
    t->unbind_ptr();
}

inline void LangBindHelper::bind_table_ptr(const Table* t) noexcept
{
    t->bind_ptr();
}

inline void LangBindHelper::set_mixed_subtable(Table& parent, size_t col_ndx, size_t row_ndx, const Table& source)
{
    parent.set_mixed_subtable(col_ndx, row_ndx, &source);
}

inline const LinkViewRef& LangBindHelper::get_linklist_ptr(Row& row, size_t col_ndx)
{
    LinkViewRef* link_view = new LinkViewRef(row.get_linklist(col_ndx));
    return *link_view;
}

inline void LangBindHelper::unbind_linklist_ptr(const LinkViewRef& link_view)
{
    delete (&link_view);
}

inline void LangBindHelper::advance_read(SharedGroup& sg, VersionID version)
{
    using sgf = _impl::SharedGroupFriend;
    _impl::NullInstructionObserver* observer = nullptr;
    sgf::advance_read(sg, observer, version); // Throws
}

template <class O>
inline void LangBindHelper::advance_read(SharedGroup& sg, O&& observer, VersionID version)
{
    using sgf = _impl::SharedGroupFriend;
    sgf::advance_read(sg, &observer, version); // Throws
}

inline void LangBindHelper::promote_to_write(SharedGroup& sg)
{
    using sgf = _impl::SharedGroupFriend;
    _impl::NullInstructionObserver* observer = nullptr;
    sgf::promote_to_write(sg, observer); // Throws
}

template <class O>
inline void LangBindHelper::promote_to_write(SharedGroup& sg, O&& observer)
{
    using sgf = _impl::SharedGroupFriend;
    sgf::promote_to_write(sg, &observer); // Throws
}

inline SharedGroup::version_type LangBindHelper::commit_and_continue_as_read(SharedGroup& sg)
{
    using sgf = _impl::SharedGroupFriend;
    return sgf::commit_and_continue_as_read(sg); // Throws
}

inline void LangBindHelper::rollback_and_continue_as_read(SharedGroup& sg)
{
    using sgf = _impl::SharedGroupFriend;
    _impl::NullInstructionObserver* observer = nullptr;
    sgf::rollback_and_continue_as_read(sg, observer); // Throws
}

template <class O>
inline void LangBindHelper::rollback_and_continue_as_read(SharedGroup& sg, O&& observer)
{
    using sgf = _impl::SharedGroupFriend;
    sgf::rollback_and_continue_as_read(sg, &observer); // Throws
}

inline SharedGroup::version_type LangBindHelper::get_version_of_latest_snapshot(SharedGroup& sg)
{
    using sgf = _impl::SharedGroupFriend;
    return sgf::get_version_of_latest_snapshot(sg); // Throws
}

} // namespace realm

#endif // REALM_LANG_BIND_HELPER_HPP
