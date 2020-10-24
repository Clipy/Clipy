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
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or utilied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#ifndef REALM_OS_CHECKED_MUTEX_HPP
#define REALM_OS_CHECKED_MUTEX_HPP

#include <mutex>

// Clang's thread safety analysis can be used to statically check that variables
// which should be guarded by a mutex actually are only accessed with that
// mutex acquired. This requires annotating variables and functions with which
// "capabilities" (i.e. a generalization of the concept of a mutex) are required.
//
// libc++ has an option to apply this for *all* uses of <mutex> by defining
// _LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS, but this doesn't make it easy to
// incrementally adopt the annotations (and it's not obvious that it's even
// worth trying to universally adopt them), so instead we have some wrappers
// for std::mutex and locks which add annotations.

// See https://clang.llvm.org/docs/ThreadSafetyAnalysis.html for more information on this

#if defined(__clang__)
    #define REALM_THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
    #define REALM_THREAD_ANNOTATION_ATTRIBUTE__(x)
#endif

#define CAPABILITY(x) \
    REALM_THREAD_ANNOTATION_ATTRIBUTE__(capability(x))

#define SCOPED_CAPABILITY \
    REALM_THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

#define GUARDED_BY(x) \
    REALM_THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

#define REQUIRES(...) \
    REALM_THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))

#define ACQUIRE(...) \
    REALM_THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))

#define RELEASE(...) \
    REALM_THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))

#define EXCLUDES(...) \
    REALM_THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

#define NO_THREAD_SAFETY_ANALYSIS \
    REALM_THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

namespace realm {
namespace util {

// std::unique_lock with thread safety annotations
class SCOPED_CAPABILITY CheckedUniqueLock {
    using Impl = std::unique_lock<std::mutex>;
public:
    template<typename Mutex>
    CheckedUniqueLock(Mutex const& m) ACQUIRE(m) : m_impl(m.lock()) { }
    ~CheckedUniqueLock() RELEASE() { }

    CheckedUniqueLock(CheckedUniqueLock&&) = default;
    CheckedUniqueLock& operator=(CheckedUniqueLock&&) = default;

    void lock() ACQUIRE() { m_impl.lock(); }
    void unlock() RELEASE() { m_impl.unlock(); }
    void lock_unchecked() { m_impl.lock(); }
    void unlock_unchecked() { m_impl.unlock(); }
    bool owns_lock() const noexcept { return m_impl.owns_lock(); }

    Impl& native_handle() { return m_impl; }

private:
    Impl m_impl;
};

// std::lock_guard with thread safety annotations
class SCOPED_CAPABILITY CheckedLockGuard {
    using Impl = std::unique_lock<std::mutex>;
public:
    template<typename Mutex>
    CheckedLockGuard(Mutex const& m) ACQUIRE(m) : m_impl(m.lock()) { }
    ~CheckedLockGuard() RELEASE() { }

    CheckedLockGuard(CheckedLockGuard&&) = delete;
    CheckedLockGuard& operator=(CheckedLockGuard&&) = delete;

private:
    Impl m_impl;
};

// std::mutex with thread safety annotations
class CAPABILITY("mutex") CheckedMutex {
public:
    CheckedMutex() = default;

    // Required for REQUIRE(!m); do not actually call
    CheckedMutex const& operator!() const { return *this; }
private:
    mutable std::mutex m_mutex;
    friend class CheckedUniqueLock;
    friend class CheckedLockGuard;

    std::unique_lock<std::mutex> lock() const
    {
        return std::unique_lock<std::mutex>(m_mutex);
    }
};

// An "optional" mutex. If constructed with enable=true, it works like a normal
// std::mutex. If constructed with enable=false, locking and unlocking it is
// a no-op.
class CAPABILITY("mutex") CheckedOptionalMutex {
public:
    CheckedOptionalMutex(bool enable=false);
    CheckedOptionalMutex(CheckedOptionalMutex const&);
    CheckedOptionalMutex& operator=(CheckedOptionalMutex const&);
    CheckedOptionalMutex(CheckedOptionalMutex&&) = default;
    CheckedOptionalMutex& operator=(CheckedOptionalMutex&&) = default;

    // Required for REQUIRE(!m); do not actually call
    CheckedOptionalMutex const& operator!() const { return *this; }
private:
    mutable std::unique_ptr<std::mutex> m_mutex;
    friend class CheckedUniqueLock;
    friend class CheckedLockGuard;

    std::unique_lock<std::mutex> lock() const;
};

inline CheckedOptionalMutex::CheckedOptionalMutex(bool enable)
: m_mutex(enable ? std::make_unique<std::mutex>() : nullptr)
{
}

inline CheckedOptionalMutex::CheckedOptionalMutex(CheckedOptionalMutex const& o)
: CheckedOptionalMutex(!!o.m_mutex)
{
}

inline CheckedOptionalMutex& CheckedOptionalMutex::operator=(CheckedOptionalMutex const& o)
{
    if (&o != this) {
        if (o.m_mutex)
            m_mutex = std::make_unique<std::mutex>();
        else
            m_mutex.reset();
    }
    return *this;
}

inline std::unique_lock<std::mutex> CheckedOptionalMutex::lock() const
{
    return m_mutex ? std::unique_lock<std::mutex>(*m_mutex) : std::unique_lock<std::mutex>();
}

} // namespace util
} // namespace realm

#endif // REALM_OS_CHECKED_MUTEX_HPP
