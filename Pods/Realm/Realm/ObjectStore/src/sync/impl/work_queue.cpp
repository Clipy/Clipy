////////////////////////////////////////////////////////////////////////////
//
// Copyright 2018 Realm Inc.
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

#include "sync/impl/work_queue.hpp"

#include <chrono>

namespace realm {
namespace _impl {
namespace partial_sync {

WorkQueue::~WorkQueue()
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_stopping = true;
    }
    m_cv.notify_one();

    if (m_thread.joinable())
        m_thread.join();
}

void WorkQueue::enqueue(std::function<void()> function)
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.push_back(std::move(function));

        if (m_stopped)
            create_thread();
    }
    m_cv.notify_one();
}

void WorkQueue::create_thread()
{
    if (m_thread.joinable())
        m_thread.join();

    m_thread = std::thread([this] {
        std::vector<std::function<void()>> queue;

        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_stopping &&
               m_cv.wait_for(lock, std::chrono::milliseconds(500),
                             [&] { return !m_queue.empty() || m_stopping; })) {

            swap(queue, m_queue);

            lock.unlock();
            for (auto& f : queue)
                f();
            queue.clear();
            lock.lock();
        }

        m_stopped = true;
    });
    m_stopped = false;
}

} // namespace partial_sync
} // namespace _impl
} // namespace realm
