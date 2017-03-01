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

#ifndef REALM_GROUP_WRITER_HPP
#define REALM_GROUP_WRITER_HPP

#include <cstdint> // unint8_t etc
#include <utility>

#include <realm/util/file.hpp>
#include <realm/alloc.hpp>
#include <realm/impl/array_writer.hpp>
#include <realm/array_integer.hpp>


namespace realm {

// Pre-declarations
class Group;
class SlabAlloc;


/// This class is not supposed to be reused for multiple write sessions. In
/// particular, do not reuse it in case any of the functions throw.
///
/// FIXME: Move this class to namespace realm::_impl and to subdir src/realm/impl.
class GroupWriter : public _impl::ArrayWriterBase {
public:
    // For groups in transactional mode (Group::m_is_shared), this constructor
    // must be called while a write transaction is in progress.
    //
    // The constructor adds free-space tracking information to the specified
    // group, if it is not already present (4th and 5th entry in
    // Group::m_top). If the specified group is in transactional mode
    // (Group::m_is_shared), the constructor also adds version tracking
    // information to the group, if it is not already present (6th and 7th entry
    // in Group::m_top).
    GroupWriter(Group&);
    ~GroupWriter();

    void set_versions(uint64_t current, uint64_t read_lock) noexcept;

    /// Write all changed array nodes into free space.
    ///
    /// Returns the new top ref. When in full durability mode, call
    /// commit() with the returned top ref.
    ref_type write_group();

    /// Flush changes to physical medium, then write the new top ref
    /// to the file header, then flush again. Pass the top ref
    /// returned by write_group().
    void commit(ref_type new_top_ref);

    size_t get_file_size() const noexcept;

    /// Write the specified chunk into free space.
    void write(const char* data, size_t size);

    ref_type write_array(const char*, size_t, uint32_t) override;

#ifdef REALM_DEBUG
    void dump();
#endif

    size_t get_free_space();
private:
    class MapWindow;
    Group& m_group;
    SlabAlloc& m_alloc;
    ArrayInteger m_free_positions; // 4th slot in Group::m_top
    ArrayInteger m_free_lengths;   // 5th slot in Group::m_top
    ArrayInteger m_free_versions;  // 6th slot in Group::m_top
    uint64_t m_current_version;
    uint64_t m_readlock_version;

    // Currently cached memory mappings. We keep as many as 16 1MB windows
    // open for writing. The allocator will favor sequential allocation
    // from a modest number of windows, depending upon fragmentation, so
    // 16 windows should be more than enough. If more than 16 windows are
    // needed, the least recently used is sync'ed and closed to make room
    // for a new one. The windows are kept in MRU (most recently used) order.
    const static int num_map_windows = 16;
    std::vector<std::unique_ptr<MapWindow>> m_map_windows;

    // Get a suitable memory mapping for later access:
    // potentially adding it to the cache, potentially closing
    // the least recently used and sync'ing it to disk
    MapWindow* get_window(ref_type start_ref, size_t size);

    // Sync all cached memory mappings
    void sync_all_mappings();

    // Merge adjacent chunks
    void merge_free_space();

    /// Allocate a chunk of free space of the specified size. The
    /// specified size must be 8-byte aligned. Extend the file if
    /// required. The returned chunk is removed from the amount of
    /// remaing free space. The returned chunk is guaranteed to be
    /// within a single contiguous memory mapping.
    ///
    /// \return The position within the database file of the allocated
    /// chunk.
    size_t get_free_space(size_t size);

    /// Find a block of free space that is at least as big as the
    /// specified size and which will allow an allocation that is mapped
    /// inside a contiguous address range. The specified size does not
    /// need to be 8-byte aligned. Extend the file if required.
    /// The returned chunk is not removed from the amount of remaing
    /// free space.
    ///
    /// \return A pair (`chunk_ndx`, `chunk_size`) where `chunk_ndx`
    /// is the index of a chunk whose size is at least the requestd
    /// size, and `chunk_size` is the size of that chunk.
    std::pair<size_t, size_t> reserve_free_space(size_t size);

    /// Search only a range of the free list for a block as big as the
    /// specified size. Return a pair with index and size of the found chunk.
    /// \param found indicates whether a suitable block was found.
    std::pair<size_t, size_t> search_free_space_in_part_of_freelist(size_t size, size_t begin, size_t end,
                                                                    bool& found);

    /// Extend the file to ensure that a chunk of free space of the
    /// specified size is available. The specified size does not need
    /// to be 8-byte aligned. This function guarantees that it will
    /// add at most one entry to the free-lists.
    ///
    /// \return A pair (`chunk_ndx`, `chunk_size`) where `chunk_ndx`
    /// is the index of a chunk whose size is at least the requestd
    /// size, and `chunk_size` is the size of that chunk.
    std::pair<size_t, size_t> extend_free_space(size_t requested_size);

    void write_array_at(MapWindow* window, ref_type, const char* data, size_t size);
    size_t split_freelist_chunk(size_t index, size_t start_pos, size_t alloc_pos, size_t chunk_size, bool is_shared);
};


// Implementation:

inline void GroupWriter::set_versions(uint64_t current, uint64_t read_lock) noexcept
{
    REALM_ASSERT(read_lock <= current);
    m_current_version = current;
    m_readlock_version = read_lock;
}

} // namespace realm

#endif // REALM_GROUP_WRITER_HPP
