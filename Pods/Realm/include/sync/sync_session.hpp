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

#include "feature_checks.hpp"
#include "sync/sync_config.hpp"

#include <realm/util/optional.hpp>
#include <realm/version_id.hpp>

#include <mutex>
#include <unordered_map>

namespace realm {

class SyncManager;
class SyncUser;

namespace _impl {
class RealmCoordinator;
struct SyncClient;
class WriteTransactionNotifyingSync;

namespace sync_session_states {
struct WaitingForAccessToken;
struct Active;
struct Dying;
struct Inactive;
}
}

namespace sync {
class Session;
}

using SyncSessionTransactCallback = void(VersionID old_version, VersionID new_version);
using SyncProgressNotifierCallback = void(uint64_t transferred_bytes, uint64_t transferrable_bytes);

namespace _impl {
class SyncProgressNotifier {
public:
    enum class NotifierType {
        upload, download
    };

    uint64_t register_callback(std::function<SyncProgressNotifierCallback>,
                               NotifierType direction, bool is_streaming);
    void unregister_callback(uint64_t);

    void set_local_version(uint64_t);
    void update(uint64_t downloaded, uint64_t downloadable,
                uint64_t uploaded, uint64_t uploadable, uint64_t, uint64_t);

private:
    mutable std::mutex m_mutex;

    // How many bytes are uploadable or downloadable.
    struct Progress {
        uint64_t uploadable;
        uint64_t downloadable;
        uint64_t uploaded;
        uint64_t downloaded;
        uint64_t snapshot_version;
    };

    // A PODS encapsulating some information for progress notifier callbacks a binding
    // can register upon this session.
    struct NotifierPackage {
        std::function<SyncProgressNotifierCallback> notifier;
        util::Optional<uint64_t> captured_transferrable;
        uint64_t snapshot_version;
        bool is_streaming;
        bool is_download;

        std::function<void()> create_invocation(const Progress&, bool&);
    };

    // A counter used as a token to identify progress notifier callbacks registered on this session.
    uint64_t m_progress_notifier_token = 1;
    // Version of the last locally-created transaction that we're expecting to be uploaded.
    uint64_t m_local_transaction_version = 0;

    // Will be `none` until we've received the initial notification from sync.  Note that this
    // happens only once ever during the lifetime of a given `SyncSession`, since these values are
    // expected to semi-monotonically increase, and a lower-bounds estimate is still useful in the
    // event more up-to-date information isn't yet available.  FIXME: If we support transparent
    // client reset in the future, we might need to reset the progress state variables if the Realm
    // is rolled back.
    util::Optional<Progress> m_current_progress;

    std::unordered_map<uint64_t, NotifierPackage> m_packages;
};

} // namespace _impl

class SyncSession : public std::enable_shared_from_this<SyncSession> {
public:
    enum class PublicState {
        WaitingForAccessToken,
        Active,
        Dying,
        Inactive,

        // FIXME: This state no longer exists. This should be removed.
        Error,
    };
    PublicState state() const;

    // FIXME: The error state no longer exists. This should be removed.
    bool is_in_error_state() const { return false; }

    // The on-disk path of the Realm file backing the Realm this `SyncSession` represents.
    std::string const& path() const { return m_realm_path; }

    // Register a callback that will be called when all pending uploads have completed.
    // The callback is run asynchronously, and upon whatever thread the underlying sync client
    // chooses to run it on. The method returns immediately with true if the callback was
    // successfully registered, false otherwise. If the method returns false the callback will
    // never be run.
    // This method will return true if the completion handler was registered, either immediately
    // or placed in a queue. If it returns true the completion handler will always be called
    // at least once, except in the case where a logged-out session is never logged back in.
    bool wait_for_upload_completion(std::function<void(std::error_code)> callback);

    // Register a callback that will be called when all pending downloads have been completed.
    // Works the same way as `wait_for_upload_completion()`.
    bool wait_for_download_completion(std::function<void(std::error_code)> callback);

