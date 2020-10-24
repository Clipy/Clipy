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

#include <functional>
#include <cstdint>
#include <limits>
#include <realm/util/features.h>
#include <realm/util/thread.hpp>
#include <realm/util/interprocess_condvar.hpp>
#include <realm/util/interprocess_mutex.hpp>
#include <realm/group.hpp>
#include <realm/handover_defs.hpp>
#include <realm/impl/transact_log.hpp>
#include <realm/metrics/metrics.hpp>
#include <realm/replication.hpp>
#include <realm/version_id.hpp>
#include <realm/db_options.hpp>

namespace realm {

namespace _impl {
class WriteLogCollector;
}

class Transaction;
using TransactionRef = std::shared_ptr<Transaction>;

/// Thrown by DB::create() if the lock file is already open in another
/// process which can't share mutexes with this process
struct IncompatibleLockFile : std::runtime_error {
    IncompatibleLockFile(const std::string& msg)
        : std::runtime_error("Incompatible lock file. " + msg)
    {
    }
};

/// Thrown by DB::create() if the type of history
/// (Replication::HistoryType) in the opened Realm file is incompatible with the
/// mode in which the Realm file is opened. For example, if there is a mismatch
/// between the history type in the file, and the history type associated with
/// the replication plugin passed to DB::create().
///
/// This exception will also be thrown if the history schema version is lower
/// than required, and no migration is possible
/// (Replication::is_upgradable_history_schema()).
struct IncompatibleHistories : util::File::AccessError {
    IncompatibleHistories(const std::string& msg, const std::string& path)
        : util::File::AccessError("Incompatible histories. " + msg, path)
    {
    }
};

/// The FileFormatUpgradeRequired exception can be thrown by the DB
/// constructor when opening a database that uses a deprecated file format
/// and/or a deprecated history schema, and the user has indicated he does not
/// want automatic upgrades to be performed. This exception indicates that until
/// an upgrade of the file format is performed, the database will be unavailable
/// for read or write operations.
/// It will also be thrown if a realm which requires upgrade is opened in read-only
/// mode (Group::open).
struct FileFormatUpgradeRequired : util::File::AccessError {
    FileFormatUpgradeRequired(const std::string& msg, const std::string& path)
        : util::File::AccessError(msg, path)
    {
    }
};


/// A DB facilitates transactions.
///
/// Access to a database is done through transactions. Transactions
/// are created by a DB object. No matter how many transactions you
/// use, you only need a single DB object per file. Methods on the DB
/// object are thread-safe.
///
/// Realm has 3 types of Transactions:
/// * A frozen transaction allows read only access
/// * A read transaction allows read only access but can be promoted
///   to a write transaction.
/// * A write transaction allows write access. A write transaction can
///   be demoted to a read transaction.
///
/// Frozen transactions are thread safe. Read and write transactions are not.
///
/// Two processes that want to share a database file must reside on
/// the same host.
///

class DB;
using DBRef = std::shared_ptr<DB>;

class DB : public std::enable_shared_from_this<DB> {
public:
    // Create a DB and associate it with a file. DB Objects can only be associated with one file,
    // the association determined on creation of the DB Object. The association can be broken by
    // calling DB::close(), but after that no new association can be established. To reopen the
    // file (or another file), a new DB object is needed.
    static DBRef create(const std::string& file, bool no_create = false, const DBOptions options = DBOptions());
    static DBRef create(Replication& repl, const DBOptions options = DBOptions());

    ~DB() noexcept;

    // Disable copying to prevent accessor errors. If you really want another
    // instance, open another DB object on the same file. But you don't.
    DB(const DB&) = delete;
    DB& operator=(const DB&) = delete;
    /// Close an open database. Calling close() is thread-safe with respect to
    /// other calls to close and with respect to deleting transactions.
    /// Calling close() while a write transaction is open is an error and close()
    /// will throw a LogicError::wrong_transact_state.
    /// Calling close() while a read transaction is open is by default treated
    /// in the same way, but close(true) will allow the error to be ignored and
    /// release resources despite open read transactions.
    /// As successfull call to close() leaves transactions (and any associated
    /// accessors) in a defunct state and the actual close() operation is not
    /// interlocked with access through those accessors, so any access through accessors
    /// may constitute a race with a call to close().
    /// Instead of using DB::close() to release resources, we recommend using transactions
    /// to control release as follows:
    ///  * explicitly close() transactions at earliest time possible and
    ///  * explicitly nullify any DBRefs you may have.
    void close(bool allow_open_read_transactions = false);

