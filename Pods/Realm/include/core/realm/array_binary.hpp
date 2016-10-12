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
#ifndef REALM_ARRAY_BINARY_HPP
#define REALM_ARRAY_BINARY_HPP

#include <realm/binary_data.hpp>
#include <realm/array_blob.hpp>
#include <realm/array_integer.hpp>
#include <realm/exceptions.hpp>

namespace realm {

/*
STORAGE FORMAT
---------------------------------------------------------------------------------------
ArrayBinary stores binary elements using two ArrayInteger and one ArrayBlob. The ArrayBlob can only store one
single concecutive array of bytes (contrary to its 'Array' name that misleadingly indicates it could store multiple
elements).

Assume we have the strings "a", "", "abc", null, "ab". Then the three arrays will contain:

ArrayInteger    m_offsets   1, 1, 5, 5, 6
ArrayBlob       m_blob      aabcab
ArrayInteger    m_nulls     0, 0, 0, 1, 0 // 1 indicates null, 0 indicates non-null

So for each element the ArrayInteger, the ArrayInteger points into the ArrayBlob at the position of the first
byte of the next element.

m_nulls is always present (except for old database files; see below), so any ArrayBinary is always nullable!
The nullable property (such as throwing exception upon set(null) on non-nullable column, etc) is handled on
column level only.

DATABASE FILE VERSION CHANGES
---------------------------------------------------------------------------------------
Old database files do not have any m_nulls array. To be backwardscompatible, many methods will have tests like
`if(Array::size() == 3)` and have a backwards compatible code paths for these (e.g. avoid writing to m_nulls
in set(), etc). This way no file format upgrade is needed to support nulls for BinaryData.
*/

class ArrayBinary: public Array {
public:
    explicit ArrayBinary(Allocator&) noexcept;
    ~ArrayBinary() noexcept override {}

    /// Create a new empty binary array and attach this accessor to
    /// it. This does not modify the parent reference information of
    /// this accessor.
    ///
    /// Note that the caller assumes ownership of the allocated
    /// underlying node. It is not owned by the accessor.
    void create();

    // Old database files will not have the m_nulls array, so we need code paths for
    // backwards compatibility for these cases.
    bool legacy_array_type() const noexcept;

    //@{
    /// Overriding functions of Array
    void init_from_ref(ref_type) noexcept;
    void init_from_mem(MemRef) noexcept;
    void init_from_parent() noexcept;
    //@}

    bool is_empty() const noexcept;
    size_t size() const noexcept;

    BinaryData get(size_t ndx) const noexcept;

    void add(BinaryData value, bool add_zero_term = false);
    void set(size_t ndx, BinaryData value, bool add_zero_term = false);
    void insert(size_t ndx, BinaryData value, bool add_zero_term = false);
    void erase(size_t ndx);
    void truncate(size_t size);
    void clear();
    void destroy();

    /// Get the specified element without the cost of constructing an
    /// array instance. If an array instance is already available, or
    /// you need to get multiple values, then this method will be
    /// slower.
    static BinaryData get(const char* header, size_t ndx, Allocator&) noexcept;

    ref_type bptree_leaf_insert(size_t ndx, BinaryData, bool add_zero_term,
                                TreeInsertBase& state);

    static size_t get_size_from_header(const char*, Allocator&) noexcept;

    /// Construct a binary array of the specified size and return just
    /// the reference to the underlying memory. All elements will be
    /// initialized to the binary value `defaults`, which can be either
    /// null or zero-length non-null (value with size > 0 is not allowed as
    /// initialization value).
    static MemRef create_array(size_t size, Allocator&, BinaryData defaults);

