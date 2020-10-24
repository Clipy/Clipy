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

#ifndef REALM_ALLOC_HPP
#define REALM_ALLOC_HPP

#include <cstdint>
#include <cstddef>
#include <atomic>

#include <realm/util/features.h>
#include <realm/util/terminate.hpp>
#include <realm/util/assert.hpp>
#include <realm/util/file.hpp>
#include <realm/exceptions.hpp>
#include <realm/util/safe_int_ops.hpp>
#include <realm/node_header.hpp>
#include <realm/util/file_mapper.hpp>

// Temporary workaround for
// https://developercommunity.visualstudio.com/content/problem/994075/64-bit-atomic-load-ices-cl-1924-with-o2-ob1.html
#if defined REALM_ARCHITECTURE_X86_32 && defined REALM_WINDOWS
#define REALM_WORKAROUND_MSVC_BUG REALM_NOINLINE
#else
#define REALM_WORKAROUND_MSVC_BUG
#endif

namespace realm {

class Allocator;

using ref_type = size_t;

int_fast64_t from_ref(ref_type) noexcept;
ref_type to_ref(int_fast64_t) noexcept;
int64_t to_int64(size_t value) noexcept;

class MemRef {
public:
    MemRef() noexcept;
    ~MemRef() noexcept;

    MemRef(char* addr, ref_type ref, Allocator& alloc) noexcept;
    MemRef(ref_type ref, Allocator& alloc) noexcept;

    char* get_addr() const;
    ref_type get_ref() const;
    void set_ref(ref_type ref);
    void set_addr(char* addr);

private:
    char* m_addr;
    ref_type m_ref;
#if REALM_ENABLE_MEMDEBUG
    // Allocator that created m_ref. Used to verify that the ref is valid whenever you call
    // get_ref()/get_addr and that it e.g. has not been free'ed
    const Allocator* m_alloc = nullptr;
#endif
};


/// The common interface for Realm allocators.
///
/// A Realm allocator must associate a 'ref' to each allocated
/// object and be able to efficiently map any 'ref' to the
/// corresponding memory address. The 'ref' is an integer and it must
/// always be divisible by 8. Also, a value of zero is used to
/// indicate a null-reference, and must therefore never be returned by
/// Allocator::alloc().
///
/// The purpose of the 'refs' is to decouple the memory reference from
/// the actual address and thereby allowing objects to be relocated in
/// memory without having to modify stored references.
///
/// \sa SlabAlloc
class Allocator {
public:
    /// The specified size must be divisible by 8, and must not be
    /// zero.
    ///
    /// \throw std::bad_alloc If insufficient memory was available.
    MemRef alloc(size_t size);

    /// Calls do_realloc().
    ///
    /// Note: The underscore has been added because the name `realloc`
    /// would conflict with a macro on the Windows platform.
    MemRef realloc_(ref_type, const char* addr, size_t old_size, size_t new_size);

    /// Calls do_free().
    ///
    /// Note: The underscore has been added because the name `free
    /// would conflict with a macro on the Windows platform.
    void free_(ref_type, const char* addr) noexcept;

    /// Shorthand for free_(mem.get_ref(), mem.get_addr()).
    void free_(MemRef mem) noexcept;

    /// Calls do_translate().
    char* translate(ref_type ref) const noexcept;

    /// Returns true if, and only if the object at the specified 'ref'
    /// is in the immutable part of the memory managed by this
    /// allocator. The method by which some objects become part of the
    /// immuatble part is entirely up to the class that implements
    /// this interface.
    bool is_read_only(ref_type) const noexcept;

    void set_read_only(bool ro)
    {
        m_is_read_only = ro;
    }
    /// Returns a simple allocator that can be used with free-standing
    /// Realm objects (such as a free-standing table). A
    /// free-standing object is one that is not part of a Group, and
    /// therefore, is not part of an actual database.
    static Allocator& get_default() noexcept;

    virtual ~Allocator() noexcept;

    // Disable copying. Copying an allocator can produce double frees.
    Allocator(const Allocator&) = delete;
    Allocator& operator=(const Allocator&) = delete;