    bool is_attached() const noexcept;

    Allocator& get_alloc()
    {
        return m_alloc;
    }

    Replication* get_replication() const
    {
        return m_replication;
    }

    void set_replication(Replication* repl) noexcept
    {
        m_replication = repl;
    }


#ifdef REALM_DEBUG
    /// Deprecated method, only called from a unit test
    ///
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
    /// specified size.
    ///
    /// It is an error to call this function on an unattached shared
    /// group. Doing so will result in undefined behavior.
    void reserve(size_t size_in_bytes);
#endif

    /// Querying for changes:
    ///
    /// NOTE:
    /// "changed" means that one or more commits has been made to the database
    /// since the presented transaction was made.
    ///
    /// No distinction is made between changes done by another process
    /// and changes done by another thread in the same process as the caller.
    ///
    /// Has db been changed ?
    bool has_changed(TransactionRef);

    /// The calling thread goes to sleep until the database is changed, or
    /// until wait_for_change_release() is called. After a call to
    /// wait_for_change_release() further calls to wait_for_change() will return
    /// immediately. To restore the ability to wait for a change, a call to
    /// enable_wait_for_change() is required. Return true if the database has
    /// changed, false if it might have.
    bool wait_for_change(TransactionRef);

    /// release any thread waiting in wait_for_change().
    void wait_for_change_release();

    /// re-enable waiting for change
    void enable_wait_for_change();
    // Transactions:

    using version_type = _impl::History::version_type;
    using VersionID = realm::VersionID;

    /// Returns the version of the latest snapshot.
    version_type get_version_of_latest_snapshot();

    /// Thrown by start_read() if the specified version does not correspond to a
    /// bound (AKA tethered) snapshot.
    struct BadVersion;


    /// Transactions are obtained from one of the following 3 methods:
    TransactionRef start_read(VersionID = VersionID());
    TransactionRef start_frozen(VersionID = VersionID());
    // If nonblocking is true and a write transaction is already active,
    // an invalid TransactionRef is returned.
    TransactionRef start_write(bool nonblocking = false);


    // report statistics of last commit done on THIS DB.
    // The free space reported is what can be expected to be freed
    // by compact(). This may not correspond to the space which is free
    // at the point where get_stats() is called, since that will include
    // memory required to hold older versions of data, which still
    // needs to be available. The locked space is the amount of memory
    // that is free in current version, but being used in still live versions.
    // Notice that we will always have two live versions - the current and the
    // previous.
    void get_stats(size_t& free_space, size_t& used_space, util::Optional<size_t&> locked_space = util::none) const;
    //@}

    enum TransactStage {
        transact_Ready,
        transact_Reading,
        transact_Writing,
        transact_Frozen,
    };

    /// Report the number of distinct versions currently stored in the database.
    /// Note: the database only cleans up versions as part of commit, so ending
    /// a read transaction will not immediately release any versions.
    uint_fast64_t get_number_of_versions();

    /// Get the size of the currently allocated slab area
    size_t get_allocated_size() const;

