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

#ifndef REALM_GROUP_SHARED_HPP
#define REALM_GROUP_SHARED_HPP

#ifdef REALM_DEBUG
#include <ctime> // usleep()
#endif

#include <functional>
#include <limits>
#include <realm/util/features.h>
#include <realm/util/thread.hpp>
#ifndef _WIN32
#include <realm/util/interprocess_condvar.hpp>
#endif
#include <realm/util/interprocess_mutex.hpp>
#include <realm/group.hpp>
#include <realm/group_shared_options.hpp>
#include <realm/handover_defs.hpp>
#include <realm/impl/transact_log.hpp>
#include <realm/replication.hpp>
#include <realm/version_id.hpp>

namespace realm {

namespace _impl {
class SharedGroupFriend;
class WriteLogCollector;
}

/// Thrown by SharedGroup::open() if the lock file is already open in another
/// process which can't share mutexes with this process
struct IncompatibleLockFile : std::runtime_error {
    IncompatibleLockFile(const std::string& msg)
        : std::runtime_error("Incompatible lock file. " + msg)
    {
    }
};


/// A SharedGroup facilitates transactions.
///
/// When multiple threads or processes need to access a database
/// concurrently, they must do so using transactions. By design,
/// Realm does not allow for multiple threads (or processes) to
/// share a single instance of SharedGroup. Instead, each concurrently
/// executing thread or process must use a separate instance of
/// SharedGroup.
///
/// Each instance of SharedGroup manages a single transaction at a
/// time. That transaction can be either a read transaction, or a
/// write transaction.
///
/// Utility classes ReadTransaction and WriteTransaction are provided
/// to make it safe and easy to work with transactions in a scoped
/// manner (by means of the RAII idiom). However, transactions can
/// also be explicitly started (begin_read(), begin_write()) and
/// stopped (end_read(), commit(), rollback()).
///
/// If a transaction is active when the SharedGroup is destroyed, that
/// transaction is implicitly terminated, either by a call to
/// end_read() or rollback().
///
/// Two processes that want to share a database file must reside on
/// the same host.
///
///
/// Desired exception behavior (not yet fully implemented)
/// ------------------------------------------------------
///
///  - If any data access API function throws an unexpected exception during a
///    read transaction, the shared group accessor is left in state "error
///    during read".
///
///  - If any data access API function throws an unexpected exception during a
///    write transaction, the shared group accessor is left in state "error
///    during write".
///
///  - If SharedGroup::begin_write() or SharedGroup::begin_read() throws an
///    unexpected exception, the shared group accessor is left in state "no
///    transaction in progress".
///
///  - SharedGroup::end_read() and SharedGroup::rollback() do not throw.
///
///  - If SharedGroup::commit() throws an unexpected exception, the shared group
///    accessor is left in state "error during write" and the transaction was
///    not committed.
///
///  - If SharedGroup::advance_read() or SharedGroup::promote_to_write() throws
///    an unexpected exception, the shared group accessor is left in state
///    "error during read".
///
///  - If SharedGroup::commit_and_continue_as_read() or
///    SharedGroup::rollback_and_continue_as_read() throws an unexpected
///    exception, the shared group accessor is left in state "error during
///    write".
///
/// It has not yet been decided exactly what an "unexpected exception" is, but
/// `std::bad_alloc` is surely one example. On the other hand, an expected
/// exception is one that is mentioned in the function specific documentation,
/// and is used to abort an operation due to a special, but expected condition.
///
/// States
/// ------
///
///  - A newly created shared group accessor is in state "no transaction in
///    progress".
///
///  - In state "error during read", almost all Realm API functions are
///    illegal on the connected group of accessors. The only valid operations
///    are destruction of the shared group, and SharedGroup::end_read(). If
///    SharedGroup::end_read() is called, the new state becomes "no transaction
///    in progress".
///
///  - In state "error during write", almost all Realm API functions are
///    illegal on the connected group of accessors. The only valid operations
///    are destruction of the shared group, and SharedGroup::rollback(). If
///    SharedGroup::end_write() is called, the new state becomes "no transaction
///    in progress"
class SharedGroup {
public:
    /// \brief Same as calling the corresponding version of open() on a instance
    /// constructed in the unattached state. Exception safety note: if the
    /// `upgrade_callback` throws, then the file will be closed properly and the
    /// upgrade will be aborted.
    explicit SharedGroup(const std::string& file, bool no_create = false,
                         const SharedGroupOptions options = SharedGroupOptions());

    /// \brief Same as calling the corresponding version of open() on a instance
    /// constructed in the unattached state. Exception safety note: if the
    /// `upgrade_callback` throws, then the file will be closed properly and
    /// the upgrade will be aborted.
    explicit SharedGroup(Replication& repl, const SharedGroupOptions options = SharedGroupOptions());

    struct unattached_tag {
    };

    /// Create a SharedGroup instance in its unattached state. It may
    /// then be attached to a database file later by calling
    /// open(). You may test whether this instance is currently in its
    /// attached state by calling is_attached(). Calling any other
    /// function (except the destructor) while in the unattached state
    /// has undefined behavior.
    SharedGroup(unattached_tag) noexcept;

