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

#include "sync_metadata.hpp"

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

SyncMetadataManager::SyncMetadataManager(std::string path,
                                         bool should_encrypt,
                                         util::Optional<std::vector<char>> encryption_key)
{
    std::lock_guard<std::mutex> lock(m_metadata_lock);

    auto nullable_string_property = [](std::string name)->Property {
        Property p = { name, PropertyType::String };
        p.is_nullable = true;
        return p;
    };

    Property primary_key = { c_sync_identity, PropertyType::String };
    primary_key.is_indexed = true;
    primary_key.is_primary = true;

    Realm::Config config;
    config.path = std::move(path);
    Schema schema = {
        { c_sync_userMetadata,
            {
                primary_key,
                { c_sync_marked_for_removal, PropertyType::Bool },
                nullable_string_property(c_sync_auth_server_url),
                nullable_string_property(c_sync_user_token),
            }
        }
    };
    config.schema = std::move(schema);
    config.schema_mode = SchemaMode::Additive;
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

    // Get data about the (hardcoded) schema.
    DescriptorRef descriptor = ObjectStore::table_for_object_type(realm->read_group(),
                                                                  c_sync_userMetadata)->get_descriptor();
    m_schema = {
        descriptor->get_column_index(c_sync_identity),
        descriptor->get_column_index(c_sync_marked_for_removal),
        descriptor->get_column_index(c_sync_user_token),
        descriptor->get_column_index(c_sync_auth_server_url)
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
    Query query = table->where().equal(m_schema.idx_marked_for_removal, marked);

    Results results(realm, std::move(query));
    return SyncUserMetadataResults(std::move(results), std::move(realm), m_schema);
}

SyncUserMetadata::SyncUserMetadata(Schema schema, SharedRealm realm, RowExpr row)
: m_invalid(row.get_bool(schema.idx_marked_for_removal))
, m_schema(std::move(schema))
, m_realm(std::move(realm))
, m_row(row)
{ }

SyncUserMetadata::SyncUserMetadata(SyncMetadataManager& manager, std::string identity, bool make_if_absent)
: m_schema(manager.m_schema)
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
    m_realm->verify_thread();
    StringData result = m_row.get_string(m_schema.idx_identity);
    return result;
}

util::Optional<std::string> SyncUserMetadata::get_optional_string_field(size_t col_idx) const
{
    REALM_ASSERT(!m_invalid);
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
    m_realm->verify_thread();
    m_realm->begin_transaction();
    m_row.set_string(m_schema.idx_user_token, *user_token);
    m_row.set_string(m_schema.idx_auth_server_url, *server_url);
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

}
