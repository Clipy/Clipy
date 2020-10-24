////////////////////////////////////////////////////////////////////////////
//
// Copyright 2020 Realm Inc.
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

#ifndef REALM_OS_UTIL_SCHEDULER
#define REALM_OS_UTIL_SCHEDULER

#include <realm/util/features.h>
#include <realm/version_id.hpp>

#include <functional>
#include <memory>

#if REALM_PLATFORM_APPLE
#include <CoreFoundation/CoreFoundation.h>
#endif

#if (defined(REALM_HAVE_UV) && REALM_HAVE_UV && !REALM_PLATFORM_APPLE) || (defined(REALM_PLATFORM_NODE) && REALM_PLATFORM_NODE)
#define REALM_USE_UV 1
#else
#define REALM_USE_UV 0
#endif

#if !defined(REALM_USE_CF) && REALM_PLATFORM_APPLE
#define REALM_USE_CF 1
#elif !defined(REALM_USE_ALOOPER) && REALM_ANDROID
#define REALM_USE_ALOOPER 1
#endif

namespace realm {
namespace util {
// A Scheduler combines two related concepts related to our implementation of
// thread confinement: checking if we are currently on the correct thread, and
// sending a notification to a thread-confined object from another thread.
class Scheduler {
public:
    virtual ~Scheduler();

    // Trigger a call to the registered notify callback on the scheduler's event loop.
    //
    // This function can be called from any thread.
    virtual void notify() = 0;
    // Check if the caller is currently running on the scheduler's thread.
    //
    // This function can be called from any thread.
    virtual bool is_on_thread() const noexcept = 0;

    // Checks if this scheduler instance wraps the same underlying instance.
    // This is up to the platforms to define, but if this method returns true,
    // caching may occur.
    virtual bool is_same_as(const Scheduler* other) const noexcept = 0;

    // Check if this scehduler actually can support notify(). Notify may be
    // either not implemented, not applicable to a scheduler type, or simply not
    // be possible currently (e.g. if the associated event loop is not actually
    // running).
    //
    // This function is not thread-safe.
    virtual bool can_deliver_notifications() const noexcept = 0;
    // Set the callback function which will be called by notify().
    //
    // This function is not thread-safe.
    virtual void set_notify_callback(std::function<void()>) = 0;

    // Get the scheduler for frozen Realms. This scheduler does not support
    // notifications and does not perform any thread checking.
    static std::shared_ptr<Scheduler> get_frozen(VersionID version);

    // Create a new instance of the default scheduler for the current platform.
    // This normally will be a thread-confined scheduler using the current
    // thread which supports notifications via an event loop.
    static std::shared_ptr<Scheduler> make_default();

#if REALM_PLATFORM_APPLE
    // Create a scheduler which is bound to the given run loop rather than the
    // current thread's run loop.
    static std::shared_ptr<Scheduler> make_runloop(CFRunLoopRef);
    // Create a scheduler which is bound to the given dispatch queue.
    static std::shared_ptr<Scheduler> make_dispatch(void*);
#else
    static std::shared_ptr<Scheduler> make_runloop();
#endif

    // For platforms with no default scheduler implementation, register a factory
    // function which can produce custom schedulers.
    static void set_default_factory(std::function<std::shared_ptr<Scheduler>()>);
};

} // namespace util
} // namespace realm

#endif // REALM_OS_UTIL_SCHEDULER