    ~SharedGroup() noexcept;

    /// Attach this SharedGroup instance to the specified database file.
    ///
    /// While at least one instance of SharedGroup exists for a specific
    /// database file, a "lock" file will be present too. The lock file will be
    /// placed in the same directory as the database file, and its name will be
    /// derived by appending ".lock" to the name of the database file.
    ///
    /// When multiple SharedGroup instances refer to the same file, they must
    /// specify the same durability level, otherwise an exception will be
    /// thrown.
    ///
    /// \param file Filesystem path to a Realm database file.
    ///
    /// \param no_create If the database file does not already exist, it will be
    /// created (unless this is set to true.) When multiple threads are involved,
    /// it is safe to let the first thread, that gets to it, create the file.
    ///
    /// \param options See SharedGroupOptions for details of each option.
    /// Sensible defaults are provided if this parameter is left out.
    ///
    /// Calling open() on a SharedGroup instance that is already in the attached
    /// state has undefined behavior.
    ///
    /// \throw util::File::AccessError If the file could not be opened. If the
    /// reason corresponds to one of the exception types that are derived from
    /// util::File::AccessError, the derived exception type is thrown. Note that
    /// InvalidDatabase is among these derived exception types.
    ///
    /// \throw FileFormatUpgradeRequired only if \a SharedGroupOptions::allow_upgrade
    /// is `false` and an upgrade is required.
    void open(const std::string& file, bool no_create = false,
              const SharedGroupOptions options = SharedGroupOptions());

    /// Open this group in replication mode. The specified Replication instance
    /// must remain in existence for as long as the SharedGroup.
    void open(Replication&, const SharedGroupOptions options = SharedGroupOptions());

    /// Close any open database, returning to the unattached state.
    void close() noexcept;

    /// A SharedGroup may be created in the unattached state, and then
    /// later attached to a file with a call to open(). Calling any
    /// function other than open(), is_attached(), and ~SharedGroup()
    /// on an unattached instance results in undefined behavior.
    bool is_attached() const noexcept;

    /// Reserve disk space now to avoid allocation errors at a later
    /// point in time, and to minimize on-disk fragmentation. In some
    /// cases, less fragmentation translates into improved
    /// performance.
    ///
    /// When supported by the system, a call to this function will
    /// make the database file at least as big as the specified size,
    /// and cause space on the target device to be allocated (note
    /// that on many systems on-disk allocation is done lazily by
    /// default). If the file is already bigger than the specified
    /// size, the size will be unchanged, and on-disk allocation will
    /// occur only for the initial section that corresponds to the
    /// specified size. On systems that do not support preallocation,
    /// this function has no effect. To know whether preallocation is
    /// supported by Realm on your platform, call
    /// util::File::is_prealloc_supported().
    ///
    /// It is an error to call this function on an unattached shared
    /// group. Doing so will result in undefined behavior.
    void reserve(size_t size_in_bytes);

    /// Querying for changes:
    ///
    /// NOTE:
    /// "changed" means that one or more commits has been made to the database
    /// since the SharedGroup (on which wait_for_change() is called) last
    /// started, committed, promoted or advanced a transaction. If the
    /// SharedGroup has not yet begun a transaction, "changed" is undefined.
    ///
    /// No distinction is made between changes done by another process
    /// and changes done by another thread in the same process as the caller.
    ///
    /// Has db been changed ?
    bool has_changed();

    /// The calling thread goes to sleep until the database is changed, or
    /// until wait_for_change_release() is called. After a call to
    /// wait_for_change_release() further calls to wait_for_change() will return
    /// immediately. To restore the ability to wait for a change, a call to
    /// enable_wait_for_change() is required. Return true if the database has
    /// changed, false if it might have.
    bool wait_for_change();

    /// release any thread waiting in wait_for_change() on *this* SharedGroup.
    void wait_for_change_release();

    /// re-enable waiting for change
    void enable_wait_for_change();
    // Transactions:

    using version_type = _impl::History::version_type;
    using VersionID = realm::VersionID;

    /// Thrown by begin_read() if the specified version does not correspond to a
    /// bound (or tethered) snapshot.
    struct BadVersion;

    /// \defgroup group_shared_transactions
    //@{

