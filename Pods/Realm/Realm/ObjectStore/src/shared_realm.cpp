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

#include "shared_realm.hpp"

#include "impl/collection_notifier.hpp"
#include "impl/realm_coordinator.hpp"
#include "impl/transact_log_handler.hpp"

#include "binding_context.hpp"
#include "list.hpp"
#include "object.hpp"
#include "object_schema.hpp"
#include "object_store.hpp"
#include "results.hpp"
#include "schema.hpp"
#include "thread_safe_reference.hpp"

#include "util/compiler.hpp"
#include "util/format.hpp"

#include <realm/history.hpp>
#include <realm/util/scope_exit.hpp>

#if REALM_ENABLE_SYNC
#include <realm/sync/history.hpp>
#endif

using namespace realm;
using namespace realm::_impl;

Realm::Realm(Config config, std::shared_ptr<_impl::RealmCoordinator> coordinator)
: m_config(std::move(config))
, m_execution_context(m_config.execution_context)
{
    open_with_config(m_config, m_history, m_shared_group, m_read_only_group, this);

    if (m_read_only_group) {
        m_group = m_read_only_group.get();
        m_schema_version = ObjectStore::get_schema_version(*m_group);
        m_schema = ObjectStore::schema_from_group(*m_group);
    }
    else if (!coordinator || !coordinator->get_cached_schema(m_schema, m_schema_version, m_schema_transaction_version)) {
        if (m_config.should_compact_on_launch_function) {
            size_t free_space = -1;
            size_t used_space = -1;
            // getting stats requires committing a write transaction beforehand.
            Group* group = nullptr;
            if (m_shared_group->try_begin_write(group)) {
                m_shared_group->commit();
                m_shared_group->get_stats(free_space, used_space);
                if (m_config.should_compact_on_launch_function(free_space + used_space, used_space))
                    compact();
            }
        }
        read_group();
        if (coordinator)
            coordinator->cache_schema(m_schema, m_schema_version, m_schema_transaction_version);
        m_shared_group->end_read();
        m_group = nullptr;
    }

    m_coordinator = std::move(coordinator);
}

REALM_NOINLINE static void translate_file_exception(StringData path, bool immutable=false)
{
    try {
        throw;
    }
    catch (util::File::PermissionDenied const& ex) {
        throw RealmFileException(RealmFileException::Kind::PermissionDenied, ex.get_path(),
                                 util::format("Unable to open a realm at path '%1'. Please use a path where your app has %2 permissions.",
                                              ex.get_path(), immutable ? "read" : "read-write"),
                                 ex.what());
    }
    catch (util::File::Exists const& ex) {
        throw RealmFileException(RealmFileException::Kind::Exists, ex.get_path(),
                                 util::format("File at path '%1' already exists.", ex.get_path()),
                                 ex.what());
    }
    catch (util::File::NotFound const& ex) {
        throw RealmFileException(RealmFileException::Kind::NotFound, ex.get_path(),
                                 util::format("Directory at path '%1' does not exist.", ex.get_path()), ex.what());
    }
    catch (util::File::AccessError const& ex) {
        // Errors for `open()` include the path, but other errors don't. We
        // don't want two copies of the path in the error, so strip it out if it
        // appears, and then include it in our prefix.
        std::string underlying = ex.what();
        RealmFileException::Kind error_kind = RealmFileException::Kind::AccessError;
        // FIXME: Replace this with a proper specific exception type once Core adds support for it.
        if (underlying == "Bad or incompatible history type")
            error_kind = RealmFileException::Kind::BadHistoryError;
        auto pos = underlying.find(ex.get_path());
        if (pos != std::string::npos && pos > 0) {
            // One extra char at each end for the quotes
            underlying.replace(pos - 1, ex.get_path().size() + 2, "");
        }
        throw RealmFileException(error_kind, ex.get_path(),
                                 util::format("Unable to open a realm at path '%1': %2.", ex.get_path(), underlying), ex.what());
    }
    catch (IncompatibleLockFile const& ex) {
        throw RealmFileException(RealmFileException::Kind::IncompatibleLockFile, path,
                                 "Realm file is currently open in another process "
                                 "which cannot share access with this process. "
                                 "All processes sharing a single file must be the same architecture.",
                                 ex.what());
    }
    catch (FileFormatUpgradeRequired const& ex) {
        throw RealmFileException(RealmFileException::Kind::FormatUpgradeRequired, path,
                                 "The Realm file format must be allowed to be upgraded "
                                 "in order to proceed.",
                                 ex.what());
    }
}

