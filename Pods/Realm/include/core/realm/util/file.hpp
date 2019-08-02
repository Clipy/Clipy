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

#ifndef REALM_UTIL_FILE_HPP
#define REALM_UTIL_FILE_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <functional>
#include <stdexcept>
#include <string>
#include <streambuf>
#include <iostream>

#ifndef _WIN32
#include <dirent.h> // POSIX.1-2001
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1900 // compiling with at least Visual Studio 2015
#include <experimental/filesystem>
namespace std {
    namespace filesystem = std::experimental::filesystem::v1;
}
#define REALM_HAVE_STD_FILESYSTEM 1
#else
#define REALM_HAVE_STD_FILESYSTEM 0
#endif

#include <realm/utilities.hpp>
#include <realm/util/backtrace.hpp>
#include <realm/util/features.h>
#include <realm/util/assert.hpp>
#include <realm/util/safe_int_ops.hpp>


namespace realm {
namespace util {

class EncryptedFileMapping;

/// Create the specified directory in the file system.
///
/// \throw File::AccessError If the directory could not be created. If
/// the reason corresponds to one of the exception types that are
/// derived from File::AccessError, the derived exception type is
/// thrown (as long as the underlying system provides the information
/// to unambiguously distinguish that particular reason).
void make_dir(const std::string& path);

/// Same as make_dir() except that this one returns false, rather than throwing
/// an exception, if the specified directory already existed. If the directory
// did not already exist and was newly created, this returns true.
bool try_make_dir(const std::string& path);

/// Remove the specified empty directory path from the file system. It is an
/// error if the specified path is not a directory, or if it is a nonempty
/// directory. In so far as the specified path is a directory, std::remove(const
/// char*) is equivalent to this function.
///
/// \throw File::AccessError If the directory could not be removed. If the
/// reason corresponds to one of the exception types that are derived from
/// File::AccessError, the derived exception type is thrown (as long as the
/// underlying system provides the information to unambiguously distinguish that
/// particular reason).
void remove_dir(const std::string& path);

/// Same as remove_dir() except that this one returns false, rather
/// than throwing an exception, if the specified directory did not
/// exist. If the directory did exist, and was deleted, this function
/// returns true.
bool try_remove_dir(const std::string& path);

/// Remove the specified directory after removing all its contents. Files
/// (nondirectory entries) will be removed as if by a call to File::remove(),
/// and empty directories as if by a call to remove_dir().
///
/// \throw File::AccessError If removal of the directory, or any of its contents
/// fail.
///
/// remove_dir_recursive() assumes that no other process or thread is making
/// simultaneous changes in the directory.
void remove_dir_recursive(const std::string& path);

/// Same as remove_dir_recursive() except that this one returns false, rather
/// than throwing an exception, if the specified directory did not
/// exist. If the directory did exist, and was deleted, this function
/// returns true.
///
/// try_remove_dir_recursive() assumes that no other process or thread is making
/// simultaneous changes in the directory.
bool try_remove_dir_recursive(const std::string& path);

/// Create a new unique directory for temporary files. The absolute
/// path to the new directory is returned without a trailing slash.
std::string make_temp_dir();

size_t page_size();


/// This class provides a RAII abstraction over the concept of a file
/// descriptor (or file handle).
///
/// Locks are automatically and immediately released when the File
/// instance is closed.
///
/// You can use CloseGuard and UnlockGuard to acheive exception-safe
/// closing or unlocking prior to the File instance being detroyed.
///
/// A single File instance must never be accessed concurrently by
/// multiple threads.
///
/// You can write to a file via an std::ostream as follows:
///
/// \code{.cpp}
///
///   File::Streambuf my_streambuf(&my_file);
///   std::ostream out(&my_strerambuf);
///   out << 7945.9;
///
/// \endcode
class File {
public:
    enum Mode {
        mode_Read,   ///< access_ReadOnly,  create_Never             (fopen: rb)
        mode_Update, ///< access_ReadWrite, create_Never             (fopen: rb+)
        mode_Write,  ///< access_ReadWrite, create_Auto, flag_Trunc  (fopen: wb+)
        mode_Append  ///< access_ReadWrite, create_Auto, flag_Append (fopen: ab+)
    };

    /// Equivalent to calling open(const std::string&, Mode) on a
    /// default constructed instance.
    explicit File(const std::string& path, Mode = mode_Read);

    /// Create an instance that is not initially attached to an open
    /// file.
    File() noexcept;

    ~File() noexcept;

    File(File&&) noexcept;
    File& operator=(File&&) noexcept;

    // Disable copying by l-value. Copying an open file will create a scenario
    // where the same file descriptor will be opened once but closed twice.
    File(const File&) = delete;
    File& operator=(const File&) = delete;

