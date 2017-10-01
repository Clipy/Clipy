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

#include "sync/sync_session.hpp"

#include "sync/impl/sync_client.hpp"
#include "sync/impl/sync_file.hpp"
#include "sync/impl/sync_metadata.hpp"
#include "sync/sync_manager.hpp"
#include "sync/sync_user.hpp"

#include <realm/sync/client.hpp>
#include <realm/sync/protocol.hpp>


using namespace realm;
using namespace realm::_impl;
using namespace realm::_impl::sync_session_states;

using SessionWaiterPointer = void(sync::Session::*)(std::function<void(std::error_code)>);

constexpr const char SyncError::c_original_file_path_key[];
constexpr const char SyncError::c_recovery_file_path_key[];

/// A state which a `SyncSession` can currently be within. State classes handle various actions
/// and state transitions.
///
/// STATES:
///
/// WAITING_FOR_ACCESS_TOKEN: upon entering this state, the binding is informed
/// that the session wants an access token. The session is now waiting for the
/// binding to provide the token.
/// From: INACTIVE
/// To:
///    * ACTIVE: when the binding successfully refreshes the token
///    * INACTIVE: if asked to log out, or if asked to close and the stop policy
///                is Immediate.
///    * ERROR: if a fatal error occurs
///
/// ACTIVE: the session is connected to the Realm Object Server and is actively
/// transferring data.
/// From: WAITING_FOR_ACCESS_TOKEN, DYING
/// To:
///    * WAITING_FOR_ACCESS_TOKEN: if the session is informed (through the error
///                                handler) that the token expired
///    * INACTIVE: if asked to log out, or if asked to close and the stop policy
///                is Immediate.
///    * DYING: if asked to close and the stop policy is AfterChangesUploaded
///    * ERROR: if a fatal error occurs
///
/// DYING: the session is performing clean-up work in preparation to be destroyed.
/// From: ACTIVE
/// To:
///    * INACTIVE: when the clean-up work completes, if the session wasn't
///                revived, or if explicitly asked to log out before the
///                clean-up work begins
///    * ACTIVE: if the session is revived
///    * ERROR: if a fatal error occurs
///
/// INACTIVE: the user owning this session has logged out, the `sync::Session`
/// owned by this session is destroyed, and the session is quiescent.
/// Note that a session briefly enters this state before being destroyed, but
/// it can also enter this state and stay there if the user has been logged out.
/// From: initial, WAITING_FOR_ACCESS_TOKEN, ACTIVE, DYING
/// To:
///    * WAITING_FOR_ACCESS_TOKEN: if the session is revived
///    * ERROR: if a fatal error occurs
///
/// ERROR: a non-recoverable error has occurred, and this session is semantically
/// invalid. The binding must create a new session with a different configuration.
/// From: WAITING_FOR_ACCESS_TOKEN, ACTIVE, DYING, INACTIVE
/// To:
///    * (none, this is a terminal state)
///
struct SyncSession::State {
    virtual ~State() { }

    // Move the given session into this state. All state transitions MUST be carried out through this method.
    virtual void enter_state(std::unique_lock<std::mutex>&, SyncSession&) const { }

    virtual void refresh_access_token(std::unique_lock<std::mutex>&,
                                      SyncSession&, std::string,
                                      const util::Optional<std::string>&) const { }

    virtual void bind_with_admin_token(std::unique_lock<std::mutex>&,
                                       SyncSession&, const std::string&, const std::string&) const { }

    // Returns true iff the lock is still locked when the method returns.
    virtual bool access_token_expired(std::unique_lock<std::mutex>&, SyncSession&) const { return true; }

    virtual void nonsync_transact_notify(std::unique_lock<std::mutex>&, SyncSession&, sync::Session::version_type) const { }

    // Perform any work needed to reactivate a session that is not already active.
    // Returns true iff the session should ask the binding to get a token for `bind()`.
    virtual bool revive_if_needed(std::unique_lock<std::mutex>&, SyncSession&) const { return false; }

    // Perform any work needed to respond to the application regaining network connectivity.
    virtual void handle_reconnect(std::unique_lock<std::mutex>&, SyncSession&) const { };

    // The user that owns this session has been logged out, and the session should take appropriate action.
    virtual void log_out(std::unique_lock<std::mutex>&, SyncSession&) const { }