void Realm::open_with_config(const Config& config,
                             std::unique_ptr<Replication>& history,
                             std::unique_ptr<SharedGroup>& shared_group,
                             std::unique_ptr<Group>& read_only_group,
                             Realm* realm)
{
    try {
        if (config.immutable()) {
            if (config.realm_data.is_null()) {
                read_only_group = std::make_unique<Group>(config.path, config.encryption_key.data(), Group::mode_ReadOnly);
            }
            else {
                // Create in-memory read-only realm from existing buffer (without taking ownership of the buffer)
                read_only_group = std::make_unique<Group>(config.realm_data, false);
            }
        }
        else {
            bool server_synchronization_mode = bool(config.sync_config) || config.force_sync_history;
            if (server_synchronization_mode) {
#if REALM_ENABLE_SYNC
                history = realm::sync::make_client_history(config.path);
#else
                REALM_TERMINATE("Realm was not built with sync enabled");
#endif
            }
            else {
                history = realm::make_in_realm_history(config.path);
            }

            SharedGroupOptions options;
            options.durability = config.in_memory ? SharedGroupOptions::Durability::MemOnly :
                                                    SharedGroupOptions::Durability::Full;
            options.encryption_key = config.encryption_key.data();
            options.allow_file_format_upgrade = !config.disable_format_upgrade &&
                                                config.schema_mode != SchemaMode::ResetFile;
            options.upgrade_callback = [&](int from_version, int to_version) {
                if (realm) {
                    realm->upgrade_initial_version = from_version;
                    realm->upgrade_final_version = to_version;
                }
            };
            shared_group = std::make_unique<SharedGroup>(*history, options);
        }
    }
    catch (realm::FileFormatUpgradeRequired const&) {
        if (config.schema_mode != SchemaMode::ResetFile) {
            translate_file_exception(config.path, config.immutable());
        }
        util::File::remove(config.path);
        open_with_config(config, history, shared_group, read_only_group, realm);
    }
    catch (...) {
        translate_file_exception(config.path, config.immutable());
    }
}

Realm::~Realm()
{
    if (m_coordinator) {
        m_coordinator->unregister_realm(this);
    }
}

Group& Realm::read_group()
{
    verify_open();

    if (!m_group)
        begin_read(VersionID{});
    return *m_group;
}

void Realm::Internal::begin_read(Realm& realm, VersionID version_id)
{
    realm.begin_read(version_id);
}

void Realm::begin_read(VersionID version_id)
{
    REALM_ASSERT(!m_group);
    m_group = &const_cast<Group&>(m_shared_group->begin_read(version_id));
    add_schema_change_handler();
    read_schema_from_group_if_needed();
}

SharedRealm Realm::get_shared_realm(Config config)
{
    auto coordinator = RealmCoordinator::get_coordinator(config.path);
    return coordinator->get_realm(std::move(config));
}

void Realm::set_schema(Schema const& reference, Schema schema)
{
    m_dynamic_schema = false;
    schema.copy_table_columns_from(reference);
    m_schema = std::move(schema);
    notify_schema_changed();
}

