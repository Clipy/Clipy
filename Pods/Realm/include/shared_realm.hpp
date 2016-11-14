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

#include "schema.hpp"

#include <realm/util/optional.hpp>
#include <realm/version_id.hpp>

#if REALM_ENABLE_SYNC
#include <realm/sync/client.hpp>
#endif

#include <memory>
#include <thread>

namespace realm {
class AnyThreadConfined;
class BinaryData;
class BindingContext;
class Group;
class Realm;
class Replication;
class SharedGroup;
class StringData;
struct SyncConfig;
struct VersionID;
typedef std::shared_ptr<Realm> SharedRealm;
typedef std::weak_ptr<Realm> WeakRealm;

namespace _impl {
    class AnyHandover;
    class CollectionNotifier;
    class ListNotifier;
    class RealmCoordinator;
    class ResultsNotifier;
}

// How to handle update_schema() being called on a file which has
// already been initialized with a different schema
enum class SchemaMode : uint8_t {
    // If the schema version has increased, automatically apply all
    // changes, then call the migration function.
    //
    // If the schema version has not changed, verify that the only
    // changes are to add new tables and add or remvoe indexes, and then
    // apply them if so. Does not call the migration function.
    //
    // This mode does not automatically remove tables which are not
    // present in the schea; that must be manually done in the migration
    // function, to support sharing a Realm file between processes using
    // different class subsets.
    //
    // This mode allows using schemata with different subsets of tables
    // on different threads, but the tables which are shared must be
    // identical.
    Automatic,

    // Open the file in read-only mode. Schema version must match the
    // version in the file, and all tables present in the file must
    // exactly match the specified schema, except for indexes. Tables
    // are allowed to be missing from the file.
    ReadOnly,

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

class Realm : public std::enable_shared_from_this<Realm> {
public:
    class HandoverPackage;

    // A callback function to be called during a migration for Automatic and
    // Manual schema modes. It is passed a SharedRealm at the version before
    // the migration, the SharedRealm in the migration, and a mutable reference
    // to the realm's Schema. Updating the schema with changes made within the
    // migration function is only required if you wish to use the ObjectStore
    // functions which take a Schema from within the migration function.
    using MigrationFunction = std::function<void (SharedRealm old_realm, SharedRealm realm, Schema&)>;

    struct Config {
        std::string path;
        // User-supplied encryption key. Must be either empty or 64 bytes.
        std::vector<char> encryption_key;

        bool in_memory = false;
        SchemaMode schema_mode = SchemaMode::Automatic;

        // Optional schema for the file.
        // If the schema and schema version are supplied, update_schema() is
        // called with the supplied schema, version and migration function when
        // the Realm is actually opened and not just retreived from the cache
        util::Optional<Schema> schema;
        uint64_t schema_version = -1;
        MigrationFunction migration_function;

        bool read_only() const { return schema_mode == SchemaMode::ReadOnly; }

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

        /// A data structure storing data used to configure the Realm for sync support.
        std::shared_ptr<SyncConfig> sync_config;
    };

    // Get a cached Realm or create a new one if no cached copies exists
    // Caching is done by path - mismatches for in_memory, schema mode or
    // encryption key will raise an exception.
    static SharedRealm get_shared_realm(Config config);

    // Updates a Realm to a given schema, using the Realm's pre-set schema mode.
    void update_schema(Schema schema, uint64_t version=0,
                       MigrationFunction migration_function=nullptr);

    // Read the schema version from the file specified by the given config, or
    // ObjectStore::NotVersioned if it does not exist
    static uint64_t get_schema_version(Config const& config);

    Config const& config() const { return m_config; }
    Schema const& schema() const { return m_schema; }
    uint64_t schema_version() const { return m_schema_version; }

    void begin_transaction();
    void commit_transaction();
    void cancel_transaction();
    bool is_in_transaction() const noexcept;
    bool is_in_read_transaction() const { return !!m_group; }

    bool refresh();
    void set_auto_refresh(bool auto_refresh) { m_auto_refresh = auto_refresh; }
    bool auto_refresh() const { return m_auto_refresh; }
    void notify();

    void invalidate();
    bool compact();
    void write_copy(StringData path, BinaryData encryption_key);

    std::thread::id thread_id() const { return m_thread_id; }
    void verify_thread() const;
    void verify_in_write() const;

    bool can_deliver_notifications() const noexcept;

    // Close this Realm and remove it from the cache. Continuing to use a
    // Realm after closing it will produce undefined behavior.
    void close();
    bool is_closed() { return !m_read_only_group && !m_shared_group; }

    // returns the file format version upgraded from if an upgrade took place
    util::Optional<int> file_format_upgraded_from_version() const;

