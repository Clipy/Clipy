////////////////////////////////////////////////////////////////////////////
//
// Copyright 2015 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#ifndef REALM_REALM_HPP
#define REALM_REALM_HPP

#include "execution_context_id.hpp"
#include "schema.hpp"

#include <realm/util/optional.hpp>
#include <realm/binary_data.hpp>

#if REALM_ENABLE_SYNC
#include <realm/sync/client.hpp>
#endif

#include <memory>

namespace realm {
class AsyncOpenTask;
class AuditInterface;
class BindingContext;
class Group;
class Realm;
class Replication;
class SharedGroup;
class StringData;
class Table;
struct SyncConfig;
class ThreadSafeReferenceBase;
template <typename T> class ThreadSafeReference;
struct VersionID;
template<typename Table> class BasicRow;
typedef BasicRow<Table> Row;
template<typename> class BasicRowExpr;
using RowExpr = BasicRowExpr<Table>;
typedef std::shared_ptr<Realm> SharedRealm;
typedef std::weak_ptr<Realm> WeakRealm;

namespace _impl {
    class AnyHandover;
    class CollectionNotifier;
    class PartialSyncHelper;
    class RealmCoordinator;
    class RealmFriend;
}
namespace sync {
    struct PermissionsCache;
    struct TableInfoCache;
}

// How to handle update_schema() being called on a file which has
// already been initialized with a different schema
enum class SchemaMode : uint8_t {
    // If the schema version has increased, automatically apply all
    // changes, then call the migration function.
    //
    // If the schema version has not changed, verify that the only
    // changes are to add new tables and add or remove indexes, and then
    // apply them if so. Does not call the migration function.
    //
    // This mode does not automatically remove tables which are not
    // present in the schema that must be manually done in the migration
    // function, to support sharing a Realm file between processes using
    // different class subsets.
    //
    // This mode allows using schemata with different subsets of tables
    // on different threads, but the tables which are shared must be
    // identical.
    Automatic,

    // Open the file in immutable mode. Schema version must match the
    // version in the file, and all tables present in the file must
    // exactly match the specified schema, except for indexes. Tables
    // are allowed to be missing from the file.
    // WARNING: This is the original ReadOnly mode.
    Immutable,

    // Open the Realm in read-only mode, transactions are not allowed to
    // be performed on the Realm instance. The schema of the existing Realm
    // file won't be changed through this Realm instance. Extra tables and
    // extra properties are allowed in the existing Realm schema. The
    // difference of indexes is allowed as well. Other schema differences
    // than those will cause an exception. This is different from Immutable
    // mode, sync Realm can be opened with ReadOnly mode. Changes
    // can be made to the Realm file through another writable Realm instance.
    // Thus, notifications are also allowed in this mode.
    // FIXME: Rename this to ReadOnly
    // WARNING: This is not the original ReadOnly mode. The original ReadOnly
    // has been renamed to Immutable.
    ReadOnlyAlternative,

    // If the schema version matches and the only schema changes are new
    // tables and indexes being added or removed, apply the changes to
    // the existing file.
    // Otherwise delete the file and recreate it from scratch.
    // The migration function is not used.
    //
    // This mode allows using schemata with different subsets of tables
    // on different threads, but the tables which are shared must be
    // identical.
    ResetFile,

    // The only changes allowed are to add new tables, add columns to
    // existing tables, and to add or remove indexes from existing
    // columns. Extra tables not present in the schema are ignored.
    // Indexes are only added to or removed from existing columns if the
    // schema version is greater than the existing one (and unlike other
    // modes, the schema version is allowed to be less than the existing
    // one).
    // The migration function is not used.
    //
    // This mode allows updating the schema with additive changes even
    // if the Realm is already open on another thread.
    Additive,

    // Verify that the schema version has increased, call the migraiton
    // function, and then verify that the schema now matches.
    // The migration function is mandatory for this mode.
    //
    // This mode requires that all threads and processes which open a
    // file use identical schemata.
    Manual
};

enum class ComputedPrivileges : uint8_t {
    None = 0,