void Realm::read_schema_from_group_if_needed()
{
    REALM_ASSERT(!m_read_only_group);

    Group& group = read_group();
    auto current_version = m_shared_group->get_version_of_current_transaction().version;
    if (m_schema_transaction_version == current_version)
        return;

    m_schema_transaction_version = current_version;
    m_schema_version = ObjectStore::get_schema_version(group);
    auto schema = ObjectStore::schema_from_group(group);
    if (m_coordinator)
        m_coordinator->cache_schema(schema, m_schema_version,
                                    m_schema_transaction_version);

    if (m_dynamic_schema) {
        if (m_schema == schema) {
            // The structure of the schema hasn't changed. Bring the table column indices up to date.
            m_schema.copy_table_columns_from(schema);
        }
        else {
            // The structure of the schema has changed, so replace our copy of the schema.
            // FIXME: This invalidates any pointers to the object schemas within the schema vector,
            // which will cause problems for anyone that caches such a pointer.
            m_schema = std::move(schema);
        }
    }
    else {
        ObjectStore::verify_valid_external_changes(m_schema.compare(schema));
        m_schema.copy_table_columns_from(schema);
    }
    notify_schema_changed();
}

bool Realm::reset_file(Schema& schema, std::vector<SchemaChange>& required_changes)
{
    // FIXME: this does not work if multiple processes try to open the file at
    // the same time, or even multiple threads if there is not any external
    // synchronization. The latter is probably fixable, but making it
    // multi-process-safe requires some sort of multi-process exclusive lock
    m_group = nullptr;
    m_shared_group = nullptr;
    m_history = nullptr;
    util::File::remove(m_config.path);

    open_with_config(m_config, m_history, m_shared_group, m_read_only_group, this);
    m_schema = ObjectStore::schema_from_group(read_group());
    m_schema_version = ObjectStore::get_schema_version(read_group());
    required_changes = m_schema.compare(schema);
    m_coordinator->clear_schema_cache_and_set_schema_version(m_schema_version);
    return false;
}

bool Realm::schema_change_needs_write_transaction(Schema& schema,
                                                  std::vector<SchemaChange>& changes,
                                                  uint64_t version)
{
    if (version == m_schema_version && changes.empty())
        return false;

    switch (m_config.schema_mode) {
        case SchemaMode::Automatic:
            if (version < m_schema_version && m_schema_version != ObjectStore::NotVersioned)
                throw InvalidSchemaVersionException(m_schema_version, version);
            return true;

        case SchemaMode::Immutable:
            if (version != m_schema_version)
                throw InvalidSchemaVersionException(m_schema_version, version);
            REALM_FALLTHROUGH;
        case SchemaMode::ReadOnlyAlternative:
            ObjectStore::verify_compatible_for_immutable_and_readonly(changes);
            return false;

        case SchemaMode::ResetFile:
            if (m_schema_version == ObjectStore::NotVersioned)
                return true;
            if (m_schema_version == version && !ObjectStore::needs_migration(changes))
                return true;
            reset_file(schema, changes);
            return true;

        case SchemaMode::Additive: {
            bool will_apply_index_changes = version > m_schema_version;
            if (ObjectStore::verify_valid_additive_changes(changes, will_apply_index_changes))
                return true;
            return version != m_schema_version;
        }

        case SchemaMode::Manual:
            if (version < m_schema_version && m_schema_version != ObjectStore::NotVersioned)
                throw InvalidSchemaVersionException(m_schema_version, version);
            if (version == m_schema_version) {
                ObjectStore::verify_no_changes_required(changes);
                REALM_UNREACHABLE(); // changes is non-empty so above line always throws
            }
            return true;
    }
    REALM_COMPILER_HINT_UNREACHABLE();
}

Schema Realm::get_full_schema()
{
    if (!m_read_only_group)
        refresh();

    // If the user hasn't specified a schema previously then m_schema is always
    // the full schema
    if (m_dynamic_schema)
        return m_schema;

    // Otherwise we may have a subset of the file's schema, so we need to get
    // the complete thing to calculate what changes to make
    if (m_read_only_group)
        return ObjectStore::schema_from_group(read_group());

    Schema actual_schema;
    uint64_t actual_version;
    uint64_t transaction = -1;
    bool got_cached = m_coordinator->get_cached_schema(actual_schema, actual_version, transaction);
    if (!got_cached || transaction != m_shared_group->get_version_of_current_transaction().version)
        return ObjectStore::schema_from_group(read_group());
    return actual_schema;
}

