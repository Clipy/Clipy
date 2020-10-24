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
#include <map>
#include <string>
#include <atomic>
#include <mutex>

#include <realm/util/features.h>
#include <realm/util/file.hpp>
#include <realm/util/thread.hpp>
#include <realm/alloc.hpp>
#include <realm/disable_sync_to_disk.hpp>

namespace realm {

// Pre-declarations
class Group;
class GroupWriter;

namespace util {
struct SharedFileInfo;
}

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
        bool disable_sync = false;
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

    bool is_file_on_streaming_form() const
    {
        const Header& header = *reinterpret_cast<const Header*>(m_data);
        return is_file_on_streaming_form(header);
    }

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
    /// Updates the memory mappings to reflect a new size for the file.
    /// Stale mappings are retained so that they remain valid for other threads,
    /// which haven't yet seen the file size change. The stale mappings are
    /// associated with a version count if one is provided.
    /// They are later purged by calls to purge_old_mappings().
    /// The version parameter is subtly different from the mapping_version obtained
    /// by get_mapping_version() below. The mapping version changes whenever a
    /// ref->ptr translation changes, and is used by Group to enforce re-translation.
    void update_reader_view(size_t file_size);
    void purge_old_mappings(uint64_t oldest_live_version, uint64_t youngest_live_version);
    void init_mapping_management(uint64_t currently_live_version);

    /// Get an ID for the current mapping version. This ID changes whenever any part
    /// of an existing mapping is changed. Such a change requires all refs to be
    /// retranslated to new pointers. The allocator tries to avoid this, and we
    /// believe it will only ever occur on Windows based platforms, and when a
    /// compatibility mapping is used to read earlier file versions.
    uint64_t get_mapping_version()
    {
        return m_mapping_version;
    }

    /// Returns true initially, and after a call to reset_free_space_tracking()
    /// up until the point of the first call to SlabAlloc::alloc(). Note that a
    /// call to SlabAlloc::alloc() corresponds to a mutation event.
    bool is_free_space_clean() const noexcept;

    /// Returns the amount of memory requested by calls to SlabAlloc::alloc().
    size_t get_commit_size() const
    {
        return m_commit_size;
    }

    /// Returns the total amount of memory currently allocated in slab area
    size_t get_allocated_size() const noexcept;

    /// Returns total amount of slab for all slab allocators
    static size_t get_total_slab_size() noexcept;

    /// Hooks used to keep the encryption layer informed of the start and stop
    /// of transactions.
    void note_reader_start(const void* reader_id);
    void note_reader_end(const void* reader_id) noexcept;

    void verify() const override;
#ifdef REALM_DEBUG
    void enable_debug(bool enable)
    {
        m_debug_out = enable;
    }
    bool is_all_free() const;
    void print() const;
#endif

protected:
    MemRef do_alloc(const size_t size) override;
    MemRef do_realloc(ref_type, char*, size_t old_size, size_t new_size) override;
    // FIXME: It would be very nice if we could detect an invalid free operation in debug mode
    void do_free(ref_type, char*) override;
    char* do_translate(ref_type) const noexcept override;

    /// Returns the first section boundary *above* the given position.
    size_t get_upper_section_boundary(size_t start_pos) const noexcept;

    /// Returns the section boundary at or above the given size
    size_t align_size_to_section_boundary(size_t size) const noexcept;

    /// Returns the first section boundary *at or below* the given position.
    size_t get_lower_section_boundary(size_t start_pos) const noexcept;

    /// Returns true if the given position is at a section boundary
    bool matches_section_boundary(size_t pos) const noexcept;

    /// Actually compute the starting offset of a section. Only used to initialize
    /// a table of predefined results, which are then used by get_section_base().
    size_t compute_section_base(size_t index) const noexcept;

    /// Find a possible allocation of 'request_size' that will fit into a section
    /// which is inside the range from 'start_pos' to 'start_pos'+'free_chunk_size'
    /// If found return the position, if not return 0.
    size_t find_section_in_range(size_t start_pos, size_t free_chunk_size, size_t request_size) const noexcept;

private:
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
    // of this file offset addressable space. Slabs are stored as rows in the
    // Slabs table in order of ascending file offsets.
    struct Slab {
        ref_type ref_end;
        char* addr;
        size_t size;

