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

#include "audit.hpp"
#include "binding_context.hpp"
#include "list.hpp"
#include "object.hpp"
#include "object_schema.hpp"
#include "object_store.hpp"
#include "results.hpp"
#include "schema.hpp"
#include "thread_safe_reference.hpp"

#include "util/scheduler.hpp"

#include <realm/db.hpp>
#include <realm/util/scope_exit.hpp>
#include <realm/util/fifo_helper.hpp>

#if REALM_ENABLE_SYNC
#include "sync/impl/sync_file.hpp"
#include "sync/sync_config.hpp"
#include "sync/sync_manager.hpp"

#include <realm/sync/history.hpp>
#include <realm/sync/permissions.hpp>
#include <realm/sync/version.hpp>
#else
namespace realm {
namespace sync {
    struct PermissionsCache {};
    struct TableInfoCache {};
}
}
#endif

using namespace realm;
using namespace realm::_impl;

Realm::Realm(Config config, util::Optional<VersionID> version, std::shared_ptr<_impl::RealmCoordinator> coordinator, MakeSharedTag)
: m_config(std::move(config))
, m_frozen_version(std::move(version))
, m_scheduler(m_config.scheduler)
{
    if (!coordinator->get_cached_schema(m_schema, m_schema_version, m_schema_transaction_version)) {
        m_group = coordinator->begin_read();
        read_schema_from_group_if_needed();
        coordinator->cache_schema(m_schema, m_schema_version, m_schema_transaction_version);
        m_group = nullptr;
    }

    m_coordinator = std::move(coordinator);
}

Realm::~Realm()
{
    if (m_coordinator) {
        m_coordinator->unregister_realm(this);
    }
}

bool Realm::is_partial() const noexcept
{
#if REALM_ENABLE_SYNC
    return m_config.sync_config && m_config.sync_config->is_partial;
#else
    return false;
#endif
}

Group& Realm::read_group()
{
    verify_open();

    if (!m_group)
        begin_read(m_frozen_version.value_or(VersionID{}));
    return *m_group;
}

Transaction& Realm::transaction()
{
    REALM_ASSERT(!m_config.immutable());
    return static_cast<Transaction&>(read_group());
}

Transaction& Realm::transaction() const
{
    REALM_ASSERT(!m_config.immutable());
    // FIXME: read_group() is not even remotly const
    return static_cast<Transaction&>(const_cast<Realm*>(this)->read_group());
}

std::shared_ptr<Transaction> Realm::transaction_ref()
{
    return std::static_pointer_cast<Transaction>(m_group);
}

std::shared_ptr<Transaction> Realm::duplicate() const
{
    return std::static_pointer_cast<Transaction>(m_coordinator->begin_read(read_transaction_version(), is_frozen()));
}

std::shared_ptr<DB>& Realm::Internal::get_db(Realm& realm) {
    return realm.m_coordinator->m_db;
}

void Realm::Internal::begin_read(Realm& realm, VersionID version_id)
{
    realm.begin_read(version_id);
}

void Realm::begin_read(VersionID version_id)
{
    REALM_ASSERT(!m_group);
    m_group = m_coordinator->begin_read(version_id, bool(m_frozen_version));
    add_schema_change_handler();
    read_schema_from_group_if_needed();
}

SharedRealm Realm::get_shared_realm(Config config)
{
    auto coordinator = RealmCoordinator::get_coordinator(config.path);
    return coordinator->get_realm(std::move(config), util::none);
}

SharedRealm Realm::get_frozen_realm(Config config, VersionID version)
{
    auto coordinator = RealmCoordinator::get_coordinator(config.path);
    SharedRealm realm = coordinator->get_realm(std::move(config), util::Optional<VersionID>(version));
    realm->set_auto_refresh(false);
    return realm;
}