void Realm::set_schema_subset(Schema schema)
{
    REALM_ASSERT(m_dynamic_schema);
    REALM_ASSERT(m_schema_version != ObjectStore::NotVersioned);

    std::vector<SchemaChange> changes = m_schema.compare(schema);
    switch (m_config.schema_mode) {
        case SchemaMode::Automatic:
        case SchemaMode::ResetFile:
            ObjectStore::verify_no_migration_required(changes);
            break;

        case SchemaMode::Immutable:
        case SchemaMode::ReadOnlyAlternative:
            ObjectStore::verify_compatible_for_immutable_and_readonly(changes);
            break;

        case SchemaMode::Additive:
            ObjectStore::verify_valid_additive_changes(changes);
            break;

        case SchemaMode::Manual:
            ObjectStore::verify_no_changes_required(changes);
            break;
    }

    set_schema(m_schema, std::move(schema));
}

void Realm::update_schema(Schema schema, uint64_t version, MigrationFunction migration_function,
                          DataInitializationFunction initialization_function, bool in_transaction)
{
    schema.validate();

    Schema actual_schema = get_full_schema();
    std::vector<SchemaChange> required_changes = actual_schema.compare(schema);

    if (!schema_change_needs_write_transaction(schema, required_changes, version)) {
        set_schema(actual_schema, std::move(schema));
        return;
    }
    // Either the schema version has changed or we need to do non-migration changes

    if (!in_transaction) {
        transaction::begin_without_validation(*m_shared_group);

        // Beginning the write transaction may have advanced the version and left
        // us with nothing to do if someone else initialized the schema on disk
        if (m_new_schema) {
            actual_schema = *m_new_schema;
            required_changes = actual_schema.compare(schema);
            if (!schema_change_needs_write_transaction(schema, required_changes, version)) {
                cancel_transaction();
                cache_new_schema();
                set_schema(actual_schema, std::move(schema));
                return;
            }
        }
        cache_new_schema();
    }

    // Cancel the write transaction if we exit this function before committing it
    auto cleanup = util::make_scope_exit([&]() noexcept {
        // When in_transaction is true, caller is responsible to cancel the transaction.
        if (!in_transaction && is_in_transaction())
            cancel_transaction();
    });

    uint64_t old_schema_version = m_schema_version;
    bool additive = m_config.schema_mode == SchemaMode::Additive;
    if (migration_function && !additive) {
        auto wrapper = [&] {
            SharedRealm old_realm(new Realm(m_config, nullptr));
            // Need to open in read-write mode so that it uses a SharedGroup, but
            // users shouldn't actually be able to write via the old realm
            old_realm->m_config.schema_mode = SchemaMode::Immutable;
            migration_function(old_realm, shared_from_this(), m_schema);
        };

        // migration function needs to see the target schema on the "new" Realm
        std::swap(m_schema, schema);
        std::swap(m_schema_version, version);
        m_in_migration = true;
        auto restore = util::make_scope_exit([&]() noexcept {
            std::swap(m_schema, schema);
            std::swap(m_schema_version, version);
            m_in_migration = false;
        });

        ObjectStore::apply_schema_changes(read_group(), version, m_schema, m_schema_version,
                                          m_config.schema_mode, required_changes, wrapper);
    }
    else {
        ObjectStore::apply_schema_changes(read_group(), m_schema_version, schema, version,
                                          m_config.schema_mode, required_changes);
        REALM_ASSERT_DEBUG(additive || (required_changes = ObjectStore::schema_from_group(read_group()).compare(schema)).empty());
    }

    if (initialization_function && old_schema_version == ObjectStore::NotVersioned) {
        // Initialization function needs to see the latest schema
        uint64_t temp_version = ObjectStore::get_schema_version(read_group());
        std::swap(m_schema, schema);
        std::swap(m_schema_version, temp_version);
        auto restore = util::make_scope_exit([&]() noexcept {
            std::swap(m_schema, schema);
            std::swap(m_schema_version, temp_version);
        });
        initialization_function(shared_from_this());
    }

    if (!in_transaction) {
        commit_transaction();
    }

    m_schema = std::move(schema);
    m_schema_version = ObjectStore::get_schema_version(read_group());
    m_dynamic_schema = false;
    m_coordinator->clear_schema_cache_and_set_schema_version(version);
    notify_schema_changed();
}

