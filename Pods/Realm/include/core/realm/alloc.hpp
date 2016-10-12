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
#ifndef REALM_ALLOC_HPP
#define REALM_ALLOC_HPP

#include <stdint.h>
#include <cstddef>

#include <realm/util/features.h>
#include <realm/util/terminate.hpp>
#include <realm/util/assert.hpp>
#include <realm/util/safe_int_ops.hpp>

namespace realm {

class Allocator;

class Replication;

using ref_type = size_t;

int_fast64_t from_ref(ref_type) noexcept;
ref_type to_ref(int_fast64_t) noexcept;

class MemRef {
public:
    char* m_addr;
    ref_type m_ref;

    MemRef() noexcept;
    ~MemRef() noexcept;
    MemRef(char* addr, ref_type ref) noexcept;
    MemRef(ref_type ref, Allocator& alloc) noexcept;
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
    MemRef realloc_(ref_type, const char* addr, size_t old_size,
                    size_t new_size);

    /// Calls do_free().
    ///
    /// Note: The underscore has been added because the name `free
    /// would conflict with a macro on the Windows platform.
    void free_(ref_type, const char* addr) noexcept;

    /// Shorthand for free_(mem.m_ref, mem.m_addr).
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

#ifdef REALM_DEBUG
    virtual void verify() const = 0;

    /// Terminate the program precisely when the specified 'ref' is
    /// freed (or reallocated). You can use this to detect whether the
    /// ref is freed (or reallocated), and even to get a stacktrace at
    /// the point where it happens. Call watch(0) to stop watching
    /// that ref.
    void watch(ref_type);
#endif

    Replication* get_replication() noexcept;

    /// \brief The version of the format of the the node structure (in file or
    /// in memory) in use by Realm objects associated with this allocator.
    ///
    /// Every allocator contains a file format version field, which is returned
    /// by this function. In some cases (as mentioned below) the file format can
    /// change.
    ///
    /// A value of zero means the the file format is not yet decided. This is
    /// only possible for empty Realms where top-ref is zero.
    ///
    /// For the default allocator (get_default()), the file format version field
    /// can never change, is never zero, and is set to whatever
    /// Group::get_target_file_format_version_for_session() would return if the
    /// original file format version was undecided and the request history type
    /// was Replication::hist_None.
    ///
    /// For the slab allocator (AllocSlab), the file format version field is set
    /// to the file format version specified by the attached file (or attached
    /// memory buffer) at the time of attachment. If no file (or buffer) is
    /// currently attached, the returned value has no meaning. If the Realm file
    /// format is later upgraded, the file form,at version filed must be updated
    /// to reflect that fact.
    ///
    /// In shared mode (when a Realm file is opened via a SharedGroup instance)
    /// it can happen that the file format is upgraded asyncronously (via
    /// another SharedGroup instance), and in that case the file format version
    /// field of the allocator can get out of date, but only for a short
    /// while. It is always garanteed to be, and remain up to date after the
    /// opening process completes (when SharedGroup::do_open() returns).
    ///
    /// An empty Realm file (one whose top-ref is zero) may specify a file
    /// format version of zero to indicate that the format is not yet
    /// decided. In that case, this function will return zero immediately after
    /// AllocSlab::attach_file() returns. It shall be guaranteed, however, that
    /// the zero is changed to a proper file format version before the opening
    /// process completes (Group::open() or SharedGroup::open()). It is the duty
    /// of the caller of AllocSlab::attach_file() to ensure this.
    ///
    /// File format versions:
    ///
    ///   1 Initial file format version
    ///
    ///   2 FIXME: Does anybody remember what happened here?
    ///
    ///   3 Supporting null on string columns broke the file format in following
    ///     way: Index appends an 'X' character to all strings except the null
    ///     string, to be able to distinguish between null and empty
    ///     string. Bumped to 3 because of null support of String columns and
    ///     because of new format of index.
    ///
    ///   4 Introduction of optional in-Realm history of changes (additional
    ///     entries in Group::m_top). Since this change is not forward
    ///     compatible, the file format version had to be bumped. This change is
    ///     implemented in a way that achieves backwards compatibility with
    ///     version 3 (and in turn with version 2).
    ///
    ///   5 Introduced the new Timestamp column type that replaces DateTime.
    ///     When opening an older database file, all DateTime columns will be
    ///     automatically upgraded Timestamp columns.
    ///
    /// IMPORTANT: When introducing a new file format version, be sure to review
    /// the file validity checks in AllocSlab::validate_buffer(), the file
    /// format selection loginc in
    /// Group::get_target_file_format_version_for_session(), and the file format
    /// upgrade logic in Group::upgrade_file_format().
    int get_file_format_version() const noexcept;

protected:
    size_t m_baseline = 0; // Separation line between immutable and mutable refs.

