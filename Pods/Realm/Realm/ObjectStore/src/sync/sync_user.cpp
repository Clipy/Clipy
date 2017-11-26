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

#include "sync/sync_user.hpp"

#include "sync/impl/sync_metadata.hpp"
#include "sync/sync_manager.hpp"
#include "sync/sync_session.hpp"

namespace realm {

SyncUserContextFactory SyncUser::s_binding_context_factory;
std::mutex SyncUser::s_binding_context_factory_mutex;

SyncUser::SyncUser(std::string refresh_token,
                   std::string identity,
                   util::Optional<std::string> server_url,
                   util::Optional<std::string> local_identity,
                   TokenType token_type)
: m_state(State::Active)
, m_server_url(server_url.value_or(""))
, m_token_type(token_type)
, m_refresh_token(std::move(refresh_token))
, m_identity(std::move(identity))
{
    {
        std::lock_guard<std::mutex> lock(s_binding_context_factory_mutex);
        if (s_binding_context_factory) {
            m_binding_context = s_binding_context_factory();
        }
    }
    if (token_type == TokenType::Normal) {
        REALM_ASSERT(m_server_url.length() > 0);
        bool updated = SyncManager::shared().perform_metadata_update([=](const auto& manager) {
            auto metadata = manager.get_or_make_user_metadata(m_identity, m_server_url);
            metadata->set_user_token(m_refresh_token);
            m_is_admin = metadata->is_admin();
            m_local_identity = metadata->local_uuid();
        });
        if (!updated)
            m_local_identity = m_identity;
    } else {
        // Admin token users. The local identity serves as the directory path.
        REALM_ASSERT(local_identity);
        m_local_identity = std::move(*local_identity);
    }
}

std::vector<std::shared_ptr<SyncSession>> SyncUser::all_sessions()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::shared_ptr<SyncSession>> sessions;
    if (m_state == State::Error) {
        return sessions;
    }
    for (auto it = m_sessions.begin(); it != m_sessions.end();) {
        if (auto ptr_to_session = it->second.lock()) {
            if (!ptr_to_session->is_in_error_state()) {
                sessions.emplace_back(std::move(ptr_to_session));
                it++;
                continue;
            }
        }
        // This session is bad, destroy it.
        it = m_sessions.erase(it);
    }
    return sessions;
}

std::shared_ptr<SyncSession> SyncUser::session_for_on_disk_path(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == State::Error) {
        return nullptr;
    }
    auto it = m_sessions.find(path);
    if (it == m_sessions.end()) {
        return nullptr;
    }
    auto locked = it->second.lock();
    if (!locked) {
        // Remove the session from the map, because it has fatally errored out or the entry is invalid.
        m_sessions.erase(it);
    }
    return locked;
}

void SyncUser::update_refresh_token(std::string token)
{
    std::vector<std::shared_ptr<SyncSession>> sessions_to_revive;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (auto session = m_management_session.lock())
            sessions_to_revive.emplace_back(std::move(session));

        if (auto session = m_permission_session.lock())
            sessions_to_revive.emplace_back(std::move(session));

        switch (m_state) {
            case State::Error:
                return;
            case State::Active:
                m_refresh_token = token;
                break;
            case State::LoggedOut: {
                sessions_to_revive.reserve(m_waiting_sessions.size());
                m_refresh_token = token;
                m_state = State::Active;
                for (auto& pair : m_waiting_sessions) {
                    if (auto ptr = pair.second.lock()) {
                        m_sessions[pair.first] = ptr;
                        sessions_to_revive.emplace_back(std::move(ptr));
                    }
                }
                m_waiting_sessions.clear();
                break;
            }
        }
        // Update persistent user metadata.
        if (m_token_type != TokenType::Admin) {
            SyncManager::shared().perform_metadata_update([=](const auto& manager) {
                auto metadata = manager.get_or_make_user_metadata(m_identity, m_server_url);
                metadata->set_user_token(token);
            });
        }
    }
    // (Re)activate all pending sessions.
    // Note that we do this after releasing the lock, since the session may
    // need to access protected User state in the process of binding itself.
    for (auto& session : sessions_to_revive) {
        session->revive_if_needed();
    }
}

void SyncUser::log_out()
{
    if (m_token_type == TokenType::Admin) {
        // Admin-token users cannot be logged out.
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == State::LoggedOut) {
        return;
    }
    m_state = State::LoggedOut;
    // Move all active sessions into the waiting sessions pool. If the user is
    // logged back in, they will automatically be reactivated.
    for (auto& pair : m_sessions) {
        if (auto ptr = pair.second.lock()) {
            ptr->log_out();
            m_waiting_sessions[pair.first] = ptr;
        }
    }
    m_sessions.clear();
    // Deactivate the sessions for the management and admin Realms.
    if (auto session = m_management_session.lock())
        session->log_out();

    if (auto session = m_permission_session.lock())
        session->log_out();

    // Mark the user as 'dead' in the persisted metadata Realm.
    SyncManager::shared().perform_metadata_update([=](const auto& manager) {
        auto metadata = manager.get_or_make_user_metadata(m_identity, m_server_url, false);
        if (metadata)
            metadata->mark_for_removal();
    });
}

void SyncUser::set_is_admin(bool is_admin)
{
    if (m_token_type == TokenType::Admin) {
        return;
    }
    m_is_admin = is_admin;
    SyncManager::shared().perform_metadata_update([=](const auto& manager) {
        auto metadata = manager.get_or_make_user_metadata(m_identity, m_server_url);
        metadata->set_is_admin(is_admin);
    });
}

void SyncUser::invalidate()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = State::Error;
}

std::string SyncUser::refresh_token() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_refresh_token;
}

SyncUser::State SyncUser::state() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

void SyncUser::register_session(std::shared_ptr<SyncSession> session)
{
    const std::string& path = session->path();
    std::unique_lock<std::mutex> lock(m_mutex);
    switch (m_state) {
        case State::Active:
            // Immediately ask the session to come online.
            m_sessions[path] = session;
            // FIXME: `SyncUser`s shouldn't even wrap admin tokens; the bindings should do that.
            if (m_token_type == TokenType::Admin) {
                session->bind_with_admin_token(m_refresh_token, session->config().realm_url());
            } else {
                lock.unlock();
                session->revive_if_needed();
            }
            break;
        case State::LoggedOut:
            m_waiting_sessions[path] = session;
            break;
        case State::Error:
            break;
    }
}

void SyncUser::set_binding_context_factory(SyncUserContextFactory factory)
{
    std::lock_guard<std::mutex> lock(s_binding_context_factory_mutex);
    s_binding_context_factory = std::move(factory);
}

void SyncUser::register_management_session(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_management_session.lock() || m_state == State::Error)
        return;

    m_management_session = SyncManager::shared().get_existing_session(path);
}

void SyncUser::register_permission_session(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_permission_session.lock() || m_state == State::Error)
        return;

    m_permission_session = SyncManager::shared().get_existing_session(path);
}

}

namespace std {
size_t hash<realm::SyncUserIdentifier>::operator()(const realm::SyncUserIdentifier& k) const
{
    return ((hash<string>()(k.user_id) ^ (hash<string>()(k.auth_server_url) << 1)) >> 1);
}
}
