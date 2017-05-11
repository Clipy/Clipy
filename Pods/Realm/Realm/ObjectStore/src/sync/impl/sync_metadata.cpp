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
#if REALM_PLATFORM_APPLE
#include "impl/apple/keychain_helper.hpp"
#endif

#include <realm/descriptor.hpp>
#include <realm/table.hpp>

namespace realm {

static const char * const c_sync_userMetadata = "UserMetadata";
static const char * const c_sync_marked_for_removal = "marked_for_removal";
static const char * const c_sync_identity = "identity";
static const char * const c_sync_auth_server_url = "auth_server_url";
static const char * const c_sync_user_token = "user_token";
static const char * const c_sync_user_is_admin = "user_is_admin";

static const char * const c_sync_fileActionMetadata = "FileActionMetadata";
static const char * const c_sync_original_name = "original_name";
static const char * const c_sync_new_name = "new_name";
static const char * const c_sync_action = "action";
static const char * const c_sync_url = "url";

namespace {

Property make_nullable_string_property(const char* name)
{
    Property p = {name, PropertyType::String};
    p.is_nullable = true;
    return p;
}

Property make_primary_key_property(const char* name)
{
    Property p = {name, PropertyType::String};
    p.is_indexed = true;
    p.is_primary = true;
    return p;
}

Schema make_schema()
{
    return Schema{
        {c_sync_userMetadata, {
            make_primary_key_property(c_sync_identity),
            {c_sync_marked_for_removal, PropertyType::Bool},
            make_nullable_string_property(c_sync_auth_server_url),
            make_nullable_string_property(c_sync_user_token),
            {c_sync_user_is_admin, PropertyType::Bool},
        }},
        {c_sync_fileActionMetadata, {
            make_primary_key_property(c_sync_original_name),
            {c_sync_action, PropertyType::Int},
            make_nullable_string_property(c_sync_new_name),
            {c_sync_url, PropertyType::String},
            {c_sync_identity, PropertyType::String},
        }},
    };
}

}

// MARK: - Sync metadata manager

SyncMetadataManager::SyncMetadataManager(std::string path,
                                         bool should_encrypt,
                                         util::Optional<std::vector<char>> encryption_key)
{
    constexpr uint64_t SCHEMA_VERSION = 1;
    std::lock_guard<std::mutex> lock(m_metadata_lock);

    Realm::Config config;
    config.path = std::move(path);
    config.schema = make_schema();
    config.schema_version = SCHEMA_VERSION;
    config.schema_mode = SchemaMode::Automatic;
#if REALM_PLATFORM_APPLE
    if (should_encrypt && !encryption_key) {
        encryption_key = keychain::metadata_realm_encryption_key();
    }
#endif
    if (should_encrypt) {
        if (!encryption_key) {
            throw std::invalid_argument("Metadata Realm encryption was specified, but no encryption key was provided.");
        }
        config.encryption_key = std::move(*encryption_key);
    }

    // Open the Realm.
    SharedRealm realm = Realm::get_shared_realm(config);

    // Get data about the (hardcoded) schemas.
    DescriptorRef descriptor = ObjectStore::table_for_object_type(realm->read_group(),
                                                                  c_sync_userMetadata)->get_descriptor();
    m_user_schema = {
        descriptor->get_column_index(c_sync_identity),
        descriptor->get_column_index(c_sync_marked_for_removal),
        descriptor->get_column_index(c_sync_user_token),
        descriptor->get_column_index(c_sync_auth_server_url),
        descriptor->get_column_index(c_sync_user_is_admin),
    };

    descriptor = ObjectStore::table_for_object_type(realm->read_group(), c_sync_fileActionMetadata)->get_descriptor();
    m_file_action_schema = {
        descriptor->get_column_index(c_sync_original_name),
        descriptor->get_column_index(c_sync_new_name),
        descriptor->get_column_index(c_sync_action),
        descriptor->get_column_index(c_sync_url),
        descriptor->get_column_index(c_sync_identity)
    };

    m_metadata_config = std::move(config);
}

Realm::Config SyncMetadataManager::get_configuration() const
{
    std::lock_guard<std::mutex> lock(m_metadata_lock);
    return m_metadata_config;
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
    // Open the Realm.
    SharedRealm realm = Realm::get_shared_realm(get_configuration());

    TableRef table = ObjectStore::table_for_object_type(realm->read_group(), c_sync_userMetadata);
    Query query = table->where().equal(m_user_schema.idx_marked_for_removal, marked);

    Results results(realm, std::move(query));
    return SyncUserMetadataResults(std::move(results), std::move(realm), m_user_schema);
}

SyncFileActionMetadataResults SyncMetadataManager::all_pending_actions() const
{
    SharedRealm realm = Realm::get_shared_realm(get_configuration());
    TableRef table = ObjectStore::table_for_object_type(realm->read_group(), c_sync_fileActionMetadata);
    Results results(realm, table->where());
    return SyncFileActionMetadataResults(std::move(results), std::move(realm), m_file_action_schema);
}

// MARK: - Sync user metadata

SyncUserMetadata::SyncUserMetadata(Schema schema, SharedRealm realm, RowExpr row)
: m_invalid(row.get_bool(schema.idx_marked_for_removal))
, m_schema(std::move(schema))
, m_realm(std::move(realm))
, m_row(row)
{ }

SyncUserMetadata::SyncUserMetadata(const SyncMetadataManager& manager, std::string identity, bool make_if_absent)
: m_schema(manager.m_user_schema)
{
    // Open the Realm.
    m_realm = Realm::get_shared_realm(manager.get_configuration());

    // Retrieve or create the row for this object.
    TableRef table = ObjectStore::table_for_object_type(m_realm->read_group(), c_sync_userMetadata);
    size_t row_idx = table->find_first_string(m_schema.idx_identity, identity);
    if (row_idx == not_found) {
        if (!make_if_absent) {
            m_invalid = true;
            m_realm = nullptr;
            return;
        }
        m_realm->begin_transaction();
        row_idx = table->find_first_string(m_schema.idx_identity, identity);
        if (row_idx == not_found) {
            row_idx = table->add_empty_row();
            table->set_string(m_schema.idx_identity, row_idx, identity);
            table->set_bool(m_schema.idx_user_is_admin, row_idx, false);
            m_realm->commit_transaction();
        } else {
            // Someone beat us to adding this user.
            m_realm->cancel_transaction();
        }
    }
    m_row = table->get(row_idx);
    if (make_if_absent) {
        // User existed in the table, but had been marked for deletion. Unmark it.
        m_realm->begin_transaction();
        table->set_bool(m_schema.idx_marked_for_removal, row_idx, false);
        m_realm->commit_transaction();
        m_invalid = false;
    } else {
        m_invalid = m_row.get_bool(m_schema.idx_marked_for_removal);
    }
}

bool SyncUserMetadata::is_valid() const
{
    return !m_invalid;
}

std::string SyncUserMetadata::identity() const
{
    REALM_ASSERT(m_realm);
    m_realm->verify_thread();
    StringData result = m_row.get_string(m_schema.idx_identity);
    return result;
}

bool SyncUserMetadata::is_admin() const
{
    REALM_ASSERT(m_realm);
    m_realm->verify_thread();
    return m_row.get_bool(m_schema.idx_user_is_admin);
}

util::Optional<std::string> SyncUserMetadata::get_optional_string_field(size_t col_idx) const
{
    REALM_ASSERT(m_realm);
    m_realm->verify_thread();
    StringData result = m_row.get_string(col_idx);
    return result.is_null() ? util::none : util::make_optional(std::string(result));
}

util::Optional<std::string> SyncUserMetadata::server_url() const
{
    return get_optional_string_field(m_schema.idx_auth_server_url);
}

util::Optional<std::string> SyncUserMetadata::user_token() const
{
    return get_optional_string_field(m_schema.idx_user_token);
}

void SyncUserMetadata::set_state(util::Optional<std::string> server_url, util::Optional<std::string> user_token)
{
    if (m_invalid) {
        return;
    }
    REALM_ASSERT_DEBUG(m_realm);
    m_realm->verify_thread();
    m_realm->begin_transaction();
    m_row.set_string(m_schema.idx_user_token, *user_token);
    m_row.set_string(m_schema.idx_auth_server_url, *server_url);
    m_realm->commit_transaction();
}

void SyncUserMetadata::set_is_admin(bool is_admin)
{
    if (m_invalid) {
        return;
    }
    REALM_ASSERT_DEBUG(m_realm);
    m_realm->verify_thread();
    m_realm->begin_transaction();
    m_row.set_bool(m_schema.idx_user_is_admin, is_admin);
    m_realm->commit_transaction();
}

void SyncUserMetadata::mark_for_removal()
{
    if (m_invalid) {
        return;
    }
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

util::Optional<SyncFileActionMetadata> SyncFileActionMetadata::metadata_for_path(const std::string& original_name, const SyncMetadataManager& manager)
{
    auto realm = Realm::get_shared_realm(manager.get_configuration());
    auto schema = manager.m_file_action_schema;
    TableRef table = ObjectStore::table_for_object_type(realm->read_group(), c_sync_fileActionMetadata);
    size_t row_idx = table->find_first_string(schema.idx_original_name, original_name);
    if (row_idx == not_found) {
        return none;
    }
    return SyncFileActionMetadata(std::move(schema), std::move(realm), table->get(row_idx));
}                   

SyncFileActionMetadata::SyncFileActionMetadata(const SyncMetadataManager& manager,
                                               Action action,
                                               const std::string& original_name,
                                               const std::string& url,
                                               const std::string& user_identity,
                                               util::Optional<std::string> new_name)
: m_schema(manager.m_file_action_schema)
{
    size_t raw_action = static_cast<size_t>(action);

    // Open the Realm.
    m_realm = Realm::get_shared_realm(manager.get_configuration());

    // Retrieve or create the row for this object.
    TableRef table = ObjectStore::table_for_object_type(m_realm->read_group(), c_sync_fileActionMetadata);
    m_realm->begin_transaction();
    size_t row_idx = table->find_first_string(m_schema.idx_original_name, original_name);
    if (row_idx == not_found) {
        row_idx = table->add_empty_row();
        table->set_string(m_schema.idx_original_name, row_idx, original_name);
    }
    table->set_string(m_schema.idx_new_name, row_idx, new_name);
    table->set_int(m_schema.idx_action, row_idx, raw_action);
    table->set_string(m_schema.idx_url, row_idx, url);
    table->set_string(m_schema.idx_user_identity, row_idx, user_identity);
    m_realm->commit_transaction();
    m_row = table->get(row_idx);
}

SyncFileActionMetadata::SyncFileActionMetadata(Schema schema, SharedRealm realm, RowExpr row)
: m_schema(std::move(schema))
, m_realm(std::move(realm))
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

std::string SyncFileActionMetadata::user_identity() const
{
    REALM_ASSERT(m_realm);
    m_realm->verify_thread();
    return m_row.get_string(m_schema.idx_user_identity);
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

}
