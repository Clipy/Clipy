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
#ifndef REALM_UTIL_MISCELLANEOUS_HPP
#define REALM_UTIL_MISCELLANEOUS_HPP

#include <type_traits>

namespace realm {
namespace util {

// FIXME: Replace this with std::add_const_t when we switch over to C++14 by
// default.
/// \brief Adds const qualifier, unless T already has the const qualifier
template <class T>
using add_const_t = typename std::add_const<T>::type;

// FIXME: Replace this with std::as_const when we switch over to C++17 by
// default.
/// \brief Forms an lvalue reference to const T
template <class T>
constexpr add_const_t<T>& as_const(T& v) noexcept
{
    return v;
}

/// \brief Disallows rvalue arguments
template <class T>
add_const_t<T>& as_const(const T&&) = delete;

} // namespace util
} // namespace realm

#endif // REALM_UTIL_MISCELLANEOUS_HPP
