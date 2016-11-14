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

#ifndef REALM_OS_SYNC_SESSION_HPP
#define REALM_OS_SYNC_SESSION_HPP

#include <realm/util/optional.hpp>
#include <realm/version_id.hpp>

#include "sync_config.hpp"

#include <mutex>

namespace realm {

class SyncManager;

namespace _impl {
class RealmCoordinator;
struct SyncClient;

namespace sync_session_states {
struct WaitingForAccessToken;
struct Active;
struct Dying;
struct Inactive;
struct Error;
}
}

namespace sync {
class Session;
}

using SyncSessionTransactCallback = void(VersionID old_version, VersionID new_version);

struct SyncSession : public std::enable_shared_from_this<SyncSession> {
    bool is_valid() const;

    std::string const& path() const { return m_realm_path; }

    void wait_for_upload_completion(std::function<void()> callback);
    void wait_for_download_completion(std::function<void()> callback);

    // If the sync session is currently `Dying`, ask it to stay alive instead.
    // If the sync session is currently `Inactive`, recreate it. Otherwise, a no-op.
    void revive_if_needed();

    void refresh_access_token(std::string access_token, util::Optional<std::string> server_url);

    // Inform the sync session that it should close.
    void close();

    // Inform the sync session that it should close, but only if it is not yet connected.
    void close_if_connecting();

    // Inform the sync session that it should log out.
    void log_out();

    // Expose some internal functionality to other parts of the ObjectStore
    // without making it public to everyone
    class Internal {
        friend class _impl::RealmCoordinator;

        static void set_sync_transact_callback(SyncSession& session,
                                               std::function<SyncSessionTransactCallback> callback)
        {
            session.set_sync_transact_callback(std::move(callback));
        }

        static void set_error_handler(SyncSession& session, std::function<SyncSessionErrorHandler> callback)
        {
            session.set_error_handler(std::move(callback));
        }

        static void nonsync_transact_notify(SyncSession& session, VersionID::version_type version)
        {
            session.nonsync_transact_notify(version);
        }
    };

private:
    struct State;
    friend struct _impl::sync_session_states::WaitingForAccessToken;
    friend struct _impl::sync_session_states::Active;
    friend struct _impl::sync_session_states::Dying;
    friend struct _impl::sync_session_states::Inactive;
    friend struct _impl::sync_session_states::Error;

    friend class realm::SyncManager;
    // Called by SyncManager {
    SyncSession(std::shared_ptr<_impl::SyncClient>, std::string realm_path, SyncConfig);

    // Check if this sync session is actually inactive
    bool is_inactive() const;
    // }

    bool can_wait_for_network_completion() const;

    void set_sync_transact_callback(std::function<SyncSessionTransactCallback>);
    void set_error_handler(std::function<SyncSessionErrorHandler>);
    void nonsync_transact_notify(VersionID::version_type);

    void advance_state(std::unique_lock<std::mutex>& lock, const State&);

    void create_sync_session();
    void unregister(std::unique_lock<std::mutex>& lock);

    std::function<SyncSessionTransactCallback> m_sync_transact_callback;
    std::function<SyncSessionErrorHandler> m_error_handler;

    mutable std::mutex m_state_mutex;

    const State* m_state = nullptr;
    size_t m_pending_upload_threads = 0;

    SyncConfig m_config;

    std::string m_realm_path;
    std::shared_ptr<_impl::SyncClient> m_client;
    std::unique_ptr<sync::Session> m_session;
    util::Optional<int_fast64_t> m_deferred_commit_notification;
    bool m_deferred_close = false;

    // The fully-resolved URL of this Realm, including the server and the path.
    util::Optional<std::string> m_server_url;
};

}

#endif // REALM_OS_SYNC_SESSION_HPP
