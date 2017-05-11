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

#ifndef REALM_UTIL_STRING_BUFFER_HPP
#define REALM_UTIL_STRING_BUFFER_HPP

#include <cstddef>
#include <cstring>
#include <string>

#include <realm/util/features.h>
#include <realm/util/buffer.hpp>

namespace realm {
namespace util {


// FIXME: Check whether this class provides anything that a C++03
// std::string does not already provide. In particular, can a C++03
// std::string be used as a contiguous mutable buffer?
class StringBuffer {
public:
    StringBuffer() noexcept;

    std::string str() const;

    /// Returns the current size of the string in this buffer. This
    /// size does not include the terminating zero.
    size_t size() const noexcept;

    /// Gives read and write access to the bytes of this buffer. The
    /// caller may read and write from *c_str() up to, but not
    /// including, *(c_str()+size()).
    char* data() noexcept;

    /// Gives read access to the bytes of this buffer. The caller may
    /// read from *c_str() up to, but not including,
    /// *(c_str()+size()).
    const char* data() const noexcept;

    /// Guarantees that the returned string is zero terminated, that
    /// is, *(c_str()+size()) is zero. The caller may read from
    /// *c_str() up to and including *(c_str()+size()).
    const char* c_str() const noexcept;

    void append(const std::string&);

    void append(const char* append_data, size_t append_size);

    /// Append a zero-terminated string to this buffer.
    void append_c_str(const char* c_string);

    /// The specified size is understood as not including the
    /// terminating zero. If the specified size is less than the
    /// current size, then the string is truncated accordingly. If the
    /// specified size is greater than the current size, then the
    /// extra characters will have undefined values, however, there
    /// will be a terminating zero at *(c_str()+size()), and the
    /// original terminating zero will also be left in place such that
    /// from the point of view of c_str(), the size of the string is
    /// unchanged.
    void resize(size_t new_size);

    /// The specified minimum capacity is understood as not including
    /// the terminating zero. This operation does not change the size
    /// of the string in the buffer as returned by size(). If the
    /// specified capacity is less than the current capacity, this
    /// operation has no effect.
    void reserve(size_t min_capacity);

    /// Set size to zero. The capacity remains unchanged.
    void clear() noexcept;

private:
    util::Buffer<char> m_buffer;
    size_t m_size; // Excluding the terminating zero
    void reallocate(size_t min_capacity);
};


// Implementation:

inline StringBuffer::StringBuffer() noexcept
    : m_size(0)
{
}

inline std::string StringBuffer::str() const
{
    return std::string(m_buffer.data(), m_size);
}

inline size_t StringBuffer::size() const noexcept
{
    return m_size;
}

inline char* StringBuffer::data() noexcept
{
    return m_buffer.data();
}

inline const char* StringBuffer::data() const noexcept
{
    return m_buffer.data();
}

inline const char* StringBuffer::c_str() const noexcept
{
    static const char zero = 0;
    const char* d = data();
    return d ? d : &zero;
}

inline void StringBuffer::append(const std::string& s)
{
    return append(s.data(), s.size());
}

inline void StringBuffer::append_c_str(const char* c_string)
{
    append(c_string, std::strlen(c_string));
}

inline void StringBuffer::reserve(size_t min_capacity)
{
    size_t capacity = m_buffer.size();
    if (capacity == 0 || capacity - 1 < min_capacity)
        reallocate(min_capacity);
}

inline void StringBuffer::resize(size_t new_size)
{
    reserve(new_size);
    // Note that even reserve(0) will attempt to allocate a
    // buffer, so we can safely write the truncating zero at this
    // time.
    m_size = new_size;
    m_buffer[new_size] = 0;
}

inline void StringBuffer::clear() noexcept
{
    if (m_buffer.size() == 0)
        return;
    m_size = 0;
    m_buffer[0] = 0;
}


} // namespace util
} // namespace realm

#endif // REALM_UTIL_STRING_BUFFER_HPP
