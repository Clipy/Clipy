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

namespace realm {
namespace util {

class MemoryInputStreambuf : public std::streambuf {
public:
    MemoryInputStreambuf();
    ~MemoryInputStreambuf() noexcept;

    /// Behavior is undefined if the size of the specified buffer exceeds
    /// PTRDIFF_MAX.
    void set_buffer(const char* begin, const char* end) noexcept;

private:
    const char* m_begin;
    const char* m_end;
    const char* m_curr;

    int_type underflow() override;
    int_type uflow() override;
    int_type pbackfail(int_type) override;
    std::streamsize showmanyc() override;
    pos_type seekoff(off_type, std::ios_base::seekdir, std::ios_base::openmode) override;
    pos_type seekpos(pos_type, std::ios_base::openmode) override;

    pos_type do_seekoff(off_type, std::ios_base::seekdir, std::ios_base::openmode);
};


class MemoryOutputStreambuf : public std::streambuf {
public:
    MemoryOutputStreambuf();
    ~MemoryOutputStreambuf() noexcept;

    /// Behavior is undefined if the size of the specified buffer exceeds
    /// PTRDIFF_MAX.
    void set_buffer(char* begin, char* end) noexcept;

    /// Returns the amount of data written to the buffer.
    size_t size() const noexcept;
};


class MemoryInputStream : public std::istream {
public:
    MemoryInputStream();
    ~MemoryInputStream() noexcept;

    /// \{ Behavior is undefined if the size of the specified buffer exceeds
    /// PTRDIFF_MAX.
    void set_buffer(const char* begin, const char* end) noexcept;
    template <size_t N> void set_buffer(const char (&buffer)[N]) noexcept;
    void set_string(const std::string&) noexcept;
    void set_c_string(const char* c_str) noexcept;
    /// \}

private:
    MemoryInputStreambuf m_streambuf;
};


class MemoryOutputStream : public std::ostream {
public:
    MemoryOutputStream();
    ~MemoryOutputStream() noexcept;

    /// \{ Behavior is undefined if the size of the specified buffer exceeds
    /// PTRDIFF_MAX.
    void set_buffer(char* begin, char* end) noexcept;
    template <size_t N> void set_buffer(char (&buffer)[N]) noexcept;
    /// \}

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

inline void MemoryInputStreambuf::set_buffer(const char* b, const char* e) noexcept
{
    m_begin = b;
    m_end = e;
    m_curr = b;
}


inline MemoryOutputStreambuf::MemoryOutputStreambuf()
{
}

inline MemoryOutputStreambuf::~MemoryOutputStreambuf() noexcept
{
}

inline void MemoryOutputStreambuf::set_buffer(char* b, char* e) noexcept
{
    setp(b, e);
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

inline void MemoryInputStream::set_buffer(const char* b, const char* e) noexcept
{
    m_streambuf.set_buffer(b, e);
    clear();
}

template <size_t N> inline void MemoryInputStream::set_buffer(const char (&buffer)[N]) noexcept
{
    const char* b = buffer;
    const char* e = b + N;
    set_buffer(b, e);
}

inline void MemoryInputStream::set_string(const std::string& str) noexcept
{
    const char* b = str.data();
    const char* e = b + str.size();
    set_buffer(b, e);
}

inline void MemoryInputStream::set_c_string(const char* c_str) noexcept
{
    const char* b = c_str;
    const char* e = b + traits_type::length(c_str);
    set_buffer(b, e);
}


inline MemoryOutputStream::MemoryOutputStream()
    : std::ostream(&m_streambuf)
{
}

inline MemoryOutputStream::~MemoryOutputStream() noexcept
{
}

inline void MemoryOutputStream::set_buffer(char* b, char* e) noexcept
{
    m_streambuf.set_buffer(b, e);
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