    /// Calling this function on an instance that is already attached
    /// to an open file has undefined behavior.
    ///
    /// \throw AccessError If the file could not be opened. If the
    /// reason corresponds to one of the exception types that are
    /// derived from AccessError, the derived exception type is thrown
    /// (as long as the underlying system provides the information to
    /// unambiguously distinguish that particular reason).
    void open(const std::string& path, Mode = mode_Read);

    /// This function is idempotent, that is, it is valid to call it
    /// regardless of whether this instance currently is attached to
    /// an open file.
    void close() noexcept;

    /// Check whether this File instance is currently attached to an
    /// open file.
    bool is_attached() const noexcept;

    enum AccessMode {
        access_ReadOnly,
        access_ReadWrite,
    };

    enum CreateMode {
        create_Auto,  ///< Create the file if it does not already exist.
        create_Never, ///< Fail if the file does not already exist.
        create_Must   ///< Fail if the file already exists.
    };

    enum {
        flag_Trunc = 1, ///< Truncate the file if it already exists.
        flag_Append = 2 ///< Move to end of file before each write.
    };

    /// See open(const std::string&, Mode).
    ///
    /// Specifying access_ReadOnly together with a create mode that is
    /// not create_Never, or together with a non-zero \a flags
    /// argument, results in undefined behavior. Specifying flag_Trunc
    /// together with create_Must results in undefined behavior.
    void open(const std::string& path, AccessMode, CreateMode, int flags);

    /// Same as open(path, access_ReadWrite, create_Auto, 0), except
    /// that this one returns an indication of whether a new file was
    /// created, or an existing file was opened.
    void open(const std::string& path, bool& was_created);

    /// Read data into the specified buffer and return the number of
    /// bytes read. If the returned number of bytes is less than \a
    /// size, then the end of the file has been reached.
    ///
    /// Calling this function on an instance, that is not currently
    /// attached to an open file, has undefined behavior.
    size_t read(char* data, size_t size);
    static size_t read_static(FileDesc fd, char* data, size_t size);

    /// Write the specified data to this file.
    ///
    /// Calling this function on an instance, that is not currently
    /// attached to an open file, has undefined behavior.
    ///
    /// Calling this function on an instance, that was opened in
    /// read-only mode, has undefined behavior.
    void write(const char* data, size_t size);
    static void write_static(FileDesc fd, const char* data, size_t size);

    // Tells current file pointer of fd
    static uint64_t get_file_pos(FileDesc fd);

    /// Calls write(s.data(), s.size()).
    void write(const std::string& s)
    {
        write(s.data(), s.size());
    }

    /// Calls read(data, N).
    template <size_t N>
    size_t read(char (&data)[N])
    {
        return read(data, N);
    }

    /// Calls write(data(), N).
    template <size_t N>
    void write(const char (&data)[N])
    {
        write(data, N);
    }

    /// Plays the same role as off_t in POSIX
    typedef int_fast64_t SizeType;

    /// Calling this function on an instance that is not attached to
    /// an open file has undefined behavior.
    SizeType get_size() const;
    static SizeType get_size_static(FileDesc fd);

    /// If this causes the file to grow, then the new section will
    /// have undefined contents. Setting the size with this function
    /// does not necessarily allocate space on the target device. If
    /// you want to ensure allocation, call alloc(). Calling this
    /// function will generally affect the read/write offset
    /// associated with this File instance.
    ///
    /// Calling this function on an instance that is not attached to
    /// an open file has undefined behavior. Calling this function on
    /// a file that is opened in read-only mode, is an error.
    void resize(SizeType);

    /// Same effect as prealloc_if_supported(original_size, new_size);
    ///
    /// The downside is that this function is not guaranteed to have
    /// atomic behaviour on all systems, that is, two processes, or
    /// two threads should never call this function concurrently for
    /// the same underlying file even though they access the file
    /// through distinct File instances.
    ///
    /// \sa prealloc_if_supported()
    void prealloc(size_t new_size);

    /// When supported by the system, allocate space on the target
    /// device for the specified region of the file. If the region
    /// extends beyond the current end of the file, the file size is
    /// increased as necessary.
    ///
    /// On systems that do not support this operation, this function
    /// has no effect. You may call is_prealloc_supported() to
    /// determine if it is supported on your system.
    ///
    /// Calling this function on an instance, that is not attached to
    /// an open file, has undefined behavior. Calling this function on
    /// a file, that is opened in read-only mode, is an error.
    ///
    /// This function is guaranteed to have atomic behaviour, that is,
    /// there is never any risk of the file size being reduced even
    /// with concurrently executing invocations.
    ///
    /// \sa prealloc()
    /// \sa is_prealloc_supported()
    bool prealloc_if_supported(SizeType offset, size_t size);

    /// See prealloc_if_supported().
    static bool is_prealloc_supported();

