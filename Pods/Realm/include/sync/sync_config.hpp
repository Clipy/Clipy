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

#ifndef REALM_OS_SYNC_CONFIG_HPP
#define REALM_OS_SYNC_CONFIG_HPP

#include "sync_user.hpp"

#include <realm/util/assert.hpp>
#include <realm/sync/client.hpp>
#include <realm/sync/protocol.hpp>

#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <unordered_map>

#include <realm/sync/history.hpp>

namespace realm {

class SyncUser;
class SyncSession;

using ChangesetTransformer = sync::ClientHistory::ChangesetCooker;

enum class SyncSessionStopPolicy;

struct SyncConfig;
using SyncBindSessionHandler = void(const std::string&,          // path on disk of the Realm file.
                                    const SyncConfig&,           // the sync configuration object.
                                    std::shared_ptr<SyncSession> // the session which should be bound.
                                    );

struct SyncError;
using SyncSessionErrorHandler = void(std::shared_ptr<SyncSession>, SyncError);

struct SyncError {
    using ProtocolError = realm::sync::ProtocolError;

    std::error_code error_code;
    std::string message;
    bool is_fatal;
    std::unordered_map<std::string, std::string> user_info;

    static constexpr const char c_original_file_path_key[] = "ORIGINAL_FILE_PATH";
    static constexpr const char c_recovery_file_path_key[] = "RECOVERY_FILE_PATH";

    /// The error is a client error, which applies to the client and all its sessions.
    bool is_client_error() const
    {
        return error_code.category() == realm::sync::client_error_category();
    }

    /// The error is a protocol error, which may either be connection-level or session-level.
    bool is_connection_level_protocol_error() const
    {
        if (error_code.category() != realm::sync::protocol_error_category()) {
            return false;
        }
        return !realm::sync::is_session_level_error(static_cast<ProtocolError>(error_code.value()));
    }

    /// The error is a connection-level protocol error.
    bool is_session_level_protocol_error() const
    {
        if (error_code.category() != realm::sync::protocol_error_category()) {
            return false;
        }
        return realm::sync::is_session_level_error(static_cast<ProtocolError>(error_code.value()));
    }

    /// The error indicates a client reset situation.
    bool is_client_reset_requested() const
    {
        if (error_code.category() != realm::sync::protocol_error_category()) {
            return false;
        }
        // Documented here: https://realm.io/docs/realm-object-server/#client-recovery-from-a-backup
        return (error_code == ProtocolError::bad_server_file_ident
                || error_code == ProtocolError::bad_client_file_ident
                || error_code == ProtocolError::bad_server_version
                || error_code == ProtocolError::diverging_histories);
    }
};

struct SyncConfig {
    std::shared_ptr<SyncUser> user;
    // The URL of the Realm, or of the reference Realm if partial sync is being used.
    // The URL that will be used when connecting to the object server is that returned by `realm_url()`,
    // and will differ from `reference_realm_url` if partial sync is being used.
    // Set this field, but read from `realm_url()`.
    std::string reference_realm_url;
    SyncSessionStopPolicy stop_policy;
    std::function<SyncBindSessionHandler> bind_session_handler;
    std::function<SyncSessionErrorHandler> error_handler;
    std::shared_ptr<ChangesetTransformer> transformer;
    util::Optional<std::array<char, 64>> realm_encryption_key;
    bool client_validate_ssl;
    util::Optional<std::string> ssl_trust_certificate_path;
    std::function<sync::Session::SSLVerifyCallback> ssl_verify_callback;
    bool is_partial;
    util::Optional<std::string> custom_partial_sync_identifier;

    bool validate_sync_history = true;

    // The URL that will be used when connecting to the object server.
    // This will differ from `reference_realm_url` when partial sync is being used.
    std::string realm_url() const;

    SyncConfig(std::shared_ptr<SyncUser> user, std::string reference_realm_url,
               SyncSessionStopPolicy stop_policy,
               std::function<SyncBindSessionHandler> bind_session_handler,
               std::function<SyncSessionErrorHandler> error_handler = nullptr,
               std::shared_ptr<ChangesetTransformer> transformer = nullptr,
               util::Optional<std::array<char, 64>> realm_encryption_key = util::none,
               bool client_validate_ssl = true, 
               util::Optional<std::string> ssl_trust_certificate_path = util::none, 
               std::function<realm::sync::Session::SSLVerifyCallback> ssl_verify_callback = nullptr,
               bool is_partial = false,
               util::Optional<std::string> custom_partial_sync_identifier = util::none
        )
        : user(std::move(user))
        , reference_realm_url(std::move(reference_realm_url))
        , stop_policy(stop_policy)
        , bind_session_handler(std::move(bind_session_handler))
        , error_handler(std::move(error_handler))
        , transformer(std::move(transformer))
        , realm_encryption_key(std::move(realm_encryption_key))
        , client_validate_ssl(client_validate_ssl)
        , ssl_trust_certificate_path(std::move(ssl_trust_certificate_path))
        , ssl_verify_callback(std::move(ssl_verify_callback))
        , is_partial(is_partial)
        , custom_partial_sync_identifier(std::move(custom_partial_sync_identifier))
    {
    }
};

} // namespace realm

#endif // REALM_OS_SYNC_CONFIG_HPP