    /// Compact the database file.
    /// - The method will throw if called inside a transaction.
    /// - The method will throw if called in unattached state.
    /// - The method will return false if other DBs are accessing the
    ///    database in which case compaction is not done. This is not
    ///    necessarily an error.
    /// It will return true following successful compaction.
    /// While compaction is in progress, attempts by other
    /// threads or processes to open the database will wait.
    /// Likewise, attempts to create new transactions will wait.
    /// Be warned that resource requirements for compaction is proportional to
    /// the amount of live data in the database.
    /// Compaction works by writing the database contents to a temporary
    /// database file and then replacing the database with the temporary one.
    /// The name of the temporary file is formed by appending
    /// ".tmp_compaction_space" to the name of the database
    ///
    /// If the output_encryption_key is `none` then the file's existing key will
    /// be used (if any). If the output_encryption_key is nullptr, the resulting
    /// file will be unencrypted. Any other value will change the encryption of
    /// the file to the new 64 byte key.
    ///
    /// FIXME: This function is not yet implemented in an exception-safe manner,
    /// therefore, if it throws, the application should not attempt to
    /// continue. If may not even be safe to destroy the DB object.
    ///
    /// WARNING / FIXME: compact() should NOT be exposed publicly on Windows
    /// because it's not crash safe! It may corrupt your database if something fails
    ///
    /// WARNING: Compact() is not thread-safe with respect to a concurrent close()
    bool compact(bool bump_version_number = false, util::Optional<const char*> output_encryption_key = util::none);

#ifdef REALM_DEBUG
    void test_ringbuf();
#endif

/// Once created, accessors belong to a transaction and can only be used for
/// access as long as that transaction is still active. Copies of accessors
/// can be created in association with another transaction, the importing transaction,
/// using said transactions import_copy_of method. This process is called
/// accessor import. Prior to Core 6, the corresponding mechanism was known
/// as "handover".
///
/// For TableViews, there are 3 forms of import determined by the PayloadPolicy.
///
/// - with payload move: the payload imported ends up as a payload
///   held by the accessor at the importing side. The accessor on the
///   exporting side will rerun its query and generate a new payload, if
///   TableView::sync_if_needed() is called. If the original payload was in
///   sync at the exporting side, it will also be in sync at the importing
///   side. This is indicated to handover_export() by the argument
///   PayloadPolicy::Move
///
/// - with payload copy: a copy of the payload is imported, so both the
///   accessors on the exporting side *and* the accessors created at the
///   importing side has their own payload. This is indicated to
///   handover_export() by the argument PayloadPolicy::Copy
///
/// - without payload: the payload stays with the accessor on the exporting
///   side. On the importing side, the new accessor is created without
///   payload. A call to TableView::sync_if_needed() will trigger generation
///   of a new payload. This form of handover is indicated to
///   handover_export() by the argument PayloadPolicy::Stay.
///
/// For all other (non-TableView) accessors, importing is done with payload
/// copy, since the payload is trivial.
///
/// Importing *without* payload is useful when you want to ship a tableview
/// with its query for execution in a background thread. Handover with
/// *payload move* is useful when you want to transfer the result back.
///
/// Importing *without* payload or with payload copy is guaranteed *not* to
/// change the accessors on the exporting side.
///
/// Importing is *not* thread safe and should be carried out
/// by the thread that "owns" the involved accessors.
///
/// Importing is transitive:
/// If the object being imported depends on other views
/// (table- or link- ), those objects will be imported as well. The mode
/// (payload copy, payload move, without payload) is applied
/// recursively. Note: If you are importing a tableview dependent upon
/// another tableview and using MutableSourcePayload::Move,
/// you are on thin ice!
///
/// On the importing side, the top-level accessor being created during
/// import takes ownership of all other accessors (if any) being created as
/// part of the import.
    std::shared_ptr<metrics::Metrics> get_metrics()
    {
        return m_metrics;
    }

    // Try to grab a exclusive lock of the given realm path's lock file. If the lock
    // can be acquired, the callback will be executed with the lock and then return true.
    // Otherwise false will be returned directly.
    // The lock taken precludes races with other threads or processes accessing the
    // files through a SharedGroup.
    // It is safe to delete/replace realm files inside the callback.
    // WARNING: It is not safe to delete the lock file in the callback.
    using CallbackWithLock = std::function<void(const std::string& realm_path)>;
    static bool call_with_lock(const std::string& realm_path, CallbackWithLock callback);

    // Return a list of files/directories core may use of the given realm file path.
    // The first element of the pair in the returned list is the path string, the
    // second one is to indicate the path is a directory or not.
    // The temporary files are not returned by this function.
    // It is safe to delete those returned files/directories in the call_with_lock's callback.
    static std::vector<std::pair<std::string, bool>> get_core_files(const std::string& realm_path);

protected:
    explicit DB(const DBOptions& options); // Is this ever used?

private:
    std::recursive_mutex m_mutex;
    int m_transaction_count = 0;
    SlabAlloc m_alloc;
    Replication* m_replication = nullptr;
    struct SharedInfo;
    struct ReadCount;
    struct ReadLockInfo {
        uint_fast64_t m_version = std::numeric_limits<version_type>::max();
        uint_fast32_t m_reader_idx = 0;
        ref_type m_top_ref = 0;
        size_t m_file_size = 0;
    };
    class ReadLockGuard;

