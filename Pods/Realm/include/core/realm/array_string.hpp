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
#ifndef REALM_ARRAY_STRING_HPP
#define REALM_ARRAY_STRING_HPP

#include <realm/array.hpp>

namespace realm {

/*
ArrayString stores strings as a concecutive list of fixed-length blocks of m_width bytes. The
longest string it can store is (m_width - 1) bytes before it needs to expand.

An example of the format for m_width = 4 is following sequence of bytes, where x is payload:

xxx0 xx01 x002 0003 0004 (strings "xxx",. "xx", "x", "", realm::null())

So each string is 0 terminated, and the last byte in a block tells how many 0s are present, except
for a realm::null() which has the byte set to m_width (4). The byte is used to compute the length of a string
in various functions.

New: If m_witdh = 0, then all elements are realm::null(). So to add an empty string we must expand m_width
New: StringData is null() if-and-only-if StringData::data() == 0.
*/

class ArrayString: public Array {
public:
    static const size_t max_width = 64;

    typedef StringData value_type;
    // Constructor defaults to non-nullable because we use non-nullable ArrayString so many places internally in core
    // (data which isn't user payload) where null isn't needed.
    explicit ArrayString(Allocator&, bool nullable = false) noexcept;
    explicit ArrayString(no_prealloc_tag) noexcept;
    ~ArrayString() noexcept override {}

    bool is_null(size_t ndx) const;
    void set_null(size_t ndx);
    StringData get(size_t ndx) const noexcept;
    void add();
    void add(StringData value);
    void set(size_t ndx, StringData value);
    void insert(size_t ndx, StringData value);
    void erase(size_t ndx);

    size_t count(StringData value, size_t begin = 0,
                      size_t end = npos) const noexcept;
    size_t find_first(StringData value, size_t begin = 0,
                           size_t end = npos) const noexcept;
    void find_all(IntegerColumn& result, StringData value, size_t add_offset = 0,
                  size_t begin = 0, size_t end = npos);

    /// Compare two string arrays for equality.
    bool compare_string(const ArrayString&) const noexcept;

    /// Get the specified element without the cost of constructing an
    /// array instance. If an array instance is already available, or
    /// you need to get multiple values, then this method will be
    /// slower.
    static StringData get(const char* header, size_t ndx, bool nullable) noexcept;

    ref_type bptree_leaf_insert(size_t ndx, StringData, TreeInsertBase& state);

    /// Construct a string array of the specified size and return just
    /// the reference to the underlying memory. All elements will be
    /// initialized to the empty string.
    static MemRef create_array(size_t size, Allocator&);

    /// Create a new empty string array and attach this accessor to
    /// it. This does not modify the parent reference information of
    /// this accessor.
    ///
    /// Note that the caller assumes ownership of the allocated
    /// underlying node. It is not owned by the accessor.
    void create();

    /// Construct a copy of the specified slice of this string array
    /// using the specified target allocator.
    MemRef slice(size_t offset, size_t size, Allocator& target_alloc) const;

#ifdef REALM_DEBUG
    void string_stats() const;
    void to_dot(std::ostream&, StringData title = StringData()) const;
#endif

private:
    size_t calc_byte_len(size_t count, size_t width) const override;
    size_t calc_item_count(size_t bytes,
                              size_t width) const noexcept override;
    WidthType GetWidthType() const override { return wtype_Multiply; }

    bool m_nullable;
};



// Implementation:

// Creates new array (but invalid, call init_from_ref() to init)
inline ArrayString::ArrayString(Allocator& alloc, bool nullable) noexcept:
Array(alloc), m_nullable(nullable)
{
}

// Fastest way to instantiate an Array. For use with GetDirect() that only fills out m_width, m_data
// and a few other basic things needed for read-only access. Or for use if you just want a way to call
// some methods written in ArrayString.*
inline ArrayString::ArrayString(no_prealloc_tag) noexcept:
    Array(*static_cast<Allocator*>(nullptr))
{
}

inline void ArrayString::create()
{
    size_t size = 0;
    MemRef mem = create_array(size, get_alloc()); // Throws
    init_from_mem(mem);
}

inline MemRef ArrayString::create_array(size_t size, Allocator& alloc)
{
    bool context_flag = false;
    int_fast64_t value = 0;
    return Array::create(type_Normal, context_flag, wtype_Multiply, size, value, alloc); // Throws
}

inline StringData ArrayString::get(size_t ndx) const noexcept
{
    REALM_ASSERT_3(ndx, <, m_size);
    if (m_width == 0)
        return m_nullable ? realm::null() : StringData("");

    const char* data = m_data + (ndx * m_width);
    size_t size = (m_width-1) - data[m_width-1];

    if (size == static_cast<size_t>(-1))
        return m_nullable ? realm::null() : StringData("");

    REALM_ASSERT(data[size] == 0); // Realm guarantees 0 terminated return strings
    return StringData(data, size);
}

inline void ArrayString::add(StringData value)
{
    REALM_ASSERT(!(!m_nullable && value.is_null()));
    insert(m_size, value); // Throws
}

inline void ArrayString::add()
{
    add(m_nullable ? realm::null() : StringData("")); // Throws
}

inline StringData ArrayString::get(const char* header, size_t ndx, bool nullable) noexcept
{
    REALM_ASSERT(ndx < get_size_from_header(header));
    size_t width = get_width_from_header(header);
    const char* data = get_data_from_header(header) + (ndx * width);

    if (width == 0)
        return nullable ? realm::null() : StringData("");

    size_t size = (width-1) - data[width-1];

    if (size == static_cast<size_t>(-1))
        return nullable ? realm::null() : StringData("");

    return StringData(data, size);
}


} // namespace realm

#endif // REALM_ARRAY_STRING_HPP