    using NotifierType = _impl::SyncProgressNotifier::NotifierType;
    // Register a notifier that updates the app regarding progress.
    //
    // If `m_current_progress` is populated when this method is called, the notifier
    // will be called synchronously, to provide the caller with an initial assessment
    // of the state of synchronization. Otherwise, the progress notifier will be
    // registered, and only called once sync has begun providing progress data.
    //
    // If `is_streaming` is true, then the notifier will be called forever, and will
    // always contain the most up-to-date number of downloadable or uploadable bytes.
    // Otherwise, the number of downloaded or uploaded bytes will always be reported
    // relative to the number of downloadable or uploadable bytes at the point in time
    // when the notifier was registered.
    //
    // An integer representing a token is returned. This token can be used to manually
    // unregister the notifier. If the integer is 0, the notifier was not registered.
    //
    // Note that bindings should dispatch the callback onto a separate thread or queue
    // in order to avoid blocking the sync client.
    uint64_t register_progress_notifier(std::function<SyncProgressNotifierCallback>, NotifierType, bool is_streaming);

    // Unregister a previously registered notifier. If the token is invalid,
    // this method does nothing.
    void unregister_progress_notifier(uint64_t);

    // If possible, take the session and do anything necessary to make it `Active`.
    // Specifically:
    // If the sync session is currently `Dying`, ask it to stay alive instead.
    // If the sync session is currently `WaitingForAccessToken`, cancel any deferred close.
    // If the sync session is currently `Inactive`, recreate it.
    // Otherwise, a no-op.
    void revive_if_needed();

    // Perform any actions needed in response to regaining network connectivity.
    // Specifically:
    // If the sync session is currently `WaitingForAccessToken`, make the binding ask the auth server for a token.
    // Otherwise, a no-op.
    void handle_reconnect();

    // Give the `SyncSession` a new, valid token, and ask it to refresh the underlying session.
    // If the session can't accept a new token, this method does nothing.
    // Note that, if this is the first time the session will be given a token, `server_url` must
    // be set.
    void refresh_access_token(std::string access_token, util::Optional<std::string> server_url);

    // FIXME: we need an API to allow the binding to tell sync that the access token fetch failed
    // or was cancelled, and cannot be retried.

    // Set the multiplex identifier used for this session. Sessions with different identifiers are
    // never multiplexed into a single connection, even if they are connecting to the same host.
    // The value of the token is otherwise treated as an opaque token.
    //
    // Has no effect if session multiplexing is not enabled (see SyncManager::enable_session_multiplexing())
    // or if called after the Sync session is created. In particular, changing the multiplex identity will
    // not make the session reconnect.
    void set_multiplex_identifier(std::string multiplex_identity);

    // Inform the sync session that it should close.
    void close();

    // Inform the sync session that it should log out.
    void log_out();

    // Override the address and port of the server that this `SyncSession` is connected to. If the
    // session is already connected, it will disconnect and then reconnect to the specified address.
    // If it's not already connected, future connection attempts will be to the specified address.
    //
    // NOTE: This is intended for use only in very specific circumstances. Please check with the
    // object store team before using it.
    void override_server(std::string address, int port);

    // An object representing the user who owns the Realm this `SyncSession` represents.
    std::shared_ptr<SyncUser> user() const
    {
        return m_config.user;
    }

    // A copy of the configuration object describing the Realm this `SyncSession` represents.
    const SyncConfig& config() const
    {
        return m_config;
    }

    // If the `SyncSession` has been configured, the full remote URL of the Realm
    // this `SyncSession` represents.
    util::Optional<std::string> full_realm_url() const
    {
        return m_server_url;
    }

    // Create an external reference to this session. The sync session attempts to remain active
    // as long as an external reference to the session exists.
    std::shared_ptr<SyncSession> external_reference();

    // Return an existing external reference to this session, if one exists. Otherwise, returns `nullptr`.
    std::shared_ptr<SyncSession> existing_external_reference();

    // Expose some internal functionality to other parts of the ObjectStore
    // without making it public to everyone
    class Internal {
        friend class _impl::RealmCoordinator;
        friend class _impl::WriteTransactionNotifyingSync;

