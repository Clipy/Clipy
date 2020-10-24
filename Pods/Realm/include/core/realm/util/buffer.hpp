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

#ifndef REALM_UTIL_BUFFER_HPP
#define REALM_UTIL_BUFFER_HPP

#include <cstddef>
#include <algorithm>
#include <exception>
#include <limits>
#include <utility>

#include <realm/util/features.h>
#include <realm/utilities.hpp>
#include <realm/util/safe_int_ops.hpp>
#include <realm/util/allocator.hpp>
#include <memory>

namespace realm {
namespace util {


/// A simple buffer concept that owns a region of memory and knows its
/// size.
template <class T, class Allocator = DefaultAllocator>
class Buffer {
public:
    Buffer(Allocator& alloc = Allocator::get_default()) noexcept
        : m_data(nullptr, STLDeleter<T[], Allocator>{alloc})
        , m_size(0)
    {
    }
    explicit Buffer(size_t initial_size, Allocator& alloc = Allocator::get_default());
    Buffer(Buffer&&) noexcept = default;
    Buffer<T, Allocator>& operator=(Buffer&&) noexcept = default;

    T& operator[](size_t i) noexcept
    {
        return m_data[i];
    }
    const T& operator[](size_t i) const noexcept
    {
        return m_data[i];
    }

    T* data() noexcept
    {
        return m_data.get();
    }
    const T* data() const noexcept
    {
        return m_data.get();
    }
    size_t size() const noexcept
    {
        return m_size;
    }

    /// False iff the data() returns null.
    explicit operator bool() const noexcept
    {
        return bool(m_data);
    }

    /// Discards the original contents.
    void set_size(size_t new_size);

    /// \param new_size Specifies the new buffer size.
    /// \param copy_begin copy_end Specifies a range of element
    /// values to be retained. \a copy_end must be less than, or equal
    /// to size().
    ///
    /// \param copy_to Specifies where the retained range should be
    /// copied to. `\a copy_to + \a copy_end - \a copy_begin` must be
    /// less than, or equal to \a new_size.
    void resize(size_t new_size, size_t copy_begin, size_t copy_end, size_t copy_to);

    void reserve(size_t used_size, size_t min_capacity);

    void reserve_extra(size_t used_size, size_t min_extra_capacity);

    /// Release the internal buffer to the caller.
    REALM_NODISCARD std::unique_ptr<T[], STLDeleter<T[], Allocator>> release() noexcept;

    friend void swap(Buffer& a, Buffer& b) noexcept
    {
        using std::swap;
        swap(a.m_data, b.m_data);
        swap(a.m_size, b.m_size);
    }

    Allocator& get_allocator() const noexcept
    {
        return m_data.get_deleter().get_allocator();
    }

private:
    std::unique_ptr<T[], STLDeleter<T[], Allocator>> m_data;
    size_t m_size;
};


/// A buffer that can be efficiently resized. It acheives this by
/// using an underlying buffer that may be larger than the logical
/// size, and is automatically expanded in progressively larger steps.
template <class T, class Allocator = DefaultAllocator>
class AppendBuffer {
public:
    AppendBuffer(Allocator& alloc = Allocator::get_default()) noexcept;
    AppendBuffer(AppendBuffer&&) noexcept = default;
    AppendBuffer& operator=(AppendBuffer&&) noexcept = default;

    /// Returns the current size of the buffer.
    size_t size() const noexcept;

    /// Gives read and write access to the elements.
    T* data() noexcept;

    /// Gives read access the elements.
    const T* data() const noexcept;

    /// Append the specified elements. This increases the size of this
    /// buffer by \a append_data_size. If the caller has previously requested
    /// a minimum capacity that is greater than, or equal to the
    /// resulting size, this function is guaranteed to not throw.
    void append(const T* append_data, size_t append_data_size);

    /// If the specified size is less than the current size, then the
    /// buffer contents is truncated accordingly. If the specified
    /// size is greater than the current size, then the extra elements
    /// will have undefined values. If the caller has previously
    /// requested a minimum capacity that is greater than, or equal to
    /// the specified size, this function is guaranteed to not throw.
    void resize(size_t new_size);

    /// This operation does not change the size of the buffer as
    /// returned by size(). If the specified capacity is less than the
    /// current capacity, this operation has no effect.
    void reserve(size_t min_capacity);