    /// Construct a copy of the specified slice of this binary array
    /// using the specified target allocator.
    MemRef slice(size_t offset, size_t size, Allocator& target_alloc) const;

#ifdef REALM_DEBUG
    void to_dot(std::ostream&, bool is_strings, StringData title = StringData()) const;
#endif
    bool update_from_parent(size_t old_baseline) noexcept;

private:
    ArrayInteger m_offsets;
    ArrayBlob m_blob;
    ArrayInteger m_nulls;
};





// Implementation:

inline ArrayBinary::ArrayBinary(Allocator& alloc) noexcept:
    Array(alloc), m_offsets(alloc), m_blob(alloc),
    m_nulls(alloc)
{
    m_offsets.set_parent(this, 0);
    m_blob.set_parent(this, 1);
    m_nulls.set_parent(this, 2);
}

inline void ArrayBinary::create()
{
    size_t size = 0;
    BinaryData defaults = BinaryData(0, 0); // This init value is ignored because size = 0
    MemRef mem = create_array(size, get_alloc(), defaults); // Throws
    init_from_mem(mem);
}

inline void ArrayBinary::init_from_ref(ref_type ref) noexcept
{
    REALM_ASSERT(ref);
    char* header = get_alloc().translate(ref);
    init_from_mem(MemRef(header, ref));
}

inline void ArrayBinary::init_from_parent() noexcept
{
    ref_type ref = get_ref_from_parent();
    init_from_ref(ref);
}

inline bool ArrayBinary::is_empty() const noexcept
{
    return m_offsets.is_empty();
}

// Old database files will not have the m_nulls array, so we need code paths for
// backwards compatibility for these cases. We can test if m_nulls exists by looking
// at number of references in this ArrayBinary.
inline bool ArrayBinary::legacy_array_type() const noexcept
{
    if (Array::size() == 3)
        return false;               // New database file
    else if (Array::size() == 2)
        return true;                // Old database file
    else
        REALM_ASSERT(false);        // Should never happen
    return false;
}

inline size_t ArrayBinary::size() const noexcept
{
    return m_offsets.size();
}

inline BinaryData ArrayBinary::get(size_t ndx) const noexcept
{
    REALM_ASSERT_3(ndx, <, m_offsets.size());

    if (!legacy_array_type() && m_nulls.get(ndx)) {
        return BinaryData();
    }
    else {
        size_t begin = ndx ? to_size_t(m_offsets.get(ndx - 1)) : 0;
        size_t end = to_size_t(m_offsets.get(ndx));

        BinaryData bd = BinaryData(m_blob.get(begin), end - begin);
        // Old database file (non-nullable column should never return null)
        REALM_ASSERT(!bd.is_null());
        return bd;
    }
}

inline void ArrayBinary::truncate(size_t size)
{
    REALM_ASSERT_3(size, <, m_offsets.size());

    size_t blob_size = size ? to_size_t(m_offsets.get(size-1)) : 0;

    m_offsets.truncate(size);
    m_blob.truncate(blob_size);
    if (!legacy_array_type())
        m_nulls.truncate(size);
}

inline void ArrayBinary::clear()
{
    m_blob.clear();
    m_offsets.clear();
    if (!legacy_array_type())
        m_nulls.clear();
}

inline void ArrayBinary::destroy()
{
    m_blob.destroy();
    m_offsets.destroy();
    if (!legacy_array_type())
        m_nulls.destroy();
    Array::destroy();
}

inline size_t ArrayBinary::get_size_from_header(const char* header,
                                                     Allocator& alloc) noexcept
{
    ref_type offsets_ref = to_ref(Array::get(header, 0));
    const char* offsets_header = alloc.translate(offsets_ref);
    return Array::get_size_from_header(offsets_header);
}

inline bool ArrayBinary::update_from_parent(size_t old_baseline) noexcept
{
    bool res = Array::update_from_parent(old_baseline);
    if (res) {
        m_blob.update_from_parent(old_baseline);
        m_offsets.update_from_parent(old_baseline);
        if (!legacy_array_type())
            m_nulls.update_from_parent(old_baseline);
    }
    return res;
}

} // namespace realm

#endif // REALM_ARRAY_BINARY_HPP