SharedRealm Realm::get_shared_realm(ThreadSafeReference ref, std::shared_ptr<util::Scheduler> scheduler)
{
    if (!scheduler)
        scheduler = util::Scheduler::make_default();
    SharedRealm realm = ref.resolve<std::shared_ptr<Realm>>(nullptr);
    REALM_ASSERT(realm);
    auto& config = realm->config();
    auto coordinator = RealmCoordinator::get_coordinator(config.path);
    if (auto realm = coordinator->get_cached_realm(config, scheduler))
        return realm;
    realm->m_scheduler = scheduler;
    coordinator->bind_to_context(*realm);
    return realm;
}

#if REALM_ENABLE_SYNC
std::shared_ptr<AsyncOpenTask> Realm::get_synchronized_realm(Config config)
{
    auto coordinator = RealmCoordinator::get_coordinator(config.path);
    return coordinator->get_synchronized_realm(std::move(config));
}
#endif

void Realm::set_schema(Schema const& reference, Schema schema)
{
    m_dynamic_schema = false;
    schema.copy_keys_from(reference);
    m_schema = std::move(schema);
    notify_schema_changed();
}

void Realm::read_schema_from_group_if_needed()
{
    if (m_config.immutable()) {
        REALM_ASSERT(m_group);
        if (m_schema.empty()) {
            m_schema_version = ObjectStore::get_schema_version(*m_group);
            m_schema = ObjectStore::schema_from_group(*m_group);
        }
        return;
    }

    Group& group = read_group();
    auto current_version = transaction().get_version_of_current_transaction().version;
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
            m_schema.copy_keys_from(schema);
        }
        else {
            // The structure of the schema has changed, so replace our copy of the schema.
            m_schema = std::move(schema);
        }
    }
    else {
        ObjectStore::verify_valid_external_changes(m_schema.compare(schema));
        m_schema.copy_keys_from(schema);
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
    m_coordinator->close();
    util::File::remove(m_config.path);

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
    if (!m_config.immutable())
        do_refresh();

    // If the user hasn't specified a schema previously then m_schema is always
    // the full schema
    if (m_dynamic_schema)
        return m_schema;

    // Otherwise we may have a subset of the file's schema, so we need to get
    // the complete thing to calculate what changes to make
    if (m_config.immutable())
        return ObjectStore::schema_from_group(read_group());

    Schema actual_schema;
    uint64_t actual_version;
    uint64_t version = -1;
    bool got_cached = m_coordinator->get_cached_schema(actual_schema, actual_version, version);
    if (!got_cached || version != transaction().get_version_of_current_transaction().version)
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

    bool was_in_read_transaction =  is_in_read_transaction();
    Schema actual_schema = get_full_schema();
    std::vector<SchemaChange> required_changes = actual_schema.compare(schema);

    if (!schema_change_needs_write_transaction(schema, required_changes, version)) {
        if (!was_in_read_transaction)
            m_group = nullptr;
        set_schema(actual_schema, std::move(schema));
        return;
    }
    // Either the schema version has changed or we need to do non-migration changes

    // Cancel the write transaction if we exit this function before committing it
    auto cleanup = util::make_scope_exit([&]() noexcept {
        // When in_transaction is true, caller is responsible to cancel the transaction.
        if (!in_transaction && is_in_transaction())
            cancel_transaction();
        if (!was_in_read_transaction)
            m_group = nullptr;
    });

    if (!in_transaction) {
        transaction().promote_to_write();

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

    uint64_t old_schema_version = m_schema_version;
    bool additive = m_config.schema_mode == SchemaMode::Additive;
    if (migration_function && !additive) {
        auto wrapper = [&] {
            auto config = m_config;
            config.schema_mode = SchemaMode::ReadOnlyAlternative;
            config.schema = util::none;
            // Don't go through the normal codepath for opening a Realm because
            // we're using a mismatched config
            auto old_realm = std::make_shared<Realm>(std::move(config), none, m_coordinator, MakeSharedTag{});
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

        ObjectStore::apply_schema_changes(transaction(), version, m_schema, m_schema_version,
                                          m_config.schema_mode, required_changes, util::none, wrapper);
    }
    else {
        util::Optional<std::string> sync_user_id;
#if REALM_ENABLE_SYNC
        if (m_config.sync_config && m_config.sync_config->is_partial)
            sync_user_id = m_config.sync_config->user->identity();
#endif
        ObjectStore::apply_schema_changes(transaction(), m_schema_version, schema, version,
                                          m_config.schema_mode, required_changes, std::move(sync_user_id));
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

    m_schema = std::move(schema);
    m_new_schema = ObjectStore::schema_from_group(read_group());
    m_schema_version = ObjectStore::get_schema_version(read_group());
    m_dynamic_schema = false;
    m_coordinator->clear_schema_cache_and_set_schema_version(version);

    if (!in_transaction) {
        m_coordinator->commit_write(*this);
        invalidate_permission_cache();
        cache_new_schema();
    }

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
            m_schema = *m_new_schema;
        }
        else
            m_schema.copy_keys_from(*m_new_schema);

        notify_schema_changed();
    });
}

