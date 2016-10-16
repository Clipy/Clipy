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

#ifndef REALM_STRING_HPP
#define REALM_STRING_HPP

#include <cstddef>
#include <algorithm>
#include <string>
#include <ostream>
#include <cstring>

#include <cfloat>
#include <cmath>

#include <realm/util/features.h>
#include <realm/util/optional.hpp>
#include <realm/utilities.hpp>
#include <realm/null.hpp>
#include <realm/owned_data.hpp>

namespace realm {

/// A reference to a chunk of character data.
///
/// An instance of this class can be thought of as a type tag on a region of
/// memory. It does not own the referenced memory, nor does it in any other way
/// attempt to manage the lifetime of it.
///
/// A null character inside the referenced region is considered a part of the
/// string by Realm.
///
/// For compatibility with C-style strings, when a string is stored in a Realm
/// database, it is always followed by a terminating null character, regardless
/// of whether the string itself has internal null characters. This means that
/// when a StringData object is extracted from Realm, the referenced region is
/// guaranteed to be followed immediately by an extra null character, but that
/// null character is not inside the referenced region. Therefore, all of the
/// following forms are guaranteed to return a pointer to a null-terminated
/// string:
///
/// \code{.cpp}
///
///   group.get_table_name(...).data()
///   table.get_column_name().data()
///   table.get_string(...).data()
///   table.get_mixed(...).get_string().data()
///
/// \endcode
///
/// Note that in general, no assumptions can be made about what follows a string
/// that is referenced by a StringData object, or whether anything follows it at
/// all. In particular, the receiver of a StringData object cannot assume that
/// the referenced string is followed by a null character unless there is an
/// externally provided guarantee.
///
/// This class makes it possible to distinguish between a 'null' reference and a
/// reference to the empty string (see is_null()).
///
/// \sa BinaryData
/// \sa Mixed
class StringData {
public:
    /// Construct a null reference.
    StringData() noexcept;

    /// If \a external_data is 'null', \a data_size must be zero.
    StringData(const char* external_data, size_t data_size) noexcept;

    template <class T, class A>
    StringData(const std::basic_string<char, T, A>&);

    template <class T, class A>
    operator std::basic_string<char, T, A>() const;

    // StringData does not store data, callers must manage their own strings.
    template <class T, class A>
    StringData(std::basic_string<char, T, A>&&) = delete;

    template <class T, class A>
    StringData(const util::Optional<std::basic_string<char, T, A>>&);

    StringData(const null&) noexcept;

    /// Initialize from a zero terminated C style string. Pass null to construct
    /// a null reference.
    StringData(const char* c_str) noexcept;

    char operator[](size_t i) const noexcept;

    const char* data() const noexcept;
    size_t size() const noexcept;

    /// Is this a null reference?
    ///
    /// An instance of StringData is a null reference when, and only when the
    /// stored size is zero (size()) and the stored pointer is the null pointer
    /// (data()).
    ///
    /// In the case of the empty string, the stored size is still zero, but the
    /// stored pointer is **not** the null pointer. It could for example point
    /// to the empty string literal. Note that the actual value of the pointer
    /// is immaterial in this case (as long as it is not zero), because when the
    /// size is zero, it is an error to dereference the pointer.
    ///
    /// Conversion of a StringData object to `bool` yields the logical negation
    /// of the result of calling this function. In other words, a StringData
    /// object is converted to true if it is not the null reference, otherwise
    /// it is converted to false.
    bool is_null() const noexcept;

    friend bool operator==(const StringData&, const StringData&) noexcept;
    friend bool operator!=(const StringData&, const StringData&) noexcept;

    //@{
    /// Trivial bytewise lexicographical comparison.
    friend bool operator<(const StringData&, const StringData&) noexcept;
    friend bool operator>(const StringData&, const StringData&) noexcept;
    friend bool operator<=(const StringData&, const StringData&) noexcept;
    friend bool operator>=(const StringData&, const StringData&) noexcept;
    //@}

    bool begins_with(StringData) const noexcept;
    bool ends_with(StringData) const noexcept;
    bool contains(StringData) const noexcept;

