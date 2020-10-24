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

#ifndef REALM_ARRAY_BINARY_HPP
#define REALM_ARRAY_BINARY_HPP

#include <realm/array_blobs_small.hpp>
#include <realm/array_blobs_big.hpp>

namespace realm {

class ArrayBinary : public ArrayPayload {
public:
    using value_type = BinaryData;

    explicit ArrayBinary(Allocator&);

    static BinaryData default_value(bool nullable)
    {
        return nullable ? BinaryData{} : BinaryData{"", 0};
    }

    void create();

    ref_type get_ref() const
    {
        return m_arr->get_ref();
    }

    void set_parent(ArrayParent* parent, size_t ndx_in_parent) noexcept override
    {
        m_arr->set_parent(parent, ndx_in_parent);
    }

    void update_parent()
    {
        m_arr->update_parent();
    }

    void init_from_mem(MemRef mem) noexcept;
    void init_from_ref(ref_type ref) noexcept override
    {
        init_from_mem(MemRef(m_alloc.translate(ref), ref, m_alloc));
    }
    void init_from_parent();

    size_t size() const;

    void add(BinaryData value);
    void set(size_t ndx, BinaryData value);
    void set_null(size_t ndx)
    {
        set(ndx, BinaryData{});
    }
    void insert(size_t ndx, BinaryData value);
    BinaryData get(size_t ndx) const;
    BinaryData get_at(size_t ndx, size_t& pos) const;
    bool is_null(size_t ndx) const;
    void erase(size_t ndx);
    void move(ArrayBinary& dst, size_t ndx);
    void clear();

    size_t find_first(BinaryData value, size_t begin, size_t end) const noexcept;

    /// Get the specified element without the cost of constructing an
    /// array instance. If an array instance is already available, or
    /// you need to get multiple values, then this method will be
    /// slower.
    static BinaryData get(const char* header, size_t ndx, Allocator& alloc) noexcept;

    void verify() const;

private:
    static constexpr size_t small_blob_max_size = 64;

    union Storage {
        std::aligned_storage<sizeof(ArraySmallBlobs), alignof(ArraySmallBlobs)>::type m_small_blobs;
        std::aligned_storage<sizeof(ArrayBigBlobs), alignof(ArrayBigBlobs)>::type m_big_blobs;
    };

    bool m_is_big = false;

    Allocator& m_alloc;
    Storage m_storage;
    Array* m_arr;

    bool upgrade_leaf(size_t value_size);
};

inline BinaryData ArrayBinary::get(const char* header, size_t ndx, Allocator& alloc) noexcept
{
    bool is_big = Array::get_context_flag_from_header(header);
    if (!is_big) {
        return ArraySmallBlobs::get(header, ndx, alloc);
    }
    else {
        return ArrayBigBlobs::get(header, ndx, alloc);
    }
}
}

#endif /* SRC_REALM_ARRAY_BINARY_HPP_ */
