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

#ifndef REALM_UTIL_INTERPROCESS_MUTEX
#define REALM_UTIL_INTERPROCESS_MUTEX

#include <realm/util/features.h>
#include <realm/util/thread.hpp>
#include <realm/util/file.hpp>
#include <realm/utilities.hpp>
#include <mutex>
#include <map>
#include <iostream>

// Enable this only on platforms where it might be needed
#if REALM_PLATFORM_APPLE || REALM_ANDROID
#define REALM_ROBUST_MUTEX_EMULATION
#endif

namespace realm {
namespace util {

// fwd decl to support friend decl below
class InterprocessCondVar;


/// Emulation of a Robust Mutex.
/// A Robust Mutex is an interprocess mutex which will automatically
/// release any locks held by a process when it crashes. Contrary to
/// Posix robust mutexes, this robust mutex is not capable of informing
/// participants that they have been granted a lock after a crash of
/// the process holding it (though it could be added if needed).

class InterprocessMutex {
public:
    InterprocessMutex();
    ~InterprocessMutex() noexcept;

    // Disable copying. Copying a locked Mutex will create a scenario
    // where the same file descriptor will be locked once but unlocked twice.
    InterprocessMutex(const InterprocessMutex&) = delete;
    InterprocessMutex& operator=(const InterprocessMutex&) = delete;

#if defined(REALM_ROBUST_MUTEX_EMULATION) || defined(_WIN32)
    struct SharedPart {
    };
#else
    using SharedPart = RobustMutex;
#endif

    /// You need to bind the emulation to a SharedPart in shared/mmapped memory.
    /// The SharedPart is assumed to have been initialized (possibly by another process)
    /// elsewhere.
    void set_shared_part(SharedPart& shared_part, const std::string& path, const std::string& mutex_name);
    void set_shared_part(SharedPart& shared_part, File&& lock_file);

    /// Destroy shared object. Potentially release system resources. Caller must
    /// ensure that the shared_part is not in use at the point of call.
    void release_shared_part();

    /// Lock the mutex. If the mutex is already locked, wait for it to be unlocked.
    void lock();

    /// Non-blocking attempt to lock the mutex. Returns true if the lock is obtained.
    /// If the lock can not be obtained return false immediately.
    bool try_lock();

    /// Unlock the mutex
    void unlock();

    /// Attempt to check if the mutex is valid (only relevant if not emulating)
    bool is_valid() noexcept;

    static bool is_robust_on_this_platform()
    {
#ifdef REALM_ROBUST_MUTEX_EMULATION
        return true; // we're faking it!
#else
        return RobustMutex::is_robust_on_this_platform();
#endif
    }

private:
#ifdef REALM_ROBUST_MUTEX_EMULATION
    struct LockInfo {
        File m_file;
        Mutex m_local_mutex;
        LockInfo() {}
        ~LockInfo() noexcept;
        // Disable copying.
        LockInfo(const LockInfo&) = delete;
        LockInfo& operator=(const LockInfo&) = delete;
    };
    /// InterprocessMutex created on the same file (same inode on POSIX) share the same LockInfo.
    /// LockInfo will be saved in a static map as a weak ptr and use the UniqueID as the key.
    /// Operations on the map need to be protected by s_mutex
    static std::map<File::UniqueID, std::weak_ptr<LockInfo>>* s_info_map;
    static Mutex* s_mutex;
    /// We manually initialize these static variables when first needed,
    /// creating them on the heap so that they last for the entire lifetime
    /// of the process. The destructor of these is never called; the
    /// process will clean up their memory when exiting. It is not enough
    /// to count instances of InterprocessMutex and clean up these statics when
    /// the count reaches zero because the program can create more
    /// InterprocessMutex instances before the process ends, so we really need
    /// these variables for the entire lifetime of the process.
    static std::once_flag s_init_flag;
    static void initialize_statics();

    /// Only used for release_shared_part
    std::string m_filename;
    File::UniqueID m_fileuid;
    std::shared_ptr<LockInfo> m_lock_info;

