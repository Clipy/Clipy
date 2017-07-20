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

#include "sync/sync_manager.hpp"

#include "sync/impl/sync_client.hpp"
#include "sync/impl/sync_file.hpp"
#include "sync/impl/sync_metadata.hpp"
#include "sync/sync_session.hpp"
#include "sync/sync_user.hpp"

using namespace realm;
using namespace realm::_impl;

SyncManager& SyncManager::shared()
{
    // The singleton is heap-allocated in order to fix an issue when running unit tests where tests would crash after
    // they were done running because the manager was destroyed too early.
    static SyncManager& manager = *new SyncManager;
    return manager;
}

void SyncManager::configure_file_system(const std::string& base_file_path,
                                        MetadataMode metadata_mode,
                                        util::Optional<std::vector<char>> custom_encryption_key,
                                        bool reset_metadata_on_error)
{
    struct UserCreationData {
        std::string identity;
        std::string user_token;
        util::Optional<std::string> server_url;
        bool is_admin;
    };

    std::vector<UserCreationData> users_to_add;
    {
        std::lock_guard<std::mutex> lock(m_file_system_mutex);

        // Set up the file manager.
        if (m_file_manager) {
            REALM_ASSERT(m_file_manager->base_path() == base_file_path);
        } else {
            m_file_manager = std::make_unique<SyncFileManager>(base_file_path);
        }

        // Set up the metadata manager, and perform initial loading/purging work.
        if (m_metadata_manager) {
            return;
        }
        switch (metadata_mode) {
            case MetadataMode::NoEncryption:
                m_metadata_manager = std::make_unique<SyncMetadataManager>(m_file_manager->metadata_path(),
                                                                           false);
                break;
            case MetadataMode::Encryption:
                try {
                    m_metadata_manager = std::make_unique<SyncMetadataManager>(m_file_manager->metadata_path(),
                                                                               true,
                                                                               std::move(custom_encryption_key));
                } catch (RealmFileException const& ex) {
                    if (reset_metadata_on_error && m_file_manager->remove_metadata_realm()) {
                        m_metadata_manager = std::make_unique<SyncMetadataManager>(m_file_manager->metadata_path(),
                                                                                   true,
                                                                                   std::move(custom_encryption_key));
                    } else {
                        throw;
                    }
                }
                break;
            case MetadataMode::NoMetadata:
                return;
        }

        REALM_ASSERT(m_metadata_manager);
        // Perform any necessary file actions.
        std::vector<SyncFileActionMetadata> completed_actions;
        SyncFileActionMetadataResults file_actions = m_metadata_manager->all_pending_actions();
        for (size_t i = 0; i < file_actions.size(); i++) {
            auto file_action = file_actions.get(i);
            if (run_file_action(file_action)) {
                completed_actions.emplace_back(std::move(file_action));
            }
        }
        for (auto& action : completed_actions) {
            action.remove();
        }
        // Load persisted users into the users map.
        SyncUserMetadataResults users = m_metadata_manager->all_unmarked_users();
        for (size_t i = 0; i < users.size(); i++) {
            // Note that 'admin' style users are not persisted.
            auto user_data = users.get(i);
            auto user_token = user_data.user_token();
            auto identity = user_data.identity();
            auto server_url = user_data.server_url();
            bool is_admin = user_data.is_admin();
            if (user_token) {
                UserCreationData data = {
                    std::move(identity),
                    std::move(*user_token),
                    std::move(server_url),
                    is_admin,
                };
                users_to_add.emplace_back(std::move(data));
            }
        }
        // Delete any users marked for death.
        std::vector<SyncUserMetadata> dead_users;
        SyncUserMetadataResults users_to_remove = m_metadata_manager->all_users_marked_for_removal();
        dead_users.reserve(users_to_remove.size());
        for (size_t i = 0; i < users_to_remove.size(); i++) {
            auto user = users_to_remove.get(i);
            // FIXME: delete user data in a different way? (This deletes a logged-out user's data as soon as the app
            // launches again, which might not be how some apps want to treat their data.)
            try {
                m_file_manager->remove_user_directory(user.identity());
                dead_users.emplace_back(std::move(user));
            } catch (util::File::AccessError const&) {
                continue;
            }
        }
        for (auto& user : dead_users) {
            user.remove();
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_user_mutex);
        for (auto& user_data : users_to_add) {
            auto user = std::make_shared<SyncUser>(user_data.user_token, user_data.identity, user_data.server_url);
            user->set_is_admin(user_data.is_admin);
            m_users.insert({ user_data.identity, std::move(user) });
        }
    }
}

