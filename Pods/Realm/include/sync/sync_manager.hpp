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

#include "sync_user.hpp"

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
class SyncFileActionMetadata;

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

    // Immediately run file actions for a single Realm at a given original path.
    // Returns whether or not a file action was successfully executed for the specified Realm.
    // Preconditions: all references to the Realm at the given path must have already been invalidated.
    // The metadata and file management subsystems must also have already been configured.
    bool immediately_run_file_actions(const std::string& original_name);

    void set_log_level(util::Logger::Level) noexcept;
    void set_logger_factory(SyncLoggerFactory&) noexcept;

    /// Control whether the sync client attempts to reconnect immediately. Only set this to `true` for testing purposes.
    void set_client_should_reconnect_immediately(bool reconnect_immediately);
    bool client_should_reconnect_immediately() const noexcept;

    /// Ask all valid sync sessions to perform whatever tasks might be necessary to
    /// re-establish connectivity with the Realm Object Server. It is presumed that
    /// the caller knows that network connectivity has been restored.
    ///
    /// Refer to `SyncSession::handle_reconnect()` to see what sort of work is done
    /// on a per-session basis.
    void reconnect();

    util::Logger::Level log_level() const noexcept;

    std::shared_ptr<SyncSession> get_session(const std::string& path, const SyncConfig& config);
    std::shared_ptr<SyncSession> get_existing_session(const std::string& path) const;
    std::shared_ptr<SyncSession> get_existing_active_session(const std::string& path) const;

    // If the metadata manager is configured, perform an update. Returns `true` iff the code was run.
    bool perform_metadata_update(std::function<void(const SyncMetadataManager&)> update_function) const;

    // Get a sync user for a given identity, or create one if none exists yet, and set its token.
    // If a logged-out user exists, it will marked as logged back in.
    std::shared_ptr<SyncUser> get_user(const SyncUserIdentifier& identifier, std::string refresh_token);

    // Get or create an admin token user based on the given identity.
    // Please note: a future version will remove this method and deprecate the
    // use of identities for admin users completely.
    // Warning: it is an error to create or get an admin token user with a given identity and
    // specifying a URL, and later get that same user by specifying only the identity and no
    // URL, or vice versa.
    std::shared_ptr<SyncUser> get_admin_token_user_from_identity(const std::string& identity,
                                                                 util::Optional<std::string> server_url,
                                                                 const std::string& token);

    // Get or create an admin token user for the given URL.
    // If the user already exists, the token value will be ignored.
    // If an old identity is provided and a directory for the user already exists, the directory
    // will be renamed.
    std::shared_ptr<SyncUser> get_admin_token_user(const std::string& server_url,
                                                   const std::string& token,
                                                   util::Optional<std::string> old_identity=none);

    // Get an existing user for a given identifier, if one exists and is logged in.
    std::shared_ptr<SyncUser> get_existing_logged_in_user(const SyncUserIdentifier&) const;

    // Get all the users that are logged in and not errored out.
    std::vector<std::shared_ptr<SyncUser>> all_logged_in_users() const;
    // Gets the currently logged in user. If there are more than 1 users logged in, an exception is thrown.
    std::shared_ptr<SyncUser> get_current_user() const;

    // Get the default path for a Realm for the given user and absolute unresolved URL.
    std::string path_for_realm(const SyncUser& user, const std::string& raw_realm_url) const;

    // Get the path of the recovery directory for backed-up or recovered Realms.
    std::string recovery_directory_path() const;

    // Reset the singleton state for testing purposes. DO NOT CALL OUTSIDE OF TESTING CODE.
    // Precondition: any synced Realms or `SyncSession`s must be closed or rendered inactive prior to
    // calling this method.
    void reset_for_testing();

private:
    using ReconnectMode = sync::Client::ReconnectMode;
    
    static constexpr const char c_admin_identity[] = "__auth";

    // Stop tracking the session for the given path if it is inactive.
    // No-op if the session is either still active or in the active sessions list
    // due to someone holding a strong reference to it.
    void unregister_session(const std::string& path);

    SyncManager() = default;
    SyncManager(const SyncManager&) = delete;
    SyncManager& operator=(const SyncManager&) = delete;

    _impl::SyncClient& get_sync_client() const;
    std::unique_ptr<_impl::SyncClient> create_sync_client() const;

    std::shared_ptr<SyncSession> get_existing_session_locked(const std::string& path) const;

    mutable std::mutex m_mutex;

    // FIXME: Should probably be util::Logger::Level::error
    util::Logger::Level m_log_level = util::Logger::Level::info;
    SyncLoggerFactory* m_logger_factory = nullptr;
    ReconnectMode m_client_reconnect_mode = ReconnectMode::normal;

    bool run_file_action(const SyncFileActionMetadata&);

    // Protects m_users
    mutable std::mutex m_user_mutex;

    // A map of user ID/auth server URL pairs to (shared pointers to) SyncUser objects.
    std::unordered_map<SyncUserIdentifier, std::shared_ptr<SyncUser>> m_users;
    // A map of local identifiers to admin token users.
    std::unordered_map<std::string, std::shared_ptr<SyncUser>> m_admin_token_users;

    mutable std::unique_ptr<_impl::SyncClient> m_sync_client;

    // Protects m_file_manager and m_metadata_manager
    mutable std::mutex m_file_system_mutex;
    std::unique_ptr<SyncFileManager> m_file_manager;
    std::unique_ptr<SyncMetadataManager> m_metadata_manager;

    // Protects m_sessions
    mutable std::mutex m_session_mutex;

    // Map of sessions by path name.
    // Sessions remove themselves from this map by calling `unregister_session` once they're
    // inactive and have performed any necessary cleanup work.
    std::unordered_map<std::string, std::shared_ptr<SyncSession>> m_sessions;
};

} // namespace realm

#endif // REALM_OS_SYNC_MANAGER_HPP
