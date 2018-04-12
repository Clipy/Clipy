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

#ifndef REALM_ALLOC_SLAB_HPP
#define REALM_ALLOC_SLAB_HPP

#include <cstdint> // unint8_t etc
#include <vector>
#include <string>
#include <atomic>

#include <realm/util/features.h>
#include <realm/util/file.hpp>
#include <realm/alloc.hpp>
#include <realm/disable_sync_to_disk.hpp>

namespace realm {

// Pre-declarations
class Group;
class GroupWriter;


/// Thrown by Group and SharedGroup constructors if the specified file
/// (or memory buffer) does not appear to contain a valid Realm
/// database.
struct InvalidDatabase;


/// The allocator that is used to manage the memory of a Realm
/// group, i.e., a Realm database.
///
/// Optionally, it can be attached to an pre-existing database (file
/// or memory buffer) which then becomes an immuatble part of the
/// managed memory.
///
/// To attach a slab allocator to a pre-existing database, call
/// attach_file() or attach_buffer(). To create a new database
/// in-memory, call attach_empty().
///
/// For efficiency, this allocator manages its mutable memory as a set
/// of slabs.
class SlabAlloc : public Allocator {
public:
    ~SlabAlloc() noexcept override;
    SlabAlloc();

    // Disable copying. Copying an allocator can produce double frees.
    SlabAlloc(const SlabAlloc&) = delete;
    SlabAlloc& operator=(const SlabAlloc&) = delete;

    /// \struct Config
    /// \brief Storage for combining setup flags for initialization to
    /// the SlabAlloc.
    ///
    /// \var Config::is_shared
    /// Must be true if, and only if we are called on behalf of SharedGroup.
    ///
    /// \var Config::read_only
    /// Open the file in read-only mode. This implies \a Config::no_create.
    ///
    /// \var Config::no_create
    /// Fail if the file does not already exist.
    ///
    /// \var Config::skip_validate
    /// Skip validation of file header. In a
    /// set of overlapping SharedGroups, only the first one (the one
    /// that creates/initlializes the coordination file) may validate
    /// the header, otherwise it will result in a race condition.
    ///
    /// \var Config::encryption_key
    /// 32-byte key to use to encrypt and decrypt the backing storage,
    /// or nullptr to disable encryption.
    ///
    /// \var Config::session_initiator
    /// If set, the caller is the session initiator and
    /// guarantees exclusive access to the file. If attaching in
    /// read/write mode, the file is modified: files on streaming form
    /// is changed to non-streaming form, and if needed the file size
    /// is adjusted to match mmap boundaries.
    /// Must be set to false if is_shared is false.
    ///
    /// \var Config::clear_file
    /// Always initialize the file as if it was a newly
    /// created file and ignore any pre-existing contents. Requires that
    /// Config::session_initiator be true as well.
    struct Config {
        bool is_shared = false;
        bool read_only = false;
        bool no_create = false;
        bool skip_validate = false;
        bool session_initiator = false;
        bool clear_file = false;
        const char* encryption_key = nullptr;
    };

    struct Retry {
    };

    /// \brief Attach this allocator to the specified file.
    ///
    /// It is an error if this function is called at a time where the specified
    /// Realm file (file system inode) is modified asynchronously.
    ///
    /// In non-shared mode (when this function is called on behalf of a
    /// free-standing Group instance), it is the responsibility of the
    /// application to ensure that the Realm file is not modified concurrently
    /// from any other thread or process.
    ///
    /// In shared mode (when this function is called on behalf of a SharedGroup
    /// instance), the caller (SharedGroup::do_open()) must take steps to ensure
    /// cross-process mutual exclusion.
    ///
    /// Except for \a file_path, the parameters are passed in through a
    /// configuration object.
    ///
    /// \return The `ref` of the root node, or zero if there is none.
    ///
    /// Please note that attach_file can fail to attach to a file due to a
    /// collision with a writer extending the file. This can only happen if the
    /// caller is *not* the session initiator. When this happens, attach_file()
    /// throws SlabAlloc::Retry, and the caller must retry the call. The caller
    /// should check if it has become the session initiator before retrying.
    /// This can happen if the conflicting thread (or process) terminates or
    /// crashes before the next retry.
    ///
    /// \throw util::File::AccessError
    /// \throw SlabAlloc::Retry
    ref_type attach_file(const std::string& file_path, Config& cfg);

    /// Get the attached file. Only valid when called on an allocator with
    /// an attached file.
    util::File& get_file();

    /// Attach this allocator to the specified memory buffer.
    ///
    /// It is an error to call this function on an attached
    /// allocator. Doing so will result in undefined behavor.
    ///
    /// \return The `ref` of the root node, or zero if there is none.
    ///
    /// \sa own_buffer()
    ///
    /// \throw InvalidDatabase
    ref_type attach_buffer(const char* data, size_t size);

