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

#ifndef REALM_OS_SYNC_USER_HPP
#define REALM_OS_SYNC_USER_HPP

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>

#include <realm/util/optional.hpp>

namespace realm {

class SyncSession;

// A `SyncUser` represents a single user account. Each user manages the sessions that
// are associated with it.
class SyncUser {
friend class SyncSession;
public:
    enum class TokenType {
        Normal,
        Admin,
    };

    enum class State {
        LoggedOut,
        Active,
        Error,
    };

    // Don't use this directly; use the `SyncManager` APIs. Public for use with `make_shared`.
    SyncUser(std::string refresh_token,
             std::string identity,
             util::Optional<std::string> server_url,
             TokenType token_type=TokenType::Normal);

    // Return a list of all sessions belonging to this user.
    std::vector<std::shared_ptr<SyncSession>> all_sessions();

    // Return a session for a given on disk path.
    // In most cases, bindings shouldn't expose this to consumers, since the on-disk
    // path for a synced Realm is an opaque implementation detail. This API is retained
    // for testing purposes, and for bindings for consumers that are servers or tools.
    std::shared_ptr<SyncSession> session_for_on_disk_path(const std::string& path);

    // Update the user's refresh token. If the user is logged out, it will log itself back in.
    // Note that this is called by the SyncManager, and should not be directly called.
    void update_refresh_token(std::string token);

    // Log the user out and mark it as such. This will also close its associated Sessions.
    void log_out();

    // Whether the user has administrator privileges.
    bool is_admin() const noexcept
    {
        return m_token_type == TokenType::Admin || m_is_admin;
    }

    TokenType token_type() const noexcept
    {
        return m_token_type;
    }

    // Specify whether the user has administrator privileges.
    // Note that this is an internal flag meant for bindings to communicate information
    // originating from the server. It is *NOT* possible to unilaterally change a user's
    // administrator status from the client through this or any other API.
    void set_is_admin(bool);

    std::string identity() const noexcept
    {
        return m_identity;
    }

    // FIXME: remove this APIs once the new token system is implemented.
    const std::string& server_url() const noexcept
    {
        return m_server_url;
    }

    std::string refresh_token() const;
    State state() const;

    // Register a session to this user.
    // A registered session will be bound at the earliest opportunity: either
    // immediately, or upon the user becoming Active.
    // Note that this is called by the SyncManager, and should not be directly called.
    void register_session(std::shared_ptr<SyncSession>);

    // Internal APIs. Do not call.
    void register_management_session(const std::string&);
    void register_permission_session(const std::string&);

private:
    State m_state;

    std::weak_ptr<SyncSession> m_management_session;
    std::weak_ptr<SyncSession> m_permission_session;

    // The auth server URL. Bindings should set this appropriately when they retrieve
    // instances of `SyncUser`s.
    // FIXME: once the new token system is implemented, this can be removed completely.
    std::string m_server_url;

    // Mark the user as invalid, since a fatal user-related error was encountered.
    void invalidate();

    mutable std::mutex m_mutex;

    // The token type of the user.
    // FIXME: remove this flag once bindings take responsible for admin token users
    TokenType m_token_type;

    bool m_is_admin;

    // The user's refresh token.
    std::string m_refresh_token;
    // Set by the server. The unique ID of the user account on the Realm Object Server.
    std::string m_identity;

    // Sessions are owned by the SyncManager, but the user keeps a map of weak references
    // to them.
    std::unordered_map<std::string, std::weak_ptr<SyncSession>> m_sessions;

    // Waiting sessions are those that should be asked to connect once this user is logged in.
    std::unordered_map<std::string, std::weak_ptr<SyncSession>> m_waiting_sessions;
};

}

#endif // REALM_OS_SYNC_USER_HPP
