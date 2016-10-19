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

#include "impl/handover.hpp"
#include "impl/realm_coordinator.hpp"
#include "impl/transact_log_handler.hpp"

#include "binding_context.hpp"
#include "object_schema.hpp"
#include "object_store.hpp"
#include "schema.hpp"
#include "thread_confined.hpp"

#include "util/format.hpp"

#include <realm/history.hpp>
#include <realm/util/scope_exit.hpp>

#if REALM_ENABLE_SYNC
#include <realm/sync/history.hpp>
#endif

using namespace realm;
using namespace realm::_impl;

Realm::Realm(Config config)
: m_config(std::move(config))
{
    open_with_config(m_config, m_history, m_shared_group, m_read_only_group, this);

    if (m_read_only_group) {
        m_group = m_read_only_group.get();
    }
}

void Realm::init(std::shared_ptr<_impl::RealmCoordinator> coordinator)
{
    // if there is an existing realm at the current path steal its schema/column mapping
    if (auto existing = coordinator ? coordinator->get_schema() : nullptr) {
        m_schema = *existing;
        m_schema_version = coordinator->get_schema_version();
    }
    else {
        // otherwise get the schema from the group
        m_schema_version = ObjectStore::get_schema_version(read_group());
        m_schema = ObjectStore::schema_from_group(read_group());

        if (m_shared_group) {
            m_schema_transaction_version = m_shared_group->get_version_of_current_transaction().version;
            m_shared_group->end_read();
            m_group = nullptr;
        }
    }

    m_coordinator = std::move(coordinator);

    if (m_config.schema) {
        try {
            auto schema = std::move(*m_config.schema);
            m_config.schema = util::none;
            update_schema(std::move(schema), m_config.schema_version,
                          std::move(m_config.migration_function));
        }
        catch (...) {
            m_coordinator = nullptr; // don't try to unregister in the destructor as it'll deadlock
            throw;
        }
    }

}