    /// begin_read() initiates a new read transaction. A read transaction is
    /// bound to, and provides access to a particular snapshot of the underlying
    /// Realm (in general the latest snapshot, but see \a version). It cannot be
    /// used to modify the Realm, and in that sense, a read transaction is not a
    /// real transaction.
    ///
    /// begin_write() initiates a new write transaction. A write transaction
    /// allows the application to both read and modify the underlying Realm
    /// file. At most one write transaction can be in progress at any given time
    /// for a particular underlying Realm file. If another write transaction is
    /// already in progress, begin_write() will block the caller until the other
    /// write transaction terminates. No guarantees are made about the order in
    /// which multiple concurrent requests will be served.
    ///
    /// It is an error to call begin_read() or begin_write() on a SharedGroup
    /// object with an active read or write transaction.
    ///
    /// If begin_read() or begin_write() throws, no transaction is initiated,
    /// and the application may try to initiate a new read or write transaction
    /// later.
    ///
    /// end_read() terminates the active read transaction. If no read
    /// transaction is active, end_read() does nothing. It is an error to call
    /// this function on a SharedGroup object with an active write
    /// transaction. end_read() does not throw.
    ///
    /// commit() commits all changes performed in the context of the active
    /// write transaction, and thereby terminates that transaction. This
    /// produces a new snapshot in the underlying Realm. commit() returns the
    /// version associated with the new snapshot. It is an error to call
    /// commit() when there is no active write transaction. If commit() throws,
    /// no changes will have been committed, and the transaction will still be
    /// active, but in a bad state. In that case, the application must either
    /// call rollback() to terminate the bad transaction (in which case a new
    /// transaction can be initiated), call close() which also terminates the
    /// bad transaction, or destroy the SharedGroup object entirely. When the
    /// transaction is in a bad state, the application is not allowed to call
    /// any method on the Group accessor or on any of its subordinate accessors
    /// (Table, Row, Descriptor). Note that the transaction is also left in a
    /// bad state when a modifying operation on any subordinate accessor throws.
    ///
    /// rollback() terminates the active write transaction and discards any
    /// changes performed in the context of it. If no write transaction is
    /// active, rollback() does nothing. It is an error to call this function in
    /// a SharedGroup object with an active read transaction. rollback() does
    /// not throw.
    ///
    /// the Group accessor and all subordinate accessors (Table, Row,
    /// Descriptor) that are obtained in the context of a particular read or
    /// write transaction will become detached upon termination of that
    /// transaction, which means that they can no longer be used to access the
    /// underlying objects.
    ///
    /// Subordinate accessors that were detached at the end of the previous
    /// read or write transaction will not be automatically reattached when a
    /// new transaction is initiated. The application must reobtain new
    /// accessors during a new transaction to regain access to the underlying
    /// objects.
    ///
    /// \param version If specified, this must be the version associated with a
    /// *bound* snapshot. A snapshot is said to be bound (or tethered) if there
    /// is at least one active read or write transaction bound to it. A read
    /// transaction is bound to the snapshot that it provides access to. A write
    /// transaction is bound to the latest snapshot available at the time of
    /// initiation of the write transaction. If the specified version is not
    /// associated with a bound snapshot, this function throws BadVersion.
    ///
    /// \throw BadVersion Thrown by begin_read() if the specified version does
    /// not correspond to a bound (or tethered) snapshot.

    const Group& begin_read(VersionID version = VersionID());
    void end_read() noexcept;
    Group& begin_write();
    version_type commit();
    void rollback() noexcept;

    //@}

    enum TransactStage {
        transact_Ready,
        transact_Reading,
        transact_Writing,
    };

    /// Get the current transaction type
    TransactStage get_transact_stage() const noexcept;

    /// Get a version id which may be used to request a different SharedGroup
    /// to start transaction at a specific version.
    VersionID get_version_of_current_transaction();

    /// Report the number of distinct versions currently stored in the database.
    /// Note: the database only cleans up versions as part of commit, so ending
    /// a read transaction will not immediately release any versions.
    uint_fast64_t get_number_of_versions();

    /// Compact the database file.
    /// - The method will throw if called inside a transaction.
    /// - The method will throw if called in unattached state.
    /// - The method will return false if other SharedGroups are accessing the
    ///    database in which case compaction is not done. This is not
    ///    necessarily an error.
    /// It will return true following successful compaction.
    /// While compaction is in progress, attempts by other
    /// threads or processes to open the database will wait.
    /// Be warned that resource requirements for compaction is proportional to
    /// the amount of live data in the database.
    /// Compaction works by writing the database contents to a temporary
    /// database file and then replacing the database with the temporary one.
    /// The name of the temporary file is formed by appending
    /// ".tmp_compaction_space" to the name of the database
    ///
    /// FIXME: This function is not yet implemented in an exception-safe manner,
    /// therefore, if it throws, the application should not attempt to
    /// continue. If may not even be safe to destroy the SharedGroup object.
    ///
    /// WARNING / FIXME: compact() should NOT be exposed publicly on Windows
    /// because it's not crash safe! It may corrupt your database if something fails
    bool compact();

#ifdef REALM_DEBUG
    void test_ringbuf();
#endif