    // The session should be closed and moved to `inactive`, in accordance with its stop policy and other state.
    virtual void close(std::unique_lock<std::mutex>&, SyncSession&) const { }

    // Returns true iff the error has been fully handled and the error handler should immediately return.
    virtual bool handle_error(std::unique_lock<std::mutex>&, SyncSession&, const SyncError&) const { return false; }

    // Register a handler to wait for sync session uploads, downloads, or synchronization.
    // PRECONDITION: the session state lock must be held at the time this method is called, until after it returns.
    // Returns true iff the handler was registered, either immediately or placed in a queue for later registration.
    virtual bool wait_for_completion(SyncSession&,
                                     std::function<void(std::error_code)>,
                                     SessionWaiterPointer) const {
        return false;
    }

    static const State& waiting_for_access_token;
    static const State& active;
    static const State& dying;
    static const State& inactive;
    static const State& error;
};

struct sync_session_states::WaitingForAccessToken : public SyncSession::State {
    void enter_state(std::unique_lock<std::mutex>&, SyncSession& session) const override
    {
        session.m_deferred_close = false;
    }

    void refresh_access_token(std::unique_lock<std::mutex>& lock, SyncSession& session,
                              std::string access_token,
                              const util::Optional<std::string>& server_url) const override
    {
        REALM_ASSERT(session.m_session);
        // Since the sync session was previously unbound, it's safe to do this from the
        // calling thread.
        if (!session.m_server_url) {
            session.m_server_url = server_url;
        }
        if (session.m_session_has_been_bound) {
            session.m_session->refresh(std::move(access_token));
            session.m_session->cancel_reconnect_delay();
        } else {
            session.m_session->bind(*session.m_server_url, std::move(access_token));
            session.m_session_has_been_bound = true;
        }

        // Register all the pending wait-for-completion blocks.
        for (auto& package : session.m_completion_wait_packages) {
            (*session.m_session.*package.waiter)(std::move(package.callback));
        }
        session.m_completion_wait_packages.clear();

        // Handle any deferred commit notification.
        if (session.m_deferred_commit_notification) {
            session.m_session->nonsync_transact_notify(*session.m_deferred_commit_notification);
            session.m_deferred_commit_notification = util::none;
        }

        session.advance_state(lock, active);
        if (session.m_deferred_close) {
            session.m_state->close(lock, session);
        }
    }

    void log_out(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        session.advance_state(lock, inactive);
    }

    bool revive_if_needed(std::unique_lock<std::mutex>&, SyncSession& session) const override
    {
        session.m_deferred_close = false;
        return false;
    }

    void handle_reconnect(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        // Ask the binding to retry getting the token for this session.
        std::shared_ptr<SyncSession> session_ptr = session.shared_from_this();
        lock.unlock();
        session.m_config.bind_session_handler(session_ptr->m_realm_path, session_ptr->m_config, session_ptr);
    }

    void nonsync_transact_notify(std::unique_lock<std::mutex>&,
                                 SyncSession& session,
                                 sync::Session::version_type version) const override
    {
        // Notify at first available opportunity.
        session.m_deferred_commit_notification = version;
    }

    void close(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        switch (session.m_config.stop_policy) {
            case SyncSessionStopPolicy::Immediately:
                // Immediately kill the session.
                session.advance_state(lock, inactive);
                break;
            case SyncSessionStopPolicy::LiveIndefinitely:
            case SyncSessionStopPolicy::AfterChangesUploaded:
                // Defer handling closing the session until after the login response succeeds.
                session.m_deferred_close = true;
                break;
        }
    }

    bool wait_for_completion(SyncSession& session,
                             std::function<void(std::error_code)> callback,
                             SessionWaiterPointer waiter) const override
    {
        session.m_completion_wait_packages.push_back({ waiter, std::move(callback) });
        return true;
    }
};

struct sync_session_states::Active : public SyncSession::State {
    void refresh_access_token(std::unique_lock<std::mutex>&, SyncSession& session,
                              std::string access_token,
                              const util::Optional<std::string>&) const override
    {
        session.m_session->refresh(std::move(access_token));
        // Cancel the session's reconnection delay. This is important if the
        // token is being refreshed as a response to a 202 (token expired)
        // error, or similar non-fatal sync errors.
        session.m_session->cancel_reconnect_delay();
    }