void Realm::cache_new_schema()
{
    if (!is_closed()) {
        auto new_version = transaction().get_version_of_current_transaction().version;
        if (m_new_schema)
            m_coordinator->cache_schema(std::move(*m_new_schema),
                                        m_schema_version, new_version);
        else
            m_coordinator->advance_schema_cache(m_schema_transaction_version, new_version);
        m_schema_transaction_version = new_version;
        m_new_schema = util::none;
    }
}

void Realm::translate_schema_error()
{
    // Read the new (incompatible) schema without changing our read transaction
    auto new_schema = ObjectStore::schema_from_group(*m_coordinator->begin_read());

    // Should always throw
    ObjectStore::verify_valid_external_changes(m_schema.compare(new_schema, true));

    // Something strange happened so just rethrow the old exception
    throw;
}

void Realm::notify_schema_changed()
{
    if (m_binding_context) {
        m_binding_context->schema_did_change(m_schema);
    }
}

static void check_can_create_any_transaction(const Realm* realm)
{
    if (realm->config().immutable()) {
        throw InvalidTransactionException("Can't perform transactions on read-only Realms.");
    }
}

static void check_can_create_write_transaction(const Realm* realm)
{
    if (realm->config().immutable() || realm->config().read_only_alternative()) {
        throw InvalidTransactionException("Can't perform transactions on read-only Realms.");
    }
    if (realm->is_frozen()) {
        throw InvalidTransactionException("Can't perform transactions on a frozen Realm");
    }
    if (!realm->is_closed() && realm->get_number_of_versions() > realm->config().max_number_of_active_versions) {
        throw InvalidTransactionException(util::format("Number of active versions (%1) in the Realm exceeded the limit of %2",
                realm->get_number_of_versions(),
                realm->config().max_number_of_active_versions));
    }
}