    /// Reposition the read/write offset of this File
    /// instance. Distinct File instances have separate independent
    /// offsets, as long as the cucrrent process is not forked.
    void seek(SizeType);
    static void seek_static(FileDesc, SizeType);

    /// Flush in-kernel buffers to disk. This blocks the caller until the
    /// synchronization operation is complete. On POSIX systems this function
    /// calls `fsync()`. On Apple platforms if calls `fcntl()` with command
    /// `F_FULLFSYNC`.
    void sync();

    /// Place an exclusive lock on this file. This blocks the caller
    /// until all other locks have been released.
    ///
    /// Locks acquired on distinct File instances have fully recursive
    /// behavior, even if they are acquired in the same process (or
    /// thread) and are attached to the same underlying file.
    ///
    /// Calling this function on an instance that is not attached to
    /// an open file, or on an instance that is already locked has
    /// undefined behavior.
    void lock_exclusive();

    /// Place an shared lock on this file. This blocks the caller
    /// until all other exclusive locks have been released.
    ///
    /// Locks acquired on distinct File instances have fully recursive
    /// behavior, even if they are acquired in the same process (or
    /// thread) and are attached to the same underlying file.
    ///
    /// Calling this function on an instance that is not attached to
    /// an open file, or on an instance that is already locked has
    /// undefined behavior.
    void lock_shared();

    /// Non-blocking version of lock_exclusive(). Returns true iff it
    /// succeeds.
    bool try_lock_exclusive();

    /// Non-blocking version of lock_shared(). Returns true iff it
    /// succeeds.
    bool try_lock_shared();

    /// Release a previously acquired lock on this file. This function
    /// is idempotent.
    void unlock() noexcept;

    /// Set the encryption key used for this file. Must be called before any
    /// mappings are created or any data is read from or written to the file.
    ///
    /// \param key A 64-byte encryption key, or null to disable encryption.
    void set_encryption_key(const char* key);

    /// Get the encryption key set by set_encryption_key(),
    /// null_ptr if no key set.
    const char* get_encryption_key();
    enum {
        /// If possible, disable opportunistic flushing of dirted
        /// pages of a memory mapped file to physical medium. On some
        /// systems this cannot be disabled. On other systems it is
        /// the default behavior. An explicit call to sync_map() will
        /// flush the buffers regardless of whether this flag is
        /// specified or not.
        map_NoSync = 1
    };

    /// Map this file into memory. The file is mapped as shared
    /// memory. This allows two processes to interact under exatly the
    /// same rules as applies to the interaction via regular memory of
    /// multiple threads inside a single process.
    ///
    /// This File instance does not need to remain in existence after
    /// the mapping is established.
    ///
    /// Multiple concurrent mappings may be created from the same File
    /// instance.
    ///
    /// Specifying access_ReadWrite for a file that is opened in
    /// read-only mode, is an error.
    ///
    /// Calling this function on an instance that is not attached to
    /// an open file, or one that is attached to an empty file has
    /// undefined behavior.
    ///
    /// Calling this function with a size that is greater than the
    /// size of the file has undefined behavior.
    void* map(AccessMode, size_t size, int map_flags = 0, size_t offset = 0) const;

    /// The same as unmap(old_addr, old_size) followed by map(a,
    /// new_size, map_flags), but more efficient on some systems.
    ///
    /// The old address range must have been acquired by a call to
    /// map() or remap() on this File instance, the specified access
    /// mode and flags must be the same as the ones specified
    /// previously, and this File instance must not have been reopend
    /// in the meantime. Failing to adhere to these rules will result
    /// in undefined behavior.
    ///
    /// If this function throws, the old address range will remain
    /// mapped.
    void* remap(void* old_addr, size_t old_size, AccessMode a, size_t new_size, int map_flags = 0,
                size_t file_offset = 0) const;

#if REALM_ENABLE_ENCRYPTION
    void* map(AccessMode, size_t size, EncryptedFileMapping*& mapping, int map_flags = 0, size_t offset = 0) const;
#endif
    /// Unmap the specified address range which must have been
    /// previously returned by map().
    static void unmap(void* addr, size_t size) noexcept;

    /// Flush in-kernel buffers to disk. This blocks the caller until
    /// the synchronization operation is complete. The specified
    /// address range must be (a subset of) one that was previously returned by
    /// map().
    static void sync_map(FileDesc fd, void* addr, size_t size);

    /// Check whether the specified file or directory exists. Note
    /// that a file or directory that resides in a directory that the
    /// calling process has no access to, will necessarily be reported
    /// as not existing.
    static bool exists(const std::string& path);

    /// Check whether the specified path exists and refers to a directory. If
    /// the referenced file system object resides in an inaccessible directory,
    /// this function returns false.
    static bool is_dir(const std::string& path);

