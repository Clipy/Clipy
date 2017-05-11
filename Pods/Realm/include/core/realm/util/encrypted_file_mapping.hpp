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

#ifndef REALM_UTIL_ENCRYPTED_FILE_MAPPING_HPP
#define REALM_UTIL_ENCRYPTED_FILE_MAPPING_HPP

#include <realm/util/file.hpp>
#include <realm/util/thread.hpp>
#include <realm/util/features.h>

#if REALM_ENABLE_ENCRYPTION

typedef size_t (*Header_to_size)(const char* addr);

#include <vector>

namespace realm {
namespace util {

struct SharedFileInfo;
class EncryptedFileMapping;

class EncryptedFileMapping {
public:
    // Adds the newly-created object to file.mappings iff it's successfully constructed
    EncryptedFileMapping(SharedFileInfo& file, size_t file_offset, void* addr, size_t size, File::AccessMode access);
    ~EncryptedFileMapping();

    // Default implementations of copy/assign can trigger multiple destructions
    EncryptedFileMapping(const EncryptedFileMapping&) = delete;
    EncryptedFileMapping& operator=(const EncryptedFileMapping&) = delete;

    // Write all dirty pages to disk and mark them read-only
    // Does not call fsync
    void flush() noexcept;

    // Sync this file to disk
    void sync() noexcept;

    // Make sure that memory in the specified range is synchronized with any
    // changes made globally visible through call to write_barrier
    void read_barrier(const void* addr, size_t size, UniqueLock& lock, Header_to_size header_to_size);

    // Ensures that any changes made to memory in the specified range
    // becomes visible to any later calls to read_barrier()
    void write_barrier(const void* addr, size_t size) noexcept;

    // Set this mapping to a new address and size
    // Flushes any remaining dirty pages from the old mapping
    void set(void* new_addr, size_t new_size, size_t new_file_offset);

private:
    SharedFileInfo& m_file;

    size_t m_page_shift;
    size_t m_blocks_per_page;

    void* m_addr = nullptr;
    size_t m_file_offset = 0;

    uintptr_t m_first_page;
    size_t m_page_count = 0;

    std::vector<char> m_up_to_date_pages;
    std::vector<bool> m_dirty_pages;

    File::AccessMode m_access;

#ifdef REALM_DEBUG
    std::unique_ptr<char[]> m_validate_buffer;
#endif

    char* page_addr(size_t i) const noexcept;

    void mark_outdated(size_t i) noexcept;
    void mark_up_to_date(size_t i) noexcept;
    void mark_unwritable(size_t i) noexcept;

    bool copy_up_to_date_page(size_t i) noexcept;
    void refresh_page(size_t i);
    void write_page(size_t i) noexcept;

    void validate_page(size_t i) noexcept;
    void validate() noexcept;
};


inline void EncryptedFileMapping::read_barrier(const void* addr, size_t size, UniqueLock& lock,
                                               Header_to_size header_to_size)
{
    size_t first_accessed_page = reinterpret_cast<uintptr_t>(addr) >> m_page_shift;
    size_t first_idx = first_accessed_page - m_first_page;

    // make sure the first page is available
    // Checking before taking the lock is important to performance.
    if (!m_up_to_date_pages[first_idx]) {
        if (!lock.holds_lock())
            lock.lock();
        // after taking the lock, we must repeat the check so that we never
        // call refresh_page() on a page which is already up to date.
        if (!m_up_to_date_pages[first_idx])
            refresh_page(first_idx);
    }

    if (header_to_size) {

        // We know it's an array, and array headers are 8-byte aligned, so it is
        // included in the first page which was handled above.
        size = header_to_size(static_cast<const char*>(addr));
    }
    size_t last_accessed_page = (reinterpret_cast<uintptr_t>(addr) + size - 1) >> m_page_shift;
    size_t last_idx = last_accessed_page - m_first_page;

    for (size_t idx = first_idx + 1; idx <= last_idx; ++idx) {
        if (!m_up_to_date_pages[idx]) {
            if (!lock.holds_lock())
                lock.lock();
            // after taking the lock, we must repeat the check so that we never
            // call refresh_page() on a page which is already up to date.
            if (!m_up_to_date_pages[idx])
                refresh_page(idx);
        }
    }
}
}
}

#endif // REALM_ENABLE_ENCRYPTION

namespace realm {
namespace util {

/// Thrown by EncryptedFileMapping if a file opened is non-empty and does not
/// contain valid encrypted data
struct DecryptionFailed : util::File::AccessError {
    DecryptionFailed()
        : util::File::AccessError("Decryption failed", std::string())
    {
    }
};
}
}

#endif // REALM_UTIL_ENCRYPTED_FILE_MAPPING_HPP