void Realm::verify_thread() const
{
    if (m_scheduler && !m_scheduler->is_on_thread())
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

bool Realm::verify_notifications_available(bool throw_on_error) const
{
    if (is_frozen()) {
        if (throw_on_error)
            throw InvalidTransactionException("Notifications are not available on frozen lists since they do not change.");
        return false;
    }
    if (config().immutable()) {
        if (throw_on_error)
            throw InvalidTransactionException("Cannot create asynchronous query for immutable Realms");
        return false;
    }
    if (is_in_transaction()) {
        if (throw_on_error)
            throw InvalidTransactionException("Cannot create asynchronous query while in a write transaction");
        return false;
    }

    return true;
}

VersionID Realm::read_transaction_version() const
{
    verify_thread();
    verify_open();
    check_can_create_any_transaction(this);
    return static_cast<Transaction&>(*m_group).get_version_of_current_transaction();
}

uint_fast64_t Realm::get_number_of_versions() const
{
    verify_open();
    check_can_create_any_transaction(this);
    return m_coordinator->get_number_of_versions();
}

bool Realm::is_in_transaction() const noexcept
{
    return !m_config.immutable()
        && !is_closed()
        && m_group && transaction().get_transact_stage() == DB::transact_Writing;
}

util::Optional<VersionID> Realm::current_transaction_version() const
{
    util::Optional<VersionID> ret;
    if (m_group) {
        ret = static_cast<Transaction&>(*m_group).get_version_of_current_transaction();
    }
    else if (m_frozen_version) {
        ret = m_frozen_version;
    }
    return ret;
}

void Realm::enable_wait_for_change()
{
    m_coordinator->enable_wait_for_change();
}

bool Realm::wait_for_change()
{
    if (m_frozen_version) {
        return false;
    }
    return m_group ? m_coordinator->wait_for_change(transaction_ref()) : false;
}

void Realm::wait_for_change_release()
{
    m_coordinator->wait_for_change_release();
}

void Realm::begin_transaction()
{
    verify_thread();
    check_can_create_write_transaction(this);

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
        transaction::begin(transaction_ref(), m_binding_context.get(), notifiers);
        return;
    }

    // make sure we have a read transaction
    read_group();

    m_is_sending_notifications = true;
    auto cleanup = util::make_scope_exit([this]() noexcept {
        m_is_sending_notifications = false;
    });

    try {
        m_coordinator->promote_to_write(*this);
    }
    catch (_impl::UnsupportedSchemaChange const&) {
        translate_schema_error();
    }
    cache_new_schema();
}

void Realm::commit_transaction()
{
    check_can_create_write_transaction(this);
    verify_thread();

    if (!is_in_transaction()) {
        throw InvalidTransactionException("Can't commit a non-existing write transaction");
    }

    if (auto audit = audit_context()) {
        auto prev_version = transaction().get_version_of_current_transaction();
        m_coordinator->commit_write(*this);
        audit->record_write(prev_version, transaction().get_version_of_current_transaction());
        // m_shared_group->unpin_version(prev_version);
    }
    else {
        m_coordinator->commit_write(*this);
    }
    cache_new_schema();
    invalidate_permission_cache();
}

void Realm::cancel_transaction()
{
    check_can_create_write_transaction(this);
    verify_thread();

    if (!is_in_transaction()) {
        throw InvalidTransactionException("Can't cancel a non-existing write transaction");
    }

    transaction::cancel(transaction(), m_binding_context.get());
    invalidate_permission_cache();
}

void Realm::invalidate()
{
    verify_open();
    verify_thread();
    check_can_create_any_transaction(this);

    if (m_is_sending_notifications) {
        return;
    }

    if (is_in_transaction()) {
        cancel_transaction();
    }

    m_permissions_cache = nullptr;
    m_table_info_cache = nullptr;
    m_group = nullptr;
}