    //@{
    /// Undefined behavior if \a n, \a i, or <tt>i+n</tt> is greater than
    /// size().
    StringData prefix(size_t n) const noexcept;
    StringData suffix(size_t n) const noexcept;
    StringData substr(size_t i, size_t n) const noexcept;
    StringData substr(size_t i) const noexcept;
    //@}

    template <class C, class T>
    friend std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>&, const StringData&);

    explicit operator bool() const noexcept;

private:
    const char* m_data;
    size_t m_size;
};


// Implementation:

inline StringData::StringData() noexcept
    : m_data(nullptr)
    , m_size(0)
{
}

inline StringData::StringData(const char* external_data, size_t data_size) noexcept
    : m_data(external_data)
    , m_size(data_size)
{
    REALM_ASSERT_DEBUG(external_data || data_size == 0);
}

template <class T, class A>
inline StringData::StringData(const std::basic_string<char, T, A>& s)
    : m_data(s.data())
    , m_size(s.size())
{
}

template <class T, class A>
inline StringData::operator std::basic_string<char, T, A>() const
{
    return std::basic_string<char, T, A>(m_data, m_size);
}

template <class T, class A>
inline StringData::StringData(const util::Optional<std::basic_string<char, T, A>>& s)
    : m_data(s ? s->data() : nullptr)
    , m_size(s ? s->size() : 0)
{
}

inline StringData::StringData(const null&) noexcept
    : m_data(nullptr)
    , m_size(0)
{
}

inline StringData::StringData(const char* c_str) noexcept
    : m_data(c_str)
    , m_size(0)
{
    if (c_str)
        m_size = std::char_traits<char>::length(c_str);
}

inline char StringData::operator[](size_t i) const noexcept
{
    return m_data[i];
}

inline const char* StringData::data() const noexcept
{
    return m_data;
}

inline size_t StringData::size() const noexcept
{
    return m_size;
}

inline bool StringData::is_null() const noexcept
{
    return !m_data;
}

inline bool operator==(const StringData& a, const StringData& b) noexcept
{
    return a.m_size == b.m_size && a.is_null() == b.is_null() && safe_equal(a.m_data, a.m_data + a.m_size, b.m_data);
}

inline bool operator!=(const StringData& a, const StringData& b) noexcept
{
    return !(a == b);
}

inline bool operator<(const StringData& a, const StringData& b) noexcept
{
    if (a.is_null() && !b.is_null()) {
        // Null strings are smaller than all other strings, and not
        // equal to empty strings.
        return true;
    }
    return std::lexicographical_compare(a.m_data, a.m_data + a.m_size, b.m_data, b.m_data + b.m_size);
}

inline bool operator>(const StringData& a, const StringData& b) noexcept
{
    return b < a;
}

inline bool operator<=(const StringData& a, const StringData& b) noexcept
{
    return !(b < a);
}

inline bool operator>=(const StringData& a, const StringData& b) noexcept
{
    return !(a < b);
}

inline bool StringData::begins_with(StringData d) const noexcept
{
    if (is_null() && !d.is_null())
        return false;
    return d.m_size <= m_size && safe_equal(m_data, m_data + d.m_size, d.m_data);
}

inline bool StringData::ends_with(StringData d) const noexcept
{
    if (is_null() && !d.is_null())
        return false;
    return d.m_size <= m_size && safe_equal(m_data + m_size - d.m_size, m_data + m_size, d.m_data);
}

inline bool StringData::contains(StringData d) const noexcept
{
    if (is_null() && !d.is_null())
        return false;

    return d.m_size == 0 || std::search(m_data, m_data + m_size, d.m_data, d.m_data + d.m_size) != m_data + m_size;
}

inline StringData StringData::prefix(size_t n) const noexcept
{
    return substr(0, n);
}

inline StringData StringData::suffix(size_t n) const noexcept
{
    return substr(m_size - n);
}

inline StringData StringData::substr(size_t i, size_t n) const noexcept
{
    return StringData(m_data + i, n);
}

inline StringData StringData::substr(size_t i) const noexcept
{
    return substr(i, m_size - i);
}

template <class C, class T>
inline std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>& out, const StringData& d)
{
    for (const char* i = d.m_data; i != d.m_data + d.m_size; ++i)
        out << *i;
    return out;
}

inline StringData::operator bool() const noexcept
{
    return !is_null();
}

} // namespace realm

#endif // REALM_STRING_HPP