    bool access_token_expired(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        session.advance_state(lock, waiting_for_access_token);
        std::shared_ptr<SyncSession> session_ptr = session.shared_from_this();
        lock.unlock();
        session.m_config.bind_session_handler(session_ptr->m_realm_path, session_ptr->m_config, session_ptr);
        return false;
    }

    void log_out(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        session.advance_state(lock, inactive);
    }

    void nonsync_transact_notify(std::unique_lock<std::mutex>&, SyncSession& session,
                                 sync::Session::version_type version) const override
    {
        // Fully ready sync session, notify immediately.
        session.m_session->nonsync_transact_notify(version);
    }

    void close(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        switch (session.m_config.stop_policy) {
            case SyncSessionStopPolicy::Immediately:
                session.advance_state(lock, inactive);
                break;
            case SyncSessionStopPolicy::LiveIndefinitely:
                // Don't do anything; session lives forever.
                break;
            case SyncSessionStopPolicy::AfterChangesUploaded:
                // Wait for all pending changes to upload.
                session.advance_state(lock, dying);
                break;
        }
    }

    bool wait_for_completion(SyncSession& session,
                             std::function<void(std::error_code)> callback,
                             SessionWaiterPointer waiter) const override
    {
        REALM_ASSERT(session.m_session);
        (*session.m_session.*waiter)(std::move(callback));
        return true;
    }
};

struct sync_session_states::Dying : public SyncSession::State {
    void enter_state(std::unique_lock<std::mutex>&, SyncSession& session) const override
    {
        size_t current_death_count = ++session.m_death_count;
        std::weak_ptr<SyncSession> weak_session = session.shared_from_this();
        session.m_session->async_wait_for_upload_completion([weak_session, current_death_count](std::error_code) {
            if (auto session = weak_session.lock()) {
                std::unique_lock<std::mutex> lock(session->m_state_mutex);
                if (session->m_state == &State::dying && session->m_death_count == current_death_count) {
                    session->advance_state(lock, inactive);
                }
            }
        });
    }

    bool handle_error(std::unique_lock<std::mutex>& lock, SyncSession& session, const SyncError& error) const override
    {
        if (error.is_fatal) {
            session.advance_state(lock, inactive);
        }
        // If the error isn't fatal, don't change state, but don't
        // allow it to be reported either.
        // FIXME: What if the token expires while a session is dying?
        // Should we allow the token to be refreshed so that changes
        // can finish being uploaded?
        return true;
    }

    bool revive_if_needed(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        // Revive.
        session.advance_state(lock, active);
        return false;
    }

    void log_out(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        session.advance_state(lock, inactive);
    }

    bool wait_for_completion(SyncSession& session,
                             std::function<void(std::error_code)> callback,
                             SessionWaiterPointer waiter) const override
    {
        REALM_ASSERT(session.m_session);
        (*session.m_session.*waiter)(std::move(callback));
        return true;
    }
};

struct sync_session_states::Inactive : public SyncSession::State {
    void enter_state(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        // Inform any queued-up completion handlers that they were cancelled.
        for (auto& package : session.m_completion_wait_packages) {
            package.callback(util::error::operation_aborted);
        }
        session.m_completion_wait_packages.clear();
        session.m_session = nullptr;
        session.unregister(lock);
    }

    void bind_with_admin_token(std::unique_lock<std::mutex>& lock, SyncSession& session,
                               const std::string& admin_token,
                               const std::string& server_url) const override
    {
        session.create_sync_session();
        session.advance_state(lock, waiting_for_access_token);
        session.m_state->refresh_access_token(lock, session, admin_token, server_url);
    }

    bool revive_if_needed(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        // Revive.
        session.create_sync_session();
        session.advance_state(lock, waiting_for_access_token);
        return true;
    }

    bool wait_for_completion(SyncSession& session,
                             std::function<void(std::error_code)> callback,
                             SessionWaiterPointer waiter) const override
    {
        session.m_completion_wait_packages.push_back({ waiter, std::move(callback) });
        return true;
    }
};