    virtual void verify() const = 0;

#ifdef REALM_DEBUG
    /// Terminate the program precisely when the specified 'ref' is
    /// freed (or reallocated). You can use this to detect whether the
    /// ref is freed (or reallocated), and even to get a stacktrace at
    /// the point where it happens. Call watch(0) to stop watching
    /// that ref.
    void watch(ref_type ref)
    {
        m_debug_watch = ref;
    }
#endif

    struct MappedFile;

protected:
    constexpr static int section_shift = 26;

    std::atomic<size_t> m_baseline; // Separation line between immutable and mutable refs.

    ref_type m_debug_watch = 0;

    // The following logically belongs in the slab allocator, but is placed
    // here to optimize a critical path:

    // The ref translation splits the full ref-space (both below and above baseline)
    // into equal chunks.
    struct RefTranslation {
        char* mapping_addr;
#if REALM_ENABLE_ENCRYPTION
        util::EncryptedFileMapping* encrypted_mapping;
#endif
    };
    // This pointer may be changed concurrently with access, so make sure it is
    // atomic!
    std::atomic<RefTranslation*> m_ref_translation_ptr;

    /// The specified size must be divisible by 8, and must not be
    /// zero.
    ///
    /// \throw std::bad_alloc If insufficient memory was available.
    virtual MemRef do_alloc(const size_t size) = 0;

    /// The specified size must be divisible by 8, and must not be
    /// zero.
    ///
    /// The default version of this function simply allocates a new
    /// chunk of memory, copies over the old contents, and then frees
    /// the old chunk.
    ///
    /// \throw std::bad_alloc If insufficient memory was available.
    virtual MemRef do_realloc(ref_type, char* addr, size_t old_size, size_t new_size) = 0;

    /// Release the specified chunk of memory.
    virtual void do_free(ref_type, char* addr) = 0;

    /// Map the specified \a ref to the corresponding memory
    /// address. Note that if is_read_only(ref) returns true, then the
    /// referenced object is to be considered immutable, and it is
    /// then entirely the responsibility of the caller that the memory
    /// is not modified by way of the returned memory pointer.
    virtual char* do_translate(ref_type ref) const noexcept = 0;

    Allocator() noexcept;
    size_t get_section_index(size_t pos) const noexcept;
    inline size_t get_section_base(size_t index) const noexcept;


    // The following counters are used to ensure accessor refresh,
    // and allows us to report many errors related to attempts to
    // access data which is no longer current.
    //
    // * storage_versioning: monotonically increasing counter
    //   bumped whenever the underlying storage layout is changed,
    //   or if the owning accessor have been detached.
    // * content_versioning: monotonically increasing counter
    //   bumped whenever the data is changed. Used to detect
    //   if queries are stale.
    // * instance_versioning: monotonically increasing counter
    //   used to detect if the allocator (and owning structure, e.g. Table)
    //   is recycled. Mismatch on this counter will cause accesors
    //   lower in the hierarchy to throw if access is attempted.
    std::atomic<uint_fast64_t> m_content_versioning_counter;

    std::atomic<uint_fast64_t> m_storage_versioning_counter;

    std::atomic<uint_fast64_t> m_instance_versioning_counter;

    inline uint_fast64_t get_storage_version(uint64_t instance_version)
    {
        if (instance_version != m_instance_versioning_counter) {
            throw LogicError(LogicError::detached_accessor);
        }
        return m_storage_versioning_counter.load(std::memory_order_acquire);
    }

    inline uint_fast64_t get_storage_version()
    {
        return m_storage_versioning_counter.load(std::memory_order_acquire);
    }

    inline void bump_storage_version() noexcept
    {
        m_storage_versioning_counter.fetch_add(1, std::memory_order_acq_rel);
    }

    REALM_WORKAROUND_MSVC_BUG inline uint_fast64_t get_content_version() noexcept
    {
        return m_content_versioning_counter.load(std::memory_order_acquire);
    }

    inline uint_fast64_t bump_content_version() noexcept
    {
        return m_content_versioning_counter.fetch_add(1, std::memory_order_acq_rel) + 1;
    }

