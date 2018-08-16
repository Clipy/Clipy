/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2015] Realm Inc
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of Realm Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Realm Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Realm Incorporated.
 *
 **************************************************************************/
#ifndef REALM_UTIL_THREAD_EXEC_GUARD_HPP
#define REALM_UTIL_THREAD_EXEC_GUARD_HPP

#include <exception>
#include <utility>
#include <string>

#include <realm/util/thread.hpp>
#include <realm/util/signal_blocker.hpp>


namespace realm {
namespace util {

/// Execute a `R::run()` using a managed thread.
///
/// \tparam R The type of the runnable object. This type must satisfy the
/// requirements of the Runnable concept. See ThreadExecGuardWithParent.
template<class R> class ThreadExecGuard {
public:
    explicit ThreadExecGuard(R& runnable);

    ThreadExecGuard(ThreadExecGuard&&) = default;

    /// If start() or start_with_signals_blocked() was successfully executed,
    /// and stop_and_rethrow() has not been called, call `R::stop()`, and then
    /// wait for the thread to terminate (join).
    ~ThreadExecGuard() noexcept = default;

    // @{
    /// Launch a thread and make it execute `R::run()` of the associated
    /// "runnable" object.
    ///
    /// At most one of these functions are allowed to be called on a particular
    /// guard object, and it must only be called once.
    void start();
    void start(const std::string& thread_name);
    void start_with_signals_blocked();
    void start_with_signals_blocked(const std::string& thread_name);
    // @}

    /// If start() or start_with_signals_blocked() was successfully executed,
    /// call `R::stop()`, wait for the thread to terminate (join), and then, if
    /// an exception was thrown by `R::run()`, rethrow it.
    void stop_and_rethrow();

private:
    struct State {
        R& runnable;
        util::Thread thread;
        std::exception_ptr exception;
        State(R&) noexcept;
        ~State() noexcept;
        void start(const std::string* thread_name);
        void stop_and_rethrow();
    };

    std::unique_ptr<State> m_state;
};


/// Execute a `R::run()` using a managed thread.
///
/// \tparam R The type of the runnable object. This type must satisfy the
/// requirements of the Runnable concept. See below.
///
/// \tparam P The type of the object representing the parent thread. This type
/// must satisfy the requirements of the Stoppable concept. See below.
///
/// A type satisfies the requirements of the *Stoppable* concept, if
///  - it has a nonthrowing member function named `stop()`, and
///  - `stop()` is thread-safe, and
///  - `stop()` is idempotent (can be called multiple times).
///
/// A type satisfies the requirements of the *Runnable* concept, if
///  - it satisfies the requirements of the Stoppable concept, and
///  - it has a member function named `run()`, and
///  - `run()` will stop executing within a reasonable amount of time after
///    `stop()` has been called.
///
template<class R, class P> class ThreadExecGuardWithParent {
public:
    explicit ThreadExecGuardWithParent(R& runnable, P& parent);

    ThreadExecGuardWithParent(ThreadExecGuardWithParent&&) = default;

    /// If start() or start_with_signals_blocked() was successfully executed,
    /// and stop_and_rethrow() has not been called, call `R::stop()`, and then
    /// wait for the thread to terminate (join).
    ~ThreadExecGuardWithParent() noexcept = default;

    // @{
    /// Launch a thread and make it execute `R::run()` of the associated
    /// "runnable" object.
    ///
    /// If `R::run()` throws, call `P::stop()` on the specified parent.
    ///
    /// At most one of these functions are allowed to be called on a particular
    /// guard object, and it must only be called once.
    void start();
    void start(const std::string& thread_name);
    void start_with_signals_blocked();
    void start_with_signals_blocked(const std::string& thread_name);
    // @}

    /// If start() or start_with_signals_blocked() was successfully executed,
    /// call `R::stop()`, wait for the thread to terminate (join), and then, if
    /// an exception was thrown by `R::run()`, rethrow it.
    void stop_and_rethrow();

private:
    struct State {
        R& runnable;
        P& parent;
        util::Thread thread;
        std::exception_ptr exception;
        State(R&, P&) noexcept;
        ~State() noexcept;
        void start(const std::string* thread_name);
        void stop_and_rethrow();
    };

    std::unique_ptr<State> m_state;
};


template<class R> ThreadExecGuard<R> make_thread_exec_guard(R& runnable);

template<class R, class P>
ThreadExecGuardWithParent<R, P> make_thread_exec_guard(R& runnable, P& parent);




// Implementation

template<class R> inline ThreadExecGuard<R>::ThreadExecGuard(R& runnable) :
    m_state{std::make_unique<State>(runnable)} // Throws
{
}

template<class R> inline void ThreadExecGuard<R>::start()
{
    const std::string* thread_name = nullptr;
    m_state->start(thread_name); // Throws
}

template<class R> inline void ThreadExecGuard<R>::start(const std::string& thread_name)
{
    m_state->start(&thread_name); // Throws
}

template<class R> inline void ThreadExecGuard<R>::start_with_signals_blocked()
{
    SignalBlocker sb;
    const std::string* thread_name = nullptr;
    m_state->start(thread_name); // Throws
}

template<class R>
inline void ThreadExecGuard<R>::start_with_signals_blocked(const std::string& thread_name)
{
    SignalBlocker sb;
    m_state->start(&thread_name); // Throws
}

template<class R> inline void ThreadExecGuard<R>::stop_and_rethrow()
{
    m_state->stop_and_rethrow(); // Throws
}

template<class R> inline ThreadExecGuard<R>::State::State(R& r) noexcept :
    runnable{r}
{
}

template<class R> inline ThreadExecGuard<R>::State::~State() noexcept
{
    if (thread.joinable()) {
        runnable.stop();
        thread.join();
    }
}

template<class R> inline void ThreadExecGuard<R>::State::start(const std::string* thread_name)
{
    bool set_thread_name = false;
    std::string thread_name_2;
    if (thread_name) {
        set_thread_name = true;
        thread_name_2 = *thread_name; // Throws (copy)
    }
    auto run = [this, set_thread_name, thread_name=std::move(thread_name_2)]() noexcept {
        try {
            if (set_thread_name)
                util::Thread::set_name(thread_name); // Throws
            runnable.run(); // Throws
        }
        catch (...) {
            exception = std::current_exception();
        }
    };
    thread.start(std::move(run)); // Throws
}

template<class R> inline void ThreadExecGuard<R>::State::stop_and_rethrow()
{
    if (thread.joinable()) {
        runnable.stop();
        thread.join();
        if (exception)
            std::rethrow_exception(exception); // Throws
    }
}

template<class R, class P>
inline ThreadExecGuardWithParent<R, P>::ThreadExecGuardWithParent(R& runnable, P& parent) :
    m_state{std::make_unique<State>(runnable, parent)} // Throws
{
}

template<class R, class P> inline void ThreadExecGuardWithParent<R, P>::start()
{
    const std::string* thread_name = nullptr;
    m_state->start(thread_name); // Throws
}

template<class R, class P>
inline void ThreadExecGuardWithParent<R, P>::start(const std::string& thread_name)
{
    m_state->start(&thread_name); // Throws
}

template<class R, class P> inline void ThreadExecGuardWithParent<R, P>::start_with_signals_blocked()
{
    SignalBlocker sb;
    const std::string* thread_name = nullptr;
    m_state->start(thread_name); // Throws
}

template<class R, class P>
inline void ThreadExecGuardWithParent<R, P>::start_with_signals_blocked(const std::string& thread_name)
{
    SignalBlocker sb;
    m_state->start(&thread_name); // Throws
}

template<class R, class P> inline void ThreadExecGuardWithParent<R, P>::stop_and_rethrow()
{
    m_state->stop_and_rethrow(); // Throws
}

template<class R, class P>
inline ThreadExecGuardWithParent<R, P>::State::State(R& r, P& p) noexcept :
    runnable{r},
    parent{p}
{
}

template<class R, class P> inline ThreadExecGuardWithParent<R, P>::State::~State() noexcept
{
    if (thread.joinable()) {
        runnable.stop();
        thread.join();
    }
}

template<class R, class P>
inline void ThreadExecGuardWithParent<R, P>::State::start(const std::string* thread_name)
{
    bool set_thread_name = false;
    std::string thread_name_2;
    if (thread_name) {
        set_thread_name = true;
        thread_name_2 = *thread_name; // Throws (copy)
    }
    auto run = [this, set_thread_name, thread_name=std::move(thread_name_2)]() noexcept {
        try {
            if (set_thread_name)
                util::Thread::set_name(thread_name); // Throws
            runnable.run(); // Throws
        }
        catch (...) {
            exception = std::current_exception();
            parent.stop();
        }
    };
    thread.start(std::move(run)); // Throws
}

template<class R, class P> inline void ThreadExecGuardWithParent<R, P>::State::stop_and_rethrow()
{
    if (thread.joinable()) {
        runnable.stop();
        thread.join();
        if (exception)
            std::rethrow_exception(exception); // Throws
    }
}

template<class R> inline ThreadExecGuard<R> make_thread_exec_guard(R& runnable)
{
    return ThreadExecGuard<R>{runnable}; // Throws
}

template<class R, class P>
inline ThreadExecGuardWithParent<R, P> make_thread_exec_guard(R& runnable, P& parent)
{
    return ThreadExecGuardWithParent<R, P>{runnable, parent}; // Throws
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_THREAD_EXEC_GUARD_HPP