void Realm::add_schema_change_handler()
{
    if (m_config.immutable())
        return;
    m_group->set_schema_change_notification_handler([&] {
        m_new_schema = ObjectStore::schema_from_group(read_group());
        m_schema_version = ObjectStore::get_schema_version(read_group());
        if (m_dynamic_schema) {
            // FIXME: This invalidates any pointers to the object schemas within the schema vector,
            // which will cause problems for anyone that caches such a pointer.
            m_schema = *m_new_schema;
        }
        else
            m_schema.copy_table_columns_from(*m_new_schema);

        notify_schema_changed();
    });
}

void Realm::cache_new_schema()
{
    if (!m_shared_group)
        return;

    auto new_version = m_shared_group->get_version_of_current_transaction().version;
    if (m_coordinator) {
        if (m_new_schema)
            m_coordinator->cache_schema(std::move(*m_new_schema), m_schema_version, new_version);
        else
            m_coordinator->advance_schema_cache(m_schema_transaction_version, new_version);
    }
    m_schema_transaction_version = new_version;
    m_new_schema = util::none;
}

void Realm::notify_schema_changed() {
    if (m_binding_context) {
        m_binding_context->schema_did_change(m_schema);
    }
}

static void check_read_write(Realm *realm)
{
    if (realm->config().immutable()) {
        throw InvalidTransactionException("Can't perform transactions on read-only Realms.");
    }
}

static void check_write(Realm* realm)
{
    if (realm->config().immutable() || realm->config().read_only_alternative()) {
        throw InvalidTransactionException("Can't perform transactions on read-only Realms.");
    }
}

void Realm::verify_thread() const
{
    if (!m_execution_context.contains<std::thread::id>())
        return;

    auto thread_id = m_execution_context.get<std::thread::id>();
    if (thread_id != std::this_thread::get_id())
        throw IncorrectThreadException();
}

void Realm::verify_in_write() const
{
    if (!is_in_transaction()) {
        throw InvalidTransactionException("Cannot modify managed objects outside of a write transaction.");
    }
}

void Realm::verify_open() const
{
    if (is_closed()) {
        throw ClosedRealmException();
    }
}

bool Realm::is_in_transaction() const noexcept
{
    if (!m_shared_group) {
        return false;
    }
    return m_shared_group->get_transact_stage() == SharedGroup::transact_Writing;
}

void Realm::begin_transaction()
{
    check_write(this);
    verify_thread();

    if (is_in_transaction()) {
        throw InvalidTransactionException("The Realm is already in a write transaction");
    }

    // Any of the callbacks to user code below could drop the last remaining
    // strong reference to `this`
    auto retain_self = shared_from_this();

    // If we're already in the middle of sending notifications, just begin the
    // write transaction without sending more notifications. If this actually
    // advances the read version this could leave the user in an inconsistent
    // state, but that's unavoidable.
    if (m_is_sending_notifications) {
        _impl::NotifierPackage notifiers;
        transaction::begin(m_shared_group, m_binding_context.get(), notifiers);
        return;
    }

    // make sure we have a read transaction
    read_group();

    m_is_sending_notifications = true;
    auto cleanup = util::make_scope_exit([this]() noexcept { m_is_sending_notifications = false; });

    m_coordinator->promote_to_write(*this);
    cache_new_schema();
}

void Realm::commit_transaction()
{
    check_write(this);
    verify_thread();

    if (!is_in_transaction()) {
        throw InvalidTransactionException("Can't commit a non-existing write transaction");
    }

    m_coordinator->commit_write(*this);
    cache_new_schema();
}

void Realm::cancel_transaction()
{
    check_write(this);
    verify_thread();

    if (!is_in_transaction()) {
        throw InvalidTransactionException("Can't cancel a non-existing write transaction");
    }

    transaction::cancel(*m_shared_group, m_binding_context.get());
}

