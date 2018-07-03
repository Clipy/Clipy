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

#ifndef REALM_OS_SYNC_METADATA_HPP
#define REALM_OS_SYNC_METADATA_HPP

#include <string>

#include <realm/row.hpp>
#include <realm/table.hpp>
#include <realm/util/optional.hpp>

#include "results.hpp"
#include "shared_realm.hpp"

namespace realm {
template<typename T> class BasicRowExpr;
using RowExpr = BasicRowExpr<Table>;
class SyncMetadataManager;

// A facade for a metadata Realm object representing a sync user.
class SyncUserMetadata {
public:
    struct Schema {
        // The ROS identity of the user. This, plus the auth server URL, uniquely identifies a user.
        size_t idx_identity;
        // A locally issued UUID for the user. This is used to generate the on-disk user directory.
        size_t idx_local_uuid;
        // Whether or not this user has been marked for removal.
        size_t idx_marked_for_removal;
        // The cached refresh token for this user.
        size_t idx_user_token;
        // The URL of the authentication server this user resides upon.
        size_t idx_auth_server_url;
        // Whether or not the auth server reported that this user is marked as an administrator.
        size_t idx_user_is_admin;
    };

    // Cannot be set after creation.
    std::string identity() const;

    // Cannot be set after creation.
    std::string local_uuid() const;

    util::Optional<std::string> user_token() const;
    void set_user_token(util::Optional<std::string>);

    // Cannot be set after creation.
    std::string auth_server_url() const;

    bool is_admin() const;
    void set_is_admin(bool);

    // Mark the user as "ready for removal". Since Realm files cannot be safely deleted after being opened, the actual
    // deletion of a user must be deferred until the next time the host application is launched.
    void mark_for_removal();

    void remove();

    bool is_valid() const
    {
        return !m_invalid;
    }

    // INTERNAL USE ONLY
    SyncUserMetadata(Schema schema, SharedRealm realm, RowExpr row);
private:
    bool m_invalid = false;
    SharedRealm m_realm;
    Schema m_schema;
    Row m_row;
};

// A facade for a metadata Realm object representing a pending action to be carried out upon a specific file(s).
class SyncFileActionMetadata {
public:
    struct Schema {
        // The original path on disk of the file (generally, the main file for an on-disk Realm).
        size_t idx_original_name;
        // A new path on disk for a file to be written to. Context-dependent.
        size_t idx_new_name;
        // An enum describing the action to take.
        size_t idx_action;
        // The full remote URL of the Realm on the ROS.
        size_t idx_url;
        // The local UUID of the user to whom the file action applies (despite the internal column name).
        size_t idx_user_identity;
    };

    enum class Action {
        // The Realm files at the given directory will be deleted.
        DeleteRealm,
        // The Realm file will be copied to a 'recovery' directory, and the original Realm files will be deleted.
        BackUpThenDeleteRealm
    };

    // The absolute path to the Realm file in question.
    std::string original_name() const;

    // The meaning of this parameter depends on the `Action` specified.
    // For `BackUpThenDeleteRealm`, it is the absolute path where the backup copy
    // of the Realm file found at `original_name()` will be placed.
    // For all other `Action`s, it is ignored.
    util::Optional<std::string> new_name() const;

    // Get the local UUID of the user associated with this file action metadata.
    std::string user_local_uuid() const;

    Action action() const;
    std::string url() const;
    void remove();

    // INTERNAL USE ONLY
    SyncFileActionMetadata(Schema schema, SharedRealm realm, RowExpr row);
private:
    SharedRealm m_realm;
    Schema m_schema;
    Row m_row;
};

class SyncClientMetadata {
public:
    struct Schema {
        // A UUID that identifies this client.
        size_t idx_uuid;
    };
};

template<class T>
class SyncMetadataResults {
public:
    size_t size() const
    {
        return m_results.size();
    }

    T get(size_t idx) const
    {
        RowExpr row = m_results.get(idx);
        return T(m_schema, m_realm, row);
    }

    SyncMetadataResults(Results results, SharedRealm realm, typename T::Schema schema)
    : m_schema(std::move(schema))
    , m_realm(std::move(realm))
    , m_results(std::move(results))
    { }
private:
    typename T::Schema m_schema;
    SharedRealm m_realm;
    // FIXME: remove 'mutable' once `realm::Results` is properly annotated for const
    mutable Results m_results;
};
using SyncUserMetadataResults = SyncMetadataResults<SyncUserMetadata>;
using SyncFileActionMetadataResults = SyncMetadataResults<SyncFileActionMetadata>;

// A facade for the application's metadata Realm.
class SyncMetadataManager {
friend class SyncUserMetadata;
friend class SyncFileActionMetadata;
public:
    // Return a Results object containing all users not marked for removal.
    SyncUserMetadataResults all_unmarked_users() const;

    // Return a Results object containing all users marked for removal. It is the binding's responsibility to call
    // `remove()` on each user to actually remove it from the database. (This is so that already-open Realm files can be
    // safely cleaned up the next time the host is launched.)
    SyncUserMetadataResults all_users_marked_for_removal() const;

    // Return a Results object containing all pending actions.
    SyncFileActionMetadataResults all_pending_actions() const;

    // Delete an existing metadata action given the original name of the Realm it involves.
    // Returns true iff there was an existing metadata action and it was deleted.
    bool delete_metadata_action(const std::string&) const;

    // Retrieve or create user metadata.
    // Note: if `make_is_absent` is true and the user has been marked for deletion, it will be unmarked.
    util::Optional<SyncUserMetadata> get_or_make_user_metadata(const std::string& identity, const std::string& url,
                                                               bool make_if_absent=true) const;

    // Retrieve file action metadata.
    util::Optional<SyncFileActionMetadata> get_file_action_metadata(const std::string& path) const;

    // Create file action metadata.
    SyncFileActionMetadata make_file_action_metadata(const std::string& original_name,
                                                     const std::string& url,
                                                     const std::string& local_uuid,
                                                     SyncFileActionMetadata::Action action,
                                                     util::Optional<std::string> new_name=none) const;

    // Get the unique identifier of this client.
    const std::string& client_uuid() const { return m_client_uuid; }

    /// Construct the metadata manager.
    ///
    /// If the platform supports it, setting `should_encrypt` to `true` and not specifying an encryption key will make
    /// the object store handle generating and persisting an encryption key for the metadata database. Otherwise, an
    /// exception will be thrown.
    SyncMetadataManager(std::string path,
                        bool should_encrypt,
                        util::Optional<std::vector<char>> encryption_key=none);

private:
    SyncUserMetadataResults get_users(bool marked) const;
    Realm::Config m_metadata_config;
    SyncUserMetadata::Schema m_user_schema;
    SyncFileActionMetadata::Schema m_file_action_schema;
    SyncClientMetadata::Schema m_client_schema;
    std::string m_client_uuid;
};

}

#endif // REALM_OS_SYNC_METADATA_HPP
