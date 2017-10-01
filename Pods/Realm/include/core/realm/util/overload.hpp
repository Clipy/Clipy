/*************************************************************************
 *
 * Copyright 2017 Realm Inc.
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

#ifndef REALM_UTIL_OVERLOAD_HPP
#define REALM_UTIL_OVERLOAD_HPP

#include <utility>

namespace realm {

namespace _impl {

template<typename Fn, typename... Fns>
struct Overloaded;

} // namespace _impl


namespace util {

// Declare an overload set using lambdas or other function objects.
// A minimal version of C++ Library Evolution Working Group proposal P0051R2.

template<typename... Fns>
_impl::Overloaded<Fns...> overload(Fns&&... f)
{
    return _impl::Overloaded<Fns...>(std::forward<Fns>(f)...);
}

} // namespace util


namespace _impl {

template<typename Fn, typename... Fns>
struct Overloaded : Fn, Overloaded<Fns...> {
    template<typename U, typename... Rest>
    Overloaded(U&& fn, Rest&&... rest) : Fn(std::forward<U>(fn)), Overloaded<Fns...>(std::forward<Rest>(rest)...) { }

    using Fn::operator();
    using Overloaded<Fns...>::operator();
};

template<typename Fn>
struct Overloaded<Fn> : Fn {
    template<typename U>
    Overloaded(U&& fn) : Fn(std::forward<U>(fn)) { }

    using Fn::operator();
};

} // namespace _impl
} // namespace realm

#endif // REALM_UTIL_OVERLOAD_HPP