    // Member variables
    size_t m_free_space = 0;
    size_t m_locked_space = 0;
    size_t m_used_space = 0;
    uint_fast32_t m_local_max_entry = 0; // highest version observed by this DB
    std::vector<ReadLockInfo> m_local_locks_held; // tracks all read locks held by this DB
    util::File m_file;
    util::File::Map<SharedInfo> m_file_map; // Never remapped, provides access to everything but the ringbuffer
    util::File::Map<SharedInfo> m_reader_map; // provides access to ringbuffer, remapped as needed when it grows
    bool m_wait_for_change_enabled = true; // Initially wait_for_change is enabled
    bool m_write_transaction_open = false;
    std::string m_lockfile_path;
    std::string m_lockfile_prefix;
    std::string m_db_path;
    std::string m_coordination_dir;
    const char* m_key;
    int m_file_format_version = 0;
    util::InterprocessMutex m_writemutex;
#ifdef REALM_ASYNC_DAEMON
    util::InterprocessMutex m_balancemutex;
#endif
    util::InterprocessMutex m_controlmutex;
#ifdef REALM_ASYNC_DAEMON
    util::InterprocessCondVar m_room_to_write;
    util::InterprocessCondVar m_work_to_do;
    util::InterprocessCondVar m_daemon_becomes_ready;
#endif
    util::InterprocessCondVar m_new_commit_available;
    util::InterprocessCondVar m_pick_next_writer;
    std::function<void(int, int)> m_upgrade_callback;

    std::shared_ptr<metrics::Metrics> m_metrics;
    /// Attach this DB instance to the specified database file.
    ///
    /// While at least one instance of DB exists for a specific
    /// database file, a "lock" file will be present too. The lock file will be
    /// placed in the same directory as the database file, and its name will be
    /// derived by appending ".lock" to the name of the database file.
    ///
    /// When multiple DB instances refer to the same file, they must
    /// specify the same durability level, otherwise an exception will be
    /// thrown.
    ///
    /// \param file Filesystem path to a Realm database file.
    ///
    /// \param no_create If the database file does not already exist, it will be
    /// created (unless this is set to true.) When multiple threads are involved,
    /// it is safe to let the first thread, that gets to it, create the file.
    ///
    /// \param options See DBOptions for details of each option.
    /// Sensible defaults are provided if this parameter is left out.
    ///
    /// \throw util::File::AccessError If the file could not be opened. If the
    /// reason corresponds to one of the exception types that are derived from
    /// util::File::AccessError, the derived exception type is thrown. Note that
    /// InvalidDatabase is among these derived exception types.
    ///
    /// \throw FileFormatUpgradeRequired if \a DBOptions::allow_upgrade
    /// is `false` and an upgrade is required.
    ///
    /// \throw UnsupportedFileFormatVersion if the file format version or
    /// history schema version is one which this version of Realm does not know
    /// how to migrate from.
    void open(const std::string& file, bool no_create = false, const DBOptions options = DBOptions());

    /// Open this group in replication mode. The specified Replication instance
    /// must remain in existence for as long as the DB.
    void open(Replication&, const DBOptions options = DBOptions());


    void do_open(const std::string& file, bool no_create, bool is_backend, const DBOptions options);

    Replication* const* get_repl() const noexcept
    {
        return &m_replication;
    }

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

    // Release all read locks held by this DB object. After release, further calls to
    // release_read_lock for locks already released must be avoided.
    void release_all_read_locks() noexcept;

    /// return true if write transaction can commence, false otherwise.
    bool do_try_begin_write();
    void do_begin_write();
    version_type do_commit(Transaction&);
    void do_end_write() noexcept;

    // make sure the given index is within the currently mapped area.
    // if not, expand the mapped area. Returns true if the area is expanded.
    bool grow_reader_mapping(uint_fast32_t index);

    // Must be called only by someone that has a lock on the write
    // mutex.
    void low_level_commit(uint_fast64_t new_version, Transaction& transaction);

    void do_async_commits();

    /// Upgrade file format and/or history schema
    void upgrade_file_format(bool allow_file_format_upgrade, int target_file_format_version,
                             int current_hist_schema_version, int target_hist_schema_version);

    int get_file_format_version() const noexcept;

