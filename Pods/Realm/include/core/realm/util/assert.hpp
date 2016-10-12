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
#ifndef REALM_UTIL_ASSERT_HPP
#define REALM_UTIL_ASSERT_HPP

#include <realm/util/features.h>
#include <realm/util/terminate.hpp>

#if REALM_ENABLE_ASSERTIONS || defined(REALM_DEBUG)
#  define REALM_ASSERTIONS_ENABLED 1
#else
#  define REALM_ASSERTIONS_ENABLED 0
#endif

#define REALM_ASSERT_RELEASE(condition) \
    ((condition) ? static_cast<void>(0) : \
    realm::util::terminate("Assertion failed: " #condition, __FILE__, __LINE__))

#if REALM_ASSERTIONS_ENABLED
#  define REALM_ASSERT(condition) REALM_ASSERT_RELEASE(condition)
#else
#  define REALM_ASSERT(condition) static_cast<void>(sizeof bool(condition))
#endif

#ifdef REALM_DEBUG
#  define REALM_ASSERT_DEBUG(condition) REALM_ASSERT_RELEASE(condition)
#else
#  define REALM_ASSERT_DEBUG(condition) static_cast<void>(sizeof bool(condition))
#endif

#define REALM_STRINGIFY(X) #X

#define REALM_ASSERT_RELEASE_EX(condition, ...) \
    ((condition) ? static_cast<void>(0) : \
    realm::util::terminate_with_info("Assertion failed: " # condition, __LINE__, __FILE__, \
                                     REALM_STRINGIFY((__VA_ARGS__)), __VA_ARGS__))

#ifdef REALM_DEBUG
#  define REALM_ASSERT_DEBUG_EX REALM_ASSERT_RELEASE_EX
#else
#  define REALM_ASSERT_DEBUG_EX(condition, ...) static_cast<void>(sizeof bool(condition))
#endif

// Becase the assert is used in noexcept methods, it's a bad idea to allocate
// buffer space for the message so therefore we must pass it to terminate which
// will 'cerr' it for us without needing any buffer
#if REALM_ENABLE_ASSERTIONS || defined(REALM_DEBUG)

#  define REALM_ASSERT_EX REALM_ASSERT_RELEASE_EX

#  define REALM_ASSERT_3(left, cmp, right) \
    (((left) cmp (right)) ? static_cast<void>(0) : \
     realm::util::terminate("Assertion failed: " \
                            "" #left " " #cmp " " #right, \
                            __FILE__, __LINE__, left, right))

#  define REALM_ASSERT_7(left1, cmp1, right1, logical, left2, cmp2, right2) \
    ((((left1) cmp1 (right1)) logical ((left2) cmp2 (right2))) ? static_cast<void>(0) : \
     realm::util::terminate("Assertion failed: " \
                            "" #left1 " " #cmp1 " " #right1 " " #logical " " \
                            "" #left2 " " #cmp2 " " #right2, \
                            __FILE__, __LINE__, left1, right1, left2, right2))

#  define REALM_ASSERT_11(left1, cmp1, right1, logical1, left2, cmp2, right2, logical2, left3, cmp3, right3) \
    ((((left1) cmp1 (right1)) logical1 ((left2) cmp2 (right2)) logical2 ((left3) cmp3 (right3))) ? static_cast<void>(0) : \
     realm::util::terminate("Assertion failed: " \
                            "" #left1 " " #cmp1 " " #right1 " " #logical1 " " \
                            "" #left2 " " #cmp2 " " #right2 " " #logical2 " " \
                            "" #left3 " " #cmp3 " " #right3, \
                            __FILE__, __LINE__, left1, right1, left2, right2, left3, right3))
#else
#  define REALM_ASSERT_EX(condition, ...) \
    static_cast<void>(sizeof bool(condition))
#  define REALM_ASSERT_3(left, cmp, right) \
    static_cast<void>(sizeof bool((left) cmp (right)))
#  define REALM_ASSERT_7(left1, cmp1, right1, logical, left2, cmp2, right2) \
    static_cast<void>(sizeof bool(((left1) cmp1 (right1)) logical ((left2) cmp2 (right2))))
#  define REALM_ASSERT_11(left1, cmp1, right1, logical1, left2, cmp2, right2, logical2, left3, cmp3, right3) \
    static_cast<void>(sizeof bool(((left1) cmp1 (right1)) logical1 ((left2) cmp2 (right2)) logical2 ((left3) cmp3 (right3))))
#endif

#ifdef REALM_COVER
#  define REALM_UNREACHABLE()
#  define REALM_COVER_NEVER(x) false
#  define REALM_COVER_ALWAYS(x) true
#else
#  define REALM_UNREACHABLE() \
      realm::util::terminate("Unreachable code", __FILE__, __LINE__)
#  define REALM_COVER_NEVER(x) (x)
#  define REALM_COVER_ALWAYS(x) (x)
#endif

#endif // REALM_UTIL_ASSERT_HPP