struct sync_session_states::Error : public SyncSession::State {
    void enter_state(std::unique_lock<std::mutex>&, SyncSession& session) const override
    {
        // Inform any queued-up completion handlers that they were cancelled.
        for (auto& package : session.m_completion_wait_packages) {
            package.callback(util::error::operation_aborted);
        }
        session.m_completion_wait_packages.clear();
        session.m_session = nullptr;
        session.m_config = { nullptr, "", SyncSessionStopPolicy::Immediately, nullptr };
    }

    // Everything else is a no-op when in the error state.
};


const SyncSession::State& SyncSession::State::waiting_for_access_token = WaitingForAccessToken();
const SyncSession::State& SyncSession::State::active = Active();
const SyncSession::State& SyncSession::State::dying = Dying();
const SyncSession::State& SyncSession::State::inactive = Inactive();
const SyncSession::State& SyncSession::State::error = Error();

SyncSession::SyncSession(SyncClient& client, std::string realm_path, SyncConfig config)
: m_state(&State::inactive)
, m_config(std::move(config))
, m_realm_path(std::move(realm_path))
, m_client(client) { }

std::string SyncSession::get_recovery_file_path()
{
    return util::reserve_unique_file_name(SyncManager::shared().recovery_directory_path(),
                                          util::create_timestamped_template("recovered_realm"));
}

void SyncSession::update_error_and_mark_file_for_deletion(SyncError& error, ShouldBackup should_backup)
{
    // Add a SyncFileActionMetadata marking the Realm as needing to be deleted.
    std::string recovery_path;
    auto original_path = path();
    error.user_info[SyncError::c_original_file_path_key] = original_path;
    if (should_backup == ShouldBackup::yes) {
        recovery_path = get_recovery_file_path();
        error.user_info[SyncError::c_recovery_file_path_key] = recovery_path;
    }
    using Action = SyncFileActionMetadata::Action;
    auto action = should_backup == ShouldBackup::yes ? Action::BackUpThenDeleteRealm : Action::DeleteRealm;
    SyncManager::shared().perform_metadata_update([this,
                                                   action,
                                                   original_path=std::move(original_path),
                                                   recovery_path=std::move(recovery_path)](const auto& manager) {
        manager.make_file_action_metadata(original_path,
                                          m_config.realm_url,
                                          m_config.user->identity(),
                                          action,
                                          util::Optional<std::string>(std::move(recovery_path)));
    });
}