void Realm::invalidate()
{
    verify_open();
    verify_thread();
    check_read_write(this);

    if (m_is_sending_notifications) {
        return;
    }

    if (is_in_transaction()) {
        cancel_transaction();
    }
    if (!m_group) {
        return;
    }

    m_shared_group->end_read();
    m_group = nullptr;
}

bool Realm::compact()
{
    verify_thread();

    if (m_config.immutable() || m_config.read_only_alternative()) {
        throw InvalidTransactionException("Can't compact a read-only Realm");
    }
    if (is_in_transaction()) {
        throw InvalidTransactionException("Can't compact a Realm within a write transaction");
    }

    Group& group = read_group();
    for (auto &object_schema : m_schema) {
        ObjectStore::table_for_object_type(group, object_schema.name)->optimize();
    }
    m_shared_group->end_read();
    m_group = nullptr;

    return m_shared_group->compact();
}

void Realm::write_copy(StringData path, BinaryData key)
{
    if (key.data() && key.size() != 64) {
        throw InvalidEncryptionKeyException();
    }
    verify_thread();
    try {
        read_group().write(path, key.data());
    }
    catch (...) {
        translate_file_exception(path);
    }
}

OwnedBinaryData Realm::write_copy()
{
    verify_thread();
    BinaryData buffer = read_group().write_to_mem();

    // Since OwnedBinaryData does not have a constructor directly taking
    // ownership of BinaryData, we have to do this to avoid copying the buffer
    return OwnedBinaryData(std::unique_ptr<char[]>((char*)buffer.data()), buffer.size());
}

void Realm::notify()
{
    if (is_closed() || is_in_transaction()) {
        return;
    }

    verify_thread();

    // Any of the callbacks to user code below could drop the last remaining
    // strong reference to `this`
    auto retain_self = shared_from_this();

    if (m_binding_context) {
        m_binding_context->before_notify();
    }

    auto cleanup = util::make_scope_exit([this]() noexcept { m_is_sending_notifications = false; });
    if (!m_shared_group->has_changed()) {
        m_is_sending_notifications = true;
        m_coordinator->process_available_async(*this);
        return;
    }

    if (m_binding_context) {
        m_binding_context->changes_available();

        // changes_available() may have advanced the read version, and if
        // so we don't need to do anything further
        if (!m_shared_group->has_changed())
            return;
    }

    m_is_sending_notifications = true;
    if (m_auto_refresh) {
        if (m_group) {
            m_coordinator->advance_to_ready(*this);
            cache_new_schema();
        }
        else  {
            if (m_binding_context) {
                m_binding_context->did_change({}, {});
            }
            if (!is_closed()) {
                m_coordinator->process_available_async(*this);
            }
        }
    }
}

bool Realm::refresh()
{
    verify_thread();
    check_read_write(this);

    // can't be any new changes if we're in a write transaction
    if (is_in_transaction()) {
        return false;
    }
    // don't advance if we're already in the process of advancing as that just
    // makes things needlessly complicated
    if (m_is_sending_notifications) {
        return false;
    }

    // Any of the callbacks to user code below could drop the last remaining
    // strong reference to `this`
    auto retain_self = shared_from_this();

    m_is_sending_notifications = true;
    auto cleanup = util::make_scope_exit([this]() noexcept { m_is_sending_notifications = false; });

    if (m_binding_context) {
        m_binding_context->before_notify();
    }
    if (m_group) {
        bool version_changed = m_coordinator->advance_to_latest(*this);
        cache_new_schema();
        return version_changed;
    }

    // No current read transaction, so just create a new one
    read_group();
    m_coordinator->process_available_async(*this);
    return true;
}

bool Realm::can_deliver_notifications() const noexcept
{
    if (m_config.immutable()) {
        return false;
    }

    if (m_binding_context && !m_binding_context->can_deliver_notifications()) {
        return false;
    }

    return true;
}

uint64_t Realm::get_schema_version(const Realm::Config &config)
{
    auto coordinator = RealmCoordinator::get_existing_coordinator(config.path);
    if (coordinator) {
        return coordinator->get_schema_version();
    }

    return ObjectStore::get_schema_version(Realm(config, nullptr).read_group());
}

