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
#ifndef REALM_UTIL_STRING_VIEW_HPP
#define REALM_UTIL_STRING_VIEW_HPP

#include <cstddef>
#include <type_traits>
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>
#include <ostream>

#include <realm/util/features.h>


namespace realm {
namespace util {

template<class C, class T = std::char_traits<C>> class BasicStringView {
public:
    using value_type             = C;
    using traits_type            = T;
    using pointer                = C*;
    using const_pointer          = const C*;
    using reference              = C&;
    using const_reference        = const C&;
    using iterator               = const_pointer;
    using const_iterator         = const_pointer;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;

    static constexpr size_type npos = size_type(-1);

    BasicStringView() noexcept;
    BasicStringView(const std::basic_string<C, T>&) noexcept;
    BasicStringView(const char* data, size_type size) noexcept;
    BasicStringView(const char* c_str) noexcept;

    explicit operator std::basic_string<C, T>() const;

    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;

    const_reverse_iterator rbegin() const noexcept;
    const_reverse_iterator rend() const noexcept;
    const_reverse_iterator crbegin() const noexcept;
    const_reverse_iterator crend() const noexcept;

    const_reference operator[](size_type i) const noexcept;
    const_reference at(size_type i) const;
    const_reference front() const noexcept;
    const_reference back() const noexcept;

    const_pointer data() const noexcept;
    size_type size() const noexcept;
    bool empty() const noexcept;

    BasicStringView substr(size_type i = 0, size_type n = npos) const;
    int compare(BasicStringView other) const noexcept;
    size_type find(BasicStringView<C, T>, size_type i = 0) const noexcept;
    size_type find(C ch, size_type i = 0) const noexcept;
    size_type find_first_of(BasicStringView<C, T>, size_type i = 0) const noexcept;
    size_type find_first_of(C ch, size_type i = 0) const noexcept;
    size_type find_first_not_of(BasicStringView<C, T>, size_type i = 0) const noexcept;
    size_type find_first_not_of(C ch, size_type i = 0) const noexcept;

private:
    const char* m_data = nullptr;
    std::size_t m_size = 0;
};

template<class C, class T> bool operator==(BasicStringView<C, T>, BasicStringView<C, T>) noexcept;
template<class C, class T> bool operator!=(BasicStringView<C, T>, BasicStringView<C, T>) noexcept;
template<class C, class T> bool operator< (BasicStringView<C, T>, BasicStringView<C, T>) noexcept;
template<class C, class T> bool operator> (BasicStringView<C, T>, BasicStringView<C, T>) noexcept;
template<class C, class T> bool operator<=(BasicStringView<C, T>, BasicStringView<C, T>) noexcept;
template<class C, class T> bool operator>=(BasicStringView<C, T>, BasicStringView<C, T>) noexcept;

template<class C, class T>
bool operator==(std::decay_t<BasicStringView<C, T>>, BasicStringView<C, T>) noexcept;
template<class C, class T>
bool operator!=(std::decay_t<BasicStringView<C, T>>, BasicStringView<C, T>) noexcept;
template<class C, class T>
bool operator< (std::decay_t<BasicStringView<C, T>>, BasicStringView<C, T>) noexcept;
template<class C, class T>
bool operator> (std::decay_t<BasicStringView<C, T>>, BasicStringView<C, T>) noexcept;
template<class C, class T>
bool operator<=(std::decay_t<BasicStringView<C, T>>, BasicStringView<C, T>) noexcept;
template<class C, class T>
bool operator>=(std::decay_t<BasicStringView<C, T>>, BasicStringView<C, T>) noexcept;

template<class C, class T>
bool operator==(BasicStringView<C, T>, std::decay_t<BasicStringView<C, T>>) noexcept;
template<class C, class T>
bool operator!=(BasicStringView<C, T>, std::decay_t<BasicStringView<C, T>>) noexcept;
template<class C, class T>
bool operator< (BasicStringView<C, T>, std::decay_t<BasicStringView<C, T>>) noexcept;
template<class C, class T>
bool operator> (BasicStringView<C, T>, std::decay_t<BasicStringView<C, T>>) noexcept;
template<class C, class T>
bool operator<=(BasicStringView<C, T>, std::decay_t<BasicStringView<C, T>>) noexcept;
template<class C, class T>
bool operator>=(BasicStringView<C, T>, std::decay_t<BasicStringView<C, T>>) noexcept;


template<class C, class T>
std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>&, BasicStringView<C, T>);


using StringView = BasicStringView<char>;





// Implementation

template<class C, class T>
inline BasicStringView<C, T>::BasicStringView() noexcept
{
}

template<class C, class T>
inline BasicStringView<C, T>::BasicStringView(const std::basic_string<C, T>& str) noexcept :
    m_data{str.data()},
    m_size{str.size()}
{
}

template<class C, class T>
inline BasicStringView<C, T>::BasicStringView(const char* data, size_type size) noexcept :
    m_data{data},
    m_size{size}
{
}

template<class C, class T>
inline BasicStringView<C, T>::BasicStringView(const char* c_str) noexcept :
    m_data{c_str},
    m_size{T::length(c_str)}
{
}

template<class C, class T>
inline BasicStringView<C, T>::operator std::basic_string<C, T>() const
{
    return {m_data, m_size}; // Throws
}

template<class C, class T>
inline auto BasicStringView<C, T>::begin() const noexcept -> const_iterator
{
    return m_data;
}

template<class C, class T>
inline auto BasicStringView<C, T>::end() const noexcept -> const_iterator
{
    return m_data + m_size;
}

template<class C, class T>
inline auto BasicStringView<C, T>::cbegin() const noexcept -> const_iterator
{
    return begin();
}

template<class C, class T>
inline auto BasicStringView<C, T>::cend() const noexcept -> const_iterator
{
    return end();
}

template<class C, class T>
inline auto BasicStringView<C, T>::rbegin() const noexcept -> const_reverse_iterator
{
    return const_reverse_iterator{end()};
}

template<class C, class T>
inline auto BasicStringView<C, T>::rend() const noexcept -> const_reverse_iterator
{
    return const_reverse_iterator{begin()};
}

template<class C, class T>
inline auto BasicStringView<C, T>::crbegin() const noexcept -> const_reverse_iterator
{
    return rbegin();
}

template<class C, class T>
inline auto BasicStringView<C, T>::crend() const noexcept -> const_reverse_iterator
{
    return rend();
}

template<class C, class T>
inline auto BasicStringView<C, T>::operator[](size_type i) const noexcept -> const_reference
{
    return m_data[i];
}

template<class C, class T>
inline auto BasicStringView<C, T>::at(size_type i) const -> const_reference
{
    if (REALM_LIKELY(i < m_size))
        return m_data[i];
    throw std::out_of_range("index");
}

template<class C, class T>
inline auto BasicStringView<C, T>::front() const noexcept -> const_reference
{
    return m_data[0];
}

template<class C, class T>
inline auto BasicStringView<C, T>::back() const noexcept -> const_reference
{
    return m_data[m_size - 1];
}

template<class C, class T>
inline auto BasicStringView<C, T>::data() const noexcept -> const_pointer
{
    return m_data;
}

template<class C, class T>
inline auto BasicStringView<C, T>::size() const noexcept -> size_type
{
    return m_size;
}

template<class C, class T>
inline bool BasicStringView<C, T>::empty() const noexcept
{
    return (size() == 0);
}

template<class C, class T>
inline BasicStringView<C, T> BasicStringView<C, T>::substr(size_type i, size_type n) const
{
    if (REALM_LIKELY(i <= m_size)) {
        size_type m = std::min(n, m_size - i);
        return BasicStringView{m_data + i, m};
    }
    throw std::out_of_range("index");
}

template<class C, class T>
inline int BasicStringView<C, T>::compare(BasicStringView other) const noexcept
{
    size_type n = std::min(m_size, other.m_size);
    int ret = T::compare(m_data, other.m_data, n);
    if (REALM_LIKELY(ret != 0))
        return ret;
    if (m_size < other.m_size)
        return -1;
    if (m_size > other.m_size)
        return 1;
    return 0;
}

template<class C, class T>
inline auto BasicStringView<C, T>::find(BasicStringView<C, T> v, size_type i) const noexcept ->
    size_type
{
    if (REALM_LIKELY(!v.empty())) {
        if (REALM_LIKELY(i < m_size)) {
            const C* p = std::search(begin() + i, end(), v.begin(), v.end());
            if (p != end())
                return size_type(p - begin());
        }
        return npos;
    }
    return i;
}

template<class C, class T>
inline auto BasicStringView<C, T>::find(C ch, size_type i) const noexcept -> size_type
{
    if (REALM_LIKELY(i < m_size)) {
        const C* p = std::find(begin() + i, end(), ch);
        if (p != end())
            return size_type(p - begin());
    }
    return npos;
}

template<class C, class T>
inline auto BasicStringView<C, T>::find_first_of(BasicStringView<C, T> v,
                                                 size_type i) const noexcept -> size_type
{
    for (size_type j = i; j < m_size; ++j) {
        if (REALM_LIKELY(v.find(m_data[j]) == npos))
            continue;
        return j;
    }
    return npos;
}

template<class C, class T>
inline auto BasicStringView<C, T>::find_first_of(C ch, size_type i) const noexcept -> size_type
{
    for (size_type j = i; j < m_size; ++j) {
        if (REALM_UNLIKELY(m_data[j] == ch))
            return j;
    }
    return npos;
}

template<class C, class T>
inline auto BasicStringView<C, T>::find_first_not_of(BasicStringView<C, T> v,
                                                     size_type i) const noexcept -> size_type
{
    for (size_type j = i; j < m_size; ++j) {
        if (REALM_UNLIKELY(v.find(m_data[j]) == npos))
            return j;
    }
    return npos;
}

template<class C, class T>
inline auto BasicStringView<C, T>::find_first_not_of(C ch, size_type i) const noexcept -> size_type
{
    for (size_type j = i; j < m_size; ++j) {
        if (REALM_UNLIKELY(m_data[j] != ch))
            return j;
    }
    return npos;
}

template<class C, class T>
inline bool operator==(BasicStringView<C, T> lhs, BasicStringView<C, T> rhs) noexcept
{
    return (lhs.compare(rhs) == 0);
}

template<class C, class T>
inline bool operator!=(BasicStringView<C, T> lhs, BasicStringView<C, T> rhs) noexcept
{
    return (lhs.compare(rhs) != 0);
}

template<class C, class T>
inline bool operator<(BasicStringView<C, T> lhs, BasicStringView<C, T> rhs) noexcept
{
    return (lhs.compare(rhs) < 0);
}

template<class C, class T>
inline bool operator>(BasicStringView<C, T> lhs, BasicStringView<C, T> rhs) noexcept
{
    return (lhs.compare(rhs) > 0);
}

template<class C, class T>
inline bool operator<=(BasicStringView<C, T> lhs, BasicStringView<C, T> rhs) noexcept
{
    return (lhs.compare(rhs) <= 0);
}

template<class C, class T>
inline bool operator>=(BasicStringView<C, T> lhs, BasicStringView<C, T> rhs) noexcept
{
    return (lhs.compare(rhs) >= 0);
}

template<class C, class T>
inline bool operator==(std::decay_t<BasicStringView<C, T>> lhs, BasicStringView<C, T> rhs) noexcept
{
    return (lhs.compare(rhs) == 0);
}

template<class C, class T>
inline bool operator!=(std::decay_t<BasicStringView<C, T>> lhs, BasicStringView<C, T> rhs) noexcept
{
    return (lhs.compare(rhs) != 0);
}

template<class C, class T>
inline bool operator<(std::decay_t<BasicStringView<C, T>> lhs, BasicStringView<C, T> rhs) noexcept
{
    return (lhs.compare(rhs) < 0);
}

template<class C, class T>
inline bool operator>(std::decay_t<BasicStringView<C, T>> lhs, BasicStringView<C, T> rhs) noexcept
{
    return (lhs.compare(rhs) > 0);
}

template<class C, class T>
inline bool operator<=(std::decay_t<BasicStringView<C, T>> lhs, BasicStringView<C, T> rhs) noexcept
{
    return (lhs.compare(rhs) <= 0);
}

template<class C, class T>
inline bool operator>=(std::decay_t<BasicStringView<C, T>> lhs, BasicStringView<C, T> rhs) noexcept
{
    return (lhs.compare(rhs) >= 0);
}

template<class C, class T>
inline bool operator==(BasicStringView<C, T> lhs, std::decay_t<BasicStringView<C, T>> rhs) noexcept
{
    return (lhs.compare(rhs) == 0);
}

template<class C, class T>
inline bool operator!=(BasicStringView<C, T> lhs, std::decay_t<BasicStringView<C, T>> rhs) noexcept
{
    return (lhs.compare(rhs) != 0);
}

template<class C, class T>
inline bool operator<(BasicStringView<C, T> lhs, std::decay_t<BasicStringView<C, T>> rhs) noexcept
{
    return (lhs.compare(rhs) < 0);
}

template<class C, class T>
inline bool operator>(BasicStringView<C, T> lhs, std::decay_t<BasicStringView<C, T>> rhs) noexcept
{
    return (lhs.compare(rhs) > 0);
}

template<class C, class T>
inline bool operator<=(BasicStringView<C, T> lhs, std::decay_t<BasicStringView<C, T>> rhs) noexcept
{
    return (lhs.compare(rhs) <= 0);
}

template<class C, class T>
inline bool operator>=(BasicStringView<C, T> lhs, std::decay_t<BasicStringView<C, T>> rhs) noexcept
{
    return (lhs.compare(rhs) >= 0);
}

template<class C, class T>
inline std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>& out,
                                            BasicStringView<C, T> view)
{
    typename std::basic_ostream<C, T>::sentry sentry{out};
    if (REALM_LIKELY(sentry))
        out.write(view.data(), view.size());
    return out;
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_STRING_VIEW_HPP