        static void set_sync_transact_callback(SyncSession& session,
                                               std::function<SyncSessionTransactCallback> callback)
        {
            session.set_sync_transact_callback(std::move(callback));
        }

        static void nonsync_transact_notify(SyncSession& session, VersionID::version_type version)
        {
            session.nonsync_transact_notify(version);
        }
    };

    // Expose some internal functionality to testing code.
    struct OnlyForTesting {
        static void handle_error(SyncSession& session, SyncError error)
        {
            session.handle_error(std::move(error));
        }
    };

private:
    using std::enable_shared_from_this<SyncSession>::shared_from_this;

    struct State;
    friend struct _impl::sync_session_states::WaitingForAccessToken;
    friend struct _impl::sync_session_states::Active;
    friend struct _impl::sync_session_states::Dying;
    friend struct _impl::sync_session_states::Inactive;

    friend class realm::SyncManager;
    // Called by SyncManager {
    static std::shared_ptr<SyncSession> create(_impl::SyncClient& client, std::string realm_path, SyncConfig config)
    {
        struct MakeSharedEnabler : public SyncSession {
            MakeSharedEnabler(_impl::SyncClient& client, std::string realm_path, SyncConfig config)
            : SyncSession(client, std::move(realm_path), std::move(config))
            {}
        };
        return std::make_shared<MakeSharedEnabler>(client, std::move(realm_path), std::move(config));
    }
    // }

    SyncSession(_impl::SyncClient&, std::string realm_path, SyncConfig);

    void handle_error(SyncError);
    void cancel_pending_waits(std::unique_lock<std::mutex>&);
    enum class ShouldBackup { yes, no };
    void update_error_and_mark_file_for_deletion(SyncError&, ShouldBackup);
    static std::string get_recovery_file_path();
    void handle_progress_update(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

    void set_sync_transact_callback(std::function<SyncSessionTransactCallback>);
    void nonsync_transact_notify(VersionID::version_type);

    void advance_state(std::unique_lock<std::mutex>& lock, const State&);

    void create_sync_session();
    void unregister(std::unique_lock<std::mutex>& lock);
    void did_drop_external_reference();

    std::function<SyncSessionTransactCallback> m_sync_transact_callback;

    mutable std::mutex m_state_mutex;

    const State* m_state = nullptr;
    size_t m_death_count = 0;

    SyncConfig m_config;

    std::string m_realm_path;
    _impl::SyncClient& m_client;

    // For storing wait-for-completion requests if the session isn't yet ready to handle them.
    struct CompletionWaitPackage {
        void(sync::Session::*waiter)(std::function<void(std::error_code)>);
        std::function<void(std::error_code)> callback;
    };
    std::vector<CompletionWaitPackage> m_completion_wait_packages;

    struct ServerOverride {
        std::string address;
        int port;
    };
    util::Optional<ServerOverride> m_server_override;

    // The underlying `Session` object that is owned and managed by this `SyncSession`.
    // The session is first created when the `SyncSession` is moved out of its initial `inactive` state.
    // The session might be destroyed if the `SyncSession` becomes inactive again (for example, if the
    // user owning the session logs out). It might be created anew if the session is revived (if a
    // logged-out user logs back in, the object store sync code will revive their sessions).
    std::unique_ptr<sync::Session> m_session;

    // Whether or not the session object in `m_session` has been `bind()`ed before.
    // This determines how the `SyncSession` behaves when refreshing tokens.
    bool m_session_has_been_bound;

    util::Optional<int_fast64_t> m_deferred_commit_notification;
    bool m_deferred_close = false;

    // The fully-resolved URL of this Realm, including the server and the path.
    util::Optional<std::string> m_server_url;

    std::string m_multiplex_identity;

    _impl::SyncProgressNotifier m_notifier;

    class ExternalReference;
    std::weak_ptr<ExternalReference> m_external_reference;
};

}

#endif // REALM_OS_SYNC_SESSION_HPP