    Read = (1 << 0),
    Update = (1 << 1),
    Delete = (1 << 2),
    SetPermissions = (1 << 3),
    Query = (1 << 4),
    Create = (1 << 5),
    ModifySchema = (1 << 6),

    AllRealm = Read | Update | SetPermissions | ModifySchema,
    AllClass = Read | Update | Create | Query | SetPermissions,
    AllObject = Read | Update | Delete | SetPermissions,
    All = (1 << 7) - 1
};

class Realm : public std::enable_shared_from_this<Realm> {
public:
    // A callback function to be called during a migration for Automatic and
    // Manual schema modes. It is passed a SharedRealm at the version before
    // the migration, the SharedRealm in the migration, and a mutable reference
    // to the realm's Schema. Updating the schema with changes made within the
    // migration function is only required if you wish to use the ObjectStore
    // functions which take a Schema from within the migration function.
    using MigrationFunction = std::function<void (SharedRealm old_realm, SharedRealm realm, Schema&)>;

    // A callback function to be called the first time when a schema is created.
    // It is passed a SharedRealm which is in a write transaction with the schema
    // initialized. So it is possible to create some initial objects inside the callback
    // with the given SharedRealm. Those changes will be committed together with the
    // schema creation in a single transaction.
    using DataInitializationFunction = std::function<void (SharedRealm realm)>;

    // A callback function called when opening a SharedRealm when no cached
    // version of this Realm exists. It is passed the total bytes allocated for
    // the file (file size) and the total bytes used by data in the file.
    // Return `true` to indicate that an attempt to compact the file should be made
    // if it is possible to do so.
    // Won't compact the file if another process is accessing it.
    //
    // WARNING / FIXME: compact() should NOT be exposed publicly on Windows
    // because it's not crash safe! It may corrupt your database if something fails
    using ShouldCompactOnLaunchFunction = std::function<bool (uint64_t total_bytes, uint64_t used_bytes)>;

    struct Config {
        // Path and binary data are mutually exclusive
        std::string path;
        BinaryData realm_data;
        // User-supplied encryption key. Must be either empty or 64 bytes.
        std::vector<char> encryption_key;

        // Core and Object Store will in some cases need to create named pipes alongside the Realm file.
        // But on some filesystems this can be a problem (e.g. external storage on Android that uses FAT32).
        // In order to work around this, a separate path can be specified for these files.
        std::string fifo_files_fallback_path;

        bool in_memory = false;
        SchemaMode schema_mode = SchemaMode::Automatic;

        // Optional schema for the file.
        // If the schema and schema version are supplied, update_schema() is
        // called with the supplied schema, version and migration function when
        // the Realm is actually opened and not just retrieved from the cache
        util::Optional<Schema> schema;
        uint64_t schema_version = -1;
        MigrationFunction migration_function;

        DataInitializationFunction initialization_function;

        // A callback function called when opening a SharedRealm when no cached
        // version of this Realm exists. It is passed the total bytes allocated for
        // the file (file size) and the total bytes used by data in the file.
        // Return `true` to indicate that an attempt to compact the file should be made
        // if it is possible to do so.
        // Won't compact the file if another process is accessing it.
        //
        // WARNING / FIXME: compact() should NOT be exposed publicly on Windows
        // because it's not crash safe! It may corrupt your database if something fails
        ShouldCompactOnLaunchFunction should_compact_on_launch_function;

        // WARNING: The original read_only() has been renamed to immutable().
        bool immutable() const { return schema_mode == SchemaMode::Immutable; }
        // FIXME: Rename this to read_only().
        bool read_only_alternative() const { return schema_mode == SchemaMode::ReadOnlyAlternative; }

        // The following are intended for internal/testing purposes and
        // should not be publicly exposed in binding APIs

