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

#ifndef REALM_UTIL_CALL_WITH_TUPLE_HPP
#define REALM_UTIL_CALL_WITH_TUPLE_HPP

#include <cstddef>
#include <tuple>

namespace realm {
namespace _impl {

/// \cond doxygen_skip
/// Doxygen warns about a recursive class relation, but this is intentional.

template <size_t...>
struct Indexes {
};
template <size_t N, size_t... I>
struct GenIndexes : GenIndexes<N - 1, N - 1, I...> {
};
template <size_t... I>
struct GenIndexes<0, I...> {
    typedef Indexes<I...> type;
};

/// \endcond

template <class F, class... A, size_t... I>
auto call_with_tuple(F func, std::tuple<A...> args, Indexes<I...>) -> decltype(func(std::get<I>(args)...))
{
    static_cast<void>(args); // Prevent GCC warning when tuple is empty
    return func(std::get<I>(args)...);
}

} // namespace _impl

namespace util {

template <class F, class... A>
auto call_with_tuple(F func, std::tuple<A...> args)
    -> decltype(_impl::call_with_tuple(std::move(func), std::move(args),
                                       typename _impl::GenIndexes<sizeof...(A)>::type()))
{
    return _impl::call_with_tuple(std::move(func), std::move(args), typename _impl::GenIndexes<sizeof...(A)>::type());
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_CALL_WITH_TUPLE_HPP
