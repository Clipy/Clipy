/*************************************************************************
 *
 * Copyright 2016 Realm Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 **************************************************************************/

#ifndef REALM_UTIL_THREAD_HPP
#define REALM_UTIL_THREAD_HPP

#include <exception>

#ifdef _WIN32
#include <thread>
#include <condition_variable> // for windows non-interprocess condvars we use std::condition_variable
#include <Windows.h>
#include <process.h> // _getpid()
#else
#include <pthread.h>
#endif

// Use below line to enable a thread bug detection tool. Note: Will make program execution slower.
// #include <../test/pthread_test.hpp>

#include <cerrno>
#include <cstddef>
#include <string>

#include <realm/util/features.h>
#include <realm/util/assert.hpp>
#include <realm/util/terminate.hpp>
#include <memory>

#include <atomic>

namespace realm {
namespace util {


/// A separate thread of execution.
///
/// This class is a C++03 compatible reproduction of a subset of std::thread
/// from C++11 (when discounting Thread::start(), Thread::set_name(), and
/// Thread::get_name()).
class Thread {
public:
    Thread();
    ~Thread() noexcept;

    template <class F>
    explicit Thread(F func);

    // Disable copying. It is an error to copy this Thread class.
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    Thread(Thread&&) noexcept;

    /// This method is an extension of the API provided by
    /// std::thread. This method exists because proper move semantics
    /// is unavailable in C++03. If move semantics had been available,
    /// calling `start(func)` would have been equivalent to `*this =
    /// Thread(func)`. Please see std::thread::operator=() for
    /// details.
    template <class F>
    void start(F func);

    bool joinable() noexcept;

    void join();

    // If supported by the platform, set the name of the calling thread (mainly
    // for debugging purposes). The name will be silently clamped to whatever
    // limit the platform places on these names. Linux places a limit of 15
    // characters for these names.
    static void set_name(const std::string&);

    // If supported by the platform, this function assigns the name of the
    // calling thread to \a name, and returns true, otherwise it does nothing
    // and returns false.
    static bool get_name(std::string& name) noexcept;

private:

#ifdef _WIN32
    std::thread m_std_thread;
#else    
    pthread_t m_id;
#endif
    bool m_joinable;
    typedef void* (*entry_func_type)(void*);

    void start(entry_func_type, void* arg);

    template <class>
    static void* entry_point(void*) noexcept;

    REALM_NORETURN static void create_failed(int);
    REALM_NORETURN static void join_failed(int);
};


/// Low-level mutual exclusion device.
class Mutex {
public:
    Mutex();
    ~Mutex() noexcept;

    struct process_shared_tag {
    };
    /// Initialize this mutex for use across multiple processes. When
    /// constructed this way, the instance may be placed in memory
    /// shared by multiple processes, as well as in a memory mapped
    /// file. Such a mutex remains valid even after the constructing
    /// process terminates. Deleting the instance (freeing the memory
    /// or deleting the file) without first calling the destructor is
    /// legal and will not cause any system resources to be leaked.
    Mutex(process_shared_tag);

    // Disable copying.
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    friend class LockGuard;
    friend class UniqueLock;
    friend class InterprocessCondVar;

    void lock() noexcept;
    bool try_lock() noexcept;
    void unlock() noexcept;

protected:
#ifdef _WIN32
    // Used for non-process-shared mutex. We only know at runtime whether or not to use it, depending on if we call
    // Mutex::Mutex(process_shared_tag)
    CRITICAL_SECTION m_critical_section;
#else
    pthread_mutex_t m_impl = PTHREAD_MUTEX_INITIALIZER;
#endif

    struct no_init_tag {
    };
    Mutex(no_init_tag)
    {
    }

    void init_as_regular();
    void init_as_process_shared(bool robust_if_available);

    REALM_NORETURN static void init_failed(int);
    REALM_NORETURN static void attr_init_failed(int);
    REALM_NORETURN static void destroy_failed(int) noexcept;
    REALM_NORETURN static void lock_failed(int) noexcept;

private:
    friend class CondVar;
    friend class RobustMutex;
};


/// A simple mutex ownership wrapper.
class LockGuard {
public:
    LockGuard(Mutex&) noexcept;
    ~LockGuard() noexcept;

private:
    Mutex& m_mutex;
    friend class CondVar;
};


/// See UniqueLock.
struct defer_lock_tag {
};

/// A general-purpose mutex ownership wrapper supporting deferred
/// locking as well as repeated unlocking and relocking.
class UniqueLock {
public:
    UniqueLock(Mutex&) noexcept;
    UniqueLock(Mutex&, defer_lock_tag) noexcept;
    ~UniqueLock() noexcept;