    /// To handover a table view, query, linkview or row accessor of type T, you
    /// must wrap it into a Handover<T> for the transfer. Wrapping and
    /// unwrapping of a handover object is done by the methods
    /// 'export_for_handover()' and 'import_from_handover()' declared below.
    /// 'export_for_handover()' returns a Handover object, and
    /// 'import_for_handover()' consumes that object, producing a new accessor
    /// which is ready for use in the context of the importing SharedGroup.
    ///
    /// The Handover always creates a new accessor object at the importing side.
    /// For TableViews, there are 3 forms of handover.
    ///
    /// - with payload move: the payload is handed over and ends up as a payload
    ///   held by the accessor at the importing side. The accessor on the
    ///   exporting side will rerun its query and generate a new payload, if
    ///   TableView::sync_if_needed() is called. If the original payload was in
    ///   sync at the exporting side, it will also be in sync at the importing
    ///   side. This is indicated to handover_export() by the argument
    ///   MutableSourcePayload::Move
    ///
    /// - with payload copy: a copy of the payload is handed over, so both the
    ///   accessors on the exporting side *and* the accessors created at the
    ///   importing side has their own payload. This is indicated to
    ///   handover_export() by the argument ConstSourcePayload::Copy
    ///
    /// - without payload: the payload stays with the accessor on the exporting
    ///   side. On the importing side, the new accessor is created without
    ///   payload. A call to TableView::sync_if_needed() will trigger generation
    ///   of a new payload. This form of handover is indicated to
    ///   handover_export() by the argument ConstSourcePayload::Stay.
    ///
    /// For all other (non-TableView) accessors, handover is done with payload
    /// copy, since the payload is trivial.
    ///
    /// Handover *without* payload is useful when you want to ship a tableview
    /// with its query for execution in a background thread. Handover with
    /// *payload move* is useful when you want to transfer the result back.
    ///
    /// Handover *without* payload or with payload copy is guaranteed *not* to
    /// change the accessors on the exporting side.
    ///
    /// Handover is *not* thread safe and should be carried out
    /// by the thread that "owns" the involved accessors.
    ///
    /// Handover is transitive:
    /// If the object being handed over depends on other views
    /// (table- or link- ), those objects will be handed over as well. The mode
    /// of handover (payload copy, payload move, without payload) is applied
    /// recursively. Note: If you are handing over a tableview dependent upon
    /// another tableview and using MutableSourcePayload::Move,
    /// you are on thin ice!
    ///
    /// On the importing side, the top-level accessor being created during
    /// import takes ownership of all other accessors (if any) being created as
    /// part of the import.

    /// Type used to support handover of accessors between shared groups.
    template <typename T>
    struct Handover;

    /// thread-safe/const export (mode is Stay or Copy)
    /// during export, the following operations on the shared group is locked:
    /// - advance_read(), promote_to_write(), commit_and_continue_as_read(),
    ///   rollback_and_continue_as_read(), close()
    template <typename T>
    std::unique_ptr<Handover<T>> export_for_handover(const T& accessor, ConstSourcePayload mode);

    // specialization for handover of Rows
    template <typename T>
    std::unique_ptr<Handover<BasicRow<T>>> export_for_handover(const BasicRow<T>& accessor);

    // destructive export (mode is Move)
    template <typename T>
    std::unique_ptr<Handover<T>> export_for_handover(T& accessor, MutableSourcePayload mode);

    /// Import an accessor wrapped in a handover object. The import will fail
    /// if the importing SharedGroup is viewing a version of the database that
    /// is different from the exporting SharedGroup. The call to
    /// import_from_handover is not thread-safe.
    template <typename T>
    std::unique_ptr<T> import_from_handover(std::unique_ptr<Handover<T>> handover);

    // We need two cases for handling of LinkViews, because they are ref counted.
    std::unique_ptr<Handover<LinkView>> export_linkview_for_handover(const LinkViewRef& accessor);
    LinkViewRef import_linkview_from_handover(std::unique_ptr<Handover<LinkView>> handover);

    // likewise for Tables.
    std::unique_ptr<Handover<Table>> export_table_for_handover(const TableRef& accessor);
    TableRef import_table_from_handover(std::unique_ptr<Handover<Table>> handover);

    /// When doing handover to background tasks that may be run later, we
    /// may want to momentarily pin the current version until the other thread
    /// has retrieved it.
    ///
    /// Pinning can be done in both read- and write-transactions, but with different
    /// semantics. When pinning during a read-transaction, the version pinned is the
    /// one accessible during the read-transaction. When pinning during a write-transaction,
    /// the version pinned will be the last version that was succesfully committed to the
    /// realm file at the point in time, when the write-transaction was started.
    ///
    /// The release is not thread-safe, so it has to be done on the SharedGroup
    /// associated with the thread calling unpin_version(), and the SharedGroup
    /// must be attached to the realm file at the point of unpinning.

    // Pin version for handover (not thread safe)
    VersionID pin_version();

    // Release pinned version (not thread safe)
    void unpin_version(VersionID version);

private:
    struct SharedInfo;
    struct ReadCount;
    struct ReadLockInfo {
        uint_fast64_t m_version = std::numeric_limits<version_type>::max();
        uint_fast32_t m_reader_idx = 0;
        ref_type m_top_ref = 0;
        size_t m_file_size = 0;
    };
    class ReadLockUnlockGuard;