REALM_NOINLINE static void translate_file_exception(StringData path, bool read_only=false)
{
    try {
        throw;
    }
    catch (util::File::PermissionDenied const& ex) {
        throw RealmFileException(RealmFileException::Kind::PermissionDenied, ex.get_path(),
                                 util::format("Unable to open a realm at path '%1'. Please use a path where your app has %2 permissions.",
                                              ex.get_path(), read_only ? "read" : "read-write"),
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
        auto pos = underlying.find(ex.get_path());
        if (pos != std::string::npos && pos > 0) {
            // One extra char at each end for the quotes
            underlying.replace(pos - 1, ex.get_path().size() + 2, "");
        }
        throw RealmFileException(RealmFileException::Kind::AccessError, ex.get_path(),
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
    if (config.encryption_key.data() && config.encryption_key.size() != 64) {
        throw InvalidEncryptionKeyException();
    }
    try {
        if (config.read_only()) {
            read_only_group = std::make_unique<Group>(config.path, config.encryption_key.data(), Group::mode_ReadOnly);
        }
        else {
            // FIXME: The SharedGroup constructor, when called below, will
            // throw a C++ exception if server_synchronization_mode is
            // inconsistent with the accessed Realm file. This exception
            // probably has to be transmuted to an NSError.
            bool server_synchronization_mode = bool(config.sync_config);
            if (server_synchronization_mode) {
#if REALM_ENABLE_SYNC
                history = realm::sync::make_sync_history(config.path);
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
            options.allow_file_format_upgrade = !config.disable_format_upgrade;
            options.upgrade_callback = [&](int from_version, int to_version) {
                if (realm) {
                    realm->upgrade_initial_version = from_version;
                    realm->upgrade_final_version = to_version;
                }
            };
            shared_group = std::make_unique<SharedGroup>(*history, options);
        }
    }
    catch (...) {
        translate_file_exception(config.path, config.read_only());
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
    if (!m_group) {
        m_group = &const_cast<Group&>(m_shared_group->begin_read());
        add_schema_change_handler();
    }
    return *m_group;
}

void Realm::Internal::begin_read(Realm& realm, VersionID version_id)
{
    REALM_ASSERT(!realm.m_group);
    realm.m_group = &const_cast<Group&>(realm.m_shared_group->begin_read(version_id));
    realm.add_schema_change_handler();
}

SharedRealm Realm::get_shared_realm(Config config)
{
    auto coordinator = RealmCoordinator::get_coordinator(config.path);
    return coordinator->get_realm(std::move(config));
}

void Realm::set_schema(Schema schema, uint64_t version)
{
    schema.copy_table_columns_from(m_schema);
    m_schema = schema;
    m_coordinator->update_schema(schema, version);
}

bool Realm::read_schema_from_group_if_needed()
{
    // schema of read-only Realms can't change
    if (m_read_only_group)
        return false;

    Group& group = read_group();
    auto current_version = m_shared_group->get_version_of_current_transaction().version;
    if (m_schema_transaction_version == current_version)
        return false;

    m_schema = ObjectStore::schema_from_group(group);
    m_schema_version = ObjectStore::get_schema_version(group);
    m_schema_transaction_version = current_version;
    return true;
}

void Realm::reset_file_if_needed(Schema const& schema, uint64_t version, std::vector<SchemaChange>& required_changes)
{
    if (m_schema_version == ObjectStore::NotVersioned)
        return;
    if (m_schema_version == version && !ObjectStore::needs_migration(required_changes))
        return;

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
}

void Realm::update_schema(Schema schema, uint64_t version, MigrationFunction migration_function)
{
    schema.validate();
    read_schema_from_group_if_needed();
    std::vector<SchemaChange> required_changes = m_schema.compare(schema);

    auto no_changes_required = [&] {
        switch (m_config.schema_mode) {
            case SchemaMode::Automatic:
                if (version < m_schema_version && m_schema_version != ObjectStore::NotVersioned) {
                    throw InvalidSchemaVersionException(m_schema_version, version);
                }
                if (version == m_schema_version) {
                    if (required_changes.empty()) {
                        set_schema(std::move(schema), version);
                        return true;
                    }
                    ObjectStore::verify_no_migration_required(required_changes);
                }
                return false;

            case SchemaMode::ReadOnly:
                if (version != m_schema_version)
                    throw InvalidSchemaVersionException(m_schema_version, version);
                ObjectStore::verify_no_migration_required(m_schema.compare(schema));
                set_schema(std::move(schema), version);
                return true;

            case SchemaMode::ResetFile:
                reset_file_if_needed(schema, version, required_changes);
                return required_changes.empty();

            case SchemaMode::Additive:
                if (required_changes.empty()) {
                    set_schema(std::move(schema), version);
                    return version == m_schema_version;
                }
                ObjectStore::verify_valid_additive_changes(required_changes);
                return false;

            case SchemaMode::Manual:
                if (version < m_schema_version && m_schema_version != ObjectStore::NotVersioned) {
                    throw InvalidSchemaVersionException(m_schema_version, version);
                }
                if (version == m_schema_version) {
                    ObjectStore::verify_no_changes_required(required_changes);
                    return true;
                }
                return false;
        }
        __builtin_unreachable();
    };

    if (no_changes_required())
        return;
    // Either the schema version has changed or we need to do non-migration changes

    m_group->set_schema_change_notification_handler(nullptr);
    transaction::begin_without_validation(*m_shared_group);
    add_schema_change_handler();

    // Cancel the write transaction if we exit this function before committing it
    struct WriteTransactionGuard {
        Realm& realm;
        ~WriteTransactionGuard() { if (realm.is_in_transaction()) realm.cancel_transaction(); }
    } write_transaction_guard{*this};

    // If beginning the write transaction advanced the version, then someone else
    // may have updated the schema and we need to re-read it
    // We can't just begin the write transaction before checking anything because
    // that means that write transactions would block opening Realms in other processes
    if (read_schema_from_group_if_needed()) {
        required_changes = m_schema.compare(schema);
        if (no_changes_required())
            return;
    }

    bool additive = m_config.schema_mode == SchemaMode::Additive;
    if (migration_function && !additive) {
        auto wrapper = [&] {
            SharedRealm old_realm(new Realm(m_config));
            old_realm->init(nullptr);
            // Need to open in read-write mode so that it uses a SharedGroup, but
            // users shouldn't actually be able to write via the old realm
            old_realm->m_config.schema_mode = SchemaMode::ReadOnly;

            migration_function(old_realm, shared_from_this(), m_schema);
        };
        ObjectStore::apply_schema_changes(read_group(), m_schema, m_schema_version,
                                          schema, version, m_config.schema_mode, required_changes, wrapper);
    }
    else {
        ObjectStore::apply_schema_changes(read_group(), m_schema, m_schema_version,
                                          schema, version, m_config.schema_mode, required_changes);
        REALM_ASSERT_DEBUG(additive || (required_changes = ObjectStore::schema_from_group(read_group()).compare(schema)).empty());
    }

    commit_transaction();
    m_coordinator->update_schema(m_schema, version);
}

void Realm::add_schema_change_handler()
{
    if (m_config.schema_mode == SchemaMode::Additive) {
        m_group->set_schema_change_notification_handler([&] {
            auto new_schema = ObjectStore::schema_from_group(read_group());
            auto required_changes = m_schema.compare(new_schema);
            ObjectStore::verify_valid_additive_changes(required_changes);
            m_schema.copy_table_columns_from(new_schema);
            m_coordinator->update_schema(m_schema, m_schema_version);
        });
    }
}

static void check_read_write(Realm *realm)
{
    if (realm->config().read_only()) {
        throw InvalidTransactionException("Can't perform transactions on read-only Realms.");
    }
}

void Realm::verify_thread() const
{
    if (m_thread_id != std::this_thread::get_id()) {
        throw IncorrectThreadException();
    }
}

void Realm::verify_in_write() const
{
    if (!is_in_transaction()) {
        throw InvalidTransactionException("Cannot modify managed objects outside of a write transaction.");
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
    check_read_write(this);
    verify_thread();

    if (is_in_transaction()) {
        throw InvalidTransactionException("The Realm is already in a write transaction");
    }

    // make sure we have a read transaction
    read_group();

    transaction::begin(*m_shared_group, m_binding_context.get(), m_config.schema_mode);
}

void Realm::commit_transaction()
{
    check_read_write(this);
    verify_thread();

    if (!is_in_transaction()) {
        throw InvalidTransactionException("Can't commit a non-existing write transaction");
    }

    transaction::commit(*m_shared_group, m_binding_context.get());
    m_coordinator->send_commit_notifications(*this);
}

void Realm::cancel_transaction()
{
    check_read_write(this);
    verify_thread();

    if (!is_in_transaction()) {
        throw InvalidTransactionException("Can't cancel a non-existing write transaction");
    }

    transaction::cancel(*m_shared_group, m_binding_context.get());
}

void Realm::invalidate()
{
    verify_thread();
    check_read_write(this);

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

    if (m_config.read_only()) {
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

void Realm::notify()
{
    if (is_closed()) {
        return;
    }

    verify_thread();

    if (m_shared_group->has_changed()) { // Throws
        if (m_binding_context) {
            m_binding_context->changes_available();
        }
        if (m_auto_refresh) {
            if (m_group) {
                m_coordinator->advance_to_ready(*this);
            }
            else if (m_binding_context) {
                m_binding_context->did_change({}, {});
            }
        }
    }
    else {
        m_coordinator->process_available_async(*this);
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

    // advance transaction if database has changed
    if (!m_shared_group->has_changed()) { // Throws
        return false;
    }

    if (m_group) {
        transaction::advance(*m_shared_group, m_binding_context.get(), m_config.schema_mode);
        m_coordinator->process_available_async(*this);
    }
    else {
        // Create the read transaction
        read_group();
    }

    return true;
}

bool Realm::can_deliver_notifications() const noexcept
{
    if (m_config.read_only()) {
        return false;
    }

    if (m_binding_context && !m_binding_context->can_deliver_notifications()) {
        return false;
    }

    return true;
}

uint64_t Realm::get_schema_version(const realm::Realm::Config &config)
{
    auto coordinator = RealmCoordinator::get_existing_coordinator(config.path);
    if (coordinator) {
        return coordinator->get_schema_version();
    }

    return ObjectStore::get_schema_version(Realm(config).read_group());
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

Realm::HandoverPackage::HandoverPackage(HandoverPackage&&) = default;
Realm::HandoverPackage& Realm::HandoverPackage::operator=(HandoverPackage&&) = default;
Realm::HandoverPackage::VersionID::VersionID() : VersionID(SharedGroup::VersionID()) { }

// Precondition: `m_version` is not greater than `new_version`
// Postcondition: `m_version` is equal to `new_version`
void Realm::HandoverPackage::advance_to_version(VersionID new_version)
{
    if (SharedGroup::VersionID(new_version) == SharedGroup::VersionID(m_version_id)) {
        return;
    }
    REALM_ASSERT_DEBUG((SharedGroup::VersionID(new_version) > SharedGroup::VersionID(m_version_id)));

    // Open `Realm` at handover version
    _impl::RealmCoordinator& coordinator = get_coordinator();
    Realm::Config config = coordinator.get_config();
    config.cache = false;
    SharedRealm realm = coordinator.get_realm(config);
    REALM_ASSERT(!realm->is_in_read_transaction());
    realm->m_group = &const_cast<Group&>(realm->m_shared_group->begin_read(m_version_id));

    // Import handover, advance version, and then repackage for handover
    auto objects = realm->accept_handover(std::move(*this));
    transaction::advance(*realm->m_shared_group, realm->m_binding_context.get(),
                         realm->m_config.schema_mode, new_version);
    *this = realm->package_for_handover(std::move(objects));
}

Realm::HandoverPackage::~HandoverPackage()
{
    if (is_awaiting_import()) {
        get_coordinator().get_realm()->m_shared_group->unpin_version(m_version_id);
        mark_not_awaiting_import();
    }
}

Realm::HandoverPackage Realm::package_for_handover(std::vector<AnyThreadConfined> objects_to_hand_over)
{
    verify_thread();
    if (is_in_transaction()) {
        throw InvalidTransactionException("Cannot package handover during a write transaction.");
    }

    HandoverPackage handover;
    auto version_id = m_shared_group->pin_version();
    handover.m_version_id = version_id;
    handover.m_source_realm = shared_from_this();
    // Since `m_source_realm` is used to determine if we need to unpin when destroyed,
    // `m_source_realm` should only be set after `pin_version` succeeds in case it throws.

    handover.m_objects.reserve(objects_to_hand_over.size());
    for (auto &object : objects_to_hand_over) {
        REALM_ASSERT(object.get_realm().get() == this);
        handover.m_objects.push_back(object.export_for_handover());
    }

    return handover;
}

std::vector<AnyThreadConfined> Realm::accept_handover(Realm::HandoverPackage handover)
{
    verify_thread();

    if (!handover.is_awaiting_import()) {
        throw std::logic_error("Handover package must not be imported more than once.");
    }

    auto unpin_version = util::make_scope_exit([&]() noexcept {
        m_shared_group->unpin_version(handover.m_version_id);
        handover.mark_not_awaiting_import();
    });

    if (is_in_transaction()) {
        throw InvalidTransactionException("Cannot accept handover during a write transaction.");
    }

    // Ensure we're on the same version as the handover
    if (!m_group) {
        // A read transaction doesn't yet exist, so create at the handover version
        m_group = &const_cast<Group&>(m_shared_group->begin_read(handover.m_version_id));
        add_schema_change_handler();
    }
    else {
        auto current_version = m_shared_group->get_version_of_current_transaction();

        if (SharedGroup::VersionID(handover.m_version_id) <= current_version) {
            // The handover is behind, so advance it to our version
            handover.advance_to_version(current_version);
        } else {
            // We're behind, so advance to the handover's version
            transaction::advance(*m_shared_group, m_binding_context.get(),
                                 m_config.schema_mode, handover.m_version_id);
            m_coordinator->process_available_async(*this);
        }
    }

    std::vector<AnyThreadConfined> objects;
    objects.reserve(handover.m_objects.size());
    for (auto &object : handover.m_objects) {
        objects.push_back(std::move(object).import_from_handover(shared_from_this()));
    }

    // Avoid weird partial-refresh semantics when importing old packages
    refresh();

    return objects;
}

MismatchedConfigException::MismatchedConfigException(StringData message, StringData path)
: std::logic_error(util::format(message.data(), path)) { }
