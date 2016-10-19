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

#ifndef REALM_BINARY_DATA_HPP
#define REALM_BINARY_DATA_HPP

#include <cstddef>
#include <algorithm>
#include <string>
#include <ostream>

#include <realm/util/features.h>
#include <realm/utilities.hpp>
#include <realm/owned_data.hpp>

namespace realm {

/// A reference to a chunk of binary data.
///
/// This class does not own the referenced memory, nor does it in any other way
/// attempt to manage the lifetime of it.
///
/// \sa StringData
class BinaryData {
public:
    BinaryData() noexcept
        : m_data(nullptr)
        , m_size(0)
    {
    }
    BinaryData(const char* external_data, size_t data_size) noexcept
        : m_data(external_data)
        , m_size(data_size)
    {
    }
    template <size_t N>
    explicit BinaryData(const char (&external_data)[N])
        : m_data(external_data)
        , m_size(N)
    {
    }
    template <class T, class A>
    explicit BinaryData(const std::basic_string<char, T, A>&);

    template <class T, class A>
    explicit operator std::basic_string<char, T, A>() const;

    char operator[](size_t i) const noexcept
    {
        return m_data[i];
    }

    const char* data() const noexcept
    {
        return m_data;
    }
    size_t size() const noexcept
    {
        return m_size;
    }

    /// Is this a null reference?
    ///
    /// An instance of BinaryData is a null reference when, and only when the
    /// stored size is zero (size()) and the stored pointer is the null pointer
    /// (data()).
    ///
    /// In the case of the empty byte sequence, the stored size is still zero,
    /// but the stored pointer is **not** the null pointer. Note that the actual
    /// value of the pointer is immaterial in this case (as long as it is not
    /// zero), because when the size is zero, it is an error to dereference the
    /// pointer.
    ///
    /// Conversion of a BinaryData object to `bool` yields the logical negation
    /// of the result of calling this function. In other words, a BinaryData
    /// object is converted to true if it is not the null reference, otherwise
    /// it is converted to false.
    ///
    /// It is important to understand that all of the functions and operators in
    /// this class, and most of the functions in the Realm API in general
    /// makes no distinction between a null reference and a reference to the
    /// empty byte sequence. These functions and operators never look at the
    /// stored pointer if the stored size is zero.
    bool is_null() const noexcept;

    friend bool operator==(const BinaryData&, const BinaryData&) noexcept;
    friend bool operator!=(const BinaryData&, const BinaryData&) noexcept;

    //@{
    /// Trivial bytewise lexicographical comparison.
    friend bool operator<(const BinaryData&, const BinaryData&) noexcept;
    friend bool operator>(const BinaryData&, const BinaryData&) noexcept;
    friend bool operator<=(const BinaryData&, const BinaryData&) noexcept;
    friend bool operator>=(const BinaryData&, const BinaryData&) noexcept;
    //@}

    bool begins_with(BinaryData) const noexcept;
    bool ends_with(BinaryData) const noexcept;
    bool contains(BinaryData) const noexcept;

    template <class C, class T>
    friend std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>&, const BinaryData&);

    explicit operator bool() const noexcept;

private:
    const char* m_data;
    size_t m_size;
};

/// A read-only chunk of binary data.
class OwnedBinaryData : public OwnedData {
public:
    using OwnedData::OwnedData;

    OwnedBinaryData() = default;
    OwnedBinaryData(const BinaryData& binary_data)
        : OwnedData(binary_data.data(), binary_data.size())
    {
    }

    BinaryData get() const
    {
        return {data(), size()};
    }
};


// Implementation:

template <class T, class A>
inline BinaryData::BinaryData(const std::basic_string<char, T, A>& s)
    : m_data(s.data())
    , m_size(s.size())
{
}

template <class T, class A>
inline BinaryData::operator std::basic_string<char, T, A>() const
{
    return std::basic_string<char, T, A>(m_data, m_size);
}

inline bool BinaryData::is_null() const noexcept
{
    return !m_data;
}

inline bool operator==(const BinaryData& a, const BinaryData& b) noexcept
{
    return a.m_size == b.m_size && a.is_null() == b.is_null() && safe_equal(a.m_data, a.m_data + a.m_size, b.m_data);
}

inline bool operator!=(const BinaryData& a, const BinaryData& b) noexcept
{
    return !(a == b);
}

inline bool operator<(const BinaryData& a, const BinaryData& b) noexcept
{
    if (a.is_null() || b.is_null())
        return !a.is_null() < !b.is_null();

    return std::lexicographical_compare(a.m_data, a.m_data + a.m_size, b.m_data, b.m_data + b.m_size);
}

inline bool operator>(const BinaryData& a, const BinaryData& b) noexcept
{
    return b < a;
}

inline bool operator<=(const BinaryData& a, const BinaryData& b) noexcept
{
    return !(b < a);
}

inline bool operator>=(const BinaryData& a, const BinaryData& b) noexcept
{
    return !(a < b);
}

inline bool BinaryData::begins_with(BinaryData d) const noexcept
{
    if (is_null() && !d.is_null())
        return false;

    return d.m_size <= m_size && safe_equal(m_data, m_data + d.m_size, d.m_data);
}

inline bool BinaryData::ends_with(BinaryData d) const noexcept
{
    if (is_null() && !d.is_null())
        return false;

    return d.m_size <= m_size && safe_equal(m_data + m_size - d.m_size, m_data + m_size, d.m_data);
}

inline bool BinaryData::contains(BinaryData d) const noexcept
{
    if (is_null() && !d.is_null())
        return false;

    return d.m_size == 0 || std::search(m_data, m_data + m_size, d.m_data, d.m_data + d.m_size) != m_data + m_size;
}

template <class C, class T>
inline std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>& out, const BinaryData& d)
{
    out << "BinaryData(" << static_cast<const void*>(d.m_data) << ", " << d.m_size << ")";
    return out;
}

inline BinaryData::operator bool() const noexcept
{
    return !is_null();
}

} // namespace realm

#endif // REALM_BINARY_DATA_HPP