        // If false, always return a new Realm instance, and don't return
        // that Realm instance for other requests for a cached Realm. Useful
        // for dynamic Realms and for tests that need multiple instances on
        // one thread
        bool cache = true;
        // Throw an exception rather than automatically upgrading the file
        // format. Used by the browser to warn the user that it'll modify
        // the file.
        bool disable_format_upgrade = false;
        // Disable the background worker thread for producing change
        // notifications. Useful for tests for those notifications so that
        // everything can be done deterministically on one thread, and
        // speeds up tests that don't need notifications.
        bool automatic_change_notifications = true;

        // The identifier of the abstract execution context in which this Realm will be used.
        // If unset, the current thread's identifier will be used to identify the execution context.
        util::Optional<AbstractExecutionContextID> execution_context;

        /// A data structure storing data used to configure the Realm for sync support.
        std::shared_ptr<SyncConfig> sync_config;

        // Open the Realm using the sync history mode even if a sync
        // configuration is not supplied.
        bool force_sync_history = false;

        // A factory function which produces an audit implementation.
        std::function<std::shared_ptr<AuditInterface>()> audit_factory;
    };

    // Get a cached Realm or create a new one if no cached copies exists
    // Caching is done by path - mismatches for in_memory, schema mode or
    // encryption key will raise an exception.
    static SharedRealm get_shared_realm(Config config);

    // Get a Realm for the given execution context (or current thread if `none`)
    // from the thread safe reference. May return a cached Realm or create a new one.
    static SharedRealm get_shared_realm(ThreadSafeReference<Realm>, util::Optional<AbstractExecutionContextID> = util::none);

#if REALM_ENABLE_SYNC
    // Open a synchronized Realm and make sure it is fully up to date before
    // returning it.
    //
    // It is possible to both cancel the download and listen to download progress
    // using the `AsyncOpenTask` returned. Note that the download doesn't actually
    // start until you call `AsyncOpenTask::start(callback)`
    static std::shared_ptr<AsyncOpenTask> get_synchronized_realm(Config config);
#endif

    // Updates a Realm to a given schema, using the Realm's pre-set schema mode.
    void update_schema(Schema schema, uint64_t version=0,
                       MigrationFunction migration_function=nullptr,
                       DataInitializationFunction initialization_function=nullptr,
                       bool in_transaction=false);

    // Set the schema used for this Realm, but do not update the file's schema
    // if it is not compatible (and instead throw an error).
    // Cannot be called multiple times on a single Realm instance or an instance
    // which has already had update_schema() called on it.
    void set_schema_subset(Schema schema);

    // Read the schema version from the file specified by the given config, or
    // ObjectStore::NotVersioned if it does not exist
    static uint64_t get_schema_version(Config const& config);

    Config const& config() const { return m_config; }
    Schema const& schema() const { return m_schema; }
    uint64_t schema_version() const { return m_schema_version; }

    // Returns `true` if this Realm is a Partially synchronized Realm.
    bool is_partial() const noexcept;

    void begin_transaction();
    void commit_transaction();
    void cancel_transaction();
    bool is_in_transaction() const noexcept;

    bool is_in_read_transaction() const { return !!m_group; }
    VersionID read_transaction_version() const;
    Group& read_group();

    bool is_in_migration() const noexcept { return m_in_migration; }

    bool refresh();
    void set_auto_refresh(bool auto_refresh) { m_auto_refresh = auto_refresh; }
    bool auto_refresh() const { return m_auto_refresh; }
    void notify();

    void invalidate();

    // WARNING / FIXME: compact() should NOT be exposed publicly on Windows
    // because it's not crash safe! It may corrupt your database if something fails
    bool compact();
    void write_copy(StringData path, BinaryData encryption_key);
    OwnedBinaryData write_copy();

    void verify_thread() const;
    void verify_in_write() const;
    void verify_open() const;

    bool can_deliver_notifications() const noexcept;