    // Member variables
    Group m_group;
    ReadLockInfo m_read_lock;
    uint_fast32_t m_local_max_entry;
    util::File m_file;
    util::File::Map<SharedInfo> m_file_map; // Never remapped
    util::File::Map<SharedInfo> m_reader_map;
    bool m_wait_for_change_enabled;
    std::string m_lockfile_path;
    std::string m_lockfile_prefix;
    std::string m_db_path;
    std::string m_coordination_dir;
    const char* m_key;
    TransactStage m_transact_stage;
    util::InterprocessMutex m_writemutex;
#ifdef REALM_ASYNC_DAEMON
    util::InterprocessMutex m_balancemutex;
#endif
    util::InterprocessMutex m_controlmutex;
#ifndef _WIN32
#ifdef REALM_ASYNC_DAEMON
    util::InterprocessCondVar m_room_to_write;
    util::InterprocessCondVar m_work_to_do;
    util::InterprocessCondVar m_daemon_becomes_ready;
#endif
    util::InterprocessCondVar m_new_commit_available;
#endif
    std::function<void(int, int)> m_upgrade_callback;

    void do_open(const std::string& file, bool no_create, bool is_backend, const SharedGroupOptions options);

    // Ring buffer management
    bool ringbuf_is_empty() const noexcept;
    size_t ringbuf_size() const noexcept;
    size_t ringbuf_capacity() const noexcept;
    bool ringbuf_is_first(size_t ndx) const noexcept;
    void ringbuf_remove_first() noexcept;
    size_t ringbuf_find(uint64_t version) const noexcept;
    ReadCount& ringbuf_get(size_t ndx) noexcept;
    ReadCount& ringbuf_get_first() noexcept;
    ReadCount& ringbuf_get_last() noexcept;
    void ringbuf_put(const ReadCount& v);
    void ringbuf_expand();

    /// Grab a read lock on the snapshot associated with the specified
    /// version. If `version_id == VersionID()`, a read lock will be grabbed on
    /// the latest available snapshot. Fails if the snapshot is no longer
    /// available.
    ///
    /// As a side effect update memory mapping to ensure that the ringbuffer
    /// entries referenced in the readlock info is accessible.
    ///
    /// FIXME: It needs to be made more clear exactly under which conditions
    /// this function fails. Also, why is it useful to promise anything about
    /// detection of bad versions? Can we really promise enough to make such a
    /// promise useful to the caller?
    void grab_read_lock(ReadLockInfo&, VersionID);

    // Release a specific read lock. The read lock MUST have been obtained by a
    // call to grab_read_lock().
    void release_read_lock(ReadLockInfo&) noexcept;

    void do_begin_read(VersionID, bool writable);
    void do_end_read() noexcept;
    void do_begin_write();
    version_type do_commit();
    void do_end_write() noexcept;

    /// Returns the version of the latest snapshot.
    version_type get_version_of_latest_snapshot();

    /// Returns the version of the snapshot bound in the current read or write
    /// transaction. It is an error to call this function when no transaction is
    /// in progress.
    version_type get_version_of_bound_snapshot() const noexcept;

    // make sure the given index is within the currently mapped area.
    // if not, expand the mapped area. Returns true if the area is expanded.
    bool grow_reader_mapping(uint_fast32_t index);

    // Must be called only by someone that has a lock on the write
    // mutex.
    void low_level_commit(uint_fast64_t new_version);

    void do_async_commits();

    void upgrade_file_format(bool allow_file_format_upgrade, int target_file_format_version);

    //@{
    /// See LangBindHelper.
    template <class O>
    void advance_read(O* observer, VersionID);
    template <class O>
    void promote_to_write(O* observer);
    version_type commit_and_continue_as_read();
    template <class O>
    void rollback_and_continue_as_read(O* observer);
    //@}

    /// Returns true if, and only if _impl::History::update_early_from_top_ref()
    /// was called during the execution of this function.
    template <class O>
    bool do_advance_read(O* observer, VersionID, _impl::History&);

    /// If there is an associated \ref Replication object, then this function
    /// returns `repl->get_history()` where `repl` is that Replication object,
    /// otherwise this function returns null.
    _impl::History* get_history();

    int get_file_format_version() const noexcept;

    friend class _impl::SharedGroupFriend;
};


class ReadTransaction {
public:
    ReadTransaction(SharedGroup& sg)
        : m_shared_group(sg)
    {
        m_shared_group.begin_read(); // Throws
    }

    ~ReadTransaction() noexcept
    {
        m_shared_group.end_read();
    }

    bool has_table(StringData name) const noexcept
    {
        return get_group().has_table(name);
    }

    ConstTableRef get_table(size_t table_ndx) const
    {
        return get_group().get_table(table_ndx); // Throws
    }

    ConstTableRef get_table(StringData name) const
    {
        return get_group().get_table(name); // Throws
    }

    template <class T>
    BasicTableRef<const T> get_table(StringData name) const
    {
        return get_group().get_table<T>(name); // Throws
    }

    const Group& get_group() const noexcept;

    /// Get the version of the snapshot to which this read transaction is bound.
    SharedGroup::version_type get_version() const noexcept;

private:
    SharedGroup& m_shared_group;
};


class WriteTransaction {
public:
    WriteTransaction(SharedGroup& sg)
        : m_shared_group(&sg)
    {
        m_shared_group->begin_write(); // Throws
    }

    ~WriteTransaction() noexcept
    {
        if (m_shared_group)
            m_shared_group->rollback();
    }