    /// Remove the specified file path from the file system. It is an error if
    /// the specified path is a directory. If the specified file is a symbolic
    /// link, the link is removed, leaving the liked file intact. In so far as
    /// the specified path is not a directory, std::remove(const char*) is
    /// equivalent to this function.
    ///
    /// The specified file must not be open by the calling process. If
    /// it is, this function has undefined behaviour. Note that an
    /// open memory map of the file counts as "the file being open".
    ///
    /// \throw AccessError If the specified directory entry could not
    /// be removed. If the reason corresponds to one of the exception
    /// types that are derived from AccessError, the derived exception
    /// type is thrown (as long as the underlying system provides the
    /// information to unambiguously distinguish that particular
    /// reason).
    static void remove(const std::string& path);

    /// Same as remove() except that this one returns false, rather
    /// than throwing an exception, if the specified file does not
    /// exist. If the file did exist, and was deleted, this function
    /// returns true.
    static bool try_remove(const std::string& path);

    /// Change the path of a directory entry. This can be used to
    /// rename a file, and/or to move it from one directory to
    /// another. This function is equivalent to std::rename(const
    /// char*, const char*).
    ///
    /// \throw AccessError If the path of the directory entry could
    /// not be changed. If the reason corresponds to one of the
    /// exception types that are derived from AccessError, the derived
    /// exception type is thrown (as long as the underlying system
    /// provides the information to unambiguously distinguish that
    /// particular reason).
    static void move(const std::string& old_path, const std::string& new_path);

    /// Copy the file at the specified origin path to the specified target path.
    static void copy(const std::string& origin_path, const std::string& target_path);

    /// Compare the two files at the specified paths for equality. Returns true
    /// if, and only if they are equal.
    static bool compare(const std::string& path_1, const std::string& path_2);

    /// Check whether two open file descriptors refer to the same
    /// underlying file, that is, if writing via one of them, will
    /// affect what is read from the other. In UNIX this boils down to
    /// comparing inode numbers.
    ///
    /// Both instances have to be attached to open files. If they are
    /// not, this function has undefined behavior.
    bool is_same_file(const File&) const;
    static bool is_same_file_static(FileDesc f1, FileDesc f2);

    // FIXME: Get rid of this method
    bool is_removed() const;

    /// Resolve the specified path against the specified base directory.
    ///
    /// If \a path is absolute, or if \a base_dir is empty, \p path is returned
    /// unmodified, otherwise \a path is resolved against \a base_dir.
    ///
    /// Examples (assuming POSIX):
    ///
    ///    resolve("file", "dir")        -> "dir/file"
    ///    resolve("../baz", "/foo/bar") -> "/foo/baz"
    ///    resolve("foo", ".")           -> "./foo"
    ///    resolve(".", "/foo/")         -> "/foo"
    ///    resolve("..", "foo")          -> "."
    ///    resolve("../..", "foo")       -> ".."
    ///    resolve("..", "..")           -> "../.."
    ///    resolve("", "")               -> "."
    ///    resolve("", "/")              -> "/."
    ///    resolve("..", "/")            -> "/."
    ///    resolve("..", "foo//bar")     -> "foo"
    ///
    /// This function does not access the file system.
    ///
    /// \param path The path to be resolved. An empty string produces the same
    /// result as as if "." was passed. The result has a trailing directory
    /// separator (`/`) if, and only if this path has a trailing directory
    /// separator.
    ///
    /// \param base_dir The base directory path, which may be relative or
    /// absolute. A final directory separator (`/`) is optional. The empty
    /// string is interpreted as a relative path.
    static std::string resolve(const std::string& path, const std::string& base_dir);

    using ForEachHandler = std::function<bool(const std::string& file, const std::string& dir)>;

    /// Scan the specified directory recursivle, and report each file
    /// (nondirectory entry) via the specified handler.
    ///
    /// The first argument passed to the handler is the name of a file (not the
    /// whole path), and the second argument is the directory in which that file
    /// resides. The directory will be specified as a path, and relative to \a
    /// dir_path. The directory will be the empty string for files residing
    /// directly in \a dir_path.
    ///
    /// If the handler returns false, scanning will be aborted immediately, and
    /// for_each() will return false. Otherwise for_each() will return true.
    ///
    /// Scanning is done as if by a recursive set of DirScanner objects.
    static bool for_each(const std::string& dir_path, ForEachHandler handler);

    struct UniqueID {
#ifdef _WIN32 // Windows version
// FIXME: This is not implemented for Windows
#else
        // NDK r10e has a bug in sys/stat.h dev_t ino_t are 4 bytes,
        // but stat.st_dev and st_ino are 8 bytes. So we just use uint64 instead.
        dev_t device;
        uint_fast64_t inode;
#endif
    };
    // Return the unique id for the current opened file descriptor.
    // Same UniqueID means they are the same file.
    UniqueID get_unique_id() const;
    // Return the file descriptor for the file
    FileDesc get_descriptor() const;
    // Return the path of the open file, or an empty string if
    // this file has never been opened.
    std::string get_path() const;
    // Return false if the file doesn't exist. Otherwise uid will be set.
    static bool get_unique_id(const std::string& path, UniqueID& uid);