    /// Set the size to zero. The capacity remains unchanged.
    void clear() noexcept;

    /// Release the underlying buffer and reset the size. Note: The returned
    /// buffer may be larger than the amount of data appended to this buffer.
    /// Callers should call `size()` prior to releasing the buffer to know the
    /// usable/logical size.
    REALM_NODISCARD Buffer<T, Allocator> release() noexcept;

private:
    util::Buffer<T, Allocator> m_buffer;
    size_t m_size;
};


// Implementation:

class BufferSizeOverflow : public std::exception {
public:
    const char* what() const noexcept override
    {
        return "Buffer size overflow";
    }
};

template <class T, class A>
inline Buffer<T, A>::Buffer(size_t initial_size, A& alloc)
    : m_data(util::make_unique<T[]>(alloc, initial_size)) // Throws
    , m_size(initial_size)
{
}

template <class T, class A>
inline void Buffer<T, A>::set_size(size_t new_size)
{
    m_data = util::make_unique<T[]>(get_allocator(), new_size); // Throws
    m_size = new_size;
}

template <class T, class A>
inline void Buffer<T, A>::resize(size_t new_size, size_t copy_begin, size_t copy_end, size_t copy_to)
{
    auto new_data = util::make_unique<T[]>(get_allocator(), new_size); // Throws
    realm::safe_copy_n(m_data.get() + copy_begin, copy_end - copy_begin, new_data.get() + copy_to);
    m_data = std::move(new_data);
    m_size = new_size;
}

template <class T, class A>
inline void Buffer<T, A>::reserve(size_t used_size, size_t min_capacity)
{
    size_t current_capacity = m_size;
    if (REALM_LIKELY(current_capacity >= min_capacity))
        return;
    size_t new_capacity = current_capacity;

    // Use growth factor 1.5.
    if (REALM_UNLIKELY(int_multiply_with_overflow_detect(new_capacity, 3)))
        new_capacity = std::numeric_limits<size_t>::max();
    new_capacity /= 2;

    if (REALM_UNLIKELY(new_capacity < min_capacity))
        new_capacity = min_capacity;
    resize(new_capacity, 0, used_size, 0); // Throws
}

template <class T, class A>
inline void Buffer<T, A>::reserve_extra(size_t used_size, size_t min_extra_capacity)
{
    size_t min_capacity = used_size;
    if (REALM_UNLIKELY(int_add_with_overflow_detect(min_capacity, min_extra_capacity)))
        throw BufferSizeOverflow();
    reserve(used_size, min_capacity); // Throws
}

template <class T, class A>
inline std::unique_ptr<T[], STLDeleter<T[], A>> Buffer<T, A>::release() noexcept
{
    m_size = 0;
    return std::move(m_data);
}


template <class T, class A>
inline AppendBuffer<T, A>::AppendBuffer(A& alloc) noexcept
    : m_buffer(alloc)
    , m_size(0)
{
}

template <class T, class A>
inline size_t AppendBuffer<T, A>::size() const noexcept
{
    return m_size;
}

template <class T, class A>
inline T* AppendBuffer<T, A>::data() noexcept
{
    return m_buffer.data();
}

template <class T, class A>
inline const T* AppendBuffer<T, A>::data() const noexcept
{
    return m_buffer.data();
}

template <class T, class A>
inline void AppendBuffer<T, A>::append(const T* append_data, size_t append_data_size)
{
    m_buffer.reserve_extra(m_size, append_data_size); // Throws
    realm::safe_copy_n(append_data, append_data_size, m_buffer.data() + m_size);
    m_size += append_data_size;
}

template <class T, class A>
inline void AppendBuffer<T, A>::reserve(size_t min_capacity)
{
    m_buffer.reserve(m_size, min_capacity);
}

template <class T, class A>
inline void AppendBuffer<T, A>::resize(size_t new_size)
{
    reserve(new_size);
    m_size = new_size;
}

template <class T, class A>
inline void AppendBuffer<T, A>::clear() noexcept
{
    m_size = 0;
}

template <class T, class A>
inline Buffer<T, A> AppendBuffer<T, A>::release() noexcept
{
    m_size = 0;
    return std::move(m_buffer);
}


} // namespace util
} // namespace realm

#endif // REALM_UTIL_BUFFER_HPP