// This method should only be called from within the error handler callback registered upon the underlying `m_session`.
void SyncSession::handle_error(SyncError error)
{
    enum class NextStateAfterError { none, inactive, error };
    auto next_state = error.is_fatal ? NextStateAfterError::error : NextStateAfterError::none;
    auto error_code = error.error_code;

    {
        // See if the current state wishes to take responsibility for handling the error.
        std::unique_lock<std::mutex> lock(m_state_mutex);
        if (m_state->handle_error(lock, *this, error)) {
            return;
        }
    }

    if (error_code.category() == realm::sync::protocol_error_category()) {
        using ProtocolError = realm::sync::ProtocolError;
        switch (static_cast<ProtocolError>(error_code.value())) {
            // Connection level errors
            case ProtocolError::connection_closed:
            case ProtocolError::other_error:
#if REALM_SYNC_VER_MAJOR == 1
            case ProtocolError::pong_timeout:
#endif
                // Not real errors, don't need to be reported to the binding.
                return;
            case ProtocolError::unknown_message:
            case ProtocolError::bad_syntax:
            case ProtocolError::limits_exceeded:
            case ProtocolError::wrong_protocol_version:
            case ProtocolError::bad_session_ident:
            case ProtocolError::reuse_of_session_ident:
            case ProtocolError::bound_in_other_session:
            case ProtocolError::bad_message_order:
#if REALM_SYNC_VER_MAJOR == 1
            case ProtocolError::malformed_http_request:
#endif
                break;
            // Session errors
            case ProtocolError::session_closed:
            case ProtocolError::other_session_error:
            case ProtocolError::disabled_session:
                // The binding doesn't need to be aware of these because they are strictly informational, and do not
                // represent actual errors.
                return;
            case ProtocolError::token_expired: {
                std::unique_lock<std::mutex> lock(m_state_mutex);
                // This isn't an error from the binding's point of view. If we're connected we'll
                // simply ask the binding to log in again.
                m_state->access_token_expired(lock, *this);
                return;
            }
            case ProtocolError::bad_authentication: {
                std::shared_ptr<SyncUser> user_to_invalidate;
                next_state = NextStateAfterError::none;
                {
                    std::unique_lock<std::mutex> lock(m_state_mutex);
                    user_to_invalidate = user();
                    advance_state(lock, State::error);
                }
                if (user_to_invalidate)
                    user_to_invalidate->invalidate();
                break;
            }
            case ProtocolError::illegal_realm_path:
            case ProtocolError::no_such_realm:
                break;
            case ProtocolError::permission_denied: {
                next_state = NextStateAfterError::inactive;
                update_error_and_mark_file_for_deletion(error, ShouldBackup::no);
                break;
            }
            case ProtocolError::bad_client_version:
                break;
            case ProtocolError::bad_server_file_ident:
            case ProtocolError::bad_client_file_ident:
            case ProtocolError::bad_server_version:
            case ProtocolError::diverging_histories:
                update_error_and_mark_file_for_deletion(error, ShouldBackup::yes);
                break;
            case ProtocolError::bad_changeset:
                break;
        }
    } else if (error_code.category() == realm::sync::client_error_category()) {
        using ClientError = realm::sync::Client::Error;
        switch (static_cast<ClientError>(error_code.value())) {
            case ClientError::connection_closed:
#if REALM_SYNC_VER_MAJOR > 1
            case ClientError::pong_timeout:
#endif
                // Not real errors, don't need to be reported to the binding.
                return;
            case ClientError::unknown_message:
            case ClientError::bad_syntax:
            case ClientError::limits_exceeded:
            case ClientError::bad_session_ident:
            case ClientError::bad_message_order:
            case ClientError::bad_file_ident_pair:
            case ClientError::bad_progress:
            case ClientError::bad_changeset_header_syntax:
            case ClientError::bad_changeset_size:
            case ClientError::bad_origin_file_ident:
            case ClientError::bad_server_version:
            case ClientError::bad_changeset:
            case ClientError::bad_request_ident:
            case ClientError::bad_error_code:
            case ClientError::bad_compression:
            case ClientError::bad_client_version:
            case ClientError::ssl_server_cert_rejected:
                // Don't do anything special for these errors.
                // Future functionality may require special-case handling for existing
                // errors, or newly introduced error codes.
                break;
        }
    } else {
        // Unrecognized error code; just ignore it.
        return;
    }
    switch (next_state) {
        case NextStateAfterError::none:
            break;
        case NextStateAfterError::inactive: {
            std::unique_lock<std::mutex> lock(m_state_mutex);
            advance_state(lock, State::inactive);
            break;
        }
        case NextStateAfterError::error: {
            std::unique_lock<std::mutex> lock(m_state_mutex);
            advance_state(lock, State::error);
            break;
        }
    }
    if (m_error_handler) {
        m_error_handler(shared_from_this(), std::move(error));
    }
}

void SyncSession::handle_progress_update(uint64_t downloaded, uint64_t downloadable,
                                         uint64_t uploaded, uint64_t uploadable, bool is_fresh)
{
    std::vector<std::function<void()>> invocations;
    {
        std::lock_guard<std::mutex> lock(m_progress_notifier_mutex);
        m_current_progress = Progress{uploadable, downloadable, uploaded, downloaded};
        m_latest_progress_data_is_fresh = is_fresh;

        for (auto it = m_notifiers.begin(); it != m_notifiers.end();) {
            auto& package = it->second;
            package.update(*m_current_progress, is_fresh);

            bool should_delete = false;
            invocations.emplace_back(package.create_invocation(*m_current_progress, should_delete));

            it = (should_delete ? m_notifiers.erase(it) : std::next(it));
        }
    }
    // Run the notifiers only after we've released the lock.
    for (auto& invocation : invocations) {
        invocation();
    }
}

void SyncSession::NotifierPackage::update(const Progress& current_progress, bool data_is_fresh)
{
    if (is_streaming || captured_transferrable || !data_is_fresh)
        return;

    captured_transferrable = direction == NotifierType::download ? current_progress.downloadable
                                                                 : current_progress.uploadable;
}

