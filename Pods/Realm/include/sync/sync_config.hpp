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

#include <realm/util/assert.hpp>
#include <realm/sync/client.hpp>
#include <realm/sync/protocol.hpp>

#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <unordered_map>

namespace realm {

class SyncUser;
class SyncSession;

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
    std::string realm_url;
    SyncSessionStopPolicy stop_policy;
    std::function<SyncBindSessionHandler> bind_session_handler;
    std::function<SyncSessionErrorHandler> error_handler;
};

} // namespace realm

#endif // REALM_OS_SYNC_CONFIG_HPP