    class ExclusiveLock;
    class SharedLock;

    template <class>
    class Map;

    class CloseGuard;
    class UnlockGuard;
    class UnmapGuard;

    class Streambuf;

    // Exceptions
    class AccessError;
    class PermissionDenied;
    class NotFound;
    class Exists;

private:
#ifdef _WIN32
    void* m_fd;
    bool m_have_lock; // Only valid when m_fd is not null
#else
    int m_fd;
#endif
    std::unique_ptr<const char[]> m_encryption_key = nullptr;
    std::string m_path;

    bool lock(bool exclusive, bool non_blocking);
    void open_internal(const std::string& path, AccessMode, CreateMode, int flags, bool* success);

    struct MapBase {
        void* m_addr = nullptr;
        size_t m_size = 0;
        FileDesc m_fd;

        MapBase() noexcept;
        ~MapBase() noexcept;

        // Disable copying. Copying an opened MapBase will create a scenario
        // where the same memory will be mapped once but unmapped twice.
        MapBase(const MapBase&) = delete;
        MapBase& operator=(const MapBase&) = delete;

        void map(const File&, AccessMode, size_t size, int map_flags, size_t offset = 0);
        void remap(const File&, AccessMode, size_t size, int map_flags);
        void unmap() noexcept;
        void sync();
#if REALM_ENABLE_ENCRYPTION
        util::EncryptedFileMapping* m_encrypted_mapping = nullptr;
        inline util::EncryptedFileMapping* get_encrypted_mapping() const
        {
            return m_encrypted_mapping;
        }
#else
        inline util::EncryptedFileMapping* get_encrypted_mapping() const
        {
            return nullptr;
        }
#endif
    };
};


class File::ExclusiveLock {
public:
    ExclusiveLock(File& f)
        : m_file(f)
    {
        f.lock_exclusive();
    }
    ~ExclusiveLock() noexcept
    {
        m_file.unlock();
    }
    // Disable copying. It is not how this class should be used.
    ExclusiveLock(const ExclusiveLock&) = delete;
    ExclusiveLock& operator=(const ExclusiveLock&) = delete;

private:
    File& m_file;
};

class File::SharedLock {
public:
    SharedLock(File& f)
        : m_file(f)
    {
        f.lock_shared();
    }
    ~SharedLock() noexcept
    {
        m_file.unlock();
    }
    // Disable copying. It is not how this class should be used.
    SharedLock(const SharedLock&) = delete;
    SharedLock& operator=(const SharedLock&) = delete;

private:
    File& m_file;
};


/// This class provides a RAII abstraction over the concept of a
/// memory mapped file.
///
/// Once created, the Map instance makes no reference to the File
/// instance that it was based upon, and that File instance may be
/// destroyed before the Map instance is destroyed.
///
/// Multiple concurrent mappings may be created from the same File
/// instance.
///
/// You can use UnmapGuard to acheive exception-safe unmapping prior
/// to the Map instance being detroyed.
///
/// A single Map instance must never be accessed concurrently by
/// multiple threads.
template <class T>
class File::Map : private MapBase {
public:
    /// Equivalent to calling map() on a default constructed instance.
    explicit Map(const File&, AccessMode = access_ReadOnly, size_t size = sizeof(T), int map_flags = 0);

    explicit Map(const File&, size_t offset, AccessMode = access_ReadOnly, size_t size = sizeof(T),
                 int map_flags = 0);

    /// Create an instance that is not initially attached to a memory
    /// mapped file.
    Map() noexcept;

    ~Map() noexcept;

    // Disable copying. Copying an opened Map will create a scenario
    // where the same memory will be mapped once but unmapped twice.
    Map(const Map&) = delete;
    Map& operator=(const Map&) = delete;

    /// Move the mapping from another Map object to this Map object
    File::Map<T>& operator=(File::Map<T>&& other)
    {
        if (m_addr)
            unmap();
        m_addr = other.get_addr();
        m_size = other.m_size;
        other.m_addr = 0;
        other.m_size = 0;
#if REALM_ENABLE_ENCRYPTION
        m_encrypted_mapping = other.m_encrypted_mapping;
        other.m_encrypted_mapping = nullptr;
#endif
        return *this;
    }

    /// See File::map().
    ///
    /// Calling this function on a Map instance that is already
    /// attached to a memory mapped file has undefined behavior. The
    /// returned pointer is the same as what will subsequently be
    /// returned by get_addr().
    T* map(const File&, AccessMode = access_ReadOnly, size_t size = sizeof(T), int map_flags = 0, size_t offset = 0);