    void lock() noexcept;
    void unlock() noexcept;
    bool holds_lock() noexcept;

private:
    Mutex* m_mutex;
    bool m_is_locked;
};


/// A robust version of a process-shared mutex.
///
/// A robust mutex is one that detects whether a thread (or process)
/// has died while holding a lock on the mutex.
///
/// When the present platform does not offer support for robust
/// mutexes, this mutex class behaves as a regular process-shared
/// mutex, which means that if a thread dies while holding a lock, any
/// future attempt at locking will block indefinitely.
class RobustMutex : private Mutex {
public:
    RobustMutex();
    ~RobustMutex() noexcept;

    static bool is_robust_on_this_platform() noexcept;

    class NotRecoverable;

    /// \param recover_func If the present platform does not support
    /// robust mutexes, this function is never called. Otherwise it is
    /// called if, and only if a thread has died while holding a
    /// lock. The purpose of the function is to reestablish a
    /// consistent shared state. If it fails to do this by throwing an
    /// exception, the mutex enters the 'unrecoverable' state where
    /// any future attempt at locking it will fail and cause
    /// NotRecoverable to be thrown. This function is advised to throw
    /// NotRecoverable when it fails, but it may throw any exception.
    ///
    /// \throw NotRecoverable If thrown by the specified recover
    /// function, or if the mutex has entered the 'unrecoverable'
    /// state due to a different thread throwing from its recover
    /// function.
    template <class Func>
    void lock(Func recover_func);

    template <class Func>
    bool try_lock(Func recover_func);

    void unlock() noexcept;

    /// Low-level locking of robust mutex.
    ///
    /// If the present platform does not support robust mutexes, this
    /// function always returns true. Otherwise it returns false if,
    /// and only if a thread has died while holding a lock.
    ///
    /// \note Most application should never call this function
    /// directly. It is called automatically when using the ordinary
    /// lock() function.
    ///
    /// \throw NotRecoverable If this mutex has entered the "not
    /// recoverable" state. It enters this state if
    /// mark_as_consistent() is not called between a call to
    /// robust_lock() that returns false and the corresponding call to
    /// unlock().
    bool low_level_lock();

    /// Low-level try-lock of robust mutex
    ///
    /// If the present platform does not support robust mutexes, this
    /// function always returns 0 or 1. Otherwise it returns -1 if,
    /// and only if a thread has died while holding a lock.
    ///
    /// Returns 1 if the lock is succesfully obtained.
    /// Returns 0 if the lock is held by somebody else (not obtained)
    /// Returns -1 if a thread has died while holding a lock.
    ///
    /// \note Most application should never call this function
    /// directly. It is called automatically when using the ordinary
    /// lock() function.
    ///
    /// \throw NotRecoverable If this mutex has entered the "not
    /// recoverable" state. It enters this state if
    /// mark_as_consistent() is not called between a call to
    /// robust_lock() that returns false and the corresponding call to
    /// unlock().
    int try_low_level_lock();

    /// Pull this mutex out of the 'inconsistent' state.
    ///
    /// Must be called only after low_level_lock() has returned false.
    ///
    /// \note Most application should never call this function
    /// directly. It is called automatically when using the ordinary
    /// lock() function.
    void mark_as_consistent() noexcept;

    /// Attempt to check if this mutex is a valid object.
    ///
    /// This attempts to trylock() the mutex, and if that fails returns false if
    /// the return value indicates that the low-level mutex is invalid (which is
    /// distinct from 'inconsistent'). Although pthread_mutex_trylock() may
    /// return EINVAL if the argument is not an initialized mutex object, merely
    /// attempting to check if an arbitrary blob of memory is a mutex object may
    /// involve undefined behavior, so it is only safe to assume that this
    /// function will run correctly when it is known that the mutex object is
    /// valid.
    bool is_valid() noexcept;