    REALM_WORKAROUND_MSVC_BUG inline uint_fast64_t get_instance_version() noexcept
    {
        return m_instance_versioning_counter.load(std::memory_order_relaxed);
    }

    inline void bump_instance_version() noexcept
    {
        m_instance_versioning_counter.fetch_add(1, std::memory_order_relaxed);
    }

private:
    bool m_is_read_only = false; // prevent any alloc or free operations

    friend class Table;
    friend class ClusterTree;
    friend class Group;
    friend class WrappedAllocator;
    friend class ConstObj;
    friend class Obj;
    friend class ConstLstBase;
};


class WrappedAllocator : public Allocator {
public:
    WrappedAllocator(Allocator& underlying_allocator)
        : m_alloc(&underlying_allocator)
    {
        m_baseline.store(m_alloc->m_baseline, std::memory_order_relaxed);
        m_debug_watch = 0;
        m_ref_translation_ptr.store(m_alloc->m_ref_translation_ptr);
    }

    ~WrappedAllocator()
    {
    }

    void switch_underlying_allocator(Allocator& underlying_allocator)
    {
        m_alloc = &underlying_allocator;
        m_baseline.store(m_alloc->m_baseline, std::memory_order_relaxed);
        m_debug_watch = 0;
        m_ref_translation_ptr.store(m_alloc->m_ref_translation_ptr);
    }

    void update_from_underlying_allocator(bool writable)
    {
        switch_underlying_allocator(*m_alloc);
        set_read_only(!writable);
    }

private:
    Allocator* m_alloc;
    MemRef do_alloc(const size_t size) override
    {
        auto result = m_alloc->do_alloc(size);
        bump_storage_version();
        m_baseline.store(m_alloc->m_baseline, std::memory_order_relaxed);
        m_ref_translation_ptr.store(m_alloc->m_ref_translation_ptr);
        return result;
    }
    virtual MemRef do_realloc(ref_type ref, char* addr, size_t old_size, size_t new_size) override
    {
        auto result = m_alloc->do_realloc(ref, addr, old_size, new_size);
        bump_storage_version();
        m_baseline.store(m_alloc->m_baseline, std::memory_order_relaxed);
        m_ref_translation_ptr.store(m_alloc->m_ref_translation_ptr);
        return result;
    }

    virtual void do_free(ref_type ref, char* addr) noexcept override
    {
        return m_alloc->do_free(ref, addr);
    }

    virtual char* do_translate(ref_type ref) const noexcept override
    {
        return m_alloc->translate(ref);
    }