// PRECONDITION: `update()` must first be called on the same package.
std::function<void()> SyncSession::NotifierPackage::create_invocation(const Progress& current_progress,
                                                                      bool& is_expired) const
{
    // It's possible for a non-streaming notifier to not yet have fresh transferrable bytes data.
    // In that case, we don't call it at all.
    // NOTE: `update()` is always called before `create_invocation()`, and will
    // set `captured_transferrable` on the notifier package if fresh data has
    // been received and the package is for a non-streaming notifier.
    if (!is_streaming && !captured_transferrable)
        return [](){ };

    bool is_download = direction == NotifierType::download;
    uint64_t transferred = is_download ? current_progress.downloaded : current_progress.uploaded;
    uint64_t transferrable;
    if (is_streaming) {
        transferrable = is_download ? current_progress.downloadable : current_progress.uploadable;
    } else {
        transferrable = *captured_transferrable;
    }
    // A notifier is expired if at least as many bytes have been transferred
    // as were originally considered transferrable.
    is_expired = !is_streaming && transferred >= *captured_transferrable;
    return [=, package=*this](){
        package.notifier(transferred, transferrable);
    };
}

void SyncSession::create_sync_session()
{
    REALM_ASSERT(!m_session);
    sync::Session::Config session_config;
    session_config.changeset_cooker = m_config.transformer;
    session_config.encryption_key = m_config.realm_encryption_key;
    session_config.verify_servers_ssl_certificate = m_config.client_validate_ssl;
    session_config.ssl_trust_certificate_path = m_config.ssl_trust_certificate_path;
    m_session = std::make_unique<sync::Session>(m_client.client, m_realm_path, session_config);

    // The next time we get a token, call `bind()` instead of `refresh()`.
    m_session_has_been_bound = false;

    // Configure the error handler.
    std::weak_ptr<SyncSession> weak_self = shared_from_this();
    auto wrapped_handler = [this, weak_self](std::error_code error_code, bool is_fatal, std::string message) {
        auto self = weak_self.lock();
        if (!self) {
            // An error was delivered after the session it relates to was destroyed. There's nothing useful
            // we can do with it.
            return;
        }
        handle_error(SyncError{error_code, std::move(message), is_fatal});
    };
    m_session->set_error_handler(std::move(wrapped_handler));

    // Configure the sync transaction callback.
    auto wrapped_callback = [this, weak_self](VersionID old_version, VersionID new_version) {
        if (auto self = weak_self.lock()) {
            if (m_sync_transact_callback) {
                m_sync_transact_callback(old_version, new_version);
            }
        }
    };
    m_session->set_sync_transact_callback(std::move(wrapped_callback));

    // Set up the wrapped progress handler callback
    auto wrapped_progress_handler = [this, weak_self](uint_fast64_t downloaded, uint_fast64_t downloadable,
                                                      uint_fast64_t uploaded, uint_fast64_t uploadable,
                                                      bool is_fresh, uint_fast64_t /*snapshot_version*/) {
        if (auto self = weak_self.lock()) {
            handle_progress_update(downloaded, downloadable, uploaded, uploadable, is_fresh);
        }
    };
    m_session->set_progress_handler(std::move(wrapped_progress_handler));
}

void SyncSession::set_sync_transact_callback(std::function<sync::Session::SyncTransactCallback> callback)
{
    m_sync_transact_callback = std::move(callback);
}

void SyncSession::set_error_handler(std::function<SyncSessionErrorHandler> handler)
{
    m_error_handler = std::move(handler);
}

void SyncSession::advance_state(std::unique_lock<std::mutex>& lock, const State& state)
{
    REALM_ASSERT(lock.owns_lock());
    REALM_ASSERT(&state != m_state);
    m_state = &state;
    m_state->enter_state(lock, *this);
}

void SyncSession::nonsync_transact_notify(sync::Session::version_type version)
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    m_state->nonsync_transact_notify(lock, *this, version);
}

void SyncSession::revive_if_needed()
{
    util::Optional<std::function<SyncBindSessionHandler>&> handler;
    {
        std::unique_lock<std::mutex> lock(m_state_mutex);
        if (m_state->revive_if_needed(lock, *this))
            handler = m_config.bind_session_handler;
    }
    if (handler)
        handler.value()(m_realm_path, m_config, shared_from_this());
}

void SyncSession::handle_reconnect()
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    m_state->handle_reconnect(lock, *this);
}

void SyncSession::log_out()
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    m_state->log_out(lock, *this);
}

void SyncSession::close()
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    m_state->close(lock, *this);
}

