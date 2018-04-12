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

#ifndef REALM_UTIL_FEATURES_H
#define REALM_UTIL_FEATURES_H

#ifdef _MSC_VER
#pragma warning(disable : 4800) // Visual Studio int->bool performance warnings
#endif

#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

#include <realm/util/config.h>

/* The maximum number of elements in a B+-tree node. Applies to inner nodes and
 * to leaves. The minimum allowable value is 2.
 */
#ifndef REALM_MAX_BPNODE_SIZE
#define REALM_MAX_BPNODE_SIZE 1000
#endif


#define REALM_QUOTE_2(x) #x
#define REALM_QUOTE(x) REALM_QUOTE_2(x)

/* See these links for information about feature check macroes in GCC,
 * Clang, and MSVC:
 *
 * http://gcc.gnu.org/projects/cxx0x.html
 * http://clang.llvm.org/cxx_status.html
 * http://clang.llvm.org/docs/LanguageExtensions.html#checks-for-standard-language-features
 * http://msdn.microsoft.com/en-us/library/vstudio/hh567368.aspx
 * http://sourceforge.net/p/predef/wiki/Compilers
 */


/* Compiler is GCC and version is greater than or equal to the specified version */
#define REALM_HAVE_AT_LEAST_GCC(maj, min) \
    (__GNUC__ > (maj) || __GNUC__ == (maj) && __GNUC_MINOR__ >= (min))

#if defined(__clang__)
#define REALM_HAVE_CLANG_FEATURE(feature) __has_feature(feature)
#define REALM_HAVE_CLANG_WARNING(warning) __has_warning(warning)
#else
#define REALM_HAVE_CLANG_FEATURE(feature) 0
#define REALM_HAVE_CLANG_WARNING(warning) 0
#endif

#if defined(__GNUC__) // clang or GCC
#define REALM_PRAGMA(v) _Pragma(REALM_QUOTE_2(v))
#elif defined(_MSC_VER) // VS
#define REALM_PRAGMA(v) __pragma(v)
#else
#define REALM_PRAGMA(v)
#endif

#if defined(__clang__)
#define REALM_DIAG(v) REALM_PRAGMA(clang diagnostic v)
#elif defined(__GNUC__)
#define REALM_DIAG(v) REALM_PRAGMA(GCC diagnostic v)
#else
#define REALM_DIAG(v)
#endif

#define REALM_DIAG_PUSH() REALM_DIAG(push)
#define REALM_DIAG_POP() REALM_DIAG(pop)

#ifdef _MSC_VER
#define REALM_VS_WARNING_DISABLE #pragma warning (default: 4297)
#endif

#if REALM_HAVE_CLANG_WARNING("-Wtautological-compare") || REALM_HAVE_AT_LEAST_GCC(6, 0)
#define REALM_DIAG_IGNORE_TAUTOLOGICAL_COMPARE() REALM_DIAG(ignored "-Wtautological-compare")
#else
#define REALM_DIAG_IGNORE_TAUTOLOGICAL_COMPARE()
#endif

#ifdef _MSC_VER
#  define REALM_DIAG_IGNORE_UNSIGNED_MINUS() REALM_PRAGMA(warning(disable:4146))
#else
#define REALM_DIAG_IGNORE_UNSIGNED_MINUS()
#endif

/* Compiler is MSVC (Microsoft Visual C++) */
#if defined(_MSC_VER) && _MSC_VER >= 1600
#define REALM_HAVE_AT_LEAST_MSVC_10_2010 1
#endif
#if defined(_MSC_VER) && _MSC_VER >= 1700
#define REALM_HAVE_AT_LEAST_MSVC_11_2012 1
#endif
#if defined(_MSC_VER) && _MSC_VER >= 1800
#define REALM_HAVE_AT_LEAST_MSVC_12_2013 1
#endif


/* The way to specify that a function never returns. */
#if REALM_HAVE_AT_LEAST_GCC(4, 8) || REALM_HAVE_CLANG_FEATURE(cxx_attributes)
#define REALM_NORETURN [[noreturn]]
#elif __GNUC__
#define REALM_NORETURN __attribute__((noreturn))
#elif defined(_MSC_VER)
#define REALM_NORETURN __declspec(noreturn)
#else
#define REALM_NORETURN
#endif


/* The way to specify that a variable or type is intended to possibly
 * not be used. Use it to suppress a warning from the compiler. */
#if __GNUC__
#define REALM_UNUSED __attribute__((unused))
#else
#define REALM_UNUSED
#endif

/* The way to specify that a function is deprecated
 * not be used. Use it to suppress a warning from the compiler. */
#if __GNUC__
#define REALM_DEPRECATED(x) [[deprecated(x)]]
#else
#define REALM_DEPRECATED(x) __declspec(deprecated(x))
#endif