    /// Reads file format from file header. Must be called from within a write
    /// transaction.
    int get_committed_file_format_version() const noexcept;

    /// Attach this allocator to an empty buffer.
    ///
    /// It is an error to call this function on an attached
    /// allocator. Doing so will result in undefined behavor.
    void attach_empty();

    /// Detach from a previously attached file or buffer.
    ///
    /// This function does not reset free space tracking. To
    /// completely reset the allocator, you must also call
    /// reset_free_space_tracking().
    ///
    /// This function has no effect if the allocator is already in the
    /// detached state (idempotency).
    void detach() noexcept;

    class DetachGuard;

    /// If a memory buffer has been attached using attach_buffer(),
    /// mark it as owned by this slab allocator. Behaviour is
    /// undefined if this function is called on a detached allocator,
    /// one that is not attached using attach_buffer(), or one for
    /// which this function has already been called during the latest
    /// attachment.
    void own_buffer() noexcept;

    /// Returns true if, and only if this allocator is currently
    /// in the attached state.
    bool is_attached() const noexcept;

    /// Returns true if, and only if this allocator is currently in
    /// the attached state and attachment was not established using
    /// attach_empty().
    bool nonempty_attachment() const noexcept;

    /// Reserve disk space now to avoid allocation errors at a later
    /// point in time, and to minimize on-disk fragmentation. In some
    /// cases, less fragmentation translates into improved
    /// performance. On flash or SSD-drives this is likely a waste.
    ///
    /// Note: File::prealloc() may misbehave under race conditions (see
    /// documentation of File::prealloc()). For that reason, to avoid race
    /// conditions, when this allocator is used in a transactional mode, this
    /// function may be called only when the caller has exclusive write
    /// access. In non-transactional mode it is the responsibility of the user
    /// to ensure non-concurrent file mutation.
    ///
    /// This function will call File::sync().
    ///
    /// It is an error to call this function on an allocator that is not
    /// attached to a file. Doing so will result in undefined behavior.
    void resize_file(size_t new_file_size);

#ifdef REALM_DEBUG
    /// Deprecated method, only called from a unit test
    ///
    /// WARNING: This method is NOT thread safe on multiple platforms; see
    /// File::prealloc().
    ///
    /// Reserve disk space now to avoid allocation errors at a later point in
    /// time, and to minimize on-disk fragmentation. In some cases, less
    /// fragmentation translates into improved performance. On SSD-drives
    /// preallocation is likely a waste.
    ///
    /// When supported by the system, a call to this function will make the
    /// database file at least as big as the specified size, and cause space on
    /// the target device to be allocated (note that on many systems on-disk
    /// allocation is done lazily by default). If the file is already bigger
    /// than the specified size, the size will be unchanged, and on-disk
    /// allocation will occur only for the initial section that corresponds to
    /// the specified size.
    ///
    /// This function will call File::sync() if it changes the size of the file.
    ///
    /// It is an error to call this function on an allocator that is not
    /// attached to a file. Doing so will result in undefined behavior.
    void reserve_disk_space(size_t size_in_bytes);
#endif

    /// Get the size of the attached database file or buffer in number
    /// of bytes. This size is not affected by new allocations. After
    /// attachment, it can only be modified by a call to update_reader_view().
    ///
    /// It is an error to call this function on a detached allocator,
    /// or one that was attached using attach_empty(). Doing so will
    /// result in undefined behavior.
    size_t get_baseline() const noexcept;

    /// Get the total amount of managed memory. This is the baseline plus the
    /// sum of the sizes of the allocated slabs. It includes any free space.
    ///
    /// It is an error to call this function on a detached
    /// allocator. Doing so will result in undefined behavior.
    size_t get_total_size() const noexcept;

    /// Mark all mutable memory (ref-space outside the attached file) as free
    /// space.
    void reset_free_space_tracking();

    /// Update the readers view of the file:
    ///
    /// Remap the attached file such that a prefix of the specified
    /// size becomes available in memory. If sucessfull,
    /// get_baseline() will return the specified new file size.
    ///
    /// It is an error to call this function on a detached allocator,
    /// or one that was not attached using attach_file(). Doing so
    /// will result in undefined behavior.
    ///
    /// The file_size argument must be aligned to a *section* boundary:
    /// The database file is logically split into sections, each section
    /// guaranteed to be mapped as a contiguous address range. The allocation
    /// of memory in the file must ensure that no allocation crosses the
    /// boundary between two sections.
    ///
    /// Clears any allocator specicific caching of address translations
    /// and force any later address translations to trigger decryption if required.
    void update_reader_view(size_t file_size);