    bool has_table(StringData name) const noexcept
    {
        return get_group().has_table(name);
    }

    TableRef get_table(size_t table_ndx) const
    {
        return get_group().get_table(table_ndx); // Throws
    }

    TableRef get_table(StringData name) const
    {
        return get_group().get_table(name); // Throws
    }

    TableRef add_table(StringData name, bool require_unique_name = true) const
    {
        return get_group().add_table(name, require_unique_name); // Throws
    }

    TableRef get_or_add_table(StringData name, bool* was_added = nullptr) const
    {
        return get_group().get_or_add_table(name, was_added); // Throws
    }

    template <class T>
    BasicTableRef<T> get_table(StringData name) const
    {
        return get_group().get_table<T>(name); // Throws
    }

    template <class T>
    BasicTableRef<T> add_table(StringData name, bool require_unique_name = true) const
    {
        return get_group().add_table<T>(name, require_unique_name); // Throws
    }

    template <class T>
    BasicTableRef<T> get_or_add_table(StringData name, bool* was_added = nullptr) const
    {
        return get_group().get_or_add_table<T>(name, was_added); // Throws
    }

    Group& get_group() const noexcept;

    /// Get the version of the snapshot on which this write transaction is
    /// based.
    SharedGroup::version_type get_version() const noexcept;

    SharedGroup::version_type commit()
    {
        REALM_ASSERT(m_shared_group);
        SharedGroup::version_type new_version = m_shared_group->commit();
        m_shared_group = nullptr;
        return new_version;
    }