    /// Free the lock info hold by this instance.
    /// If it is the last reference, underly resources will be freed as well.
    void free_lock_info();
#else
    SharedPart* m_shared_part = nullptr;

#ifdef _WIN32
    HANDLE m_handle = 0;
#endif

#endif
    friend class InterprocessCondVar;
};

inline InterprocessMutex::InterprocessMutex()
{
#ifdef REALM_ROBUST_MUTEX_EMULATION
    std::call_once(s_init_flag, initialize_statics);
#endif
}

inline InterprocessMutex::~InterprocessMutex() noexcept
{
#ifdef _WIN32
    if (m_handle) {
        bool b = CloseHandle(m_handle);
        REALM_ASSERT_RELEASE(b);
    }
#endif

#ifdef REALM_ROBUST_MUTEX_EMULATION
    free_lock_info();
#endif
}

#ifdef REALM_ROBUST_MUTEX_EMULATION
inline InterprocessMutex::LockInfo::~LockInfo() noexcept
{
    if (m_file.is_attached()) {
        m_file.close();
    }
}

inline void InterprocessMutex::free_lock_info()
{
    // It has not been initialized yet.
    if (!m_lock_info)
        return;

    std::lock_guard<Mutex> guard(*s_mutex);

    m_lock_info.reset();
    if ((*s_info_map)[m_fileuid].expired()) {
        s_info_map->erase(m_fileuid);
    }
    m_filename.clear();
}

inline void InterprocessMutex::initialize_statics()
{
    s_mutex = new Mutex();
    s_info_map = new std::map<File::UniqueID, std::weak_ptr<LockInfo>>();
}
#endif

inline void InterprocessMutex::set_shared_part(SharedPart& shared_part, const std::string& path,
                                               const std::string& mutex_name)
{
#ifdef REALM_ROBUST_MUTEX_EMULATION
    static_cast<void>(shared_part);

    free_lock_info();

    m_filename = path + "." + mutex_name + ".mx";

    std::lock_guard<Mutex> guard(*s_mutex);

    // Try to get the file uid if the file exists
    if (File::get_unique_id(m_filename, m_fileuid)) {
        auto result = s_info_map->find(m_fileuid);
        if (result != s_info_map->end()) {
            // File exists and the lock info has been created in the map.
            m_lock_info = result->second.lock();
            return;
        }
    }

    // LockInfo has not been created yet.
    m_lock_info = std::make_shared<LockInfo>();
    // Always use mod_Write to open file and retreive the uid in case other process
    // deletes the file.
    m_lock_info->m_file.open(m_filename, File::mode_Write);
    m_fileuid = m_lock_info->m_file.get_unique_id();

    (*s_info_map)[m_fileuid] = m_lock_info;
#elif defined(_WIN32)
    if (m_handle) {
        bool b = CloseHandle(m_handle);
        REALM_ASSERT_RELEASE(b);
    }
    // replace backslashes because they're significant in object namespace names
    std::string path_escaped = path;
    std::replace(path_escaped.begin(), path_escaped.end(), '\\', '/');
    std::string name = "Local\\realm_named_intermutex_" + path_escaped + mutex_name;

    std::wstring wname(name.begin(), name.end());
    m_handle = CreateMutexW(0, false, wname.c_str());
    if (!m_handle) {
        throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Error opening mutex");
    }
#else
    m_shared_part = &shared_part;
    static_cast<void>(path);
    static_cast<void>(mutex_name);
#endif
}

inline void InterprocessMutex::set_shared_part(SharedPart& shared_part, File&& lock_file)
{
#ifdef REALM_ROBUST_MUTEX_EMULATION
    static_cast<void>(shared_part);

    free_lock_info();

    std::lock_guard<Mutex> guard(*s_mutex);

    m_fileuid = lock_file.get_unique_id();
    auto result = s_info_map->find(m_fileuid);
    if (result == s_info_map->end()) {
        m_lock_info = std::make_shared<LockInfo>();
        m_lock_info->m_file = std::move(lock_file);
        (*s_info_map)[m_fileuid] = m_lock_info;
    }
    else {
        // File exists and the lock info has been created in the map.
        m_lock_info = result->second.lock();
        lock_file.close();
    }
#else
    m_shared_part = &shared_part;
    static_cast<void>(lock_file);
#endif
}

inline void InterprocessMutex::release_shared_part()
{
#ifdef REALM_ROBUST_MUTEX_EMULATION
    if (!m_filename.empty())
        File::try_remove(m_filename);

    free_lock_info();
#else
    m_shared_part = nullptr;
#endif
}

inline void InterprocessMutex::lock()
{
#ifdef REALM_ROBUST_MUTEX_EMULATION
    std::unique_lock<Mutex> mutex_lock(m_lock_info->m_local_mutex);
    m_lock_info->m_file.lock_exclusive();
    mutex_lock.release();
#else

#ifdef _WIN32
    DWORD d = WaitForSingleObject(m_handle, INFINITE);
    REALM_ASSERT_RELEASE(d != WAIT_FAILED);
#else
    REALM_ASSERT(m_shared_part);
    m_shared_part->lock([]() {});
#endif
#endif
}

inline bool InterprocessMutex::try_lock()
{
#ifdef REALM_ROBUST_MUTEX_EMULATION
    std::unique_lock<Mutex> mutex_lock(m_lock_info->m_local_mutex, std::try_to_lock_t());
    if (!mutex_lock.owns_lock()) {
        return false;
    }
    bool success = m_lock_info->m_file.try_lock_exclusive();
    if (success) {
        mutex_lock.release();
        return true;
    }
    else {
        return false;
    }
#else

#ifdef _WIN32
    DWORD ret = WaitForSingleObject(m_handle, 0);
    REALM_ASSERT_RELEASE(ret != WAIT_FAILED);

    if (ret == WAIT_OBJECT_0) {
        return true;
    }
    else {
        return false;
    }
#else
    REALM_ASSERT(m_shared_part);
    return m_shared_part->try_lock([]() {});
#endif
#endif
}


inline void InterprocessMutex::unlock()
{
#ifdef REALM_ROBUST_MUTEX_EMULATION
    m_lock_info->m_file.unlock();
    m_lock_info->m_local_mutex.unlock();
#else
#ifdef _WIN32
    bool b = ReleaseMutex(m_handle);
    REALM_ASSERT_RELEASE(b);
#else
    REALM_ASSERT(m_shared_part);
    m_shared_part->unlock();
#endif
#endif
}


inline bool InterprocessMutex::is_valid() noexcept
{
#ifdef REALM_ROBUST_MUTEX_EMULATION
    return true;
#elif defined(_WIN32)
    // There is no safe way of testing if the m_handle mutex handle is valid on Windows, without having bad side effects
    // for the cases where it is indeed invalid. If m_handle contains an arbitrary value, it might by coincidence be equal
    // to a real live handle of another kind. This excludes a try_lock implementation and many other ideas.
    return true;
#else
    REALM_ASSERT(m_shared_part);
    return m_shared_part->is_valid();
#endif
}


} // namespace util
} // namespace realm

#endif // #ifndef REALM_UTIL_INTERPROCESS_MUTEX
