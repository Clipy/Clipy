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

#include <atomic>
#include <CoreFoundation/CFRunLoop.h>

namespace realm {
namespace util {
template<typename Callback>
class EventLoopSignal {
public:
    EventLoopSignal(Callback&& callback)
    {
        struct RefCountedRunloopCallback {
            Callback callback;
            std::atomic<size_t> ref_count;
        };

        CFRunLoopSourceContext ctx{};
        ctx.info = new RefCountedRunloopCallback{std::move(callback), {0}};
        ctx.perform = [](void* info) {
            static_cast<RefCountedRunloopCallback*>(info)->callback();
        };
        ctx.retain = [](const void* info) {
            static_cast<RefCountedRunloopCallback*>(const_cast<void*>(info))->ref_count.fetch_add(1, std::memory_order_relaxed);
            return info;
        };
        ctx.release = [](const void* info) {
            auto ptr = static_cast<RefCountedRunloopCallback*>(const_cast<void*>(info));
            if (ptr->ref_count.fetch_add(-1, std::memory_order_acq_rel) == 1) {
                delete ptr;
            }
        };

        m_runloop = CFRunLoopGetCurrent();
        CFRetain(m_runloop);
        m_signal = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &ctx);
        CFRunLoopAddSource(m_runloop, m_signal, kCFRunLoopDefaultMode);
    }

    ~EventLoopSignal()
    {
        CFRunLoopSourceInvalidate(m_signal);
        CFRelease(m_signal);
        CFRelease(m_runloop);
    }

    EventLoopSignal(EventLoopSignal&&) = delete;
    EventLoopSignal& operator=(EventLoopSignal&&) = delete;
    EventLoopSignal(EventLoopSignal const&) = delete;
    EventLoopSignal& operator=(EventLoopSignal const&) = delete;

    void notify()
    {
        CFRunLoopSourceSignal(m_signal);
        // Signalling the source makes it run the next time the runloop gets
        // to it, but doesn't make the runloop start if it's currently idle
        // waiting for events
        CFRunLoopWakeUp(m_runloop);
    }

private:
    CFRunLoopRef m_runloop;
    CFRunLoopSourceRef m_signal;
};
} // namespace util
} // namespace realm
