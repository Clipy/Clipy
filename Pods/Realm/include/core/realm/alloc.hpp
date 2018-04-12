﻿/*************************************************************************
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

namespace realm {

class Allocator;

class Replication;

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

    char* get_addr();
    ref_type get_ref();
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

    Replication* get_replication() noexcept;

protected:
    size_t m_baseline = 0; // Separation line between immutable and mutable refs.

    Replication* m_replication = nullptr;

    ref_type m_debug_watch = 0;

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
    virtual MemRef do_realloc(ref_type, const char* addr, size_t old_size, size_t new_size) = 0;

    /// Release the specified chunk of memory.
    virtual void do_free(ref_type, const char* addr) noexcept = 0;

    /// Map the specified \a ref to the corresponding memory
    /// address. Note that if is_read_only(ref) returns true, then the
    /// referenced object is to be considered immutable, and it is
    /// then entirely the responsibility of the caller that the memory
    /// is not modified by way of the returned memory pointer.
    virtual char* do_translate(ref_type ref) const noexcept = 0;

    Allocator() noexcept;

    // FIXME: This really doesn't belong in an allocator, but it is the best
    // place for now, because every table has a pointer leading here. It would
    // be more obvious to place it in Group, but that would add a runtime overhead,
    // and access is time critical.
    //
    // This means that multiple threads that allocate Realm objects through the
    // default allocator will share this variable, which is a logical design flaw
    // that can make sync_if_needed() re-run queries even though it is not required.
    // It must be atomic because it's shared.
    std::atomic<uint_fast64_t> m_table_versioning_counter;

    /// Bump the global version counter. This method should be called when
    /// version bumping is initiated. Then following calls to should_propagate_version()
    /// can be used to prune the version bumping.
    void bump_global_version() noexcept;

    /// Determine if the "local_version" is out of sync, so that it should
    /// be updated. In that case: also update it. Called from Table::bump_version
    /// to control propagation of version updates on tables within the group.
    bool should_propagate_version(uint_fast64_t& local_version) noexcept;

    friend class Table;
    friend class Group;
};

inline void Allocator::bump_global_version() noexcept
{
    m_table_versioning_counter += 1;
}


inline bool Allocator::should_propagate_version(uint_fast64_t& local_version) noexcept
{
    if (local_version != m_table_versioning_counter) {
        local_version = m_table_versioning_counter;
        return true;
    }
    else {
        return false;
    }
}


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

inline char* MemRef::get_addr()
{
#if REALM_ENABLE_MEMDEBUG
    // Asserts if the ref has been freed
    m_alloc->translate(m_ref);
#endif
    return m_addr;
}

inline ref_type MemRef::get_ref()
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
    return do_alloc(size);
}

inline MemRef Allocator::realloc_(ref_type ref, const char* addr, size_t old_size, size_t new_size)
{
#ifdef REALM_DEBUG
    if (ref == m_debug_watch)
        REALM_TERMINATE("Allocator watch: Ref was reallocated");
#endif
    return do_realloc(ref, addr, old_size, new_size);
}

inline void Allocator::free_(ref_type ref, const char* addr) noexcept
{
#ifdef REALM_DEBUG
    if (ref == m_debug_watch)
        REALM_TERMINATE("Allocator watch: Ref was freed");
#endif
    return do_free(ref, addr);
}

inline void Allocator::free_(MemRef mem) noexcept
{
    free_(mem.get_ref(), mem.get_addr());
}

inline char* Allocator::translate(ref_type ref) const noexcept
{
    return do_translate(ref);
}

inline bool Allocator::is_read_only(ref_type ref) const noexcept
{
    REALM_ASSERT_DEBUG(ref != 0);
    REALM_ASSERT_DEBUG(m_baseline != 0); // Attached SlabAlloc
    return ref < m_baseline;
}

inline Allocator::Allocator() noexcept
{
    m_table_versioning_counter = 0;
}

inline Allocator::~Allocator() noexcept
{
}

inline Replication* Allocator::get_replication() noexcept
{
    return m_replication;
}

} // namespace realm

#endif // REALM_ALLOC_HPP