bool SyncManager::immediately_run_file_actions(const std::string& realm_path)
{
    if (!m_metadata_manager) {
        return false;
    }
    auto metadata = SyncFileActionMetadata::metadata_for_path(realm_path, *m_metadata_manager);
    if (!metadata) {
        return false;
    }
    if (run_file_action(*metadata)) {
        metadata->remove();
        return true;
    }
    return false;
}

// Perform a file action. Returns whether or not the file action can be removed.
bool SyncManager::run_file_action(const SyncFileActionMetadata& md)
{
    switch (md.action()) {
        case SyncFileActionMetadata::Action::DeleteRealm:
            // Delete all the files for the given Realm.
            m_file_manager->remove_realm(md.original_name());
            return true;
        case SyncFileActionMetadata::Action::HandleRealmForClientReset:
            // Copy the primary Realm file to the recovery dir, and then delete the Realm.
            auto new_name = md.new_name();
            auto original_name = md.original_name();
            if (!util::File::exists(original_name)) {
                // The Realm file doesn't exist anymore.
                return true;
            } 
            if (new_name && !util::File::exists(*new_name) && m_file_manager->copy_realm_file(original_name, *new_name)) {
                // We successfully copied the Realm file to the recovery directory.
                m_file_manager->remove_realm(original_name);
                return true;
            }
            return false;
    }
    return false;
}

void SyncManager::reset_for_testing()
{
    std::lock_guard<std::mutex> lock(m_file_system_mutex);
    m_file_manager = nullptr;
    m_metadata_manager = nullptr;
    {
        // Destroy all the users.
        std::lock_guard<std::mutex> lock(m_user_mutex);
        m_users.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Stop the client. This will abort any uploads that inactive sessions are waiting for.
        if (m_sync_client)
            m_sync_client->stop();

        {
            std::lock_guard<std::mutex> lock(m_session_mutex);

#if REALM_ASSERTIONS_ENABLED
            // Callers of `SyncManager::reset_for_testing` should ensure there are no active sessions
            // prior to calling `reset_for_testing`.
            auto no_active_sessions = std::none_of(m_sessions.begin(), m_sessions.end(), [](auto& element){
                return element.second->existing_external_reference();
            });
            REALM_ASSERT(no_active_sessions);
#endif

            // Destroy any inactive sessions.
            // FIXME: We shouldn't have any inactive sessions at this point! Sessions are expected to
            // remain inactive until their final upload completes, at which point they are unregistered
            // and destroyed. Our call to `sync::Client::stop` above aborts all uploads, so all sessions
            // should have already been destroyed.
            m_sessions.clear();
        }

        // Destroy the client now that we have no remaining sessions.
        m_sync_client = nullptr;

        // Reset even more state.
        // NOTE: these should always match the defaults.
        m_log_level = util::Logger::Level::info;
        m_logger_factory = nullptr;
        m_client_reconnect_mode = ReconnectMode::normal;
    }
}

void SyncManager::set_log_level(util::Logger::Level level) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_log_level = level;
}

void SyncManager::set_logger_factory(SyncLoggerFactory& factory) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logger_factory = &factory;
}

void SyncManager::set_client_should_reconnect_immediately(bool reconnect_immediately)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_client_reconnect_mode = reconnect_immediately ? ReconnectMode::immediate : ReconnectMode::normal;
}

bool SyncManager::client_should_reconnect_immediately() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_client_reconnect_mode == ReconnectMode::immediate;
}

void SyncManager::reconnect()
{
    std::lock_guard<std::mutex> lock(m_session_mutex);
    for (auto& it : m_sessions) {
        it.second->handle_reconnect();
    }
}

util::Logger::Level SyncManager::log_level() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_log_level;
}

bool SyncManager::perform_metadata_update(std::function<void(const SyncMetadataManager&)> update_function) const
{
    std::lock_guard<std::mutex> lock(m_file_system_mutex);
    if (!m_metadata_manager) {
        return false;
    }
    update_function(*m_metadata_manager);
    return true;
}

std::shared_ptr<SyncUser> SyncManager::get_user(const std::string& identity,
                                                std::string refresh_token,
                                                util::Optional<std::string> auth_server_url,
                                                SyncUser::TokenType token_type)
{
    std::lock_guard<std::mutex> lock(m_user_mutex);
    auto it = m_users.find(identity);
    if (it == m_users.end()) {
        // No existing user.
        auto new_user = std::make_shared<SyncUser>(std::move(refresh_token), identity, auth_server_url, token_type);
        m_users.insert({ identity, new_user });
        return new_user;
    } else {
        auto user = it->second;
        if (auth_server_url && *auth_server_url != user->server_url()) {
            throw std::invalid_argument("Cannot retrieve an existing user specifying a different auth server.");
        }
        if (user->token_type() != token_type) {
            throw std::invalid_argument("Cannot retrieve a user specifying a different token type.");
        }
        if (user->state() == SyncUser::State::Error) {
            return nullptr;
        }
        user->update_refresh_token(std::move(refresh_token));
        return user;
    }
}