    Realm(const Realm&) = delete;
    Realm& operator=(const Realm&) = delete;
    Realm(Realm&&) = delete;
    Realm& operator=(Realm&&) = delete;
    ~Realm();
    
    // Pins the current version and exports each object for handover.
    HandoverPackage package_for_handover(std::vector<AnyThreadConfined> objects_to_hand_over);

    // Unpins the handover version, ending the current read transaction and beginning a new one at this version,
    // importing each object for handover.
    std::vector<AnyThreadConfined> accept_handover(Realm::HandoverPackage handover);

    // Opaque type representing a vector of packaged objects for handover
    class HandoverPackage {
    public:
        HandoverPackage(const HandoverPackage&) = delete;
        HandoverPackage& operator=(const HandoverPackage&) = delete;
        HandoverPackage(HandoverPackage&&);
        HandoverPackage& operator=(HandoverPackage&&);
        ~HandoverPackage();

        bool is_awaiting_import() const { return m_source_realm != nullptr; };

    private:
        friend HandoverPackage Realm::package_for_handover(std::vector<AnyThreadConfined> objects_to_hand_over);
        friend std::vector<AnyThreadConfined> Realm::accept_handover(Realm::HandoverPackage handover);

        VersionID m_version_id;
        std::vector<_impl::AnyHandover> m_objects;
        SharedRealm m_source_realm; // Strong reference keeps alive so version stays pinned! Don't touch!!

        HandoverPackage() = default;

        _impl::RealmCoordinator& get_coordinator() const { return *m_source_realm->m_coordinator; }
        void mark_not_awaiting_import() { m_source_realm = nullptr; };
        void advance_to_version(VersionID version);
    };

    static SharedRealm make_shared_realm(Config config) {
        struct make_shared_enabler : public Realm {
            make_shared_enabler(Config config) : Realm(std::move(config)) {}
        };
        return std::make_shared<make_shared_enabler>(std::move(config));
    }
    void init(std::shared_ptr<_impl::RealmCoordinator> coordinator);

    // Expose some internal functionality to other parts of the ObjectStore
    // without making it public to everyone
    class Internal {
        friend class AnyThreadConfined;
        friend class _impl::CollectionNotifier;
        friend class _impl::ListNotifier;
        friend class _impl::RealmCoordinator;
        friend class _impl::ResultsNotifier;
        friend class _impl::AnyHandover;

        // ResultsNotifier and ListNotifier need access to the SharedGroup
        // to be able to call the handover functions, which are not very wrappable
        static SharedGroup& get_shared_group(Realm& realm) { return *realm.m_shared_group; }

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
    Realm(Config config);

    Config m_config;
    std::thread::id m_thread_id = std::this_thread::get_id();
    bool m_auto_refresh = true;

    std::unique_ptr<Replication> m_history;
    std::unique_ptr<SharedGroup> m_shared_group;
    std::unique_ptr<Group> m_read_only_group;

    Group *m_group = nullptr;

    uint64_t m_schema_version;
    Schema m_schema;
    uint64_t m_schema_transaction_version = -1;

    std::shared_ptr<_impl::RealmCoordinator> m_coordinator;

    // File format versions populated when a file format upgrade takes place during realm opening
    int upgrade_initial_version = 0, upgrade_final_version = 0;

    void set_schema(Schema schema, uint64_t version);
    bool reset_file_if_needed(Schema& schema, uint64_t version, std::vector<SchemaChange>& changes_required);

    // Ensure that m_schema and m_schema_version match that of the current
    // version of the file, and return true if it changed
    bool read_schema_from_group_if_needed();

    void add_schema_change_handler();

public:
    std::unique_ptr<BindingContext> m_binding_context;

    // FIXME private
    Group& read_group();
};

class RealmFileException : public std::runtime_error {
public:
    enum class Kind {
        /** Thrown for any I/O related exception scenarios when a realm is opened. */
        AccessError,
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

class InvalidTransactionException : public std::logic_error {
public:
    InvalidTransactionException(std::string message) : std::logic_error(message) {}
};

class IncorrectThreadException : public std::logic_error {
public:
    IncorrectThreadException() : std::logic_error("Realm accessed from incorrect thread.") {}
};

class UninitializedRealmException : public std::runtime_error {
public:
    UninitializedRealmException(std::string message) : std::runtime_error(message) {}
};

class InvalidEncryptionKeyException : public std::logic_error {
public:
    InvalidEncryptionKeyException() : std::logic_error("Encryption key must be 64 bytes.") {}
};
} // namespace realm

#endif /* defined(REALM_REALM_HPP) */
