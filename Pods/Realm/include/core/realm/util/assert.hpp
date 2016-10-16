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

#ifndef REALM_UTIL_ASSERT_HPP
#define REALM_UTIL_ASSERT_HPP

#include <realm/util/features.h>
#include <realm/util/terminate.hpp>

#if REALM_ENABLE_ASSERTIONS || defined(REALM_DEBUG)
#define REALM_ASSERTIONS_ENABLED 1
#else
#define REALM_ASSERTIONS_ENABLED 0
#endif

#define REALM_ASSERT_RELEASE(condition)                                                                              \
    (REALM_LIKELY(condition) ? static_cast<void>(0)                                                                  \
                             : realm::util::terminate("Assertion failed: " #condition, __FILE__, __LINE__))

#if REALM_ASSERTIONS_ENABLED
#define REALM_ASSERT(condition) REALM_ASSERT_RELEASE(condition)
#else
#define REALM_ASSERT(condition) static_cast<void>(sizeof bool(condition))
#endif

#ifdef REALM_DEBUG
#define REALM_ASSERT_DEBUG(condition) REALM_ASSERT_RELEASE(condition)
#else
#define REALM_ASSERT_DEBUG(condition) static_cast<void>(sizeof bool(condition))
#endif

#define REALM_STRINGIFY(X) #X

#define REALM_ASSERT_RELEASE_EX(condition, ...)                                                                      \
    (REALM_LIKELY(condition) ? static_cast<void>(0)                                                                  \
                             : realm::util::terminate_with_info("Assertion failed: " #condition, __LINE__, __FILE__, \
                                                                REALM_STRINGIFY((__VA_ARGS__)), __VA_ARGS__))

#ifdef REALM_DEBUG
#define REALM_ASSERT_DEBUG_EX REALM_ASSERT_RELEASE_EX
#else
#define REALM_ASSERT_DEBUG_EX(condition, ...) static_cast<void>(sizeof bool(condition))
#endif

// Becase the assert is used in noexcept methods, it's a bad idea to allocate
// buffer space for the message so therefore we must pass it to terminate which
// will 'cerr' it for us without needing any buffer
#if REALM_ENABLE_ASSERTIONS || defined(REALM_DEBUG)

#define REALM_ASSERT_EX REALM_ASSERT_RELEASE_EX

#define REALM_ASSERT_3(left, cmp, right)                                                                             \
    (REALM_LIKELY((left)cmp(right)) ? static_cast<void>(0)                                                           \
                                    : realm::util::terminate("Assertion failed: "                                    \
                                                             "" #left " " #cmp " " #right,                           \
                                                             __FILE__, __LINE__, left, right))

#define REALM_ASSERT_7(left1, cmp1, right1, logical, left2, cmp2, right2)                                            \
    (REALM_LIKELY(((left1)cmp1(right1))logical((left2)cmp2(right2)))                                                 \
         ? static_cast<void>(0)                                                                                      \
         : realm::util::terminate("Assertion failed: "                                                               \
                                  "" #left1 " " #cmp1 " " #right1 " " #logical " "                                   \
                                  "" #left2 " " #cmp2 " " #right2,                                                   \
                                  __FILE__, __LINE__, left1, right1, left2, right2))

#define REALM_ASSERT_11(left1, cmp1, right1, logical1, left2, cmp2, right2, logical2, left3, cmp3, right3)           \
    (REALM_LIKELY(((left1)cmp1(right1))logical1((left2)cmp2(right2)) logical2((left3)cmp3(right3)))                  \
         ? static_cast<void>(0)                                                                                      \
         : realm::util::terminate("Assertion failed: "                                                               \
                                  "" #left1 " " #cmp1 " " #right1 " " #logical1 " "                                  \
                                  "" #left2 " " #cmp2 " " #right2 " " #logical2 " "                                  \
                                  "" #left3 " " #cmp3 " " #right3,                                                   \
                                  __FILE__, __LINE__, left1, right1, left2, right2, left3, right3))
#else
#define REALM_ASSERT_EX(condition, ...) static_cast<void>(sizeof bool(condition))
#define REALM_ASSERT_3(left, cmp, right) static_cast<void>(sizeof bool((left)cmp(right)))
#define REALM_ASSERT_7(left1, cmp1, right1, logical, left2, cmp2, right2)                                            \
    static_cast<void>(sizeof bool(((left1)cmp1(right1))logical((left2)cmp2(right2))))
#define REALM_ASSERT_11(left1, cmp1, right1, logical1, left2, cmp2, right2, logical2, left3, cmp3, right3)           \
    static_cast<void>(sizeof bool(((left1)cmp1(right1))logical1((left2)cmp2(right2)) logical2((left3)cmp3(right3))))
#endif

#ifdef REALM_COVER
#define REALM_UNREACHABLE()
#define REALM_COVER_NEVER(x) false
#define REALM_COVER_ALWAYS(x) true
#else
#define REALM_UNREACHABLE() realm::util::terminate("Unreachable code", __FILE__, __LINE__)
#define REALM_COVER_NEVER(x) (x)
#define REALM_COVER_ALWAYS(x) (x)
#endif

#endif // REALM_UTIL_ASSERT_HPP