    /// finish up the process of starting a write transaction. Internal use only.
    void finish_begin_write();

    void reset_free_space_tracking()
    {
        m_alloc.reset_free_space_tracking();
    }

    void close_internal(std::unique_lock<InterprocessMutex>, bool allow_open_read_transactions);
    friend class Transaction;
};

inline void DB::get_stats(size_t& free_space, size_t& used_space, util::Optional<size_t&> locked_space) const
{
    free_space = m_free_space;
    used_space = m_used_space;
    if (locked_space) {
        *locked_space = m_locked_space;
    }
}


class Transaction : public Group {
public:
    Transaction(DBRef _db, SlabAlloc* alloc, DB::ReadLockInfo& rli, DB::TransactStage stage);
    // convenience, so you don't need to carry a reference to the DB around
    ~Transaction();

    DB::version_type get_version() const noexcept
    {
        return m_read_lock.m_version;
    }
    DB::version_type get_version_of_latest_snapshot()
    {
        return db->get_version_of_latest_snapshot();
    }
    void close();
    bool is_attached()
    {
        return m_transact_stage != DB::transact_Ready && db->is_attached();
    }

    /// Get the approximate size of the data that would be written to the file if
    /// a commit were done at this point. The reported size will always be bigger
    /// than what will eventually be needed as we reserve a bit more memory that
    /// will be needed.
    size_t get_commit_size() const;

    DB::version_type commit();
    void rollback();
    void end_read();

    // Live transactions state changes, often taking an observer functor:
    DB::version_type commit_and_continue_as_read();
    template <class O>
    void rollback_and_continue_as_read(O* observer);
    void rollback_and_continue_as_read()
    {
        _impl::NullInstructionObserver* o = nullptr;
        rollback_and_continue_as_read(o);
    }
    template <class O>
    void advance_read(O* observer, VersionID target_version = VersionID());
    void advance_read(VersionID target_version = VersionID())
    {
        _impl::NullInstructionObserver* o = nullptr;
        advance_read(o, target_version);
    }
    template <class O>
    bool promote_to_write(O* observer, bool nonblocking = false);
    bool promote_to_write(bool nonblocking = false)
    {
        _impl::NullInstructionObserver* o = nullptr;
        return promote_to_write(o, nonblocking);
    }
    TransactionRef freeze();
    // Frozen transactions are created by freeze() or DB::start_frozen()
    bool is_frozen() const noexcept override { return m_transact_stage == DB::transact_Frozen; }
    TransactionRef duplicate();

    _impl::History* get_history() const;

    // direct handover of accessor instances
    Obj import_copy_of(const ConstObj& original); // slicing is OK for Obj/ConstObj
    TableRef import_copy_of(const ConstTableRef original);
    LnkLst import_copy_of(const ConstLnkLst& original);
    LstBasePtr import_copy_of(const LstBase& original);
    LnkLstPtr import_copy_of(const LnkLstPtr& original);
    LnkLstPtr import_copy_of(const ConstLnkLstPtr& original);

    // handover of the heavier Query and TableView
    std::unique_ptr<Query> import_copy_of(Query&, PayloadPolicy);
    std::unique_ptr<TableView> import_copy_of(TableView&, PayloadPolicy);
    std::unique_ptr<ConstTableView> import_copy_of(ConstTableView&, PayloadPolicy);

    /// Get the current transaction type
    DB::TransactStage get_transact_stage() const noexcept;

    /// Get a version id which may be used to request a different SharedGroup
    /// to start transaction at a specific version.
    VersionID get_version_of_current_transaction();

    void upgrade_file_format(int target_file_format_version);

private:
    DBRef get_db() const
    {
        return db;
    }

    Replication* const* get_repl() const final
    {
        return db->get_repl();
    }

    template <class O>
    bool internal_advance_read(O* observer, VersionID target_version, _impl::History&, bool);
    void set_transact_stage(DB::TransactStage stage) noexcept;
    void do_end_read() noexcept;
    void commit_and_continue_writing();
    void initialize_replication();

    DBRef db;
    mutable std::unique_ptr<_impl::History> m_history_read;
    mutable _impl::History* m_history = nullptr;

    DB::ReadLockInfo m_read_lock;
    DB::TransactStage m_transact_stage = DB::transact_Ready;