bool Realm::compact()
{
    verify_thread();
    verify_open();

    if (m_config.immutable() || m_config.read_only_alternative()) {
        throw InvalidTransactionException("Can't compact a read-only Realm");
    }
    if (is_in_transaction()) {
        throw InvalidTransactionException("Can't compact a Realm within a write transaction");
    }

    verify_open();
    m_group = nullptr;
    return m_coordinator->compact();
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
        _impl::translate_file_exception(path);
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
    if (is_closed() || is_in_transaction() || is_frozen()) {
        return;
    }

    verify_thread();
    invalidate_permission_cache();

    // Any of the callbacks to user code below could drop the last remaining
    // strong reference to `this`
    auto retain_self = shared_from_this();

    if (m_binding_context) {
        m_binding_context->before_notify();
        if (is_closed() || is_in_transaction()) {
            return;
        }
    }

    auto cleanup = util::make_scope_exit([this]() noexcept { m_is_sending_notifications = false; });
    if (!m_coordinator->can_advance(*this)) {
        m_is_sending_notifications = true;
        m_coordinator->process_available_async(*this);
        return;
    }

    if (m_binding_context) {
        m_binding_context->changes_available();

        // changes_available() may have advanced the read version, and if
        // so we don't need to do anything further
        if (!m_coordinator->can_advance(*this))
            return;
    }

    m_is_sending_notifications = true;
    if (m_auto_refresh) {
        if (m_group) {
            try {
                m_coordinator->advance_to_ready(*this);
            }
            catch (_impl::UnsupportedSchemaChange const&) {
                translate_schema_error();
            }
            if (!is_closed())
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
    check_can_create_any_transaction(this);
    return do_refresh();
}

bool Realm::do_refresh()
{
    // Frozen Realms never change.
    if (is_frozen()) {
        return false;
    }

    // can't be any new changes if we're in a write transaction
    if (is_in_transaction()) {
        return false;
    }
    // don't advance if we're already in the process of advancing as that just
    // makes things needlessly complicated
    if (m_is_sending_notifications) {
        return false;
    }
    invalidate_permission_cache();

    // Any of the callbacks to user code below could drop the last remaining
    // strong reference to `this`
    auto retain_self = shared_from_this();

    m_is_sending_notifications = true;
    auto cleanup = util::make_scope_exit([this]() noexcept { m_is_sending_notifications = false; });

    if (m_binding_context) {
        m_binding_context->before_notify();
    }
    if (m_group) {
        try {
            bool version_changed = m_coordinator->advance_to_latest(*this);
            if (is_closed())
                return false;
            cache_new_schema();
            return version_changed;
        }
        catch (_impl::UnsupportedSchemaChange const&) {
            translate_schema_error();
        }
    }

    // No current read transaction, so just create a new one
    read_group();
    m_coordinator->process_available_async(*this);
    return true;
}

void Realm::set_auto_refresh(bool auto_refresh)
{
    if (is_frozen() && auto_refresh) {
        throw std::logic_error("Auto-refresh cannot be enabled for frozen Realms.");
    }
    m_auto_refresh = auto_refresh;
}


bool Realm::can_deliver_notifications() const noexcept
{
    if (m_config.immutable() || !m_config.automatic_change_notifications) {
        return false;
    }

    if (!m_scheduler || !m_scheduler->can_deliver_notifications()) {
        return false;
    }

    return true;
}

uint64_t Realm::get_schema_version(const Realm::Config &config)
{
    auto coordinator = RealmCoordinator::get_coordinator(config.path);
    auto version = coordinator->get_schema_version();
    if (version == ObjectStore::NotVersioned)
        version = ObjectStore::get_schema_version(coordinator->get_realm(config, util::none)->read_group());
    return version;
}


bool Realm::is_frozen() const
{
    bool result = bool(m_frozen_version);
    REALM_ASSERT_DEBUG((result && m_group) ? m_group->is_frozen() : true);
    return result;
}

SharedRealm Realm::freeze()
{
    auto config = m_config;
    auto version = read_transaction_version();
    config.scheduler = util::Scheduler::get_frozen(version);
    return Realm::get_frozen_realm(std::move(config), version);
}

void Realm::close()
{
    if (m_coordinator) {
        m_coordinator->unregister_realm(this);
    }
    if (!m_config.immutable() && m_group) {
        transaction().close();
    }

    m_permissions_cache = nullptr;
    m_table_info_cache = nullptr;
    m_group = nullptr;
    m_binding_context = nullptr;
    m_coordinator = nullptr;
}

AuditInterface* Realm::audit_context() const noexcept
{
    return m_coordinator ? m_coordinator->audit_context() : nullptr;
}

#if REALM_ENABLE_SYNC
static_assert(static_cast<int>(ComputedPrivileges::Read) == static_cast<int>(sync::Privilege::Read), "");
static_assert(static_cast<int>(ComputedPrivileges::Update) == static_cast<int>(sync::Privilege::Update), "");
static_assert(static_cast<int>(ComputedPrivileges::Delete) == static_cast<int>(sync::Privilege::Delete), "");
static_assert(static_cast<int>(ComputedPrivileges::SetPermissions) == static_cast<int>(sync::Privilege::SetPermissions), "");
static_assert(static_cast<int>(ComputedPrivileges::Query) == static_cast<int>(sync::Privilege::Query), "");
static_assert(static_cast<int>(ComputedPrivileges::Create) == static_cast<int>(sync::Privilege::Create), "");
static_assert(static_cast<int>(ComputedPrivileges::ModifySchema) == static_cast<int>(sync::Privilege::ModifySchema), "");

static constexpr const uint8_t s_allRealmPrivileges = sync::Privilege::Read
                                                    | sync::Privilege::Update
                                                    | sync::Privilege::SetPermissions
                                                    | sync::Privilege::ModifySchema;
static constexpr const uint8_t s_allClassPrivileges = sync::Privilege::Read
                                                    | sync::Privilege::Update
                                                    | sync::Privilege::Create
                                                    | sync::Privilege::Query
                                                    | sync::Privilege::SetPermissions;
static constexpr const uint8_t s_allObjectPrivileges = sync::Privilege::Read
                                                     | sync::Privilege::Update
                                                     | sync::Privilege::Delete
                                                     | sync::Privilege::SetPermissions;

bool Realm::init_permission_cache()
{
    verify_thread();

    if (m_permissions_cache) {
        // Rather than trying to track changes to permissions tables, just skip the caching
        // entirely within write transactions for now
        if (is_in_transaction())
            m_permissions_cache->clear();
        return true;
    }

    // Admin users bypass permissions checks outside of the logic in PermissionsCache
    if (m_config.sync_config && m_config.sync_config->is_partial && !m_config.sync_config->user->is_admin()) {
        m_table_info_cache = std::make_unique<sync::TableInfoCache>(transaction());
        m_permissions_cache = std::make_unique<sync::PermissionsCache>(transaction(), *m_table_info_cache,
                                                                       m_config.sync_config->user->identity());
        return true;
    }
    return false;
}

void Realm::invalidate_permission_cache()
{
    if (m_permissions_cache)
        m_permissions_cache->clear();
}

ComputedPrivileges Realm::get_privileges()
{
    if (!init_permission_cache())
        return static_cast<ComputedPrivileges>(s_allRealmPrivileges);
    return static_cast<ComputedPrivileges>(m_permissions_cache->get_realm_privileges() & s_allRealmPrivileges);
}

static uint8_t inherited_mask(uint32_t privileges)
{
    uint8_t mask = ~0;
    if (!(privileges & sync::Privilege::Read))
        mask = 0;
    else if (!(privileges & sync::Privilege::Update))
        mask = static_cast<uint8_t>(sync::Privilege::Read | sync::Privilege::Query);
    return mask;
}

ComputedPrivileges Realm::get_privileges(StringData object_type)
{
    if (!init_permission_cache())
        return static_cast<ComputedPrivileges>(s_allClassPrivileges);
    auto privileges = inherited_mask(m_permissions_cache->get_realm_privileges())
                    & m_permissions_cache->get_class_privileges(object_type);
    return static_cast<ComputedPrivileges>(privileges & s_allClassPrivileges);
}

ComputedPrivileges Realm::get_privileges(ConstObj const& obj)
{
    if (!init_permission_cache())
        return static_cast<ComputedPrivileges>(s_allObjectPrivileges);

    auto table = obj.get_table();
    auto object_type = ObjectStore::object_type_for_table_name(table->get_name());
    sync::GlobalID global_id{object_type, table->get_object_id(obj.get_key())};
    auto privileges = inherited_mask(m_permissions_cache->get_realm_privileges())
                    & inherited_mask(m_permissions_cache->get_class_privileges(object_type))
                    & m_permissions_cache->get_object_privileges(global_id);
    return static_cast<ComputedPrivileges>(privileges & s_allObjectPrivileges);
}
#else
void Realm::invalidate_permission_cache() { }
#endif

MismatchedConfigException::MismatchedConfigException(StringData message, StringData path)
: std::logic_error(util::format(message.data(), path)) { }

MismatchedRealmException::MismatchedRealmException(StringData message)
: std::logic_error(message.data()) { }