std::shared_ptr<SyncUser> SyncManager::get_existing_logged_in_user(const std::string& identity) const
{
    std::lock_guard<std::mutex> lock(m_user_mutex);
    auto it = m_users.find(identity);
    if (it == m_users.end()) {
        return nullptr;
    }
    auto ptr = it->second;
    return (ptr->state() == SyncUser::State::Active ? ptr : nullptr);
}

std::vector<std::shared_ptr<SyncUser>> SyncManager::all_logged_in_users() const
{
    std::lock_guard<std::mutex> lock(m_user_mutex);
    std::vector<std::shared_ptr<SyncUser>> users;
    users.reserve(m_users.size());
    for (auto& it : m_users) {
        auto user = it.second;
        if (user->state() == SyncUser::State::Active) {
            users.emplace_back(std::move(user));
        }
    }
    return users;
}

std::shared_ptr<SyncUser> SyncManager::get_current_user() const
{
    std::lock_guard<std::mutex> lock(m_user_mutex);
    
    auto is_active_user = [](auto& el) { return el.second->state() == SyncUser::State::Active; };
    auto it = std::find_if(m_users.begin(), m_users.end(), is_active_user);
    if (it == m_users.end()) {
        return nullptr;
    }
    if (std::find_if(std::next(it), m_users.end(), is_active_user) != m_users.end()) {
        throw std::logic_error("Current user is not valid if more that one valid, logged-in user exists.");
    }
    return it->second;
}

std::string SyncManager::path_for_realm(const std::string& user_identity, const std::string& raw_realm_url) const
{
    std::lock_guard<std::mutex> lock(m_file_system_mutex);
    REALM_ASSERT(m_file_manager);
    return m_file_manager->path(user_identity, raw_realm_url);
}

std::string SyncManager::recovery_directory_path() const
{
    std::lock_guard<std::mutex> lock(m_file_system_mutex);
    REALM_ASSERT(m_file_manager);
    return m_file_manager->recovery_directory_path();        
}

std::shared_ptr<SyncSession> SyncManager::get_existing_active_session(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_session_mutex);
    if (auto session = get_existing_session_locked(path)) {
        if (auto external_reference = session->existing_external_reference())
            return external_reference;
    }
    return nullptr;
}

std::shared_ptr<SyncSession> SyncManager::get_existing_session_locked(const std::string& path) const
{
    REALM_ASSERT(!m_session_mutex.try_lock());
    auto it = m_sessions.find(path);
    if (it == m_sessions.end()) {
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<SyncSession> SyncManager::get_existing_session(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_session_mutex);
    if (auto session = get_existing_session_locked(path)) {
        return session->external_reference();
    }
    return nullptr;
}

std::shared_ptr<SyncSession> SyncManager::get_session(const std::string& path, const SyncConfig& sync_config)
{
    auto& client = get_sync_client(); // Throws

    std::lock_guard<std::mutex> lock(m_session_mutex);
    if (auto session = get_existing_session_locked(path)) {
        sync_config.user->register_session(session);
        return session->external_reference();
    }

    std::shared_ptr<SyncSession> shared_session(new SyncSession(client, path, sync_config));
    m_sessions[path] = shared_session;

    // Create the external reference immediately to ensure that the session will become
    // inactive if an exception is thrown in the following code.
    auto external_reference = shared_session->external_reference();

    sync_config.user->register_session(shared_session);

    return external_reference;
}

void SyncManager::unregister_session(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_session_mutex);
    auto it = m_sessions.find(path);
    REALM_ASSERT(it != m_sessions.end());

    // If the session has an active external reference, leave it be. This will happen if the session
    // moves to an inactive state while still externally reference, for instance, as a result of
    // the session's user being logged out.
    if (it->second->existing_external_reference())
        return;

    m_sessions.erase(path);
}

SyncClient& SyncManager::get_sync_client() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sync_client)
        m_sync_client = create_sync_client(); // Throws
    return *m_sync_client;
}

std::unique_ptr<SyncClient> SyncManager::create_sync_client() const
{
    REALM_ASSERT(!m_mutex.try_lock());

    std::unique_ptr<util::Logger> logger;
    if (m_logger_factory) {
        logger = m_logger_factory->make_logger(m_log_level); // Throws
    }
    else {
        auto stderr_logger = std::make_unique<util::StderrLogger>(); // Throws
        stderr_logger->set_level_threshold(m_log_level);
        logger = std::move(stderr_logger);
    }
    return std::make_unique<SyncClient>(std::move(logger),
                                        m_client_reconnect_mode);
}