    /// See File::unmap(). This function is idempotent, that is, it is
    /// valid to call it regardless of whether this instance is
    /// currently attached to a memory mapped file.
    void unmap() noexcept;

    /// See File::remap().
    ///
    /// Calling this function on a Map instance that is not currently
    /// attached to a memory mapped file has undefined behavior. The
    /// returned pointer is the same as what will subsequently be
    /// returned by get_addr().
    T* remap(const File&, AccessMode = access_ReadOnly, size_t size = sizeof(T), int map_flags = 0);

    /// See File::sync_map().
    ///
    /// Calling this function on an instance that is not currently
    /// attached to a memory mapped file, has undefined behavior.
    void sync();

    /// Check whether this Map instance is currently attached to a
    /// memory mapped file.
    bool is_attached() const noexcept;

    /// Returns a pointer to the beginning of the memory mapped file,
    /// or null if this instance is not currently attached.
    T* get_addr() const noexcept;

    /// Returns the size of the mapped region, or zero if this
    /// instance does not currently refer to a memory mapped
    /// file. When this instance refers to a memory mapped file, the
    /// returned value will always be identical to the size passed to
    /// the constructor or to map().
    size_t get_size() const noexcept;

    /// Release the currently attached memory mapped file from this
    /// Map instance. The address range may then be unmapped later by
    /// a call to File::unmap().
    T* release() noexcept;

#if REALM_ENABLE_ENCRYPTION
    /// Get the encrypted file mapping corresponding to this mapping
    inline EncryptedFileMapping* get_encrypted_mapping() const
    {
        return m_encrypted_mapping;
    }
#else
    inline EncryptedFileMapping* get_encrypted_mapping() const
    {
        return nullptr;
    }
#endif

    friend class UnmapGuard;
};


class File::CloseGuard {
public:
    CloseGuard(File& f) noexcept
        : m_file(&f)
    {
    }
    ~CloseGuard() noexcept
    {
        if (m_file)
            m_file->close();
    }
    void release() noexcept
    {
        m_file = nullptr;
    }
    // Disallow the default implementation of copy/assign, this is not how this
    // class is intended to be used. For example we could get unexpected
    // behaviour if one CloseGuard is copied and released but the other is not.
    CloseGuard(const CloseGuard&) = delete;
    CloseGuard& operator=(const CloseGuard&) = delete;

private:
    File* m_file;
};


class File::UnlockGuard {
public:
    UnlockGuard(File& f) noexcept
        : m_file(&f)
    {
    }
    ~UnlockGuard() noexcept
    {
        if (m_file)
            m_file->unlock();
    }
    void release() noexcept
    {
        m_file = nullptr;
    }
    // Disallow the default implementation of copy/assign, this is not how this
    // class is intended to be used. For example we could get unexpected
    // behaviour if one UnlockGuard is copied and released but the other is not.
    UnlockGuard(const UnlockGuard&) = delete;
    UnlockGuard& operator=(const UnlockGuard&) = delete;

private:
    File* m_file;
};


class File::UnmapGuard {
public:
    template <class T>
    UnmapGuard(Map<T>& m) noexcept
        : m_map(&m)
    {
    }
    ~UnmapGuard() noexcept
    {
        if (m_map)
            m_map->unmap();
    }
    void release() noexcept
    {
        m_map = nullptr;
    }
    // Disallow the default implementation of copy/assign, this is not how this
    // class is intended to be used. For example we could get unexpected
    // behaviour if one UnmapGuard is copied and released but the other is not.
    UnmapGuard(const UnmapGuard&) = delete;
    UnmapGuard& operator=(const UnmapGuard&) = delete;

private:
    MapBase* m_map;
};


/// Only output is supported at this point.
class File::Streambuf : public std::streambuf {
public:
    explicit Streambuf(File*, size_t = 4096);
    ~Streambuf() noexcept;

    // Disable copying
    Streambuf(const Streambuf&) = delete;
    Streambuf& operator=(const Streambuf&) = delete;

private:
    File& m_file;
    std::unique_ptr<char[]> const m_buffer;

    int_type overflow(int_type) override;
    int sync() override;
    pos_type seekpos(pos_type, std::ios_base::openmode) override;
    void flush();
};

/// Used for any I/O related exception. Note the derived exception
/// types that are used for various specific types of errors.
class File::AccessError : public std::runtime_error {
public:
    AccessError(const std::string& msg, const std::string& path);

    /// Return the associated file system path, or the empty string if there is
    /// no associated file system path, or if the file system path is unknown.
    std::string get_path() const;