    /// Returns true initially, and after a call to reset_free_space_tracking()
    /// up until the point of the first call to SlabAlloc::alloc(). Note that a
    /// call to SlabAlloc::alloc() corresponds to a mutation event.
    bool is_free_space_clean() const noexcept;

    void verify() const override;
#ifdef REALM_DEBUG
    void enable_debug(bool enable)
    {
        m_debug_out = enable;
    }
    bool is_all_free() const;
    void print() const;
#endif
    struct MappedFile;

protected:
    MemRef do_alloc(const size_t size) override;
    MemRef do_realloc(ref_type, const char*, size_t old_size, size_t new_size) override;
    // FIXME: It would be very nice if we could detect an invalid free operation in debug mode
    void do_free(ref_type, const char*) noexcept override;
    char* do_translate(ref_type) const noexcept override;

    /// Returns the first section boundary *above* the given position.
    size_t get_upper_section_boundary(size_t start_pos) const noexcept;

    /// Returns the first section boundary *at or below* the given position.
    size_t get_lower_section_boundary(size_t start_pos) const noexcept;

    /// Returns true if the given position is at a section boundary
    bool matches_section_boundary(size_t pos) const noexcept;

    /// Returns the index of the section holding a given address.
    /// The section index is determined solely by the minimal section size,
    /// and does not necessarily reflect the mapping. A mapping may
    /// cover multiple sections - the initial mapping often does.
    size_t get_section_index(size_t pos) const noexcept;

    /// Reverse: get the base offset of a section at a given index. Since the
    /// computation is very time critical, this method just looks it up in
    /// a table. The actual computation and setup of that table is done
    /// during initialization with the help of compute_section_base() below.
    inline size_t get_section_base(size_t index) const noexcept;

    /// Actually compute the starting offset of a section. Only used to initialize
    /// a table of predefined results, which are then used by get_section_base().
    size_t compute_section_base(size_t index) const noexcept;

    /// Find a possible allocation of 'request_size' that will fit into a section
    /// which is inside the range from 'start_pos' to 'start_pos'+'free_chunk_size'
    /// If found return the position, if not return 0.
    size_t find_section_in_range(size_t start_pos, size_t free_chunk_size, size_t request_size) const noexcept;

private:
    void internal_invalidate_cache() noexcept;
    enum AttachMode {
        attach_None,        // Nothing is attached
        attach_OwnedBuffer, // We own the buffer (m_data = nullptr for empty buffer)
        attach_UsersBuffer, // We do not own the buffer
        attach_SharedFile,  // On behalf of SharedGroup
        attach_UnsharedFile // Not on behalf of SharedGroup
    };

    // A slab is a dynamically allocated contiguous chunk of memory used to
    // extend the amount of space available for database node
    // storage. Inter-node references are represented as file offsets
    // (a.k.a. "refs"), and each slab creates an apparently seamless extension
    // of this file offset addressable space. Slabes are stored as rows in the
    // Slabs table in order of ascending file offsets.
    struct Slab {
        ref_type ref_end;
        char* addr;
    };
    struct Chunk {
        ref_type ref;
        size_t size;
    };

    // Values of each used bit in m_flags
    enum {
        flags_SelectBit = 1,
    };

    // 24 bytes
    struct Header {
        uint64_t m_top_ref[2]; // 2 * 8 bytes
        // Info-block 8-bytes
        uint8_t m_mnemonic[4];    // "T-DB"
        uint8_t m_file_format[2]; // See `library_file_format`
        uint8_t m_reserved;
        // bit 0 of m_flags is used to select between the two top refs.
        uint8_t m_flags;
    };

    // 16 bytes
    struct StreamingFooter {
        uint64_t m_top_ref;
        uint64_t m_magic_cookie;
    };

    static_assert(sizeof(Header) == 24, "Bad header size");
    static_assert(sizeof(StreamingFooter) == 16, "Bad footer size");

    static const Header empty_file_header;
    static void init_streaming_header(Header*, int file_format_version);

    static const uint_fast64_t footer_magic_cookie = 0x3034125237E526C8ULL;

    // The mappings are shared, if they are from a file
    std::shared_ptr<MappedFile> m_file_mappings;

    // We are caching local copies of all the additional mappings to allow
    // for lock-free lookup during ref->address translation (we do not need
    // to cache the first mapping, because it is immutable) (well, all the
    // mappings are immutable, but the array holding them is not - it may
    // have to be relocated)
    std::unique_ptr<std::shared_ptr<const util::File::Map<char>>[]> m_local_mappings;
    size_t m_num_local_mappings = 0;

    const char* m_data = nullptr;
    size_t m_initial_chunk_size = 0;
    size_t m_initial_section_size = 0;
    int m_section_shifts = 0;
    std::unique_ptr<size_t[]> m_section_bases;
    size_t m_num_section_bases = 0;
    AttachMode m_attach_mode = attach_None;
    enum FeeeSpaceState {
        free_space_Clean,
        free_space_Dirty,
        free_space_Invalid,
    };

