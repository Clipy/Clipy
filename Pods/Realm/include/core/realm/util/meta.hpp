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

#ifndef REALM_UTIL_META_HPP
#define REALM_UTIL_META_HPP

namespace realm {
namespace util {


template <class T, class A, class B>
struct EitherTypeIs {
    static const bool value = false;
};
template <class T, class A>
struct EitherTypeIs<T, T, A> {
    static const bool value = true;
};
template <class T, class A>
struct EitherTypeIs<T, A, T> {
    static const bool value = true;
};
template <class T>
struct EitherTypeIs<T, T, T> {
    static const bool value = true;
};


} // namespace util
} // namespace realm

#endif // REALM_UTIL_META_HPP
