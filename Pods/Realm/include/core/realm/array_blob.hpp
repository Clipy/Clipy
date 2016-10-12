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
#ifndef REALM_ARRAY_BLOB_HPP
#define REALM_ARRAY_BLOB_HPP

#include <realm/array.hpp>

namespace realm {


class ArrayBlob: public Array {
public:
    explicit ArrayBlob(Allocator&) noexcept;
    ~ArrayBlob() noexcept override {}

    const char* get(size_t index) const noexcept;
    bool is_null(size_t index) const noexcept;
    void add(const char* data, size_t size, bool add_zero_term = false);
    void insert(size_t pos, const char* data, size_t size, bool add_zero_term = false);
    void replace(size_t begin, size_t end, const char* data, size_t size,
                 bool add_zero_term = false);
    void erase(size_t begin, size_t end);

    /// Get the specified element without the cost of constructing an
    /// array instance. If an array instance is already available, or
    /// you need to get multiple values, then this method will be
    /// slower.
    static const char* get(const char* header, size_t index) noexcept;

    /// Create a new empty blob (binary) array and attach this
    /// accessor to it. This does not modify the parent reference
    /// information of this accessor.
    ///
    /// Note that the caller assumes ownership of the allocated
    /// underlying node. It is not owned by the accessor.
    void create();

    /// Construct a blob of the specified size and return just the
    /// reference to the underlying memory. All bytes will be
    /// initialized to zero.
    static MemRef create_array(size_t size, Allocator&);

#ifdef REALM_DEBUG
    void verify() const;
    void to_dot(std::ostream&, StringData title = StringData()) const;
#endif

private:
    size_t calc_byte_len(size_t count, size_t width) const override;
    size_t calc_item_count(size_t bytes,
                              size_t width) const noexcept override;
    WidthType GetWidthType() const override { return wtype_Ignore; }
};




// Implementation:

// Creates new array (but invalid, call init_from_ref() to init)
inline ArrayBlob::ArrayBlob(Allocator& alloc) noexcept:
    Array(alloc)
{
}

inline bool ArrayBlob::is_null(size_t index) const noexcept
{
    return (get(index) == nullptr);
}

inline const char* ArrayBlob::get(size_t index) const noexcept
{
    return m_data + index;
}

inline void ArrayBlob::add(const char* data, size_t size, bool add_zero_term)
{
    replace(m_size, m_size, data, size, add_zero_term);
}

inline void ArrayBlob::insert(size_t pos, const char* data, size_t size,
                              bool add_zero_term)
{
    replace(pos, pos, data, size, add_zero_term);
}

inline void ArrayBlob::erase(size_t begin, size_t end)
{
    const char* data = nullptr;
    size_t size = 0;
    replace(begin, end, data, size);
}

inline const char* ArrayBlob::get(const char* header, size_t pos) noexcept
{
    const char* data = get_data_from_header(header);
    return data + pos;
}

inline void ArrayBlob::create()
{
    size_t size = 0;
    MemRef mem = create_array(size, get_alloc()); // Throws
    init_from_mem(mem);
}

inline MemRef ArrayBlob::create_array(size_t size, Allocator& alloc)
{
    bool context_flag = false;
    int_fast64_t value = 0;
    return Array::create(type_Normal, context_flag, wtype_Ignore, size, value, alloc); // Throws
}

inline size_t ArrayBlob::calc_byte_len(size_t count, size_t) const
{
    return header_size + count;
}

inline size_t ArrayBlob::calc_item_count(size_t bytes, size_t) const noexcept
{
    return bytes - header_size;
}


} // namespace realm

#endif // REALM_ARRAY_BLOB_HPP