    friend class DB;
    friend class DisableReplication;
};

class DisableReplication {
public:
    DisableReplication(Transaction& t)
        : m_tr(t)
        , m_owner(t.get_db())
        , m_repl(m_owner->get_replication())
        , m_version(t.get_version())
    {
        m_owner->set_replication(nullptr);
        t.get_version();
        t.m_history = nullptr;
    }

    ~DisableReplication()
    {
        m_owner->set_replication(m_repl);
        if (m_version != m_tr.get_version())
            m_tr.initialize_replication();
    }

private:
    Transaction& m_tr;
    DBRef m_owner;
    Replication* m_repl;
    DB::version_type m_version;
};


/*
 * classes providing backward Compatibility with the older
 * ReadTransaction and WriteTransaction types.
 */

class ReadTransaction {
public:
    ReadTransaction(DBRef sg)
        : trans(sg->start_read())
    {
    }

    ~ReadTransaction() noexcept
    {
    }

    operator Transaction&()
    {
        return *trans;
    }

    bool has_table(StringData name) const noexcept
    {
        return trans->has_table(name);
    }

    ConstTableRef get_table(TableKey key) const
    {
        return trans->get_table(key); // Throws
    }

    ConstTableRef get_table(StringData name) const
    {
        return trans->get_table(name); // Throws
    }

    const Group& get_group() const noexcept
    {
        return *trans.get();
    }

    /// Get the version of the snapshot to which this read transaction is bound.
    DB::version_type get_version() const noexcept
    {
        return trans->get_version();
    }

private:
    TransactionRef trans;
};


class WriteTransaction {
public:
    WriteTransaction(DBRef sg)
        : trans(sg->start_write())
    {
    }

    ~WriteTransaction() noexcept
    {
    }

    operator Transaction&()
    {
        return *trans;
    }

    bool has_table(StringData name) const noexcept
    {
        return trans->has_table(name);
    }

    TableRef get_table(TableKey key) const
    {
        return trans->get_table(key); // Throws
    }

    TableRef get_table(StringData name) const
    {
        return trans->get_table(name); // Throws
    }

    TableRef add_table(StringData name) const
    {
        return trans->add_table(name); // Throws
    }

    TableRef get_or_add_table(StringData name, bool* was_added = nullptr) const
    {
        return trans->get_or_add_table(name, was_added); // Throws
    }

    Group& get_group() const noexcept
    {
        return *trans.get();
    }

    /// Get the version of the snapshot on which this write transaction is
    /// based.
    DB::version_type get_version() const noexcept
    {
        return trans->get_version();
    }

    DB::version_type commit()
    {
        return trans->commit();
    }

