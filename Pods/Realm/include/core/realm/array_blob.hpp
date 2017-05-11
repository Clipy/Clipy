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

#ifndef REALM_ARRAY_BLOB_HPP
#define REALM_ARRAY_BLOB_HPP

#include <realm/array.hpp>

namespace realm {


class ArrayBlob : public Array {
public:
    static constexpr size_t max_binary_size = 0xFFFFF8 - Array::header_size;

    explicit ArrayBlob(Allocator&) noexcept;
    ~ArrayBlob() noexcept override
    {
    }

    // Disable copying, this is not allowed.
    ArrayBlob& operator=(const ArrayBlob&) = delete;
    ArrayBlob(const ArrayBlob&) = delete;

    const char* get(size_t index) const noexcept;
    BinaryData get_at(size_t& pos) const noexcept;
    bool is_null(size_t index) const noexcept;
    ref_type add(const char* data, size_t data_size, bool add_zero_term = false);
    void insert(size_t pos, const char* data, size_t data_size, bool add_zero_term = false);
    ref_type replace(size_t begin, size_t end, const char* data, size_t data_size, bool add_zero_term = false);
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
    static MemRef create_array(size_t init_size, Allocator&);

    size_t blob_size() const noexcept;
#ifdef REALM_DEBUG
    void verify() const;
    void to_dot(std::ostream&, StringData title = StringData()) const;
#endif

private:
    size_t calc_byte_len(size_t for_size, size_t width) const override;
    size_t calc_item_count(size_t bytes, size_t width) const noexcept override;
};


// Implementation:

// Creates new array (but invalid, call init_from_ref() to init)
inline ArrayBlob::ArrayBlob(Allocator& allocator) noexcept
    : Array(allocator)
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

inline ref_type ArrayBlob::add(const char* data, size_t data_size, bool add_zero_term)
{
    return replace(m_size, m_size, data, data_size, add_zero_term);
}

inline void ArrayBlob::insert(size_t pos, const char* data, size_t data_size, bool add_zero_term)
{
    replace(pos, pos, data, data_size, add_zero_term);
}

inline void ArrayBlob::erase(size_t begin, size_t end)
{
    const char* data = nullptr;
    size_t data_size = 0;
    replace(begin, end, data, data_size);
}

inline const char* ArrayBlob::get(const char* header, size_t pos) noexcept
{
    const char* data = get_data_from_header(header);
    return data + pos;
}

inline void ArrayBlob::create()
{
    size_t init_size = 0;
    MemRef mem = create_array(init_size, get_alloc()); // Throws
    init_from_mem(mem);
}

inline MemRef ArrayBlob::create_array(size_t init_size, Allocator& allocator)
{
    bool context_flag = false;
    int_fast64_t value = 0;
    return Array::create(type_Normal, context_flag, wtype_Ignore, init_size, value, allocator); // Throws
}

inline size_t ArrayBlob::calc_byte_len(size_t for_size, size_t) const
{
    return header_size + for_size;
}

inline size_t ArrayBlob::calc_item_count(size_t bytes, size_t) const noexcept
{
    return bytes - header_size;
}


} // namespace realm

#endif // REALM_ARRAY_BLOB_HPP
