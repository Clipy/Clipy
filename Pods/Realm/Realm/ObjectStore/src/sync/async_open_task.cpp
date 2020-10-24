////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Realm Inc.
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

#include "sync/async_open_task.hpp"

#include "impl/realm_coordinator.hpp"
#include "sync/sync_manager.hpp"
#include "sync/sync_session.hpp"
#include "thread_safe_reference.hpp"

namespace realm {

AsyncOpenTask::AsyncOpenTask(std::shared_ptr<_impl::RealmCoordinator> coordinator, std::shared_ptr<realm::SyncSession> session)
: m_coordinator(coordinator)
, m_session(session)
{
}

void AsyncOpenTask::start(std::function<void(ThreadSafeReference, std::exception_ptr)> callback)
{
    auto session = m_session.load();
    if (!session)
        return;

    std::shared_ptr<AsyncOpenTask> self(shared_from_this());
    session->wait_for_download_completion([callback, self, this](std::error_code ec) {
       auto session = m_session.exchange(nullptr);
        if (!session)
            return; // Swallow all events if the task as been canceled.

        // Release our references to the coordinator after calling the callback
        auto coordinator = std::move(m_coordinator);
        m_coordinator = nullptr;

        if (ec)
            return callback({}, std::make_exception_ptr(std::system_error(ec)));

        ThreadSafeReference realm;
        try {
            realm = coordinator->get_unbound_realm();
        }
        catch (...) {
            return callback({}, std::current_exception());
        }
        callback(std::move(realm), nullptr);
    });
}

void AsyncOpenTask::cancel()
{
    if (auto session = m_session.exchange(nullptr)) {
        // Does a better way exists for canceling the download?
        session->log_out();
        m_coordinator = nullptr;
    }
}

uint64_t AsyncOpenTask::register_download_progress_notifier(std::function<SyncProgressNotifierCallback> callback)
{
    if (auto session = m_session.load()) {
        return session->register_progress_notifier(callback, realm::SyncSession::NotifierType::download, false);
    }
    else {
        return 0;
    }
}

void AsyncOpenTask::unregister_download_progress_notifier(uint64_t token)
{
    if (auto session = m_session.load())
        session->unregister_progress_notifier(token);
}

}