    // Close this Realm and remove it from the cache. Continuing to use a
    // Realm after closing it will throw ClosedRealmException
    void close();
    bool is_closed() const { return !m_read_only_group && !m_shared_group; }

    // returns the file format version upgraded from if an upgrade took place
    util::Optional<int> file_format_upgraded_from_version() const;

    Realm(const Realm&) = delete;
    Realm& operator=(const Realm&) = delete;
    Realm(Realm&&) = delete;
    Realm& operator=(Realm&&) = delete;
    ~Realm();

    // Construct a thread safe reference, pinning the version in the process.
    template <typename T>
    ThreadSafeReference<T> obtain_thread_safe_reference(T const& value);

    // Advances the read transaction to the latest version, resolving the thread safe reference and unpinning the
    // version in the process.
    template <typename T>
    T resolve_thread_safe_reference(ThreadSafeReference<T> reference);

    ComputedPrivileges get_privileges();
    ComputedPrivileges get_privileges(StringData object_type);
    ComputedPrivileges get_privileges(RowExpr row);

    AuditInterface* audit_context() const noexcept;

    static SharedRealm make_shared_realm(Config config, std::shared_ptr<_impl::RealmCoordinator> coordinator = nullptr) {
        struct make_shared_enabler : public Realm {
            make_shared_enabler(Config config, std::shared_ptr<_impl::RealmCoordinator> coordinator)
            : Realm(std::move(config), std::move(coordinator)) { }
        };
        return std::make_shared<make_shared_enabler>(std::move(config), std::move(coordinator));
    }

    // Expose some internal functionality to other parts of the ObjectStore
    // without making it public to everyone
    class Internal {
        friend class _impl::CollectionNotifier;
        friend class _impl::PartialSyncHelper;
        friend class _impl::RealmCoordinator;
        friend class ThreadSafeReferenceBase;
        friend class GlobalNotifier;
        friend class TestHelper;

        // ResultsNotifier and ListNotifier need access to the SharedGroup
        // to be able to call the handover functions, which are not very wrappable
        static const std::unique_ptr<SharedGroup>& get_shared_group(Realm& realm) { return realm.m_shared_group; }

        // CollectionNotifier needs to be able to access the owning
        // coordinator to wake up the worker thread when a callback is
        // added, and coordinators need to be able to get themselves from a Realm
        static _impl::RealmCoordinator& get_coordinator(Realm& realm) { return *realm.m_coordinator; }

        static void begin_read(Realm&, VersionID);
    };

    static void open_with_config(const Config& config,
                                 std::unique_ptr<Replication>& history,
                                 std::unique_ptr<SharedGroup>& shared_group,
                                 std::unique_ptr<Group>& read_only_group,
                                 Realm* realm);

private:
    // `enable_shared_from_this` is unsafe with public constructors; use `make_shared_realm` instead
    Realm(Config config, std::shared_ptr<_impl::RealmCoordinator> coordinator);

    Config m_config;
    AnyExecutionContextID m_execution_context;
    bool m_auto_refresh = true;

    std::unique_ptr<Replication> m_history;
    std::unique_ptr<SharedGroup> m_shared_group;
    std::unique_ptr<Group> m_read_only_group;

    Group *m_group = nullptr;

    uint64_t m_schema_version;
    Schema m_schema;
    util::Optional<Schema> m_new_schema;
    uint64_t m_schema_transaction_version = -1;

    // FIXME: this should be a Dynamic schema mode instead, but only once
    // that's actually fully working
    bool m_dynamic_schema = true;

    std::shared_ptr<_impl::RealmCoordinator> m_coordinator;
    std::unique_ptr<sync::TableInfoCache> m_table_info_cache;
    std::unique_ptr<sync::PermissionsCache> m_permissions_cache;

    // File format versions populated when a file format upgrade takes place during realm opening
    int upgrade_initial_version = 0, upgrade_final_version = 0;