    friend class CondVar;
};

class RobustMutex::NotRecoverable : public std::exception {
public:
    const char* what() const noexcept override
    {
        return "Failed to recover consistent state of shared memory";
    }
};


/// A simple robust mutex ownership wrapper.
class RobustLockGuard {
public:
    /// \param m the mutex to guard
    /// \param func See RobustMutex::lock().
    template <class TFunc>
    RobustLockGuard(RobustMutex& m, TFunc func);
    ~RobustLockGuard() noexcept;

private:
    RobustMutex& m_mutex;
    friend class CondVar;
};


/// Condition variable for use in synchronization monitors.
class CondVar {
public:
    CondVar();
    ~CondVar() noexcept;

    struct process_shared_tag {
    };

    /// Initialize this condition variable for use across multiple
    /// processes. When constructed this way, the instance may be
    /// placed in memory shared by multimple processes, as well as in
    /// a memory mapped file. Such a condition variable remains valid
    /// even after the constructing process terminates. Deleting the
    /// instance (freeing the memory or deleting the file) without
    /// first calling the destructor is legal and will not cause any
    /// system resources to be leaked.
    CondVar(process_shared_tag);

    /// Wait for another thread to call notify() or notify_all().
    void wait(LockGuard& l) noexcept;
    template <class Func>
    void wait(RobustMutex& m, Func recover_func, const struct timespec* tp = nullptr);

    /// If any threads are wating for this condition, wake up at least
    /// one.
    void notify() noexcept;

    /// Wake up every thread that is currently wating on this
    /// condition.
    void notify_all() noexcept;

private:
#ifdef _WIN32
    CONDITION_VARIABLE m_condvar = CONDITION_VARIABLE_INIT;
#else
    pthread_cond_t m_impl;
#endif