    void rollback() noexcept
    {
        trans->rollback();
    }

private:
    TransactionRef trans;
};


// Implementation:

struct DB::BadVersion : std::exception {
};

inline bool DB::is_attached() const noexcept
{
    return m_file_map.is_attached();
}

inline DB::TransactStage Transaction::get_transact_stage() const noexcept
{
    return m_transact_stage;
}

class DB::ReadLockGuard {
public:
    ReadLockGuard(DB& shared_group, ReadLockInfo& read_lock) noexcept
        : m_shared_group(shared_group)
        , m_read_lock(&read_lock)
    {
    }
    ~ReadLockGuard() noexcept
    {
        if (m_read_lock)
            m_shared_group.release_read_lock(*m_read_lock);
    }
    void release() noexcept
    {
        m_read_lock = 0;
    }

private:
    DB& m_shared_group;
    ReadLockInfo* m_read_lock;
};

template <class O>
inline void Transaction::advance_read(O* observer, VersionID version_id)
{
    if (m_transact_stage != DB::transact_Reading)
        throw LogicError(LogicError::wrong_transact_state);

    // It is an error if the new version precedes the currently bound one.
    if (version_id.version < m_read_lock.m_version)
        throw LogicError(LogicError::bad_version);

    auto hist = get_history(); // Throws
    if (!hist)
        throw LogicError(LogicError::no_history);

    internal_advance_read(observer, version_id, *hist, false); // Throws
}

template <class O>
inline bool Transaction::promote_to_write(O* observer, bool nonblocking)
{
    if (m_transact_stage != DB::transact_Reading)
        throw LogicError(LogicError::wrong_transact_state);

    if (nonblocking) {
        bool succes = db->do_try_begin_write();
        if (!succes) {
            return false;
        }
    }
    else {
        db->do_begin_write(); // Throws
    }
    try {
        Replication* repl = db->get_replication();
        if (!repl)
            throw LogicError(LogicError::no_history);

        VersionID version = VersionID();                                              // Latest
        m_history = repl->_get_history_write();
        bool history_updated = internal_advance_read(observer, version, *m_history, true); // Throws

        REALM_ASSERT(repl); // Presence of `repl` follows from the presence of `hist`
        DB::version_type current_version = m_read_lock.m_version;
        repl->initiate_transact(*this, current_version, history_updated); // Throws

        // If the group has no top array (top_ref == 0), create a new node
        // structure for an empty group now, to be ready for modifications. See
        // also Group::attach_shared().
        if (!m_top.is_attached())
            create_empty_group(); // Throws
    }
    catch (...) {
        db->do_end_write();
        m_history = nullptr;
        throw;
    }

    set_transact_stage(DB::transact_Writing);
    return true;
}

template <class O>
inline void Transaction::rollback_and_continue_as_read(O* observer)
{
    if (m_transact_stage != DB::transact_Writing)
        throw LogicError(LogicError::wrong_transact_state);

    Replication* repl = db->get_replication();
    if (!repl)
        throw LogicError(LogicError::no_history);

    BinaryData uncommitted_changes = repl->get_uncommitted_changes();

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

    // Mark all managed space (beyond the attached file) as free.
    db->reset_free_space_tracking(); // Throws

    ref_type top_ref = m_read_lock.m_top_ref;
    size_t file_size = m_read_lock.m_file_size;
    _impl::ReversedNoCopyInputStream reversed_in(reverser);
    advance_transact(top_ref, file_size, reversed_in, false); // Throws

    db->do_end_write();

    repl->abort_transact();

    m_history = nullptr;
    set_transact_stage(DB::transact_Reading);
}

template <class O>
inline bool Transaction::internal_advance_read(O* observer, VersionID version_id, _impl::History& hist, bool writable)
{
    DB::ReadLockInfo new_read_lock;
    db->grab_read_lock(new_read_lock, version_id); // Throws
    REALM_ASSERT(new_read_lock.m_version >= m_read_lock.m_version);
    if (new_read_lock.m_version == m_read_lock.m_version) {
        db->release_read_lock(new_read_lock);
        // _impl::History::update_early_from_top_ref() was not called
        // update allocator wrappers merely to update write protection
        update_allocator_wrappers(writable);
        return false;
    }

    DB::ReadLockGuard g(*db, new_read_lock);
    {
        DB::version_type new_version = new_read_lock.m_version;
        size_t new_file_size = new_read_lock.m_file_size;
        ref_type new_top_ref = new_read_lock.m_top_ref;

        // Synchronize readers view of the file
        SlabAlloc& alloc = m_alloc;
        alloc.update_reader_view(new_file_size);
        update_allocator_wrappers(writable);
        using gf = _impl::GroupFriend;
        // remap(new_file_size); // Throws
        ref_type hist_ref = gf::get_history_ref(alloc, new_top_ref);

        hist.update_from_ref_and_version(hist_ref, new_version);
    }

    if (observer) {
        // This has to happen in the context of the originally bound snapshot
        // and while the read transaction is still in a fully functional state.
        _impl::TransactLogParser parser;
        DB::version_type old_version = m_read_lock.m_version;
        DB::version_type new_version = new_read_lock.m_version;
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
        DB::version_type old_version = m_read_lock.m_version;
        DB::version_type new_version = new_read_lock.m_version;
        ref_type new_top_ref = new_read_lock.m_top_ref;
        size_t new_file_size = new_read_lock.m_file_size;
        _impl::ChangesetInputStream in(hist, old_version, new_version);
        advance_transact(new_top_ref, new_file_size, in, writable); // Throws
    }
    g.release();
    db->release_read_lock(m_read_lock);
    m_read_lock = new_read_lock;

    return true; // _impl::History::update_early_from_top_ref() was called
}

inline int DB::get_file_format_version() const noexcept
{
    return m_file_format_version;
}

} // namespace realm

#endif // REALM_GROUP_SHARED_HPP