    // True while sending the notifications caused by advancing the read
    // transaction version, to avoid recursive notifications where possible
    bool m_is_sending_notifications = false;

    // True while we're performing a schema migration via this Realm instance
    // to allow for different behavior (such as allowing modifications to
    // primary key values)
    bool m_in_migration = false;

    void begin_read(VersionID);

    void set_schema(Schema const& reference, Schema schema);
    bool reset_file(Schema& schema, std::vector<SchemaChange>& changes_required);
    bool schema_change_needs_write_transaction(Schema& schema, std::vector<SchemaChange>& changes, uint64_t version);
    Schema get_full_schema();

    // Ensure that m_schema and m_schema_version match that of the current
    // version of the file
    void read_schema_from_group_if_needed();

    void add_schema_change_handler();
    void cache_new_schema();
    void translate_schema_error();
    void notify_schema_changed();

    bool init_permission_cache();
    void invalidate_permission_cache();

public:
    std::unique_ptr<BindingContext> m_binding_context;

    // FIXME: This is currently needed by the adapter to get access to its changeset cooker
    Replication* history() { return m_history.get(); }

    friend class _impl::RealmFriend;
};

class RealmFileException : public std::runtime_error {
public:
    enum class Kind {
        /** Thrown for any I/O related exception scenarios when a realm is opened. */
        AccessError,
        /** Thrown if the history type of the on-disk Realm is unexpected or incompatible. */
        BadHistoryError,
        /** Thrown if the user does not have permission to open or create
         the specified file in the specified access mode when the realm is opened. */
        PermissionDenied,
        /** Thrown if create_Always was specified and the file did already exist when the realm is opened. */
        Exists,
        /** Thrown if no_create was specified and the file was not found when the realm is opened. */
        NotFound,
        /** Thrown if the database file is currently open in another
         process which cannot share with the current process due to an
         architecture mismatch. */
        IncompatibleLockFile,
        /** Thrown if the file needs to be upgraded to a new format, but upgrades have been explicitly disabled. */
        FormatUpgradeRequired,
        /** Thrown if the local copy of a synced Realm file was created using an incompatible version of Realm.
         The specified path is where the local file was moved for recovery. */
        IncompatibleSyncedRealm,
    };
    RealmFileException(Kind kind, std::string path, std::string message, std::string underlying)
    : std::runtime_error(std::move(message)), m_kind(kind), m_path(std::move(path)), m_underlying(std::move(underlying)) {}
    Kind kind() const { return m_kind; }
    const std::string& path() const { return m_path; }
    const std::string& underlying() const { return m_underlying; }

private:
    Kind m_kind;
    std::string m_path;
    std::string m_underlying;
};

class MismatchedConfigException : public std::logic_error {
public:
    MismatchedConfigException(StringData message, StringData path);
};

class MismatchedRealmException : public std::logic_error {
public:
    MismatchedRealmException(StringData message);
};

class InvalidTransactionException : public std::logic_error {
public:
    InvalidTransactionException(std::string message) : std::logic_error(message) {}
};

class IncorrectThreadException : public std::logic_error {
public:
    IncorrectThreadException() : std::logic_error("Realm accessed from incorrect thread.") {}
};

class ClosedRealmException : public std::logic_error {
public:
    ClosedRealmException() : std::logic_error("Cannot access realm that has been closed.") {}
};

class UninitializedRealmException : public std::runtime_error {
public:
    UninitializedRealmException(std::string message) : std::runtime_error(message) {}
};

class InvalidEncryptionKeyException : public std::logic_error {
public:
    InvalidEncryptionKeyException() : std::logic_error("Encryption key must be 64 bytes.") {}
};

// FIXME Those are exposed for Java async queries, mainly because of handover related methods.
class _impl::RealmFriend {
public:
    static SharedGroup& get_shared_group(Realm& realm);
    static Group& read_group_to(Realm& realm, VersionID version);
};

} // namespace realm

#endif /* defined(REALM_REALM_HPP) */