    REALM_NORETURN static void init_failed(int);
    REALM_NORETURN static void attr_init_failed(int);
    REALM_NORETURN static void destroy_failed(int) noexcept;
    void handle_wait_error(int error);
};


class RaceDetector {
    std::atomic<bool> busy;

public:
    RaceDetector()
    {
        busy.store(false);
    }
    void enter()
    {
        bool already_busy = busy.exchange(true, std::memory_order_acq_rel);
        if (already_busy)
            throw std::runtime_error("Race detected - critical section busy on entry");
    }
    void leave()
    {
        busy.store(false, std::memory_order_release);
    }
    friend class CriticalSection;
};

class CriticalSection {
    RaceDetector& rd;

public:
    CriticalSection(RaceDetector& race)
        : rd(race)
    {
        rd.enter();
    }
    ~CriticalSection()
    {
        rd.leave();
    }
};

// Implementation:

inline Thread::Thread()
    : m_joinable(false)
{
}

template <class F>
inline Thread::Thread(F func)
    : m_joinable(true)
{
    std::unique_ptr<F> func2(new F(func));       // Throws
    start(&Thread::entry_point<F>, func2.get()); // Throws
    func2.release();
}

inline Thread::Thread(Thread&& thread) noexcept
{
#ifndef _WIN32
    m_id = thread.m_id;
    m_joinable = thread.m_joinable;
    thread.m_joinable = false;
#endif
}

template <class F>
inline void Thread::start(F func)
{
    if (m_joinable)
        std::terminate();
    std::unique_ptr<F> func2(new F(func));       // Throws
    start(&Thread::entry_point<F>, func2.get()); // Throws
    func2.release();
    m_joinable = true;
}

inline Thread::~Thread() noexcept
{
    if (m_joinable)
        REALM_TERMINATE("Destruction of joinable thread");
}

inline bool Thread::joinable() noexcept
{
    return m_joinable;
}

inline void Thread::start(entry_func_type entry_func, void* arg)
{
#ifdef _WIN32
    m_std_thread = std::thread(entry_func, arg);
#else
    const pthread_attr_t* attr = nullptr; // Use default thread attributes
    int r = pthread_create(&m_id, attr, entry_func, arg);
    if (REALM_UNLIKELY(r != 0))
        create_failed(r); // Throws
#endif
}

template <class F>
inline void* Thread::entry_point(void* cookie) noexcept
{
    std::unique_ptr<F> func(static_cast<F*>(cookie));
    try {
        (*func)();
    }
    catch (...) {
        std::terminate();
    }
    return 0;
}


inline Mutex::Mutex()
{
    init_as_regular();
}

inline Mutex::Mutex(process_shared_tag)
{
    bool robust_if_available = false;
    init_as_process_shared(robust_if_available);
}

inline Mutex::~Mutex() noexcept
{
#ifndef _WIN32
    int r = pthread_mutex_destroy(&m_impl);
    if (REALM_UNLIKELY(r != 0))
        destroy_failed(r);
#else
    DeleteCriticalSection(&m_critical_section);
#endif
}

inline void Mutex::init_as_regular()
{
#ifndef _WIN32
    int r = pthread_mutex_init(&m_impl, 0);
    if (REALM_UNLIKELY(r != 0))
        init_failed(r);
#else
    InitializeCriticalSection(&m_critical_section);
#endif
}

inline void Mutex::lock() noexcept
{
#ifdef _WIN32
    EnterCriticalSection(&m_critical_section);
#else
    int r = pthread_mutex_lock(&m_impl);
    if (REALM_LIKELY(r == 0))
        return;
    lock_failed(r);
#endif
}

inline bool Mutex::try_lock() noexcept
{
#ifdef _WIN32
    return TryEnterCriticalSection(&m_critical_section);
#else
    int r = pthread_mutex_trylock(&m_impl);
    if (r == EBUSY) {
        return false;
    }
    else if (r == 0) {
        return true;
    }
    lock_failed(r);
#endif
}

inline void Mutex::unlock() noexcept
{
#ifdef _WIN32
    LeaveCriticalSection(&m_critical_section);
#else
    int r = pthread_mutex_unlock(&m_impl);
    REALM_ASSERT(r == 0);
#endif
}


inline LockGuard::LockGuard(Mutex& m) noexcept
    : m_mutex(m)
{
    m_mutex.lock();
}

inline LockGuard::~LockGuard() noexcept
{
    m_mutex.unlock();
}


inline UniqueLock::UniqueLock(Mutex& m) noexcept
    : m_mutex(&m)
{
    m_mutex->lock();
    m_is_locked = true;
}

inline UniqueLock::UniqueLock(Mutex& m, defer_lock_tag) noexcept
    : m_mutex(&m)
{
    m_is_locked = false;
}

inline UniqueLock::~UniqueLock() noexcept
{
    if (m_is_locked)
        m_mutex->unlock();
}

inline bool UniqueLock::holds_lock() noexcept
{
    return m_is_locked;
}

inline void UniqueLock::lock() noexcept
{
    m_mutex->lock();
    m_is_locked = true;
}

inline void UniqueLock::unlock() noexcept
{
    m_mutex->unlock();
    m_is_locked = false;
}

template <typename TFunc>
inline RobustLockGuard::RobustLockGuard(RobustMutex& m, TFunc func)
    : m_mutex(m)
{
    m_mutex.lock(func);
}

inline RobustLockGuard::~RobustLockGuard() noexcept
{
    m_mutex.unlock();
}


inline RobustMutex::RobustMutex()
    : Mutex(no_init_tag())
{
    bool robust_if_available = true;
    init_as_process_shared(robust_if_available);
}

inline RobustMutex::~RobustMutex() noexcept
{
}

template <class Func>
inline void RobustMutex::lock(Func recover_func)
{
    bool no_thread_has_died = low_level_lock(); // Throws
    if (REALM_LIKELY(no_thread_has_died))
        return;
    try {
        recover_func(); // Throws
        mark_as_consistent();
        // If we get this far, the protected memory has been
        // brought back into a consistent state, and the mutex has
        // been notified about this. This means that we can safely
        // enter the applications critical section.
    }
    catch (...) {
        // Unlocking without first calling mark_as_consistent()
        // means that the mutex enters the "not recoverable"
        // state, which will cause all future attempts at locking
        // to fail.
        unlock();
        throw;
    }
}

template <class Func>
inline bool RobustMutex::try_lock(Func recover_func)
{
    int lock_result = try_low_level_lock(); // Throws
    if (lock_result == 0) return false;
    bool no_thread_has_died = lock_result == 1;
    if (REALM_LIKELY(no_thread_has_died))
        return true;
    try {
        recover_func(); // Throws
        mark_as_consistent();
        // If we get this far, the protected memory has been
        // brought back into a consistent state, and the mutex has
        // been notified aboit this. This means that we can safely
        // enter the applications critical section.
    }
    catch (...) {
        // Unlocking without first calling mark_as_consistent()
        // means that the mutex enters the "not recoverable"
        // state, which will cause all future attempts at locking
        // to fail.
        unlock();
        throw;
    }
    return true;
}

inline void RobustMutex::unlock() noexcept
{
    Mutex::unlock();
}


inline CondVar::CondVar()
{
#ifndef _WIN32
    int r = pthread_cond_init(&m_impl, 0);
    if (REALM_UNLIKELY(r != 0))
        init_failed(r);
#endif
}

inline CondVar::~CondVar() noexcept
{
#ifndef _WIN32
    int r = pthread_cond_destroy(&m_impl);
    if (REALM_UNLIKELY(r != 0))
        destroy_failed(r);
#endif
}

inline void CondVar::wait(LockGuard& l) noexcept
{
#ifdef _WIN32
    SleepConditionVariableCS(&m_condvar, &l.m_mutex.m_critical_section, INFINITE);
#else
    int r = pthread_cond_wait(&m_impl, &l.m_mutex.m_impl);
    if (REALM_UNLIKELY(r != 0))
        REALM_TERMINATE("pthread_cond_wait() failed");
#endif
}

template <class Func>
inline void CondVar::wait(RobustMutex& m, Func recover_func, const struct timespec* tp)
{
    int r;

    if (!tp) {
#ifdef _WIN32
        if (!SleepConditionVariableCS(&m_condvar, &m.m_critical_section, INFINITE))
            r = GetLastError();
        else
            r = 0;
#else
        r = pthread_cond_wait(&m_impl, &m.m_impl);
#endif
    }
    else {
#ifdef _WIN32
        if (!SleepConditionVariableCS(&m_condvar, &m.m_critical_section, tp->tv_sec / 1000)) {
            r = GetLastError();
            if (r == ERROR_TIMEOUT)
                return;
        } else {
            r = 0
        }
#else
        r = pthread_cond_timedwait(&m_impl, &m.m_impl, tp);
        if (r == ETIMEDOUT)
            return;
#endif
    }

    if (REALM_LIKELY(r == 0))
        return;

    handle_wait_error(r);

    try {
        recover_func(); // Throws
        m.mark_as_consistent();
        // If we get this far, the protected memory has been
        // brought back into a consistent state, and the mutex has
        // been notified aboit this. This means that we can safely
        // enter the applications critical section.
    }
    catch (...) {
        // Unlocking without first calling mark_as_consistent()
        // means that the mutex enters the "not recoverable"
        // state, which will cause all future attempts at locking
        // to fail.
        m.unlock();
        throw;
    }
}

inline void CondVar::notify() noexcept
{
#ifdef _WIN32
    WakeConditionVariable(&m_condvar);
#else
    int r = pthread_cond_signal(&m_impl);
    REALM_ASSERT(r == 0);
#endif
}

inline void CondVar::notify_all() noexcept
{
#ifdef _WIN32
    WakeAllConditionVariable(&m_condvar);
#else
    int r = pthread_cond_broadcast(&m_impl);
    REALM_ASSERT(r == 0);
#endif
}

// helpers which can ensure atomic access to memory which has not itself been declared atomic.
// This can be used to e.g. ensure atomic access to members of a vector. Vectors does not
// fully allow atomic members because operations on vector may relocate the underlying memory.
// use with care!
template <typename T>
T load_atomic(T& t_ref, std::memory_order order)
{
    std::atomic<T>* t_ptr = reinterpret_cast<std::atomic<T>*>(&t_ref);
    T t = atomic_load_explicit(t_ptr, order);
    return t;
}

template <typename T>
void store_atomic(T& t_ref, T value, std::memory_order order)
{
    std::atomic<T>* t_ptr = reinterpret_cast<std::atomic<T>*>(&t_ref);
    atomic_store_explicit(t_ptr, value, order);
}


} // namespace util
} // namespace realm

#endif // REALM_UTIL_THREAD_HPP