    Replication* m_replication;

    /// See get_file_format_version().
    int m_file_format_version = 0;

#ifdef REALM_DEBUG
    ref_type m_watch;
#endif

    /// The specified size must be divisible by 8, and must not be
    /// zero.
    ///
    /// \throw std::bad_alloc If insufficient memory was available.
    virtual MemRef do_alloc(size_t size) = 0;

    /// The specified size must be divisible by 8, and must not be
    /// zero.
    ///
    /// The default version of this function simply allocates a new
    /// chunk of memory, copies over the old contents, and then frees
    /// the old chunk.
    ///
    /// \throw std::bad_alloc If insufficient memory was available.
    virtual MemRef do_realloc(ref_type, const char* addr, size_t old_size,
                              size_t new_size) = 0;

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
    uint_fast64_t m_table_versioning_counter;

    /// Bump the global version counter. This method should be called when
    /// version bumping is initiated. Then following calls to should_propagate_version()
    /// can be used to prune the version bumping.
    uint_fast64_t bump_global_version() noexcept;

    /// Determine if the "local_version" is out of sync, so that it should
    /// be updated. In that case: also update it. Called from Table::bump_version
    /// to control propagation of version updates on tables within the group.
    bool should_propagate_version(uint_fast64_t& local_version) noexcept;

    friend class Table;
    friend class Group;
};

inline uint_fast64_t Allocator::bump_global_version() noexcept
{
    ++m_table_versioning_counter;
    return m_table_versioning_counter;
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
    return util::from_twos_compl<int_fast64_t>(v);
}

inline ref_type to_ref(int_fast64_t v) noexcept
{
    REALM_ASSERT_DEBUG(!util::int_cast_has_overflow<ref_type>(v));
    // Check that v is divisible by 8 (64-bit aligned).
    REALM_ASSERT_DEBUG(v % 8 == 0);
    return ref_type(v);
}

inline MemRef::MemRef() noexcept:
    m_addr(nullptr),
    m_ref(0)
{
}

inline MemRef::~MemRef() noexcept
{
}

inline MemRef::MemRef(char* addr, ref_type ref) noexcept:
    m_addr(addr),
    m_ref(ref)
{
}

inline MemRef::MemRef(ref_type ref, Allocator& alloc) noexcept:
    m_addr(alloc.translate(ref)),
    m_ref(ref)
{
}


inline MemRef Allocator::alloc(size_t size)
{
    return do_alloc(size);
}

inline MemRef Allocator::realloc_(ref_type ref, const char* addr, size_t old_size,
                                  size_t new_size)
{
#ifdef REALM_DEBUG
    if (ref == m_watch)
        REALM_TERMINATE("Allocator watch: Ref was reallocated");
#endif
    return do_realloc(ref, addr, old_size, new_size);
}

inline void Allocator::free_(ref_type ref, const char* addr) noexcept
{
#ifdef REALM_DEBUG
    if (ref == m_watch)
        REALM_TERMINATE("Allocator watch: Ref was freed");
#endif
    return do_free(ref, addr);
}

inline void Allocator::free_(MemRef mem) noexcept
{
    free_(mem.m_ref, mem.m_addr);
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

inline Allocator::Allocator() noexcept:
    m_replication(nullptr)
{
#ifdef REALM_DEBUG
    m_watch = 0;
#endif
    m_table_versioning_counter = 0;
}

inline Allocator::~Allocator() noexcept
{
}

inline Replication* Allocator::get_replication() noexcept
{
    return m_replication;
}

#ifdef REALM_DEBUG
inline void Allocator::watch(ref_type ref)
{
    m_watch = ref;
}
#endif

inline int Allocator::get_file_format_version() const noexcept
{
    return m_file_format_version;
}


} // namespace realm

#endif // REALM_ALLOC_HPP
