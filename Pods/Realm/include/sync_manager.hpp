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

#ifndef REALM_OS_SYNC_MANAGER_HPP
#define REALM_OS_SYNC_MANAGER_HPP

#include "shared_realm.hpp"

#include <realm/sync/client.hpp>
#include <realm/util/logger.hpp>

#include <memory>
#include <mutex>
#include <unordered_map>

namespace realm {

struct SyncConfig;
struct SyncSession;

namespace _impl {
struct SyncClient;
}

enum class SyncSessionStopPolicy {
    Immediately,                    // Immediately stop the session as soon as all Realms/Sessions go out of scope.
    LiveIndefinitely,               // Never stop the session.
    AfterChangesUploaded,           // Once all Realms/Sessions go out of scope, wait for uploads to complete and stop.
};

class SyncLoggerFactory {
public:
    virtual std::unique_ptr<util::Logger> make_logger(util::Logger::Level) = 0;
};

class SyncManager {
friend struct SyncSession;
public:
    static SyncManager& shared();

    void set_log_level(util::Logger::Level) noexcept;
    void set_logger_factory(SyncLoggerFactory&) noexcept;
    void set_error_handler(std::function<sync::Client::ErrorHandler>);

    /// Control whether the sync client attempts to reconnect immediately. Only set this to `true` for testing purposes.
    void set_client_should_reconnect_immediately(bool reconnect_immediately);
    /// Control whether the sync client validates SSL certificates. Should *always* be `true` in production use.
    void set_client_should_validate_ssl(bool validate_ssl);

    std::shared_ptr<SyncSession> get_session(const std::string& path, const SyncConfig& config);
    std::shared_ptr<SyncSession> get_existing_active_session(const std::string& path) const;

private:
    void dropped_last_reference_to_session(SyncSession*);

    // Stop tracking the session for the given path if it is inactive.
    // No-op if the session is either still active or in the active sessions list
    // due to someone holding a strong reference to it.
    void unregister_session(const std::string& path);

    SyncManager() = default;
    SyncManager(const SyncManager&) = delete;
    SyncManager& operator=(const SyncManager&) = delete;

    std::shared_ptr<_impl::SyncClient> get_sync_client() const;
    std::shared_ptr<_impl::SyncClient> create_sync_client() const;

    std::shared_ptr<SyncSession> get_existing_active_session_locked(const std::string& path) const;
    std::unique_ptr<SyncSession> get_existing_inactive_session_locked(const std::string& path);

    mutable std::mutex m_mutex;

    // FIXME: Should probably be util::Logger::Level::error
    util::Logger::Level m_log_level = util::Logger::Level::info;
    SyncLoggerFactory* m_logger_factory = nullptr;
    std::function<sync::Client::ErrorHandler> m_error_handler;
    sync::Client::Reconnect m_client_reconnect_mode = sync::Client::Reconnect::normal;
    bool m_client_validate_ssl = true;

    mutable std::shared_ptr<_impl::SyncClient> m_sync_client;

    // Protects m_active_sessions and m_inactive_sessions
    mutable std::mutex m_session_mutex;

    // Active sessions are sessions which the client code holds a strong
    // reference to. When the last strong reference is released, the session is
    // moved to inactive sessions. Inactive sessions are promoted back to active
    // sessions until the session itself calls unregister_session to remove
    // itself from inactive sessions once it's done with whatever async cleanup
    // it needs to do.
    std::unordered_map<std::string, std::weak_ptr<SyncSession>> m_active_sessions;
    std::unordered_map<std::string, std::unique_ptr<SyncSession>> m_inactive_sessions;
};

} // namespace realm

#endif // REALM_OS_SYNC_MANAGER_HPP
