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
#ifndef REALM_UTIL_CALL_WITH_TUPLE_HPP
#define REALM_UTIL_CALL_WITH_TUPLE_HPP

#include <stddef.h>
#include <tuple>

namespace realm {
namespace _impl {

template<size_t...> struct Indexes {};
template<size_t N, size_t... I> struct GenIndexes: GenIndexes<N-1, N-1, I...> {};
template<size_t... I> struct GenIndexes<0, I...> { typedef Indexes<I...> type; };

template<class F, class... A, size_t... I>
auto call_with_tuple(F func, std::tuple<A...> args, Indexes<I...>)
    -> decltype(func(std::get<I>(args)...))
{
    static_cast<void>(args); // Prevent GCC warning when tuple is empty
    return func(std::get<I>(args)...);
}

} // namespace _impl

namespace util {

template<class F, class... A>
auto call_with_tuple(F func, std::tuple<A...> args)
    -> decltype(_impl::call_with_tuple(std::move(func), std::move(args),
                                       typename _impl::GenIndexes<sizeof... (A)>::type()))
{
    return _impl::call_with_tuple(std::move(func), std::move(args),
                                  typename _impl::GenIndexes<sizeof... (A)>::type());
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_CALL_WITH_TUPLE_HPP
