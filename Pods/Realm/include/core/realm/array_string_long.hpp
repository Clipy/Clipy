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

#ifndef REALM_ARRAY_STRING_LONG_HPP
#define REALM_ARRAY_STRING_LONG_HPP

#include <realm/array_blob.hpp>
#include <realm/array_integer.hpp>

namespace realm {


class ArrayStringLong : public Array {
public:
    typedef StringData value_type;

    explicit ArrayStringLong(Allocator&, bool nullable) noexcept;
    ~ArrayStringLong() noexcept override
    {
    }

    // Disable copying, this is not allowed.
    ArrayStringLong& operator=(const ArrayStringLong&) = delete;
    ArrayStringLong(const ArrayStringLong&) = delete;

    /// Create a new empty long string array and attach this accessor to
    /// it. This does not modify the parent reference information of
    /// this accessor.
    ///
    /// Note that the caller assumes ownership of the allocated
    /// underlying node. It is not owned by the accessor.
    void create();

    //@{
    /// Overriding functions of Array
    void init_from_ref(ref_type) noexcept;
    void init_from_mem(MemRef) noexcept;
    void init_from_parent() noexcept;
    //@}

    bool is_empty() const noexcept;
    size_t size() const noexcept;

    StringData get(size_t ndx) const noexcept;


    void add(StringData value);
    void set(size_t ndx, StringData value);
    void insert(size_t ndx, StringData value);
    void erase(size_t ndx);
    void truncate(size_t size);
    void clear();
    void destroy();

    bool is_null(size_t ndx) const;
    void set_null(size_t ndx);

    size_t count(StringData value, size_t begin = 0, size_t end = npos) const noexcept;
    size_t find_first(StringData value, size_t begin = 0, size_t end = npos) const noexcept;
    void find_all(IntegerColumn& result, StringData value, size_t add_offset = 0, size_t begin = 0,
                  size_t end = npos) const;

    /// Get the specified element without the cost of constructing an
    /// array instance. If an array instance is already available, or
    /// you need to get multiple values, then this method will be
    /// slower.
    static StringData get(const char* header, size_t ndx, Allocator&, bool nullable) noexcept;

    ref_type bptree_leaf_insert(size_t ndx, StringData, TreeInsertBase&);

    static size_t get_size_from_header(const char*, Allocator&) noexcept;

    /// Construct a long string array of the specified size and return
    /// just the reference to the underlying memory. All elements will
    /// be initialized to zero size blobs.
    static MemRef create_array(size_t size, Allocator&, bool nullable);

    /// Construct a copy of the specified slice of this long string
    /// array using the specified target allocator.
    MemRef slice(size_t offset, size_t slice_size, Allocator& target_alloc) const;

#ifdef REALM_DEBUG
    void to_dot(std::ostream&, StringData title = StringData()) const;
#endif

    bool update_from_parent(size_t old_baseline) noexcept;

private:
    ArrayInteger m_offsets;
    ArrayBlob m_blob;
    Array m_nulls;
    bool m_nullable;
};


// Implementation:
inline ArrayStringLong::ArrayStringLong(Allocator& allocator, bool nullable) noexcept
    : Array(allocator)
    , m_offsets(allocator)
    , m_blob(allocator)
    , m_nulls(nullable ? allocator : Allocator::get_default())
    , m_nullable(nullable)
{
    m_offsets.set_parent(this, 0);
    m_blob.set_parent(this, 1);
    if (nullable)
        m_nulls.set_parent(this, 2);
}

inline void ArrayStringLong::create()
{
    size_t init_size = 0;
    MemRef mem = create_array(init_size, get_alloc(), m_nullable); // Throws
    init_from_mem(mem);
}

inline void ArrayStringLong::init_from_ref(ref_type ref) noexcept
{
    REALM_ASSERT(ref);
    char* header = get_alloc().translate(ref);
    init_from_mem(MemRef(header, ref, m_alloc));
    m_nullable = (Array::size() == 3);
}

inline void ArrayStringLong::init_from_parent() noexcept
{
    ref_type ref = get_ref_from_parent();
    init_from_ref(ref);
}

inline bool ArrayStringLong::is_empty() const noexcept
{
    return m_offsets.is_empty();
}

inline size_t ArrayStringLong::size() const noexcept
{
    return m_offsets.size();
}

inline StringData ArrayStringLong::get(size_t ndx) const noexcept
{
    REALM_ASSERT_3(ndx, <, m_offsets.size());

    if (m_nullable && m_nulls.get(ndx) == 0)
        return realm::null();

    size_t begin, end;
    if (0 < ndx) {
        begin = to_size_t(m_offsets.get(ndx - 1));
        end = to_size_t(m_offsets.get(ndx));
    }
    else {
        begin = 0;
        end = to_size_t(m_offsets.get(0));
    }
    --end; // Discount the terminating zero

    return StringData(m_blob.get(begin), end - begin);
}

inline void ArrayStringLong::truncate(size_t new_size)
{
    REALM_ASSERT_3(new_size, <, m_offsets.size());

    size_t blob_size = new_size ? to_size_t(m_offsets.get(new_size - 1)) : 0;

    m_offsets.truncate(new_size);
    m_blob.truncate(blob_size);
    if (m_nullable)
        m_nulls.truncate(new_size);
}

inline void ArrayStringLong::clear()
{
    m_blob.clear();
    m_offsets.clear();
    if (m_nullable)
        m_nulls.clear();
}

inline void ArrayStringLong::destroy()
{
    m_blob.destroy();
    m_offsets.destroy();
    if (m_nullable)
        m_nulls.destroy();
    Array::destroy();
}

inline bool ArrayStringLong::update_from_parent(size_t old_baseline) noexcept
{
    bool res = Array::update_from_parent(old_baseline);
    if (res) {
        m_blob.update_from_parent(old_baseline);
        m_offsets.update_from_parent(old_baseline);
        if (m_nullable)
            m_nulls.update_from_parent(old_baseline);
    }
    return res;
}

inline size_t ArrayStringLong::get_size_from_header(const char* header, Allocator& alloc) noexcept
{
    ref_type offsets_ref = to_ref(Array::get(header, 0));
    const char* offsets_header = alloc.translate(offsets_ref);
    return Array::get_size_from_header(offsets_header);
}


} // namespace realm

#endif // REALM_ARRAY_STRING_LONG_HPP
