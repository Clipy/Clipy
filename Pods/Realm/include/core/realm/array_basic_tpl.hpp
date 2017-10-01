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

#ifndef REALM_ARRAY_BASIC_TPL_HPP
#define REALM_ARRAY_BASIC_TPL_HPP

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <iomanip>

#include <realm/impl/destroy_guard.hpp>

namespace realm {

template <class T>
inline BasicArray<T>::BasicArray(Allocator& allocator) noexcept
    : Array(allocator)
{
}

template <class T>
inline MemRef BasicArray<T>::create_array(size_t init_size, Allocator& allocator)
{
    size_t byte_size_0 = calc_aligned_byte_size(init_size); // Throws
    // Adding zero to Array::initial_capacity to avoid taking the
    // address of that member
    size_t byte_size = std::max(byte_size_0, Array::initial_capacity + 0); // Throws

    MemRef mem = allocator.alloc(byte_size); // Throws

    bool is_inner_bptree_node = false;
    bool has_refs = false;
    bool context_flag = false;
    int width = sizeof(T);
    init_header(mem.get_addr(), is_inner_bptree_node, has_refs, context_flag, wtype_Multiply, width, init_size,
                byte_size);

    return mem;
}


template <class T>
inline MemRef BasicArray<T>::create_array(Array::Type type, bool context_flag, size_t init_size, T value,
                                          Allocator& allocator)
{
    REALM_ASSERT(type == Array::type_Normal);
    REALM_ASSERT(!context_flag);
    MemRef mem = create_array(init_size, allocator);
    if (init_size) {
        BasicArray<T> tmp(allocator);
        tmp.init_from_mem(mem);
        T* p = reinterpret_cast<T*>(tmp.m_data);
        T* end = p + init_size;
        while (p < end) {
            *p++ = value;
        }
    }
    return mem;
}


template <class T>
inline void BasicArray<T>::create(Array::Type type, bool context_flag)
{
    REALM_ASSERT(type == Array::type_Normal);
    REALM_ASSERT(!context_flag);
    size_t length = 0;
    MemRef mem = create_array(length, get_alloc()); // Throws
    init_from_mem(mem);
}


template <class T>
MemRef BasicArray<T>::slice(size_t offset, size_t slice_size, Allocator& target_alloc) const
{
    REALM_ASSERT(is_attached());

    // FIXME: This can be optimized as a single contiguous copy
    // operation.
    BasicArray array_slice(target_alloc);
    _impl::ShallowArrayDestroyGuard dg(&array_slice);
    array_slice.create(); // Throws
    size_t begin = offset;
    size_t end = offset + slice_size;
    for (size_t i = begin; i != end; ++i) {
        T value = get(i);
        array_slice.add(value); // Throws
    }
    dg.release();
    return array_slice.get_mem();
}

template <class T>
MemRef BasicArray<T>::slice_and_clone_children(size_t offset, size_t slice_size, Allocator& target_alloc) const
{
    // BasicArray<T> never contains refs, so never has children.
    return slice(offset, slice_size, target_alloc);
}


template <class T>
inline void BasicArray<T>::add(T value)
{
    insert(m_size, value);
}


template <class T>
inline T BasicArray<T>::get(size_t ndx) const noexcept
{
    return *(reinterpret_cast<const T*>(m_data) + ndx);
}


template <class T>
inline bool BasicArray<T>::is_null(size_t ndx) const noexcept
{
    // FIXME: This assumes BasicArray will only ever be instantiated for float-like T.
    static_assert(realm::is_any<T, float, double>::value, "T can only be float or double");
    auto x = get(ndx);
    return null::is_null_float(x);
}


template <class T>
inline T BasicArray<T>::get(const char* header, size_t ndx) noexcept
{
    const char* data = get_data_from_header(header);
    // This casting assumes that T can be aliged on an 8-bype
    // boundary (since data is aligned on an 8-byte boundary.)
    return *(reinterpret_cast<const T*>(data) + ndx);
}


template <class T>
inline void BasicArray<T>::set(size_t ndx, T value)
{
    REALM_ASSERT_3(ndx, <, m_size);
    if (get(ndx) == value)
        return;

    // Check if we need to copy before modifying
    copy_on_write(); // Throws

    // Set the value
    T* data = reinterpret_cast<T*>(m_data) + ndx;
    *data = value;
}

template <class T>
inline void BasicArray<T>::set_null(size_t ndx)
{
    // FIXME: This assumes BasicArray will only ever be instantiated for float-like T.
    set(ndx, null::get_null_float<T>());
}

template <class T>
void BasicArray<T>::insert(size_t ndx, T value)
{
    REALM_ASSERT_3(ndx, <=, m_size);

    // Check if we need to copy before modifying
    copy_on_write(); // Throws

    // Make room for the new value
    alloc(m_size + 1, m_width); // Throws

    // Move values below insertion
    if (ndx != m_size) {
        char* src_begin = m_data + ndx * m_width;
        char* src_end = m_data + m_size * m_width;
        char* dst_end = src_end + m_width;
        std::copy_backward(src_begin, src_end, dst_end);
    }

    // Set the value
    T* data = reinterpret_cast<T*>(m_data) + ndx;
    *data = value;

    ++m_size;
}

template <class T>
void BasicArray<T>::erase(size_t ndx)
{
    REALM_ASSERT_3(ndx, <, m_size);

    // Check if we need to copy before modifying
    copy_on_write(); // Throws

    // move data under deletion up
    if (ndx < m_size - 1) {
        char* dst_begin = m_data + ndx * m_width;
        const char* src_begin = dst_begin + m_width;
        const char* src_end = m_data + m_size * m_width;
        realm::safe_copy_n(src_begin, src_end - src_begin, dst_begin);
    }

    // Update size (also in header)
    --m_size;
    set_header_size(m_size);
}

template <class T>
void BasicArray<T>::truncate(size_t to_size)
{
    REALM_ASSERT(is_attached());
    REALM_ASSERT_3(to_size, <=, m_size);

    copy_on_write(); // Throws

    // Update size in accessor and in header. This leaves the capacity
    // unchanged.
    m_size = to_size;
    set_header_size(to_size);
}

template <class T>
inline void BasicArray<T>::clear()
{
    truncate(0); // Throws
}

template <class T>
bool BasicArray<T>::compare(const BasicArray<T>& a) const
{
    size_t n = size();
    if (a.size() != n)
        return false;
    const T* data_1 = reinterpret_cast<const T*>(m_data);
    const T* data_2 = reinterpret_cast<const T*>(a.m_data);
    return realm::safe_equal(data_1, data_1 + n, data_2);
}


template <class T>
size_t BasicArray<T>::calc_byte_len(size_t for_size, size_t) const
{
    // FIXME: Consider calling `calc_aligned_byte_size(size)`
    // instead. Note however, that calc_byte_len() is supposed to return
    // the unaligned byte size. It is probably the case that no harm
    // is done by returning the aligned version, and most callers of
    // calc_byte_len() will actually benefit if calc_byte_len() was
    // changed to always return the aligned byte size.
    return header_size + for_size * sizeof(T);
}

template <class T>
size_t BasicArray<T>::calc_item_count(size_t bytes, size_t) const noexcept
{
    size_t bytes_without_header = bytes - header_size;
    return bytes_without_header / sizeof(T);
}

template <class T>
size_t BasicArray<T>::find(T value, size_t begin, size_t end) const
{
    if (end == npos)
        end = m_size;
    REALM_ASSERT(begin <= m_size && end <= m_size && begin <= end);
    const T* data = reinterpret_cast<const T*>(m_data);
    const T* i = std::find(data + begin, data + end, value);
    return i == data + end ? not_found : size_t(i - data);
}

template <class T>
inline size_t BasicArray<T>::find_first(T value, size_t begin, size_t end) const
{
    return this->find(value, begin, end);
}

template <class T>
void BasicArray<T>::find_all(IntegerColumn* result, T value, size_t add_offset, size_t begin, size_t end) const
{
    size_t first = begin - 1;
    for (;;) {
        first = this->find(value, first + 1, end);
        if (first == not_found)
            break;

        Array::add_to_column(result, first + add_offset);
    }
}

template <class T>
size_t BasicArray<T>::count(T value, size_t begin, size_t end) const
{
    if (end == npos)
        end = m_size;
    REALM_ASSERT(begin <= m_size && end <= m_size && begin <= end);
    const T* data = reinterpret_cast<const T*>(m_data);
    return std::count(data + begin, data + end, value);
}

#if 0
// currently unused
template <class T>
double BasicArray<T>::sum(size_t begin, size_t end) const
{
    if (end == npos)
        end = m_size;
    REALM_ASSERT(begin <= m_size && end <= m_size && begin <= end);
    const T* data = reinterpret_cast<const T*>(m_data);
    return std::accumulate(data + begin, data + end, double(0));
}
#endif

template <class T>
template <bool find_max>
bool BasicArray<T>::minmax(T& result, size_t begin, size_t end) const
{
    if (end == npos)
        end = m_size;
    if (m_size == 0)
        return false;
    REALM_ASSERT(begin < m_size && end <= m_size && begin < end);

    T m = get(begin);
    ++begin;
    for (; begin < end; ++begin) {
        T val = get(begin);
        if (find_max ? val > m : val < m)
            m = val;
    }
    result = m;
    return true;
}

template <class T>
bool BasicArray<T>::maximum(T& result, size_t begin, size_t end) const
{
    return minmax<true>(result, begin, end);
}

template <class T>
bool BasicArray<T>::minimum(T& result, size_t begin, size_t end) const
{
    return minmax<false>(result, begin, end);
}


template <class T>
ref_type BasicArray<T>::bptree_leaf_insert(size_t ndx, T value, TreeInsertBase& state)
{
    size_t leaf_size = size();
    REALM_ASSERT_3(leaf_size, <=, REALM_MAX_BPNODE_SIZE);
    if (leaf_size < ndx)
        ndx = leaf_size;
    if (REALM_LIKELY(leaf_size < REALM_MAX_BPNODE_SIZE)) {
        insert(ndx, value);
        return 0; // Leaf was not split
    }

    // Split leaf node
    BasicArray<T> new_leaf(get_alloc());
    new_leaf.create(); // Throws
    if (ndx == leaf_size) {
        new_leaf.add(value);
        state.m_split_offset = ndx;
    }
    else {
        // FIXME: Could be optimized by first resizing the target
        // array, then copy elements with std::copy().
        for (size_t i = ndx; i != leaf_size; ++i)
            new_leaf.add(get(i));
        truncate(ndx);
        add(value);
        state.m_split_offset = ndx + 1;
    }
    state.m_split_size = leaf_size + 1;
    return new_leaf.get_ref();
}

template <class T>
inline size_t BasicArray<T>::lower_bound(T value) const noexcept
{
    const T* begin = reinterpret_cast<const T*>(m_data);
    const T* end = begin + size();
    return std::lower_bound(begin, end, value) - begin;
}

template <class T>
inline size_t BasicArray<T>::upper_bound(T value) const noexcept
{
    const T* begin = reinterpret_cast<const T*>(m_data);
    const T* end = begin + size();
    return std::upper_bound(begin, end, value) - begin;
}

template <class T>
inline size_t BasicArray<T>::calc_aligned_byte_size(size_t size)
{
    size_t max = std::numeric_limits<size_t>::max();
    size_t max_2 = max & ~size_t(7); // Allow for upwards 8-byte alignment
    if (size > (max_2 - header_size) / sizeof(T))
        throw std::runtime_error("Byte size overflow");
    size_t byte_size = header_size + size * sizeof(T);
    REALM_ASSERT_3(byte_size, >, 0);
    size_t aligned_byte_size = ((byte_size - 1) | 7) + 1; // 8-byte alignment
    return aligned_byte_size;
}


#ifdef REALM_DEBUG

// LCOV_EXCL_START
template <class T>
void BasicArray<T>::to_dot(std::ostream& out, StringData title) const
{
    ref_type ref = get_ref();
    if (title.size() != 0) {
        out << "subgraph cluster_" << ref << " {\n";
        out << " label = \"" << title << "\";\n";
        out << " color = white;\n";
    }

    out << "n" << std::hex << ref << std::dec << "[shape=none,label=<";
    out << "<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\"><TR>\n";

    // Header
    out << "<TD BGCOLOR=\"lightgrey\"><FONT POINT-SIZE=\"7\"> ";
    out << "0x" << std::hex << ref << std::dec << "<BR/>";
    out << "</FONT></TD>\n";

    // Values
    size_t n = m_size;
    for (size_t i = 0; i != n; ++i)
        out << "<TD>" << get(i) << "</TD>\n";

    out << "</TR></TABLE>>];\n";

    if (title.size() != 0)
        out << "}\n";

    to_dot_parent_edge(out);
}
// LCOV_EXCL_STOP

#endif // REALM_DEBUG


} // namespace realm

#endif // REALM_ARRAY_BASIC_TPL_HPP