    virtual void verify() const override
    {
        m_alloc->verify();
    }
};


// Implementation:

inline int_fast64_t from_ref(ref_type v) noexcept
{
    // Check that v is divisible by 8 (64-bit aligned).
    REALM_ASSERT_DEBUG(v % 8 == 0);

    static_assert(std::is_same<ref_type, size_t>::value,
                  "If ref_type changes, from_ref and to_ref should probably be updated");

    // Make sure that we preserve the bit pattern of the ref_type (without sign extension).
    return util::from_twos_compl<int_fast64_t>(uint_fast64_t(v));
}

inline ref_type to_ref(int_fast64_t v) noexcept
{
    // Check that v is divisible by 8 (64-bit aligned).
    REALM_ASSERT_DEBUG(v % 8 == 0);

    // C++11 standard, paragraph 4.7.2 [conv.integral]:
    // If the destination type is unsigned, the resulting value is the least unsigned integer congruent to the source
    // integer (modulo 2n where n is the number of bits used to represent the unsigned type). [ Note: In a two's
    // complement representation, this conversion is conceptual and there is no change in the bit pattern (if there is
    // no truncation). - end note ]
    static_assert(std::is_unsigned<ref_type>::value,
                  "If ref_type changes, from_ref and to_ref should probably be updated");
    return ref_type(v);
}

inline int64_t to_int64(size_t value) noexcept
{
    //    FIXME: Enable once we get clang warning flags correct
    //    REALM_ASSERT_DEBUG(value <= std::numeric_limits<int64_t>::max());
    return static_cast<int64_t>(value);
}


inline MemRef::MemRef() noexcept
    : m_addr(nullptr)
    , m_ref(0)
{
}

inline MemRef::~MemRef() noexcept
{
}

inline MemRef::MemRef(char* addr, ref_type ref, Allocator& alloc) noexcept
    : m_addr(addr)
    , m_ref(ref)
{
    static_cast<void>(alloc);
#if REALM_ENABLE_MEMDEBUG
    m_alloc = &alloc;
#endif
}

inline MemRef::MemRef(ref_type ref, Allocator& alloc) noexcept
    : m_addr(alloc.translate(ref))
    , m_ref(ref)
{
    static_cast<void>(alloc);
#if REALM_ENABLE_MEMDEBUG
    m_alloc = &alloc;
#endif
}

inline char* MemRef::get_addr() const
{
#if REALM_ENABLE_MEMDEBUG
    // Asserts if the ref has been freed
    m_alloc->translate(m_ref);
#endif
    return m_addr;
}

inline ref_type MemRef::get_ref() const
{
#if REALM_ENABLE_MEMDEBUG
    // Asserts if the ref has been freed
    m_alloc->translate(m_ref);
#endif
    return m_ref;
}

inline void MemRef::set_ref(ref_type ref)
{
#if REALM_ENABLE_MEMDEBUG
    // Asserts if the ref has been freed
    m_alloc->translate(ref);
#endif
    m_ref = ref;
}

inline void MemRef::set_addr(char* addr)
{
    m_addr = addr;
}

inline MemRef Allocator::alloc(size_t size)
{
    if (m_is_read_only)
        throw realm::LogicError(realm::LogicError::wrong_transact_state);
    return do_alloc(size);
}

inline MemRef Allocator::realloc_(ref_type ref, const char* addr, size_t old_size, size_t new_size)
{
#ifdef REALM_DEBUG
    if (ref == m_debug_watch)
        REALM_TERMINATE("Allocator watch: Ref was reallocated");
#endif
    if (m_is_read_only)
        throw realm::LogicError(realm::LogicError::wrong_transact_state);
    return do_realloc(ref, const_cast<char*>(addr), old_size, new_size);
}

inline void Allocator::free_(ref_type ref, const char* addr) noexcept
{
#ifdef REALM_DEBUG
    if (ref == m_debug_watch)
        REALM_TERMINATE("Allocator watch: Ref was freed");
#endif
    REALM_ASSERT(!m_is_read_only);

    return do_free(ref, const_cast<char*>(addr));
}

inline void Allocator::free_(MemRef mem) noexcept
{
    free_(mem.get_ref(), mem.get_addr());
}

inline size_t Allocator::get_section_base(size_t index) const noexcept
{
    return index << section_shift; // 64MB chunks
}

inline size_t Allocator::get_section_index(size_t pos) const noexcept
{
    return pos >> section_shift; // 64Mb chunks
}

inline bool Allocator::is_read_only(ref_type ref) const noexcept
{
    REALM_ASSERT_DEBUG(ref != 0);
    // REALM_ASSERT_DEBUG(m_baseline != 0); // Attached SlabAlloc
    return ref < m_baseline.load(std::memory_order_relaxed);
}

inline Allocator::Allocator() noexcept
{
    m_content_versioning_counter = 0;
    m_storage_versioning_counter = 0;
    m_instance_versioning_counter = 0;
    m_ref_translation_ptr = nullptr;
}

inline Allocator::~Allocator() noexcept
{
}

inline char* Allocator::translate(ref_type ref) const noexcept
{
    if (auto ref_translation_ptr = m_ref_translation_ptr.load(std::memory_order_acquire)) {
        char* base_addr;
        size_t idx = get_section_index(ref);
        base_addr = ref_translation_ptr[idx].mapping_addr;
        size_t offset = ref - get_section_base(idx);
        auto addr = base_addr + offset;
#if REALM_ENABLE_ENCRYPTION
        realm::util::encryption_read_barrier(addr, NodeHeader::header_size,
                                             ref_translation_ptr[idx].encrypted_mapping,
                                             NodeHeader::get_byte_size_from_header);
#endif
        return addr;
    }
    else
        return do_translate(ref);
}

} // namespace realm

#endif // REALM_ALLOC_HPP
