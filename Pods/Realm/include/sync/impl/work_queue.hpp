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

#ifndef REALM_OS_PARTIAL_SYNC_WORK_QUEUE
#define REALM_OS_PARTIAL_SYNC_WORK_QUEUE

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace realm {
namespace _impl {
namespace partial_sync {

class WorkQueue {
public:
    ~WorkQueue();
    void enqueue(std::function<void()> function);

private:
    void create_thread();

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::vector<std::function<void()>> m_queue;
    std::thread m_thread;
    bool m_stopping = false;
    bool m_stopped = true;
};


} // namespace partial_sync
} // namespace _impl
} // namespace realm

#endif // REALM_OS_PARTIAL_SYNC_WORK_QUEUE
