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
#ifndef REALM_UTIL_META_HPP
#define REALM_UTIL_META_HPP

namespace realm {
namespace util {


template<class T, class A, class B>
struct EitherTypeIs { static const bool value = false; };
template<class T, class A>
struct EitherTypeIs<T,T,A> { static const bool value = true; };
template<class T, class A>
struct EitherTypeIs<T,A,T> { static const bool value = true; };
template<class T>
struct EitherTypeIs<T,T,T> { static const bool value = true; };


} // namespace util
} // namespace realm

#endif // REALM_UTIL_META_HPP
