////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
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

#include "sync/impl/sync_metadata.hpp"

#include "object_schema.hpp"
#include "object_store.hpp"
#include "property.hpp"
#include "results.hpp"
#include "schema.hpp"
#include "util/uuid.hpp"
#if REALM_PLATFORM_APPLE
#include "impl/apple/keychain_helper.hpp"
#endif

#include <realm/descriptor.hpp>
#include <realm/table.hpp>

namespace {
static const char * const c_sync_userMetadata = "UserMetadata";
static const char * const c_sync_marked_for_removal = "marked_for_removal";
static const char * const c_sync_identity = "identity";
static const char * const c_sync_local_uuid = "local_uuid";
static const char * const c_sync_auth_server_url = "auth_server_url";
static const char * const c_sync_user_token = "user_token";
static const char * const c_sync_user_is_admin = "user_is_admin";

static const char * const c_sync_fileActionMetadata = "FileActionMetadata";
static const char * const c_sync_original_name = "original_name";
static const char * const c_sync_new_name = "new_name";
static const char * const c_sync_action = "action";
static const char * const c_sync_url = "url";

static const char * const c_sync_clientMetadata = "ClientMetadata";
static const char * const c_sync_uuid = "uuid";

realm::Schema make_schema()
{
    using namespace realm;
    return Schema{
        {c_sync_userMetadata, {
            {c_sync_identity, PropertyType::String},
            {c_sync_local_uuid, PropertyType::String},
            {c_sync_marked_for_removal, PropertyType::Bool},
            {c_sync_user_token, PropertyType::String|PropertyType::Nullable},
            {c_sync_auth_server_url, PropertyType::String},
            {c_sync_user_is_admin, PropertyType::Bool},
        }},
        {c_sync_fileActionMetadata, {
            {c_sync_original_name, PropertyType::String, Property::IsPrimary{true}},
            {c_sync_new_name, PropertyType::String|PropertyType::Nullable},
            {c_sync_action, PropertyType::Int},
            {c_sync_url, PropertyType::String},
            {c_sync_identity, PropertyType::String},
        }},
        {c_sync_clientMetadata, {
            {c_sync_uuid, PropertyType::String},
        }}
    };
}

} // anonymous namespace