    void rollback() noexcept
    {
        REALM_ASSERT(m_shared_group);
        m_shared_group->rollback();
        m_shared_group = nullptr;
    }

private:
    SharedGroup* m_shared_group;
};


// Implementation:

struct SharedGroup::BadVersion : std::exception {
};

inline SharedGroup::SharedGroup(const std::string& file, bool no_create, const SharedGroupOptions options)
    : m_group(Group::shared_tag())
    , m_upgrade_callback(std::move(options.upgrade_callback))
{
    open(file, no_create, options); // Throws
}

inline SharedGroup::SharedGroup(unattached_tag) noexcept
    : m_group(Group::shared_tag())
{
}

inline SharedGroup::SharedGroup(Replication& repl, const SharedGroupOptions options)
    : m_group(Group::shared_tag())
    , m_upgrade_callback(std::move(options.upgrade_callback))
{
    open(repl, options); // Throws
}

inline void SharedGroup::open(const std::string& path, bool no_create_file, const SharedGroupOptions options)
{
    // Exception safety: Since open() is called from constructors, if it throws,
    // it must leave the file closed.

    bool is_backend = false;
    do_open(path, no_create_file, is_backend, options); // Throws
}

inline void SharedGroup::open(Replication& repl, const SharedGroupOptions options)
{
    // Exception safety: Since open() is called from constructors, if it throws,
    // it must leave the file closed.

    REALM_ASSERT(!is_attached());

    repl.initialize(*this); // Throws

    typedef _impl::GroupFriend gf;
    gf::set_replication(m_group, &repl);

    std::string file = repl.get_database_path();
    bool no_create = false;
    bool is_backend = false;
    do_open(file, no_create, is_backend, options); // Throws
}

inline bool SharedGroup::is_attached() const noexcept
{
    return m_file_map.is_attached();
}

inline SharedGroup::TransactStage SharedGroup::get_transact_stage() const noexcept
{
    return m_transact_stage;
}

inline SharedGroup::version_type SharedGroup::get_version_of_bound_snapshot() const noexcept
{
    return m_read_lock.m_version;
}

class SharedGroup::ReadLockUnlockGuard {
public:
    ReadLockUnlockGuard(SharedGroup& shared_group, ReadLockInfo& read_lock) noexcept
        : m_shared_group(shared_group)
        , m_read_lock(&read_lock)
    {
    }
    ~ReadLockUnlockGuard() noexcept
    {
        if (m_read_lock)
            m_shared_group.release_read_lock(*m_read_lock);
    }
    void release() noexcept
    {
        m_read_lock = 0;
    }

private:
    SharedGroup& m_shared_group;
    ReadLockInfo* m_read_lock;
};


template <typename T>
struct SharedGroup::Handover {
    std::unique_ptr<typename T::HandoverPatch> patch;
    std::unique_ptr<T> clone;
    VersionID version;
};

template <typename T>
std::unique_ptr<SharedGroup::Handover<T>> SharedGroup::export_for_handover(const T& accessor, ConstSourcePayload mode)
{
    if (m_transact_stage != transact_Reading)
        throw LogicError(LogicError::wrong_transact_state);
    std::unique_ptr<Handover<T>> result(new Handover<T>());
    // Implementation note:
    // often, the return value from clone will be T*, BUT it may be ptr to some
    // base of T instead, so we must cast it to T*. This is always safe, because
    // no matter the type, clone() will clone the actual accessor instance, and
    // hence return an instance of the same type.
    result->clone.reset(dynamic_cast<T*>(accessor.clone_for_handover(result->patch, mode).release()));
    result->version = get_version_of_current_transaction();
    return move(result);
}


template <typename T>
std::unique_ptr<SharedGroup::Handover<BasicRow<T>>> SharedGroup::export_for_handover(const BasicRow<T>& accessor)
{
    if (m_transact_stage != transact_Reading)
        throw LogicError(LogicError::wrong_transact_state);
    std::unique_ptr<Handover<BasicRow<T>>> result(new Handover<BasicRow<T>>());
    // See implementation note above.
    result->clone.reset(dynamic_cast<BasicRow<T>*>(accessor.clone_for_handover(result->patch).release()));
    result->version = get_version_of_current_transaction();
    return move(result);
}


template <typename T>
std::unique_ptr<SharedGroup::Handover<T>> SharedGroup::export_for_handover(T& accessor, MutableSourcePayload mode)
{
    if (m_transact_stage != transact_Reading)
        throw LogicError(LogicError::wrong_transact_state);
    std::unique_ptr<Handover<T>> result(new Handover<T>());
    // see implementation note above.
    result->clone.reset(dynamic_cast<T*>(accessor.clone_for_handover(result->patch, mode).release()));
    result->version = get_version_of_current_transaction();
    return move(result);
}


template <typename T>
std::unique_ptr<T> SharedGroup::import_from_handover(std::unique_ptr<SharedGroup::Handover<T>> handover)
{
    if (handover->version != get_version_of_current_transaction()) {
        throw BadVersion();
    }
    std::unique_ptr<T> result = move(handover->clone);
    result->apply_and_consume_patch(handover->patch, m_group);
    return result;
}

template <class O>
inline void SharedGroup::advance_read(O* observer, VersionID version_id)
{
    if (m_transact_stage != transact_Reading)
        throw LogicError(LogicError::wrong_transact_state);

    // It is an error if the new version precedes the currently bound one.
    if (version_id.version < m_read_lock.m_version)
        throw LogicError(LogicError::bad_version);

    _impl::History* hist = get_history(); // Throws
    if (!hist)
        throw LogicError(LogicError::no_history);

    do_advance_read(observer, version_id, *hist); // Throws
}

template <class O>
inline void SharedGroup::promote_to_write(O* observer)
{
    if (m_transact_stage != transact_Reading)
        throw LogicError(LogicError::wrong_transact_state);

    _impl::History* hist = get_history(); // Throws
    if (!hist)
        throw LogicError(LogicError::no_history);

    do_begin_write(); // Throws
    try {
        VersionID version = VersionID();                                  // Latest
        bool history_updated = do_advance_read(observer, version, *hist); // Throws

        Replication* repl = m_group.get_replication();
        REALM_ASSERT(repl); // Presence of `repl` follows from the presence of `hist`
        version_type current_version = m_read_lock.m_version;
        repl->initiate_transact(current_version, history_updated); // Throws

        // If the group has no top array (top_ref == 0), create a new node
        // structure for an empty group now, to be ready for modifications. See
        // also Group::attach_shared().
        using gf = _impl::GroupFriend;
        gf::create_empty_group_when_missing(m_group); // Throws
    }
    catch (...) {
        do_end_write();
        throw;
    }

    m_transact_stage = transact_Writing;
}

template <class O>
inline void SharedGroup::rollback_and_continue_as_read(O* observer)
{
    if (m_transact_stage != transact_Writing)
        throw LogicError(LogicError::wrong_transact_state);

    _impl::History* hist = get_history(); // Throws
    if (!hist)
        throw LogicError(LogicError::no_history);

    // Mark all managed space (beyond the attached file) as free.
    using gf = _impl::GroupFriend;
    gf::reset_free_space_tracking(m_group); // Throws

    BinaryData uncommitted_changes = hist->get_uncommitted_changes();

    // FIXME: We are currently creating two transaction log parsers, one here,
    // and one in advance_transact(). That is wasteful as the parser creation is
    // expensive.
    _impl::SimpleInputStream in(uncommitted_changes.data(), uncommitted_changes.size());
    _impl::TransactLogParser parser; // Throws
    _impl::TransactReverser reverser;
    parser.parse(in, reverser); // Throws

    if (observer && uncommitted_changes.size()) {
        _impl::ReversedNoCopyInputStream reversed_in(reverser);
        parser.parse(reversed_in, *observer); // Throws
        observer->parse_complete();           // Throws
    }

    ref_type top_ref = m_read_lock.m_top_ref;
    size_t file_size = m_read_lock.m_file_size;
    _impl::ReversedNoCopyInputStream reversed_in(reverser);
    gf::advance_transact(m_group, top_ref, file_size, reversed_in); // Throws

    do_end_write();

    Replication* repl = gf::get_replication(m_group);
    REALM_ASSERT(repl); // Presence of `repl` follows from the presence of `hist`
    repl->abort_transact();

    m_transact_stage = transact_Reading;
}

template <class O>
inline bool SharedGroup::do_advance_read(O* observer, VersionID version_id, _impl::History& hist)
{
    ReadLockInfo new_read_lock;
    grab_read_lock(new_read_lock, version_id); // Throws
    REALM_ASSERT(new_read_lock.m_version >= m_read_lock.m_version);
    if (new_read_lock.m_version == m_read_lock.m_version) {
        release_read_lock(new_read_lock);
        // _impl::History::update_early_from_top_ref() was not called
        return false;
    }

    ReadLockUnlockGuard g(*this, new_read_lock);
    {
        version_type new_version = new_read_lock.m_version;
        size_t new_file_size = new_read_lock.m_file_size;
        ref_type new_top_ref = new_read_lock.m_top_ref;
        hist.update_early_from_top_ref(new_version, new_file_size, new_top_ref); // Throws
    }

    if (observer) {
        // This has to happen in the context of the originally bound snapshot
        // and while the read transaction is still in a fully functional state.
        _impl::TransactLogParser parser;
        version_type old_version = m_read_lock.m_version;
        version_type new_version = new_read_lock.m_version;
        _impl::ChangesetInputStream in(hist, old_version, new_version);
        parser.parse(in, *observer); // Throws
        observer->parse_complete();  // Throws
    }

    // The old read lock must be retained for as long as the change history is
    // accessed (until Group::advance_transact() returns). This ensures that the
    // oldest needed changeset remains in the history, even when the history is
    // implemented as a separate unversioned entity outside the Realm (i.e., the
    // old implementation and ShortCircuitHistory in
    // test_lang_Bind_helper.cpp). On the other hand, if it had been the case,
    // that the history was always implemented as a versioned entity, that was
    // part of the Realm state, then it would not have been necessary to retain
    // the old read lock beyond this point.

    {
        version_type old_version = m_read_lock.m_version;
        version_type new_version = new_read_lock.m_version;
        ref_type new_top_ref = new_read_lock.m_top_ref;
        size_t new_file_size = new_read_lock.m_file_size;
        _impl::ChangesetInputStream in(hist, old_version, new_version);
        m_group.advance_transact(new_top_ref, new_file_size, in); // Throws
    }

    g.release();
    release_read_lock(m_read_lock);
    m_read_lock = new_read_lock;

    return true; // _impl::History::update_early_from_top_ref() was called
}

inline _impl::History* SharedGroup::get_history()
{
    using gf = _impl::GroupFriend;
    if (Replication* repl = gf::get_replication(m_group))
        return repl->get_history();
    return 0;
}

inline int SharedGroup::get_file_format_version() const noexcept
{
    using gf = _impl::GroupFriend;
    return gf::get_file_format_version(m_group);
}


// The purpose of this class is to give internal access to some, but
// not all of the non-public parts of the SharedGroup class.
class _impl::SharedGroupFriend {
public:
    static Group& get_group(SharedGroup& sg) noexcept
    {
        return sg.m_group;
    }

