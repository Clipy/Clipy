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

#include <thread>

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
        // Load persisted users into the users map.
        SyncUserMetadataResults users = m_metadata_manager->all_unmarked_users();
        for (size_t i = 0; i < users.size(); i++) {
            // Note that 'admin' style users are not persisted.
            auto user_data = users.get(i);
            auto user_token = user_data.user_token();
            auto identity = user_data.identity();
            auto server_url = user_data.server_url();
            if (user_token) {
                UserCreationData data = { std::move(identity), std::move(*user_token), std::move(server_url) };
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
            m_users.insert({ user_data.identity, std::make_shared<SyncUser>(user_data.user_token,
                                                                            user_data.identity,
                                                                            user_data.server_url) });
        }
    }
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
        // Destroy the client.
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sync_client = nullptr;
        // Reset even more state.
        // NOTE: these should always match the defaults.
        m_log_level = util::Logger::Level::info;
        m_logger_factory = nullptr;
        m_client_reconnect_mode = sync::Client::Reconnect::normal;
        m_client_validate_ssl = true;
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

void SyncManager::set_error_handler(std::function<sync::Client::ErrorHandler> handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto wrapped_handler = [=](int error_code, std::string message) {
        // FIXME: If the sync team decides to route all errors through the session-level error handler, the client-level
        // error handler might go away altogether.
        switch (error_code) {
            case 100:       // Connection closed (no error)
            case 101:       // Unspecified non-critical error
                return;
            default:
                handler(error_code, message);
        }
    };
    m_error_handler = std::move(wrapped_handler);
}

void SyncManager::set_client_should_reconnect_immediately(bool reconnect_immediately)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    using Reconnect = sync::Client::Reconnect;
    m_client_reconnect_mode = reconnect_immediately ? Reconnect::immediately : Reconnect::normal;
}

bool SyncManager::client_should_reconnect_immediately() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    using Reconnect = sync::Client::Reconnect;
    return m_client_reconnect_mode == Reconnect::immediately;
}

void SyncManager::set_client_should_validate_ssl(bool validate_ssl)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_client_validate_ssl = validate_ssl;
}

bool SyncManager::client_should_validate_ssl() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_client_validate_ssl;
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
                                                bool is_admin)
{
    std::lock_guard<std::mutex> lock(m_user_mutex);
    auto it = m_users.find(identity);
    if (it == m_users.end()) {
        // No existing user.
        auto new_user = std::make_shared<SyncUser>(std::move(refresh_token), identity, auth_server_url, is_admin);
        m_users.insert({ identity, new_user });
        return new_user;
    } else {
        auto user = it->second;
        if (auth_server_url && *auth_server_url != user->server_url()) {
            throw std::invalid_argument("Cannot retrieve an existing user specifying a different auth server.");
        }
        if (is_admin != user->is_admin()) {
            throw std::invalid_argument("Cannot retrieve an existing user with a different admin status.");
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

std::vector<std::shared_ptr<SyncUser>> SyncManager::all_users() const
{
    std::lock_guard<std::mutex> lock(m_user_mutex);
    std::vector<std::shared_ptr<SyncUser>> users;
    users.reserve(m_users.size());
    for (auto& it : m_users) {
        auto user = it.second;
        if (user->state() != SyncUser::State::Error) {
            users.emplace_back(std::move(user));
        }
    }
    return users;
}

std::string SyncManager::path_for_realm(const std::string& user_identity, const std::string& raw_realm_url) const
{
    std::lock_guard<std::mutex> lock(m_file_system_mutex);
    REALM_ASSERT(m_file_manager);
    return m_file_manager->path(user_identity, raw_realm_url);
}

std::shared_ptr<SyncSession> SyncManager::get_existing_active_session(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_session_mutex);
    return get_existing_active_session_locked(path);
}

std::shared_ptr<SyncSession> SyncManager::get_existing_active_session_locked(const std::string& path) const
{
    REALM_ASSERT(!m_session_mutex.try_lock());
    auto it = m_active_sessions.find(path);
    if (it == m_active_sessions.end()) {
        return nullptr;
    }
    if (auto session = it->second.lock()) {
        return session;
    }
    return nullptr;
}

std::unique_ptr<SyncSession> SyncManager::get_existing_inactive_session_locked(const std::string& path)
{
    REALM_ASSERT(!m_session_mutex.try_lock());
    auto it = m_inactive_sessions.find(path);
    if (it == m_inactive_sessions.end()) {
        return nullptr;
    }
    auto ret = std::move(it->second);
    m_inactive_sessions.erase(it);
    return ret;
}

std::shared_ptr<SyncSession> SyncManager::get_session(const std::string& path, const SyncConfig& sync_config)
{
    auto client = get_sync_client(); // Throws

    std::lock_guard<std::mutex> lock(m_session_mutex);
    if (auto session = get_existing_active_session_locked(path)) {
        return session;
    }

    std::unique_ptr<SyncSession> session = get_existing_inactive_session_locked(path);
    bool session_is_new = false;
    if (!session) {
        session_is_new = true;
        session.reset(new SyncSession(std::move(client), path, sync_config));
    }

    auto session_deleter = [this](SyncSession *session) { dropped_last_reference_to_session(session); };
    auto shared_session = std::shared_ptr<SyncSession>(session.release(), std::move(session_deleter));
    m_active_sessions[path] = shared_session;
    if (session_is_new) {
        sync_config.user->register_session(shared_session);
    } else {
        SyncSession::revive_if_needed(shared_session);
    }
    return shared_session;
}

void SyncManager::dropped_last_reference_to_session(SyncSession* session)
{
    {
        std::lock_guard<std::mutex> lock(m_session_mutex);
        auto path = session->path();
        REALM_ASSERT_DEBUG(m_active_sessions.count(path));
        m_active_sessions.erase(path);
        m_inactive_sessions[path].reset(session);
    }
    session->close();
}

void SyncManager::unregister_session(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_session_mutex);
    if (m_active_sessions.count(path))
        return;
    auto it = m_inactive_sessions.find(path);
    REALM_ASSERT(it != m_inactive_sessions.end());
    if (it->second->can_be_safely_destroyed())
        m_inactive_sessions.erase(path);
}

std::shared_ptr<SyncClient> SyncManager::get_sync_client() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sync_client)
        m_sync_client = create_sync_client(); // Throws
    return m_sync_client;
}

std::shared_ptr<SyncClient> SyncManager::create_sync_client() const
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
    return std::make_shared<SyncClient>(std::move(logger),
                                        std::move(m_error_handler),
                                        m_client_reconnect_mode,
                                        m_client_validate_ssl);
}
