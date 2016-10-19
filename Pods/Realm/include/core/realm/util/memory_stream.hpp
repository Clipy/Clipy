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

#ifndef REALM_UTIL_MEMORY_STREAM_HPP
#define REALM_UTIL_MEMORY_STREAM_HPP

#include <cstddef>
#include <string>
#include <istream>
#include <ostream>

#include <realm/util/features.h>

namespace realm {
namespace util {

class MemoryInputStreambuf : public std::streambuf {
public:
    MemoryInputStreambuf();
    ~MemoryInputStreambuf() noexcept;

    void set_buffer(const char* begin, const char* end) noexcept;

private:
    int_type underflow() override;
    int_type uflow() override;
    int_type pbackfail(int_type ch) override;
    std::streamsize showmanyc() override;
    pos_type seekoff(off_type off, std::ios_base::seekdir dir,
                     std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override;
    pos_type seekpos(pos_type pos, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override;


    const char* m_begin;
    const char* m_end;
    const char* m_curr;
};


class MemoryOutputStreambuf : public std::streambuf {
public:
    MemoryOutputStreambuf();
    ~MemoryOutputStreambuf() noexcept;

    void set_buffer(char* begin, char* end) noexcept;

    /// Returns the amount of data written to the buffer.
    size_t size() const noexcept;
};


class MemoryInputStream : public std::istream {
public:
    MemoryInputStream();
    ~MemoryInputStream() noexcept;

    void set_buffer(const char* begin, const char* end) noexcept;

    void set_string(const std::string&);

    void set_c_string(const char* c_str) noexcept;

private:
    MemoryInputStreambuf m_streambuf;
};


class MemoryOutputStream : public std::ostream {
public:
    MemoryOutputStream();
    ~MemoryOutputStream() noexcept;

    void set_buffer(char* begin, char* end) noexcept;

    template <size_t N>
    void set_buffer(char (&buffer)[N]) noexcept;

    /// Returns the amount of data written to the underlying buffer.
    size_t size() const noexcept;

private:
    MemoryOutputStreambuf m_streambuf;
};


// Implementation

inline MemoryInputStreambuf::MemoryInputStreambuf()
    : m_begin(nullptr)
    , m_end(nullptr)
    , m_curr(nullptr)
{
}

inline MemoryInputStreambuf::~MemoryInputStreambuf() noexcept
{
}

inline void MemoryInputStreambuf::set_buffer(const char* begin, const char* end) noexcept
{
    m_begin = begin;
    m_end = end;
    m_curr = begin;
}


inline MemoryOutputStreambuf::MemoryOutputStreambuf()
{
}

inline MemoryOutputStreambuf::~MemoryOutputStreambuf() noexcept
{
}

inline void MemoryOutputStreambuf::set_buffer(char* begin, char* end) noexcept
{
    setp(begin, end);
}

inline size_t MemoryOutputStreambuf::size() const noexcept
{
    return pptr() - pbase();
}


inline MemoryInputStream::MemoryInputStream()
    : std::istream(&m_streambuf)
{
}

inline MemoryInputStream::~MemoryInputStream() noexcept
{
}

inline void MemoryInputStream::set_buffer(const char* begin, const char* end) noexcept
{
    m_streambuf.set_buffer(begin, end);
    clear();
}

inline void MemoryInputStream::set_string(const std::string& str)
{
    const char* begin = str.data();
    const char* end = begin + str.size();
    set_buffer(begin, end);
}

inline void MemoryInputStream::set_c_string(const char* c_str) noexcept
{
    const char* begin = c_str;
    const char* end = begin + traits_type::length(c_str);
    set_buffer(begin, end);
}


inline MemoryOutputStream::MemoryOutputStream()
    : std::ostream(&m_streambuf)
{
}

inline MemoryOutputStream::~MemoryOutputStream() noexcept
{
}

inline void MemoryOutputStream::set_buffer(char* begin, char* end) noexcept
{
    m_streambuf.set_buffer(begin, end);
    clear();
}

template <size_t N>
inline void MemoryOutputStream::set_buffer(char (&buffer)[N]) noexcept
{
    set_buffer(buffer, buffer + N);
}

inline size_t MemoryOutputStream::size() const noexcept
{
    return m_streambuf.size();
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_MEMORY_STREAM_HPP
