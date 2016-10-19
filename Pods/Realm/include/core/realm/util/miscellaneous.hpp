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