    template <class O>
    static void advance_read(SharedGroup& sg, O* obs, SharedGroup::VersionID ver)
    {
        sg.advance_read(obs, ver); // Throws
    }

    template <class O>
    static void promote_to_write(SharedGroup& sg, O* obs)
    {
        sg.promote_to_write(obs); // Throws
    }

    static SharedGroup::version_type commit_and_continue_as_read(SharedGroup& sg)
    {
        return sg.commit_and_continue_as_read(); // Throws
    }

    template <class O>
    static void rollback_and_continue_as_read(SharedGroup& sg, O* obs)
    {
        sg.rollback_and_continue_as_read(obs); // Throws
    }

    static void async_daemon_open(SharedGroup& sg, const std::string& file)
    {
        bool no_create = true;
        bool is_backend = true;
        SharedGroupOptions options;
        options.durability = SharedGroupOptions::Durability::Async;
        options.encryption_key = nullptr;
        options.allow_file_format_upgrade = false;
        sg.do_open(file, no_create, is_backend, options); // Throws
    }

    static int get_file_format_version(const SharedGroup& sg) noexcept
    {
        return sg.get_file_format_version();
    }

    static SharedGroup::version_type get_version_of_latest_snapshot(SharedGroup& sg)
    {
        return sg.get_version_of_latest_snapshot();
    }

    static SharedGroup::version_type get_version_of_bound_snapshot(const SharedGroup& sg) noexcept
    {
        return sg.get_version_of_bound_snapshot();
    }
};

inline const Group& ReadTransaction::get_group() const noexcept
{
    using sgf = _impl::SharedGroupFriend;
    return sgf::get_group(m_shared_group);
}

inline SharedGroup::version_type ReadTransaction::get_version() const noexcept
{
    using sgf = _impl::SharedGroupFriend;
    return sgf::get_version_of_bound_snapshot(m_shared_group);
}

inline Group& WriteTransaction::get_group() const noexcept
{
    REALM_ASSERT(m_shared_group);
    using sgf = _impl::SharedGroupFriend;
    return sgf::get_group(*m_shared_group);
}

inline SharedGroup::version_type WriteTransaction::get_version() const noexcept
{
    using sgf = _impl::SharedGroupFriend;
    return sgf::get_version_of_bound_snapshot(*m_shared_group);
}

} // namespace realm

#endif // REALM_GROUP_SHARED_HPP