#if __GNUC__ || defined __INTEL_COMPILER
#define REALM_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#define REALM_LIKELY(expr) __builtin_expect(!!(expr), 1)
#else
#define REALM_UNLIKELY(expr) (expr)
#define REALM_LIKELY(expr) (expr)
#endif


#if defined(__GNUC__) || defined(__HP_aCC)
#define REALM_FORCEINLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define REALM_FORCEINLINE __forceinline
#else
#define REALM_FORCEINLINE inline
#endif


#if defined(__GNUC__) || defined(__HP_aCC)
#define REALM_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define REALM_NOINLINE __declspec(noinline)
#else
#define REALM_NOINLINE
#endif


/* Thread specific data (only for POD types) */
#if defined __clang__
#define REALM_THREAD_LOCAL __thread
#else
#define REALM_THREAD_LOCAL thread_local
#endif


#if defined ANDROID
#define REALM_ANDROID 1
#else
#define REALM_ANDROID 0
#endif

#if defined _WIN32
#  include <winapifamily.h>
#  if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)
#    define REALM_WINDOWS 1
#    define REALM_UWP 0
#  elif WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
#    define REALM_WINDOWS 0
#    define REALM_UWP 1
#  endif
#else
#define REALM_WINDOWS 0
#define REALM_UWP 0
#endif

// Some documentation of the defines provided by Apple:
// http://developer.apple.com/library/mac/documentation/Porting/Conceptual/PortingUnix/compiling/compiling.html#//apple_ref/doc/uid/TP40002850-SW13
#if defined __APPLE__ && defined __MACH__
#define REALM_PLATFORM_APPLE 1
/* Apple OSX and iOS (Darwin). */
#include <Availability.h>
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE == 1
/* Device (iPhone or iPad) or simulator. */
#define REALM_IOS 1
#else
#define REALM_IOS 0
#endif
#if TARGET_OS_WATCH == 1
/* Device (Apple Watch) or simulator. */
#define REALM_WATCHOS 1
#else
#define REALM_WATCHOS 0
#endif
#if TARGET_OS_TV
/* Device (Apple TV) or simulator. */
#define REALM_TVOS 1
#else
#define REALM_TVOS 0
#endif
#else
#define REALM_PLATFORM_APPLE 0
#define REALM_IOS 0
#define REALM_WATCHOS 0
#define REALM_TVOS 0
#endif

// asl_log is deprecated in favor of os_log as of the following versions:
// macos(10.12), ios(10.0), watchos(3.0), tvos(10.0)
// versions are defined in /usr/include/Availability.h
// __MAC_10_12   101200
// __IPHONE_10_0 100000
// __WATCHOS_3_0  30000
// __TVOS_10_0   100000
#if REALM_PLATFORM_APPLE \
    && ( \
        (REALM_IOS && defined(__IPHONE_OS_VERSION_MIN_REQUIRED) \
         && __IPHONE_OS_VERSION_MIN_REQUIRED >= 100000) \
     || (REALM_TVOS && defined(__TV_OS_VERSION_MIN_REQUIRED) \
         &&  __TV_OS_VERSION_MIN_REQUIRED >= 100000) \
     || (REALM_WATCHOS && defined(__WATCH_OS_VERSION_MIN_REQUIRED) \
         && __WATCH_OS_VERSION_MIN_REQUIRED >= 30000) \
     || (defined(__MAC_OS_X_VERSION_MIN_REQUIRED) \
         && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200) \
       )
#define REALM_APPLE_OS_LOG 1
#else
#define REALM_APPLE_OS_LOG 0
#endif

#if REALM_ANDROID || REALM_IOS || REALM_WATCHOS || REALM_TVOS || REALM_UWP
#define REALM_MOBILE 1
#else
#define REALM_MOBILE 0
#endif


#if defined(REALM_DEBUG) && !defined(REALM_COOKIE_CHECK)
#define REALM_COOKIE_CHECK
#endif

#if !REALM_IOS && !REALM_WATCHOS && !REALM_TVOS && !defined(_WIN32) && !REALM_ANDROID
#define REALM_ASYNC_DAEMON
#endif

// We're in i686 mode
#if defined(__i386) || defined(__i386__) || defined(__i686__) || defined(_M_I86) || defined(_M_IX86)
#define REALM_ARCHITECTURE_X86_32 1
#else
#define REALM_ARCHITECTURE_X86_32 0
#endif

// We're in amd64 mode
#if defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(_M_X64) || \
    defined(_M_AMD64)
#define REALM_ARCHITECTURE_X86_64 1
#else
#define REALM_ARCHITECTURE_X86_64 0
#endif

#endif /* REALM_UTIL_FEATURES_H */
