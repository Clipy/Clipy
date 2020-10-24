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
#ifndef REALM_UTIL_SYSTEM_PROCESS_HPP
#define REALM_UTIL_SYSTEM_PROCESS_HPP

#include <string>
#include <vector>
#include <map>
#include <thread>

#include <realm/util/logger.hpp>


namespace realm {
namespace util {
namespace sys_proc {

using Environment = std::map<std::string, std::string>;

/// This function is safe to call only when the caller can be sure that there
/// are no threads that modify the environment concurrently.
///
/// When possible, call this function from the main thread before any other
/// threads are created, such as early in `main()`.
Environment copy_local_environment();


struct ExitInfo {
    /// If nonzero, the process was killed by a signal. The value is the
    /// signal number.
    int killed_by_signal = 0;

    /// Zero if the process was killed by a signal, otherwise this is the value
    /// returned by the `main()` function, or passed to `exit()`.
    ///
    /// On a POSIX system, if an error occurs during ::execve(), that is, after
    /// ::fork(), an exit status of 127 will be used (aligned with
    /// ::posix_spawn()).
    int status = 0;

    /// In some cases, ChildHandle::join() will set `signal_name` when it sets
    /// `killed_by_signal` to a non-zero value. In those cases, `signal_name` is
    /// set to point to a null-terminated string specifying the name of the
    /// signal that killed the child process.
    const char* signal_name = nullptr;

    /// Returns true if, and only if both `killed_by_signal` and `status` are
    /// zero.
    explicit operator bool() const noexcept;
};


struct SpawnConfig {
    /// When set to true, the child process will be able to use a
    /// ParentDeathGuard to detect the destruction of the SystemProcess object
    /// in the parent process, even when this happens implicitly due to abrupt
    /// termination of the parent process.
    bool parent_death_guard = false;

    /// If a logger is specified here, the child process will be able to
    /// instantiate a ParentLogger object, and messages logged through that
    /// ParentLogger object will be transported to the parent process and
    /// submitted to the logger pointed to by `logger`. The specified logger is
    /// guaranteed to only be accessed while ChildHandle::join() is executing,
    /// and only by the thread that executes ChildHandle::join(). See
    /// ParentLogger for further details.
    Logger* logger = nullptr;
};


class ChildHandle {
public:
    /// Wait for the child process to exit.
    ///
    /// If a logger was passed to spawn() (SpawnConfig::logger), then this
    /// function will also transport log messages from the child to the parent
    /// process while waiting for the child process to exit. See ParentLogger
    /// for details.
    ExitInfo join();

    ChildHandle(ChildHandle&&) noexcept;
    ~ChildHandle() noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    ChildHandle(Impl*) noexcept;

    friend ChildHandle spawn(const std::string&, const std::vector<std::string>&,
                             const Environment&, const SpawnConfig&);
};


/// Returns true if, and only if the spawn() functions work on this platform. If
/// this function returns false, the spawn() functions will throw.
bool is_spawn_supported() noexcept;


//@{
/// Spawn a child process.
ChildHandle spawn(const std::string& path, const std::vector<std::string>& args = {},
                  const Environment& = {});
ChildHandle spawn(const std::string& path, const std::vector<std::string>& args,
                  const Environment&, const SpawnConfig&);
//@}


/// Force a child process to terminate immediately if the parent process is
/// terminated, or if the parent process destroys the ChildHandle object
/// representing the child process.
///
/// If a child process instantiates an object of this type, and keeps it alive,
/// and the child process was spawned with support for detection of parent
/// termination (SpawnConfig::parent_death_guard), then the child process will
/// be killed shortly after the parent destroys its ChildHandle object, even
/// when this happens implicitly due to abrupt termination of the parent
/// process.
///
/// If a child process instantiates an object of this type, that object must be
/// instantiated by the main thread, and before any other thread is spawned in
/// the child process.
///
/// In order for the guard to have the intended effect, it must be instantiated
/// immediately in the child process, and be kept alive for as long as the child
/// process is running.
class ParentDeathGuard {
public:
    ParentDeathGuard();
    ~ParentDeathGuard() noexcept;

private:
    std::thread m_thread;
    int m_stop_pipe_write = -1;
};


/// A logger that can transport log messages from the child to the parent
/// process.
///
/// If the parent process specifies a logger when spawning a child process
/// (SpawnConfig::logger), then that child process can instantiate a
/// ParentLogger object, and messages logged through it will be transported to
/// the parent process. While the parent process is executing
/// ChildHandle::join(), those messages will be written to the logger specified
/// by the parent process.
///
/// If a child process instantiates an object of this type, that object must be
/// instantiated by the main thread, and before any other thread is spawned in
/// the child process.
///
/// At most one ParentLogger object may be instantiated per child process.
///
/// This logger is **not** thread-safe.
class ParentLogger : public RootLogger {
public:
    ParentLogger();
    ~ParentLogger() noexcept;

protected:
    void do_log(Level, std::string) override final;

private:
    int m_pipe_write = -1;
};




// Implementation

inline ExitInfo::operator bool() const noexcept
{
    return (killed_by_signal == 0 && status == 0);
}

inline ChildHandle spawn(const std::string& path, const std::vector<std::string>& args,
                         const Environment& env)
{
    return spawn(path, args, env, SpawnConfig{}); // Throws
}

} // namespace sys_proc
} // namespace util
} // namespace realm

#endif // REALM_UTIL_SYSTEM_PROCESS_HPP
