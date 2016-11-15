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

#include "sync_manager.hpp"

#include "impl/sync_client.hpp"
#include "sync_session.hpp"

#include <thread>

using namespace realm;
using namespace realm::_impl;

SyncManager& SyncManager::shared()
{
    static SyncManager& manager = *new SyncManager;
    return manager;
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

void SyncManager::set_client_should_validate_ssl(bool validate_ssl)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_client_validate_ssl = validate_ssl;
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
    if (!session)
        session.reset(new SyncSession(std::move(client), path, sync_config));
    session->revive_if_needed();

    auto session_deleter = [this](SyncSession *session) { dropped_last_reference_to_session(session); };
    auto shared_session = std::shared_ptr<SyncSession>(session.release(), std::move(session_deleter));
    m_active_sessions[path] = shared_session;
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
    if (it->second->is_inactive())
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
