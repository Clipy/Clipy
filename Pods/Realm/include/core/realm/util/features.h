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

#ifndef REALM_NO_CONFIG
#include <realm/util/config.h>
#endif

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

#ifdef __has_cpp_attribute
#define REALM_HAS_CPP_ATTRIBUTE(attr) __has_cpp_attribute(attr)
#else
#define REALM_HAS_CPP_ATTRIBUTE(attr) 0
#endif

#if REALM_HAS_CPP_ATTRIBUTE(clang::fallthrough)
#define REALM_FALLTHROUGH [[clang::fallthrough]]
#elif REALM_HAS_CPP_ATTRIBUTE(gnu::fallthrough)
#define REALM_FALLTHROUGH [[gnu::fallthrough]]
#elif REALM_HAS_CPP_ATTRIBUTE(fallthrough)
#define REALM_FALLTHROUGH [[fallthrough]]
#else
#define REALM_FALLTHROUGH
#endif

// This should be renamed to REALM_UNREACHABLE as soon as REALM_UNREACHABLE is renamed to
// REALM_ASSERT_NOT_REACHED which will better reflect its nature
#if defined(__GNUC__) || defined(__clang__)
#define REALM_COMPILER_HINT_UNREACHABLE __builtin_unreachable
#else
#define REALM_COMPILER_HINT_UNREACHABLE abort
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


// FIXME: Change this to use [[nodiscard]] in C++17.
#if defined(__GNUC__) || defined(__HP_aCC)
#define REALM_NODISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define REALM_NODISCARD _Check_return_
#else
#define REALM_NODISCARD
#endif


/* Thread specific data (only for POD types) */
#if defined __clang__
#define REALM_THREAD_LOCAL __thread
#else
#define REALM_THREAD_LOCAL thread_local
#endif


#if defined ANDROID || defined __ANDROID_API__
#define REALM_ANDROID 1
#else
#define REALM_ANDROID 0
#endif

#if defined _WIN32
#include <winapifamily.h>
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)
#define REALM_WINDOWS 1
#define REALM_UWP 0
#elif WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
#define REALM_WINDOWS 0
#define REALM_UWP 1
#endif
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
#if TARGET_OS_IPHONE == 1 && TARGET_OS_IOS == 1
/* Device (iPhone or iPad) or simulator. */
#define REALM_IOS 1
#define REALM_IOS_DEVICE !TARGET_OS_SIMULATOR
#else
#define REALM_IOS 0
#define REALM_IOS_DEVICE 0
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
#define REALM_IOS_DEVICE 0
#define REALM_WATCHOS 0
#define REALM_TVOS 0
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
// #define REALM_ASYNC_DAEMON FIXME Async commits not supported
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

// Address Sanitizer
#if defined(__has_feature) // Clang
#  if __has_feature(address_sanitizer)
#    define REALM_SANITIZE_ADDRESS 1
#  else
#    define REALM_SANITIZE_ADDRESS 0
#  endif
#elif defined(__SANITIZE_ADDRESS__) && __SANITIZE_ADDRESS__ // GCC
#  define REALM_SANITIZE_ADDRESS 1
#else
#  define REALM_SANITIZE_ADDRESS 0
#endif

// Thread Sanitizer
#if defined(__has_feature) // Clang
#  if __has_feature(thread_sanitizer)
#    define REALM_SANITIZE_THREAD 1
#  else
#    define REALM_SANITIZE_THREAD 0
#  endif
#elif defined(__SANITIZE_THREAD__) && __SANITIZE_THREAD__ // GCC
#  define REALM_SANITIZE_THREAD 1
#else
#  define REALM_SANITIZE_THREAD 0
#endif

#endif /* REALM_UTIL_FEATURES_H */