    /// When set to free_space_Invalid, the free lists are no longer
    /// up-to-date. This happens if do_free() or
    /// reset_free_space_tracking() fails, presumably due to
    /// std::bad_alloc being thrown during updating of the free space
    /// list. In this this case, alloc(), realloc_(), and
    /// get_free_read_only() must throw. This member is deliberately
    /// placed here (after m_attach_mode) in the hope that it leads to
    /// less padding between members due to alignment requirements.
    FeeeSpaceState m_free_space_state = free_space_Clean;

    typedef std::vector<Slab> slabs;
    typedef std::vector<Chunk> chunks;
    slabs m_slabs;
    chunks m_free_space;
    chunks m_free_read_only;

    bool m_debug_out = false;
    struct hash_entry {
        ref_type ref = 0;
        const char* addr = nullptr;
        size_t version = 0;
    };
    mutable hash_entry cache[256];
    mutable size_t version = 1;

    /// Throws if free-lists are no longer valid.
    void consolidate_free_read_only();
    /// Throws if free-lists are no longer valid.
    const chunks& get_free_read_only() const;

    /// Throws InvalidDatabase if the file is not a Realm file, if the file is
    /// corrupted, or if the specified encryption key is incorrect. This
    /// function will not detect all forms of corruption, though.
    void validate_buffer(const char* data, size_t len, const std::string& path);

    static bool is_file_on_streaming_form(const Header& header);
    /// Read the top_ref from the given buffer and set m_file_on_streaming_form
    /// if the buffer contains a file in streaming form
    static ref_type get_top_ref(const char* data, size_t len);

    class ChunkRefEq;
    class ChunkRefEndEq;
    class SlabRefEndEq;
    static bool ref_less_than_slab_ref_end(ref_type, const Slab&) noexcept;

    Replication* get_replication() const noexcept
    {
        return m_replication;
    }
    void set_replication(Replication* r) noexcept
    {
        m_replication = r;
    }

    friend class Group;
    friend class SharedGroup;
    friend class GroupWriter;
};

inline void SlabAlloc::internal_invalidate_cache() noexcept
{
    ++version;
}

class SlabAlloc::DetachGuard {
public:
    DetachGuard(SlabAlloc& alloc) noexcept
        : m_alloc(&alloc)
    {
    }
    ~DetachGuard() noexcept;
    SlabAlloc* release() noexcept;

private:
    SlabAlloc* m_alloc;
};


// Implementation:

struct InvalidDatabase : util::File::AccessError {
    InvalidDatabase(const std::string& msg, const std::string& path)
        : util::File::AccessError(msg, path)
    {
    }
};

inline void SlabAlloc::own_buffer() noexcept
{
    REALM_ASSERT_3(m_attach_mode, ==, attach_UsersBuffer);
    REALM_ASSERT(m_data);
    REALM_ASSERT(m_file_mappings == nullptr);
    m_attach_mode = attach_OwnedBuffer;
}

inline bool SlabAlloc::is_attached() const noexcept
{
    return m_attach_mode != attach_None;
}

inline bool SlabAlloc::nonempty_attachment() const noexcept
{
    return is_attached() && m_data;
}

inline size_t SlabAlloc::get_baseline() const noexcept
{
    REALM_ASSERT_DEBUG(is_attached());
    return m_baseline;
}

inline bool SlabAlloc::is_free_space_clean() const noexcept
{
    return m_free_space_state == free_space_Clean;
}

inline SlabAlloc::DetachGuard::~DetachGuard() noexcept
{
    if (m_alloc)
        m_alloc->detach();
}

inline SlabAlloc* SlabAlloc::DetachGuard::release() noexcept
{
    SlabAlloc* alloc = m_alloc;
    m_alloc = nullptr;
    return alloc;
}

inline bool SlabAlloc::ref_less_than_slab_ref_end(ref_type ref, const Slab& slab) noexcept
{
    return ref < slab.ref_end;
}

inline size_t SlabAlloc::get_upper_section_boundary(size_t start_pos) const noexcept
{
    return get_section_base(1 + get_section_index(start_pos));
}

inline size_t SlabAlloc::get_lower_section_boundary(size_t start_pos) const noexcept
{
    return get_section_base(get_section_index(start_pos));
}

inline bool SlabAlloc::matches_section_boundary(size_t pos) const noexcept
{
    return pos == get_lower_section_boundary(pos);
}

inline size_t SlabAlloc::get_section_base(size_t index) const noexcept
{
    return m_section_bases[index];
}

} // namespace realm

#endif // REALM_ALLOC_SLAB_HPP
