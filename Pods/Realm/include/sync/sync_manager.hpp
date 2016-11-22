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
#include <realm/util/optional.hpp>

#include <memory>
#include <mutex>
#include <unordered_map>

namespace realm {

struct SyncConfig;
class SyncSession;
class SyncUser;
class SyncFileManager;
class SyncMetadataManager;

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
friend class SyncSession;
public:
    enum class MetadataMode {
        NoEncryption,                   // Enable metadata, but disable encryption.
        Encryption,                     // Enable metadata, and use encryption (automatic if possible).
        NoMetadata,                     // Disable metadata.
    };

    static SyncManager& shared();

    // Configure the metadata and file management subsystems. This MUST be called upon startup.
    void configure_file_system(const std::string& base_file_path,
                               MetadataMode metadata_mode=MetadataMode::Encryption,
                               util::Optional<std::vector<char>> custom_encryption_key=none,
                               bool reset_metadata_on_error=false);

    void set_log_level(util::Logger::Level) noexcept;
    void set_logger_factory(SyncLoggerFactory&) noexcept;
    void set_error_handler(std::function<sync::Client::ErrorHandler>);

    /// Control whether the sync client attempts to reconnect immediately. Only set this to `true` for testing purposes.
    void set_client_should_reconnect_immediately(bool reconnect_immediately);
    bool client_should_reconnect_immediately() const noexcept;

    /// Control whether the sync client validates SSL certificates. Should *always* be `true` in production use.
    void set_client_should_validate_ssl(bool validate_ssl);
    bool client_should_validate_ssl() const noexcept;

    util::Logger::Level log_level() const noexcept;

    std::shared_ptr<SyncSession> get_session(const std::string& path, const SyncConfig& config);
    std::shared_ptr<SyncSession> get_existing_active_session(const std::string& path) const;

    // If the metadata manager is configured, perform an update. Returns `true` iff the code was run.
    bool perform_metadata_update(std::function<void(const SyncMetadataManager&)> update_function) const;

    // Get a sync user for a given identity, or create one if none exists yet, and set its token.
    // If a logged-out user exists, it will marked as logged back in.
    std::shared_ptr<SyncUser> get_user(const std::string& identity,
                                       std::string refresh_token,
                                       util::Optional<std::string> auth_server_url=none,
                                       bool is_admin=false);
    // Get an existing user for a given identity, if one exists and is logged in.
    std::shared_ptr<SyncUser> get_existing_logged_in_user(const std::string& identity) const;
    // Get all the users.
    std::vector<std::shared_ptr<SyncUser>> all_users() const;

    // Get the default path for a Realm for the given user and absolute unresolved URL.
    std::string path_for_realm(const std::string& user_identity, const std::string& raw_realm_url) const;

    // Reset part of the singleton state for testing purposes. DO NOT CALL OUTSIDE OF TESTING CODE.
    void reset_for_testing();

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

    // Protects m_users
    mutable std::mutex m_user_mutex;

    // A map of user identities to (shared pointers to) SyncUser objects.
    std::unordered_map<std::string, std::shared_ptr<SyncUser>> m_users;

    mutable std::shared_ptr<_impl::SyncClient> m_sync_client;

    // Protects m_active_sessions and m_inactive_sessions
    mutable std::mutex m_session_mutex;

    // Protects m_file_manager and m_metadata_manager
    mutable std::mutex m_file_system_mutex;
    std::unique_ptr<SyncFileManager> m_file_manager;
    std::unique_ptr<SyncMetadataManager> m_metadata_manager;

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