        Slab(ref_type r, size_t s);
        ~Slab();

        Slab(const Slab&) = delete;
        Slab(Slab&& other) noexcept
            : ref_end(other.ref_end)
            , size(other.size)
        {
            addr = other.addr;
            other.addr = nullptr;
            other.size = 0;
        }
    };

    // free blocks that are in the slab area are managed using the following structures:
    // - FreeBlock: Placed at the start of any free space. Holds the 'ref' corresponding to
    //              the start of the space, and prev/next links used to place it in a size-specific
    //              freelist.
    // - BetweenBlocks: Structure sitting between any two free OR allocated spaces.
    //                  describes the size of the space before and after.
    // Each slab (area obtained from the underlying system) has a terminating BetweenBlocks
    // at the beginning and at the end of the Slab.
    struct FreeBlock {
        ref_type ref;    // ref for this entry. Saves a reverse translate / representing links as refs
        FreeBlock* prev; // circular doubly linked list
        FreeBlock* next;
        void clear_links()
        {
            prev = next = nullptr;
        }
        void unlink();
    };
    struct BetweenBlocks {         // stores sizes and used/free status of blocks before and after.
        int32_t block_before_size; // negated if block is in use,
        int32_t block_after_size;  // positive if block is free - and zero at end
    };

    Config m_cfg;
    using FreeListMap = std::map<int, FreeBlock*>; // log(N) addressing for larger blocks
    FreeListMap m_block_map;

    // abstract notion of a freelist - used to hide whether a freelist
    // is residing in the small blocks or the large blocks structures.
    struct FreeList {
        int size = 0; // size of every element in the list, 0 if not found
        FreeListMap::iterator it;
        bool found_something()
        {
            return size != 0;
        }
        bool found_exact(int sz)
        {
            return size == sz;
        }
    };

    // simple helper functions for accessing/navigating blocks and betweenblocks (TM)
    BetweenBlocks* bb_before(FreeBlock* entry) const
    {
        return reinterpret_cast<BetweenBlocks*>(entry) - 1;
    }
    BetweenBlocks* bb_after(FreeBlock* entry) const
    {
        auto bb = bb_before(entry);
        size_t sz = bb->block_after_size;
        char* addr = reinterpret_cast<char*>(entry) + sz;
        return reinterpret_cast<BetweenBlocks*>(addr);
    }
    FreeBlock* block_before(BetweenBlocks* bb) const
    {
        size_t sz = bb->block_before_size;
        if (sz <= 0)
            return nullptr; // only blocks that are not in use
        char* addr = reinterpret_cast<char*>(bb) - sz;
        return reinterpret_cast<FreeBlock*>(addr);
    }
    FreeBlock* block_after(BetweenBlocks* bb) const
    {
        if (bb->block_after_size <= 0)
            return nullptr;
        return reinterpret_cast<FreeBlock*>(bb + 1);
    }
    int size_from_block(FreeBlock* entry) const
    {
        return bb_before(entry)->block_after_size;
    }
    void mark_allocated(FreeBlock* entry);
    // mark the entry freed in bordering BetweenBlocks. Also validate size.
    void mark_freed(FreeBlock* entry, int size);

    // hook for the memory verifier in Group.
    template <typename Func>
    void for_all_free_entries(Func f) const;

    // Main entry points for alloc/free:
    FreeBlock* allocate_block(int size);
    void free_block(ref_type ref, FreeBlock* addr);

    // Searching/manipulating freelists
    FreeList find(int size);
    FreeList find_larger(FreeList hint, int size);
    FreeBlock* pop_freelist_entry(FreeList list);
    void push_freelist_entry(FreeBlock* entry);
    void remove_freelist_entry(FreeBlock* element);
    void rebuild_freelists_from_slab();
    void clear_freelists();

    // grow the slab area.
    // returns a free block large enough to handle the request.
    FreeBlock* grow_slab(int size);
    // create a single free chunk with "BetweenBlocks" at both ends and a
    // single free chunk between them. This free chunk will be of size:
    //   slab_size - 2 * sizeof(BetweenBlocks)
    FreeBlock* slab_to_entry(const Slab& slab, ref_type ref_start);

