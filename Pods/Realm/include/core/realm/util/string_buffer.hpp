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
    ~StringBuffer() noexcept {}

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
    /// *c_str() up to and including *(c_str()+size()), the caller may
    /// write from *c_str() up to, but not including,
    /// *(c_str()+size()).
    char* c_str() noexcept;

    /// Guarantees that the returned string is zero terminated, that
    /// is, *(c_str()+size()) is zero. The caller may read from
    /// *c_str() up to and including *(c_str()+size()).
    const char* c_str() const noexcept;

    void append(const std::string&);

    void append(const char* data, size_t size);

    /// Append a zero-terminated string to this buffer.
    void append_c_str(const char* c_str);

    /// The specified size is understood as not including the
    /// terminating zero. If the specified size is less than the
    /// current size, then the string is truncated accordingly. If the
    /// specified size is greater than the current size, then the
    /// extra characters will have undefined values, however, there
    /// will be a terminating zero at *(c_str()+size()), and the
    /// original terminating zero will also be left in place such that
    /// from the point of view of c_str(), the size of the string is
    /// unchanged.
    void resize(size_t size);

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
    static char m_zero;

    void reallocate(size_t min_capacity);
};





// Implementation:

inline StringBuffer::StringBuffer() noexcept: m_size(0)
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

inline char* StringBuffer::c_str() noexcept
{
    char* d = data();
    return d ? d : &m_zero;
}

inline const char* StringBuffer::c_str() const noexcept
{
    const char* d = data();
    return d ? d : &m_zero;
}

inline void StringBuffer::append(const std::string& s)
{
    return append(s.data(), s.size());
}

inline void StringBuffer::append_c_str(const char* c_str)
{
    append(c_str, std::strlen(c_str));
}

inline void StringBuffer::reserve(size_t min_capacity)
{
    size_t capacity = m_buffer.size();
    if (capacity == 0 || capacity-1 < min_capacity)
        reallocate(min_capacity);
}

inline void StringBuffer::resize(size_t size)
{
    reserve(size);
    // Note that even reserve(0) will attempt to allocate a
    // buffer, so we can safely write the truncating zero at this
    // time.
    m_size = size;
    m_buffer[size] = 0;
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