void Realm::close()
{
    if (m_coordinator) {
        m_coordinator->unregister_realm(this);
    }

    m_group = nullptr;
    m_shared_group = nullptr;
    m_history = nullptr;
    m_read_only_group = nullptr;
    m_binding_context = nullptr;
    m_coordinator = nullptr;
}

util::Optional<int> Realm::file_format_upgraded_from_version() const
{
    if (upgrade_initial_version != upgrade_final_version) {
        return upgrade_initial_version;
    }
    return util::none;
}

template <typename T>
realm::ThreadSafeReference<T> Realm::obtain_thread_safe_reference(T const& value)
{
    verify_thread();
    if (is_in_transaction()) {
        throw InvalidTransactionException("Cannot obtain thread safe reference during a write transaction.");
    }
    return ThreadSafeReference<T>(value);
}

template ThreadSafeReference<Object> Realm::obtain_thread_safe_reference(Object const& value);
template ThreadSafeReference<List> Realm::obtain_thread_safe_reference(List const& value);
template ThreadSafeReference<Results> Realm::obtain_thread_safe_reference(Results const& value);

template <typename T>
T Realm::resolve_thread_safe_reference(ThreadSafeReference<T> reference)
{
    verify_thread();
    if (is_in_transaction()) {
        throw InvalidTransactionException("Cannot resolve thread safe reference during a write transaction.");
    }
    if (reference.is_invalidated()) {
        throw std::logic_error("Cannot resolve thread safe reference more than once.");
    }
    if (!reference.has_same_config(*this)) {
        throw MismatchedRealmException("Cannot resolve thread safe reference in Realm with different configuration "
                                       "than the source Realm.");
    }

    // Any of the callbacks to user code below could drop the last remaining
    // strong reference to `this`
    auto retain_self = shared_from_this();

    // Ensure we're on the same version as the reference
    if (!m_group) {
        // A read transaction doesn't yet exist, so create at the reference's version
        begin_read(reference.m_version_id);
    }
    else {
        // A read transaction does exist, but let's make sure that its version matches the reference's
        auto current_version = m_shared_group->get_version_of_current_transaction();
        VersionID reference_version(reference.m_version_id);

        if (reference_version == current_version) {
            return std::move(reference).import_into_realm(shared_from_this());
        }

        refresh();

        current_version = m_shared_group->get_version_of_current_transaction();

        // If the reference's version is behind, advance it to our version
        if (reference_version < current_version) {
            // Duplicate config for uncached Realm so we don't advance the user's Realm
            Realm::Config config = m_coordinator->get_config();
            config.cache = false;
            config.schema = util::none;
            SharedRealm temporary_realm = m_coordinator->get_realm(config);
            temporary_realm->begin_read(reference_version);

            // With reference imported, advance temporary Realm to our version
            T imported_value = std::move(reference).import_into_realm(temporary_realm);
            transaction::advance(*temporary_realm->m_shared_group, nullptr, current_version);
            reference = ThreadSafeReference<T>(imported_value);
        }
    }

    return std::move(reference).import_into_realm(shared_from_this());
}

template Object Realm::resolve_thread_safe_reference(ThreadSafeReference<Object> reference);
template List Realm::resolve_thread_safe_reference(ThreadSafeReference<List> reference);
template Results Realm::resolve_thread_safe_reference(ThreadSafeReference<Results> reference);

MismatchedConfigException::MismatchedConfigException(StringData message, StringData path)
: std::logic_error(util::format(message.data(), path)) { }

MismatchedRealmException::MismatchedRealmException(StringData message)
: std::logic_error(message.data()) { }

// FIXME Those are exposed for Java async queries, mainly because of handover related methods.
SharedGroup& RealmFriend::get_shared_group(Realm& realm)
{
    return *realm.m_shared_group;
}

Group& RealmFriend::read_group_to(Realm& realm, VersionID version)
{
    if (realm.m_group && realm.m_shared_group->get_version_of_current_transaction() == version)
        return *realm.m_group;

    if (realm.m_group)
        realm.m_shared_group->end_read();
    realm.begin_read(version);
    return *realm.m_group;
}