    // breaking/merging of blocks
    FreeBlock* get_prev_block_if_mergeable(FreeBlock* block);
    FreeBlock* get_next_block_if_mergeable(FreeBlock* block);
    // break 'block' to give it 'new_size'. Return remaining block.
    // If the block is too small to split, return nullptr.
    FreeBlock* break_block(FreeBlock* block, int new_size);
    FreeBlock* merge_blocks(FreeBlock* first, FreeBlock* second);

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

    // Description of to-be-deleted memory mapping
    struct OldMapping {
        OldMapping(uint64_t version, util::File::Map<char>&& map) noexcept
            : replaced_at_version(version)
            , mapping(std::move(map))
        {
        }
        OldMapping(OldMapping&& other) noexcept
            : replaced_at_version(other.replaced_at_version)
            , mapping()
        {
            mapping = std::move(other.mapping);
        }
        void operator=(OldMapping&& other) noexcept
        {
            replaced_at_version = other.replaced_at_version;
            mapping = std::move(other.mapping);
        }
        uint64_t replaced_at_version;
        util::File::Map<char> mapping;
    };
    struct OldRefTranslation {
        OldRefTranslation(uint64_t v, RefTranslation* m) noexcept
        {
            replaced_at_version = v;
            translations = m;
        }
        uint64_t replaced_at_version;
        RefTranslation* translations;
    };
    static_assert(sizeof(Header) == 24, "Bad header size");
    static_assert(sizeof(StreamingFooter) == 16, "Bad footer size");

    static const Header empty_file_header;
    static void init_streaming_header(Header*, int file_format_version);

    static const uint_fast64_t footer_magic_cookie = 0x3034125237E526C8ULL;

    util::RaceDetector changes;

    // mappings used by newest transactions - additional mappings may be open
    // and in use by older transactions. These translations are in m_old_mappings.
    std::vector<util::File::Map<char>> m_mappings;
    // The section nr for the first mapping in m_mappings. Will be 0 for newer file formats,
    // but will be nonzero if a compatibility mapping is in use. In that case, the ref for
    // the first mapping is the *last* section boundary in the file. Note: in this
    // mode, the first mapping in m_mappings may overlap with the last part of the
    // file, leading to aliasing.
    int m_sections_in_compatibility_mapping = 0;
    // if the file has an older format, it needs to be mapped by a single
    // mapping. This is the compatibility mapping. As such files extend, additional
    // mappings are added to m_mappings (above) - the compatibility mapping remains
    // unchanged until the file is closed.
    // Note: If the initial file is smaller than a single section, the compatibility
    // mapping is not needed and not used. Hence, it is not possible for the first mapping
    // in m_mappings to completely overlap the compatibility mapping. Hence, we do not
    // need special logic to detect if the compatibility mapping can be unmapped.
    util::File::Map<char> m_compatibility_mapping;

    size_t m_translation_table_size = 0;
    uint64_t m_mapping_version = 1;
    uint64_t m_youngest_live_version = 1;
    std::mutex m_mapping_mutex;
    util::File m_file;
    util::SharedFileInfo* m_realm_file_info = nullptr;
    // vectors where old mappings, are held from deletion to ensure translations are
    // kept open and ref->ptr translations work for other threads..
    std::vector<OldMapping> m_old_mappings;
    std::vector<OldRefTranslation> m_old_translations;
    // Rebuild the ref translations in a thread-safe manner. Save the old one along with it's
    // versioning information for later deletion - 'requires_new_fast_mapping' must be
    // true if there are changes to entries among the existing translations. Must be called
    // with m_mapping_mutex locked.
    void rebuild_translations(bool requires_new_fast_mapping, size_t old_num_sections);
    // Add a translation covering a new section in the slab area. The translation is always
    // added at the end.
    void extend_fast_mapping_with_slab(char* address);
    // Prepare the initial mapping for a file which requires use of the compatibility mapping
    void setup_compatibility_mapping(size_t file_size);