namespace realm {

// MARK: - Sync metadata manager

SyncMetadataManager::SyncMetadataManager(std::string path,
                                         bool should_encrypt,
                                         util::Optional<std::vector<char>> encryption_key)
{
    constexpr uint64_t SCHEMA_VERSION = 2;

    Realm::Config config;
    config.path = path;
    config.schema = make_schema();
    config.schema_version = SCHEMA_VERSION;
    config.schema_mode = SchemaMode::Automatic;
#if REALM_PLATFORM_APPLE
    if (should_encrypt && !encryption_key) {
        encryption_key = keychain::metadata_realm_encryption_key(File::exists(path));
    }
#endif
    if (should_encrypt) {
        if (!encryption_key) {
            throw std::invalid_argument("Metadata Realm encryption was specified, but no encryption key was provided.");
        }
        config.encryption_key = std::move(*encryption_key);
    }

    config.migration_function = [](SharedRealm old_realm, SharedRealm realm, Schema&) {
        if (old_realm->schema_version() < 2) {
            TableRef old_table = ObjectStore::table_for_object_type(old_realm->read_group(), c_sync_userMetadata);
            TableRef table = ObjectStore::table_for_object_type(realm->read_group(), c_sync_userMetadata);

            // Get all the SyncUserMetadata objects.
            Results results(old_realm, *old_table);

            // Column indices.
            size_t old_idx_identity = old_table->get_column_index(c_sync_identity);
            size_t old_idx_url = old_table->get_column_index(c_sync_auth_server_url);
            size_t idx_local_uuid = table->get_column_index(c_sync_local_uuid);
            size_t idx_url = table->get_column_index(c_sync_auth_server_url);

            for (size_t i = 0; i < results.size(); i++) {
                RowExpr entry = results.get(i);
                // Set the UUID equal to the user identity for existing users.
                auto identity = entry.get_string(old_idx_identity);
                table->set_string(idx_local_uuid, entry.get_index(), identity);
                // Migrate the auth server URLs to a non-nullable property.
                auto url = entry.get_string(old_idx_url);
                table->set_string(idx_url, entry.get_index(), url.is_null() ? "" : url);
            }
        }
    };

    SharedRealm realm = Realm::get_shared_realm(config);

    // Get data about the (hardcoded) schemas
    auto object_schema = realm->schema().find(c_sync_userMetadata);
    m_user_schema = {
        object_schema->persisted_properties[0].table_column,
        object_schema->persisted_properties[1].table_column,
        object_schema->persisted_properties[2].table_column,
        object_schema->persisted_properties[3].table_column,
        object_schema->persisted_properties[4].table_column,
        object_schema->persisted_properties[5].table_column,
    };

    object_schema = realm->schema().find(c_sync_fileActionMetadata);
    m_file_action_schema = {
        object_schema->persisted_properties[0].table_column,
        object_schema->persisted_properties[1].table_column,
        object_schema->persisted_properties[2].table_column,
        object_schema->persisted_properties[3].table_column,
        object_schema->persisted_properties[4].table_column,
    };

    object_schema = realm->schema().find(c_sync_clientMetadata);
    m_client_schema = {
        object_schema->persisted_properties[0].table_column,
    };

    m_metadata_config = std::move(config);

    m_client_uuid = [&]() -> std::string {
        TableRef table = ObjectStore::table_for_object_type(realm->read_group(), c_sync_clientMetadata);
        if (table->is_empty()) {
            realm->begin_transaction();
            if (table->is_empty()) {
                size_t idx = table->add_empty_row();
                REALM_ASSERT_DEBUG(idx == 0);
                auto uuid = uuid_string();
                table->set_string(m_client_schema.idx_uuid, idx, uuid);
                realm->commit_transaction();
                return uuid;
            }
            realm->cancel_transaction();
        }
        return table->get_string(m_client_schema.idx_uuid, 0);
    }();
}

SyncUserMetadataResults SyncMetadataManager::all_unmarked_users() const
{
    return get_users(false);
}

SyncUserMetadataResults SyncMetadataManager::all_users_marked_for_removal() const
{
    return get_users(true);
}

SyncUserMetadataResults SyncMetadataManager::get_users(bool marked) const
{
    SharedRealm realm = Realm::get_shared_realm(m_metadata_config);

    TableRef table = ObjectStore::table_for_object_type(realm->read_group(), c_sync_userMetadata);
    Query query = table->where().equal(m_user_schema.idx_marked_for_removal, marked);

    Results results(realm, std::move(query));
    return SyncUserMetadataResults(std::move(results), std::move(realm), m_user_schema);
}

SyncFileActionMetadataResults SyncMetadataManager::all_pending_actions() const
{
    SharedRealm realm = Realm::get_shared_realm(m_metadata_config);
    TableRef table = ObjectStore::table_for_object_type(realm->read_group(), c_sync_fileActionMetadata);
    Results results(realm, table->where());
    return SyncFileActionMetadataResults(std::move(results), std::move(realm), m_file_action_schema);
}

bool SyncMetadataManager::delete_metadata_action(const std::string& original_name) const
{
    auto shared_realm = Realm::get_shared_realm(m_metadata_config);

    // Retrieve the row for this object.
    TableRef table = ObjectStore::table_for_object_type(shared_realm->read_group(), c_sync_fileActionMetadata);
    shared_realm->begin_transaction();
    size_t row_idx = table->find_first_string(m_file_action_schema.idx_original_name, original_name);
    if (row_idx == not_found) {
        shared_realm->cancel_transaction();
        return false;
    }
    table->move_last_over(row_idx);
    shared_realm->commit_transaction();
    return true;
}

util::Optional<SyncUserMetadata> SyncMetadataManager::get_or_make_user_metadata(const std::string& identity,
                                                                                const std::string& url,
                                                                                bool make_if_absent) const
{
    auto realm = Realm::get_shared_realm(m_metadata_config);
    auto& schema = m_user_schema;

    // Retrieve or create the row for this object.
    TableRef table = ObjectStore::table_for_object_type(realm->read_group(), c_sync_userMetadata);
    Query query = table->where().equal(schema.idx_identity, identity).equal(schema.idx_auth_server_url, url);
    Results results(realm, std::move(query));
    REALM_ASSERT_DEBUG(results.size() < 2);
    auto row = results.first();

    if (!row) {
        if (!make_if_absent)
            return none;

        realm->begin_transaction();
        // Check the results again.
        row = results.first();
        if (!row) {
            auto row = table->get(table->add_empty_row());
            std::string uuid = util::uuid_string();
            row.set_string(schema.idx_identity, identity);
            row.set_string(schema.idx_auth_server_url, url);
            row.set_string(schema.idx_local_uuid, uuid);
            row.set_bool(schema.idx_user_is_admin, false);
            row.set_bool(schema.idx_marked_for_removal, false);
            realm->commit_transaction();
            return SyncUserMetadata(schema, std::move(realm), std::move(row));
        } else {
            // Someone beat us to adding this user.
            if (row->get_bool(schema.idx_marked_for_removal)) {
                // User is dead. Revive or return none.
                if (make_if_absent) {
                    row->set_bool(schema.idx_marked_for_removal, false);
                    realm->commit_transaction();
                } else {
                    realm->cancel_transaction();
                    return none;
                }
            } else {
                // User is alive, nothing else to do.
                realm->cancel_transaction();
            }
            return SyncUserMetadata(schema, std::move(realm), std::move(*row));
        }
    }

    // Got an existing user.
    if (row->get_bool(schema.idx_marked_for_removal)) {
        // User is dead. Revive or return none.
        if (make_if_absent) {
            realm->begin_transaction();
            row->set_bool(schema.idx_marked_for_removal, false);
            realm->commit_transaction();
        } else {
            return none;
        }
    }
    return SyncUserMetadata(schema, std::move(realm), std::move(*row));
}

SyncFileActionMetadata SyncMetadataManager::make_file_action_metadata(const std::string &original_name,
                                                                      const std::string &url,
                                                                      const std::string &local_uuid,
                                                                      SyncFileActionMetadata::Action action,
                                                                      util::Optional<std::string> new_name) const
{
    size_t raw_action = static_cast<size_t>(action);

    // Open the Realm.
    auto realm = Realm::get_shared_realm(m_metadata_config);
    auto& schema = m_file_action_schema;

    // Retrieve or create the row for this object.
    TableRef table = ObjectStore::table_for_object_type(realm->read_group(), c_sync_fileActionMetadata);
    realm->begin_transaction();
    size_t row_idx = table->find_first_string(schema.idx_original_name, original_name);
    if (row_idx == not_found) {
        row_idx = table->add_empty_row();
        table->set_string(schema.idx_original_name, row_idx, original_name);
    }
    table->set_string(schema.idx_new_name, row_idx, new_name);
    table->set_int(schema.idx_action, row_idx, raw_action);
    table->set_string(schema.idx_url, row_idx, url);
    table->set_string(schema.idx_user_identity, row_idx, local_uuid);
    realm->commit_transaction();
    return SyncFileActionMetadata(schema, std::move(realm), table->get(row_idx));
}

util::Optional<SyncFileActionMetadata> SyncMetadataManager::get_file_action_metadata(const std::string& original_name) const
{
    auto realm = Realm::get_shared_realm(m_metadata_config);
    auto schema = m_file_action_schema;
    TableRef table = ObjectStore::table_for_object_type(realm->read_group(), c_sync_fileActionMetadata);
    size_t row_idx = table->find_first_string(schema.idx_original_name, original_name);
    if (row_idx == not_found)
        return none;

    return SyncFileActionMetadata(std::move(schema), std::move(realm), table->get(row_idx));
}

// MARK: - Sync user metadata

SyncUserMetadata::SyncUserMetadata(Schema schema, SharedRealm realm, RowExpr row)
: m_realm(std::move(realm))
, m_schema(std::move(schema))
, m_row(row)
{ }

std::string SyncUserMetadata::identity() const
{
    REALM_ASSERT(m_realm);
    m_realm->verify_thread();
    return m_row.get_string(m_schema.idx_identity);
}

std::string SyncUserMetadata::local_uuid() const
{
    REALM_ASSERT(m_realm);
    m_realm->verify_thread();
    return m_row.get_string(m_schema.idx_local_uuid);
}

util::Optional<std::string> SyncUserMetadata::user_token() const
{
    REALM_ASSERT(m_realm);
    m_realm->verify_thread();
    StringData result = m_row.get_string(m_schema.idx_user_token);
    return result.is_null() ? util::none : util::make_optional(std::string(result));
}

std::string SyncUserMetadata::auth_server_url() const
{
    REALM_ASSERT(m_realm);
    m_realm->verify_thread();
    return m_row.get_string(m_schema.idx_auth_server_url);
}

bool SyncUserMetadata::is_admin() const
{
    REALM_ASSERT(m_realm);
    m_realm->verify_thread();
    return m_row.get_bool(m_schema.idx_user_is_admin);
}

void SyncUserMetadata::set_user_token(util::Optional<std::string> user_token)
{
    if (m_invalid)
        return;

    REALM_ASSERT_DEBUG(m_realm);
    m_realm->verify_thread();
    m_realm->begin_transaction();
    m_row.set_string(m_schema.idx_user_token, *user_token);
    m_realm->commit_transaction();
}

void SyncUserMetadata::set_is_admin(bool is_admin)
{
    if (m_invalid)
        return;

    REALM_ASSERT_DEBUG(m_realm);
    m_realm->verify_thread();
    m_realm->begin_transaction();
    m_row.set_bool(m_schema.idx_user_is_admin, is_admin);
    m_realm->commit_transaction();
}

void SyncUserMetadata::mark_for_removal()
{
    if (m_invalid)
        return;

    m_realm->verify_thread();
    m_realm->begin_transaction();
    m_row.set_bool(m_schema.idx_marked_for_removal, true);
    m_realm->commit_transaction();
}

void SyncUserMetadata::remove()
{
    m_invalid = true;
    m_realm->begin_transaction();
    TableRef table = ObjectStore::table_for_object_type(m_realm->read_group(), c_sync_userMetadata);
    table->move_last_over(m_row.get_index());
    m_realm->commit_transaction();
    m_realm = nullptr;
}

// MARK: - File action metadata

SyncFileActionMetadata::SyncFileActionMetadata(Schema schema, SharedRealm realm, RowExpr row)
: m_realm(std::move(realm))
, m_schema(std::move(schema))
, m_row(row)
{ }

std::string SyncFileActionMetadata::original_name() const
{
    REALM_ASSERT(m_realm);
    m_realm->verify_thread();
    return m_row.get_string(m_schema.idx_original_name);
}

util::Optional<std::string> SyncFileActionMetadata::new_name() const
{
    REALM_ASSERT(m_realm);
    m_realm->verify_thread();
    StringData result = m_row.get_string(m_schema.idx_new_name);
    return result.is_null() ? util::none : util::make_optional(std::string(result));
}

std::string SyncFileActionMetadata::user_local_uuid() const
{
    REALM_ASSERT(m_realm);
    m_realm->verify_thread();
    return m_row.get_string(m_schema.idx_user_identity);
}

SyncFileActionMetadata::Action SyncFileActionMetadata::action() const
{
    REALM_ASSERT(m_realm);
    m_realm->verify_thread();
    return static_cast<SyncFileActionMetadata::Action>(m_row.get_int(m_schema.idx_action));
}

std::string SyncFileActionMetadata::url() const
{
    REALM_ASSERT(m_realm);
    m_realm->verify_thread();
    return m_row.get_string(m_schema.idx_url);
}

void SyncFileActionMetadata::remove()
{
    REALM_ASSERT(m_realm);
    m_realm->verify_thread();
    m_realm->begin_transaction();
    TableRef table = ObjectStore::table_for_object_type(m_realm->read_group(), c_sync_fileActionMetadata);
    table->move_last_over(m_row.get_index());
    m_realm->commit_transaction();
    m_realm = nullptr;
}

} // namespace realm