    const char* what() const noexcept
    {
        m_buffer = std::runtime_error::what();
        if (m_path.size() > 0)
            m_buffer += (std::string(" Path: ") + m_path);
        return m_buffer.c_str();
    }

private:
    std::string m_path;
    mutable std::string m_buffer;
};


/// Thrown if the user does not have permission to open or create
/// the specified file in the specified access mode.
class File::PermissionDenied : public AccessError {
public:
    PermissionDenied(const std::string& msg, const std::string& path);
};


/// Thrown if the directory part of the specified path was not
/// found, or create_Never was specified and the file did no
/// exist.
class File::NotFound : public AccessError {
public:
    NotFound(const std::string& msg, const std::string& path);
};


/// Thrown if create_Always was specified and the file did already
/// exist.
class File::Exists : public AccessError {
public:
    Exists(const std::string& msg, const std::string& path);
};


class DirScanner {
public:
    DirScanner(const std::string& path, bool allow_missing = false);
    ~DirScanner() noexcept;
    bool next(std::string& name);

private:
#ifndef _WIN32
    DIR* m_dirp;
#elif REALM_HAVE_STD_FILESYSTEM
    std::filesystem::directory_iterator m_iterator;
#endif
};


// Implementation:

inline File::File(const std::string& path, Mode m)
{
#ifdef _WIN32
    m_fd = nullptr;
#else
    m_fd = -1;
#endif

    open(path, m);
}

inline File::File() noexcept
{
#ifdef _WIN32
    m_fd = nullptr;
#else
    m_fd = -1;
#endif
}

inline File::~File() noexcept
{
    close();
}

inline File::File(File&& f) noexcept
{
#ifdef _WIN32
    m_fd = f.m_fd;
    m_have_lock = f.m_have_lock;
    f.m_fd = nullptr;
#else
    m_fd = f.m_fd;
    f.m_fd = -1;
#endif
    m_encryption_key = std::move(f.m_encryption_key);
}

inline File& File::operator=(File&& f) noexcept
{
    close();
#ifdef _WIN32
    m_fd = f.m_fd;
    m_have_lock = f.m_have_lock;
    f.m_fd = nullptr;
#else
    m_fd = f.m_fd;
    f.m_fd = -1;
#endif
    m_encryption_key = std::move(f.m_encryption_key);
    return *this;
}

inline void File::open(const std::string& path, Mode m)
{
    AccessMode a = access_ReadWrite;
    CreateMode c = create_Auto;
    int flags = 0;
    switch (m) {
        case mode_Read:
            a = access_ReadOnly;
            c = create_Never;
            break;
        case mode_Update:
            c = create_Never;
            break;
        case mode_Write:
            flags = flag_Trunc;
            break;
        case mode_Append:
            flags = flag_Append;
            break;
    }
    open(path, a, c, flags);
}

inline void File::open(const std::string& path, AccessMode am, CreateMode cm, int flags)
{
    open_internal(path, am, cm, flags, nullptr);
}


inline void File::open(const std::string& path, bool& was_created)
{
    while (1) {
        bool success;
        open_internal(path, access_ReadWrite, create_Must, 0, &success);
        if (success) {
            was_created = true;
            return;
        }
        open_internal(path, access_ReadWrite, create_Never, 0, &success);
        if (success) {
            was_created = false;
            return;
        }
    }
}

inline bool File::is_attached() const noexcept
{
#ifdef _WIN32
    return (m_fd != nullptr);
#else
    return 0 <= m_fd;
#endif
}

inline void File::lock_exclusive()
{
    lock(true, false);
}

inline void File::lock_shared()
{
    lock(false, false);
}

inline bool File::try_lock_exclusive()
{
    return lock(true, true);
}

inline bool File::try_lock_shared()
{
    return lock(false, true);
}

inline File::MapBase::MapBase() noexcept
{
    m_addr = nullptr;
}

inline File::MapBase::~MapBase() noexcept
{
    unmap();
}

inline void File::MapBase::map(const File& f, AccessMode a, size_t size, int map_flags, size_t offset)
{
    REALM_ASSERT(!m_addr);
#if REALM_ENABLE_ENCRYPTION
    m_addr = f.map(a, size, m_encrypted_mapping, map_flags, offset);
#else
    m_addr = f.map(a, size, map_flags, offset);
#endif
    m_size = size;
    m_fd = f.m_fd;
}

inline void File::MapBase::unmap() noexcept
{
    if (!m_addr)
        return;
    File::unmap(m_addr, m_size);
    m_addr = nullptr;
#if REALM_ENABLE_ENCRYPTION
    m_encrypted_mapping = nullptr;
#endif
    m_fd = 0;
}

inline void File::MapBase::remap(const File& f, AccessMode a, size_t size, int map_flags)
{
    REALM_ASSERT(m_addr);

    m_addr = f.remap(m_addr, m_size, a, size, map_flags);
    m_size = size;
    m_fd = f.m_fd;
}

inline void File::MapBase::sync()
{
    REALM_ASSERT(m_addr);

    File::sync_map(m_fd, m_addr, m_size);
}

template <class T>
inline File::Map<T>::Map(const File& f, AccessMode a, size_t size, int map_flags)
{
    map(f, a, size, map_flags);
}

template <class T>
inline File::Map<T>::Map(const File& f, size_t offset, AccessMode a, size_t size, int map_flags)
{
    map(f, a, size, map_flags, offset);
}

template <class T>
inline File::Map<T>::Map() noexcept
{
}

template <class T>
inline File::Map<T>::~Map() noexcept
{
}

template <class T>
inline T* File::Map<T>::map(const File& f, AccessMode a, size_t size, int map_flags, size_t offset)
{
    MapBase::map(f, a, size, map_flags, offset);
    return static_cast<T*>(m_addr);
}

template <class T>
inline void File::Map<T>::unmap() noexcept
{
    MapBase::unmap();
}

template <class T>
inline T* File::Map<T>::remap(const File& f, AccessMode a, size_t size, int map_flags)
{
    MapBase::remap(f, a, size, map_flags);
    return static_cast<T*>(m_addr);
}

template <class T>
inline void File::Map<T>::sync()
{
    MapBase::sync();
}

template <class T>
inline bool File::Map<T>::is_attached() const noexcept
{
    return (m_addr != nullptr);
}

template <class T>
inline T* File::Map<T>::get_addr() const noexcept
{
    return static_cast<T*>(m_addr);
}

template <class T>
inline size_t File::Map<T>::get_size() const noexcept
{
    return m_addr ? m_size : 0;
}

template <class T>
inline T* File::Map<T>::release() noexcept
{
    T* addr = static_cast<T*>(m_addr);
    m_addr = nullptr;
    m_fd = 0;
    return addr;
}


inline File::Streambuf::Streambuf(File* f, size_t buffer_size)
    : m_file(*f)
    , m_buffer(new char[buffer_size])
{
    char* b = m_buffer.get();
    setp(b, b + buffer_size);
}

inline File::Streambuf::~Streambuf() noexcept
{
    try {
        if (m_file.is_attached())
            flush();
    }
    catch (...) {
        // Errors deliberately ignored
    }
}

inline File::Streambuf::int_type File::Streambuf::overflow(int_type c)
{
    flush();
    if (c == traits_type::eof())
        return traits_type::not_eof(c);
    *pptr() = traits_type::to_char_type(c);
    pbump(1);
    return c;
}

inline int File::Streambuf::sync()
{
    flush();
    return 0;
}

inline File::Streambuf::pos_type File::Streambuf::seekpos(pos_type pos, std::ios_base::openmode)
{
    flush();
    SizeType pos2 = 0;
    if (int_cast_with_overflow_detect(std::streamsize(pos), pos2))
        throw util::overflow_error("Seek position overflow");
    m_file.seek(pos2);
    return pos;
}

inline void File::Streambuf::flush()
{
    size_t n = pptr() - pbase();
    if (n > 0) {
        m_file.write(pbase(), n);
        setp(m_buffer.get(), epptr());
    }
}

inline File::AccessError::AccessError(const std::string& msg, const std::string& path)
    : std::runtime_error(msg)
    , m_path(path)
{
}

inline std::string File::AccessError::get_path() const
{
    return m_path;
}

inline File::PermissionDenied::PermissionDenied(const std::string& msg, const std::string& path)
    : AccessError(msg, path)
{
}

inline File::NotFound::NotFound(const std::string& msg, const std::string& path)
    : AccessError(msg, path)
{
}

inline File::Exists::Exists(const std::string& msg, const std::string& path)
    : AccessError(msg, path)
{
}

inline bool operator==(const File::UniqueID& lhs, const File::UniqueID& rhs)
{
#ifdef _WIN32 // Windows version
    throw util::runtime_error("Not yet supported");
#else // POSIX version
    return lhs.device == rhs.device && lhs.inode == rhs.inode;
#endif
}

inline bool operator!=(const File::UniqueID& lhs, const File::UniqueID& rhs)
{
    return !(lhs == rhs);
}

inline bool operator<(const File::UniqueID& lhs, const File::UniqueID& rhs)
{
#ifdef _WIN32 // Windows version
    throw util::runtime_error("Not yet supported");
#else // POSIX version
    if (lhs.device < rhs.device)
        return true;
    if (lhs.device > rhs.device)
        return false;
    if (lhs.inode < rhs.inode)
        return true;
    return false;
#endif
}

inline bool operator>(const File::UniqueID& lhs, const File::UniqueID& rhs)
{
    return rhs < lhs;
}

inline bool operator<=(const File::UniqueID& lhs, const File::UniqueID& rhs)
{
    return !(lhs > rhs);
}

inline bool operator>=(const File::UniqueID& lhs, const File::UniqueID& rhs)
{
    return !(lhs < rhs);
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_FILE_HPP