    const char* m_data = nullptr;
    size_t m_initial_section_size = 0;
    int m_section_shifts = 0;
    AttachMode m_attach_mode = attach_None;
    enum FeeeSpaceState {
        free_space_Clean,
        free_space_Dirty,
        free_space_Invalid,
    };
    constexpr static int minimal_alloc = 128 * 1024;
    constexpr static int maximal_alloc = 1 << section_shift;

    /// When set to free_space_Invalid, the free lists are no longer
    /// up-to-date. This happens if do_free() or
    /// reset_free_space_tracking() fails, presumably due to
    /// std::bad_alloc being thrown during updating of the free space
    /// list. In this this case, alloc(), realloc_(), and
    /// get_free_read_only() must throw. This member is deliberately
    /// placed here (after m_attach_mode) in the hope that it leads to
    /// less padding between members due to alignment requirements.
    FeeeSpaceState m_free_space_state = free_space_Clean;

    typedef std::vector<Slab> Slabs;
    using Chunks = std::map<ref_type, size_t>;
    Slabs m_slabs;
    Chunks m_free_read_only;
    size_t m_commit_size = 0;

    bool m_debug_out = false;

    /// Throws if free-lists are no longer valid.
    size_t consolidate_free_read_only();
    /// Throws if free-lists are no longer valid.
    const Chunks& get_free_read_only() const;

    /// Throws InvalidDatabase if the file is not a Realm file, if the file is
    /// corrupted, or if the specified encryption key is incorrect. This
    /// function will not detect all forms of corruption, though.
    void validate_header(const char* data, size_t len, const std::string& path);
    void throw_header_exception(std::string msg, const Header& header, const std::string& path);

    static bool is_file_on_streaming_form(const Header& header);
    /// Read the top_ref from the given buffer and set m_file_on_streaming_form
    /// if the buffer contains a file in streaming form
    static ref_type get_top_ref(const char* data, size_t len);

    // Gets the path of the attached file, or other relevant debugging info.
    std::string get_file_path_for_assertions() const;

    static bool ref_less_than_slab_ref_end(ref_type, const Slab&) noexcept;

    friend class Group;
    friend class DB;
    friend class GroupWriter;
};


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
    return m_baseline.load(std::memory_order_relaxed);
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

inline size_t SlabAlloc::align_size_to_section_boundary(size_t size) const noexcept
{
    if (matches_section_boundary(size))
        return size;
    else
        return get_upper_section_boundary(size);
}

inline size_t SlabAlloc::get_lower_section_boundary(size_t start_pos) const noexcept
{
    return get_section_base(get_section_index(start_pos));
}

inline bool SlabAlloc::matches_section_boundary(size_t pos) const noexcept
{
    auto boundary = get_lower_section_boundary(pos);
    return pos == boundary;
}

template <typename Func>
void SlabAlloc::for_all_free_entries(Func f) const
{
    ref_type ref = align_size_to_section_boundary(m_baseline.load(std::memory_order_relaxed));
    for (const auto& e : m_slabs) {
        BetweenBlocks* bb = reinterpret_cast<BetweenBlocks*>(e.addr);
        REALM_ASSERT(bb->block_before_size == 0);
        while (1) {
            int size = bb->block_after_size;
            f(ref, sizeof(BetweenBlocks));
            ref += sizeof(BetweenBlocks);
            if (size == 0) {
                break;
            }
            if (size > 0) { // freeblock.
                f(ref, size);
                bb = reinterpret_cast<BetweenBlocks*>(reinterpret_cast<char*>(bb) + sizeof(BetweenBlocks) + size);
                ref += size;
            }
            else {
                bb = reinterpret_cast<BetweenBlocks*>(reinterpret_cast<char*>(bb) + sizeof(BetweenBlocks) - size);
                ref -= size;
            }
        }
        // any gaps in ref-space is reported as a free block to the validator:
        auto next_ref = align_size_to_section_boundary(ref);
        if (next_ref > ref) {
            f(ref, next_ref - ref);
            ref = next_ref;
        }
    }
}

} // namespace realm

#endif // REALM_ALLOC_SLAB_HPP