void SyncSession::unregister(std::unique_lock<std::mutex>& lock)
{
    REALM_ASSERT(lock.owns_lock());
    REALM_ASSERT(m_state == &State::inactive); // Must stop an active session before unregistering.

    lock.unlock();
    SyncManager::shared().unregister_session(m_realm_path);
}

bool SyncSession::wait_for_upload_completion(std::function<void(std::error_code)> callback)
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    return m_state->wait_for_completion(*this, std::move(callback), &sync::Session::async_wait_for_upload_completion);
}

bool SyncSession::wait_for_download_completion(std::function<void(std::error_code)> callback)
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    return m_state->wait_for_completion(*this, std::move(callback), &sync::Session::async_wait_for_download_completion);
}

uint64_t SyncSession::register_progress_notifier(std::function<SyncProgressNotifierCallback> notifier,
                                                 NotifierType direction, bool is_streaming)
{
    std::function<void()> invocation;
    uint64_t token_value = 0;
    {
        std::lock_guard<std::mutex> lock(m_progress_notifier_mutex);
        token_value = m_progress_notifier_token++;
        NotifierPackage package{std::move(notifier), is_streaming, direction};
        if (!m_current_progress) {
            // Simply register the package, since we have no data yet.
            m_notifiers.emplace(token_value, std::move(package));
            return token_value;
        }
        package.update(*m_current_progress, m_latest_progress_data_is_fresh);
        bool skip_registration = false;
        invocation = package.create_invocation(*m_current_progress, skip_registration);
        if (skip_registration) {
            token_value = 0;
        } else {
            m_notifiers.emplace(token_value, std::move(package));
        }
    }
    invocation();
    return token_value;
}

void SyncSession::unregister_progress_notifier(uint64_t token)
{
    std::lock_guard<std::mutex> lock(m_progress_notifier_mutex);
    m_notifiers.erase(token);
}

void SyncSession::refresh_access_token(std::string access_token, util::Optional<std::string> server_url)
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    if (!m_server_url && !server_url) {
        // The first time this method is called, the server URL must be provided.
        return;
    }
    m_state->refresh_access_token(lock, *this, std::move(access_token), server_url);
}

void SyncSession::bind_with_admin_token(std::string admin_token, std::string server_url)
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    m_state->bind_with_admin_token(lock, *this, admin_token, server_url);
}

SyncSession::PublicState SyncSession::state() const
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    if (m_state == &State::waiting_for_access_token) {
        return PublicState::WaitingForAccessToken;
    } else if (m_state == &State::active) {
        return PublicState::Active;
    } else if (m_state == &State::dying) {
        return PublicState::Dying;
    } else if (m_state == &State::inactive) {
        return PublicState::Inactive;
    } else if (m_state == &State::error) {
        return PublicState::Error;
    }
    REALM_UNREACHABLE();
}

// Represents a reference to the SyncSession from outside of the sync subsystem.
// We attempt to keep the SyncSession in an active state as long as it has an external reference.
class SyncSession::ExternalReference {
public:
    ExternalReference(std::shared_ptr<SyncSession> session) : m_session(std::move(session))
    {}

    ~ExternalReference()
    {
        m_session->did_drop_external_reference();
    }

private:
    std::shared_ptr<SyncSession> m_session;
};

std::shared_ptr<SyncSession> SyncSession::external_reference()
{
    std::unique_lock<std::mutex> lock(m_state_mutex);

    if (auto external_reference = m_external_reference.lock())
        return std::shared_ptr<SyncSession>(external_reference, this);

    auto external_reference = std::make_shared<ExternalReference>(shared_from_this());
    m_external_reference = external_reference;
    return std::shared_ptr<SyncSession>(external_reference, this);
}

std::shared_ptr<SyncSession> SyncSession::existing_external_reference()
{
    std::unique_lock<std::mutex> lock(m_state_mutex);

    if (auto external_reference = m_external_reference.lock())
        return std::shared_ptr<SyncSession>(external_reference, this);

    return nullptr;
}

void SyncSession::did_drop_external_reference()
{
    std::unique_lock<std::mutex> lock(m_state_mutex);

    // If the session is being resurrected we should not close the session.
    if (!m_external_reference.expired())
        return;

    m_state->close(lock, *this);
}
