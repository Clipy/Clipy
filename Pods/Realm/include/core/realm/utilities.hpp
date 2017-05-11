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

#ifndef REALM_UTILITIES_HPP
#define REALM_UTILITIES_HPP

#include <cstdint>
#include <cstdlib>
#include <cstdlib> // size_t
#include <cstdio>
#include <algorithm>
#include <functional>

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include <realm/util/features.h>
#include <realm/util/assert.hpp>
#include <realm/util/safe_int_ops.hpp>

// GCC defines __i386__ and __x86_64__
#if (defined(__X86__) || defined(__i386__) || defined(i386) || defined(_M_IX86) || defined(__386__) ||               \
     defined(__x86_64__) || defined(_M_X64))
#define REALM_X86_OR_X64
#define REALM_X86_OR_X64_TRUE true
#else
#define REALM_X86_OR_X64_TRUE false
#endif

// GCC defines __arm__
#ifdef __arm__
#define REALM_ARCH_ARM
#endif

#if defined _LP64 || defined __LP64__ || defined __64BIT__ || defined _ADDR64 || defined _WIN64 ||                   \
    defined __arch64__ || (defined(__WORDSIZE) && __WORDSIZE == 64) || (defined __sparc && defined __sparcv9) ||     \
    defined __x86_64 || defined __amd64 || defined __x86_64__ || defined _M_X64 || defined _M_IA64 ||                \
    defined __ia64 || defined __IA64__
#define REALM_PTR_64
#endif


#if defined(REALM_PTR_64) && defined(REALM_X86_OR_X64)
#define REALM_COMPILER_SSE // Compiler supports SSE 4.2 through __builtin_ accessors or back-end assembler
#define REALM_COMPILER_AVX
#endif

namespace realm {

using StringCompareCallback = std::function<bool(const char* string1, const char* string2)>;

extern signed char sse_support;
extern signed char avx_support;

template <int version>
REALM_FORCEINLINE bool sseavx()
{
    /*
    Return whether or not SSE 3.0 (if version = 30) or 4.2 (for version = 42) is supported. Return value
    is based on the CPUID instruction.

    sse_support = -1: No SSE support
    sse_support = 0: SSE3
    sse_support = 1: SSE42

    avx_support = -1: No AVX support
    avx_support = 0: AVX1 supported
    sse_support = 1: AVX2 supported (not yet implemented for detection in our cpuid_init(), todo)

    This lets us test very rapidly at runtime because we just need 1 compare instruction (with 0) to test both for
    SSE 3 and 4.2 by caller (compiler optimizes if calls are concecutive), and can decide branch with ja/jl/je because
    sse_support is signed type. Also, 0 requires no immediate operand. Same for AVX.

    We runtime-initialize sse_support in a constructor of a static variable which is not guaranteed to be called
    prior to cpu_sse(). So we compile-time initialize sse_support to -2 as fallback.
    */
    static_assert(version == 1 || version == 2 || version == 30 || version == 42,
                  "Only version == 1 (AVX), 2 (AVX2), 30 (SSE 3) and 42 (SSE 4.2) are supported for detection");
#ifdef REALM_COMPILER_SSE
    if (version == 30)
        return (sse_support >= 0);
    else if (version == 42)
        return (sse_support > 0); // faster than == 1 (0 requres no immediate operand)
    else if (version == 1)        // avx
        return (avx_support >= 0);
    else if (version == 2) // avx2
        return (avx_support > 0);
    else
        return false;
#else
    return false;
#endif
}

void cpuid_init();
void* round_up(void* p, size_t align);
void* round_down(void* p, size_t align);
size_t round_up(size_t p, size_t align);
size_t round_down(size_t p, size_t align);
void millisleep(unsigned long milliseconds);

#ifdef REALM_SLAB_ALLOC_TUNE
void process_mem_usage(double& vm_usage, double& resident_set);
#endif
// popcount
int fast_popcount32(int32_t x);
int fast_popcount64(int64_t x);
uint64_t fastrand(uint64_t max = 0xffffffffffffffffULL, bool is_seed = false);

// log2 - returns -1 if x==0, otherwise log2(x)
inline int log2(size_t x)
{
    if (x == 0)
        return -1;
#if defined(__GNUC__)
#ifdef REALM_PTR_64
    return 63 - __builtin_clzll(x); // returns int
#else
    return 31 - __builtin_clz(x); // returns int
#endif
#elif defined(_WIN32)
    unsigned long index = 0;
#ifdef REALM_PTR_64
    unsigned char c = _BitScanReverse64(&index, x); // outputs unsigned long
#else
    unsigned char c = _BitScanReverse(&index, x); // outputs unsigned long
#endif
    return static_cast<int>(index);
#else // not __GNUC__ and not _WIN32
    int r = 0;
    while (x >>= 1) {
        r++;
    }
    return r;
#endif
}

// Implementation:

// Safe cast from 64 to 32 bits on 32 bit architecture. Differs from to_ref() by not testing alignment and
// REF-bitflag.
inline size_t to_size_t(int_fast64_t v) noexcept
{
    REALM_ASSERT_DEBUG(!util::int_cast_has_overflow<size_t>(v));
    return size_t(v);
}


template <typename ReturnType, typename OriginalType>
ReturnType type_punning(OriginalType variable) noexcept
{
    union Both {
        OriginalType in;
        ReturnType out;
    };
    Both both;
    both.out = ReturnType(); // Clear all bits in case ReturnType is larger than OriginalType
    both.in = variable;
    return both.out;
}

// Also see the comments in Array::index_string()
enum FindRes {
    // Indicate that no results were found in the search
    FindRes_not_found,
    // Indicates a single result is found
    FindRes_single,
    // Indicates more than one result is found and they are stored in a column
    FindRes_column
};

enum IndexMethod {
    index_FindFirst,
    index_FindAll_nocopy,
    index_Count,
};

// Combined result of the index_FindAll_nocopy operation. The column returned
// can contain results that are not matches but all matches are within the
// returned start_ndx and end_ndx.
struct InternalFindResult {
    // Reference to a IntegerColumn containing result rows, or a single row
    // value if the result is FindRes_single.
    size_t payload;
    // Offset into the result column to start at.
    size_t start_ndx;
    // Offset index in the result column to end at.
    size_t end_ndx;
};


// realm::is_any<T, U1, U2, U3, ...> ==
// std::is_same<T, U1>::value || std::is_same<T, U2>::value || std::is_same<T, U3>::value ...
template <typename... T>
struct is_any : std::false_type {
};

template <typename T, typename... Ts>
struct is_any<T, T, Ts...> : std::true_type {
};

template <typename T, typename U, typename... Ts>
struct is_any<T, U, Ts...> : is_any<T, Ts...> {
};


// Use realm::safe_equal() instead of std::equal() if one of the parameters can be a null pointer.
template <class InputIterator1, class InputIterator2>
bool safe_equal(InputIterator1 first1, InputIterator1 last1, InputIterator2 first2)
{
#if defined(_MSC_VER)
    // VS has a special check in debug mode against passing a null pointer std::equal(); it will give a warning
    // at runtime if this is observed.
    // It's uncertain if this is allowed by the C++ standard. For details, see
    // http://stackoverflow.com/questions/19120779/is-char-p-0-stdequalp-p-p-well-defined-according-to-the-c-standard.
    // So we use a safe C++14 method instead that takes two range pairs.
    size_t len = last1 - first1;
    return std::equal(first1, last1, first2, first2 + len);
#else
    return std::equal(first1, last1, first2);
#endif
}

// Use realm::safe_copy_n() instead of std::copy_n() if one of the parameters can be a null pointer. See the
// explanation of safe_equal() above; same things apply.
template< class InputIt, class Size, class OutputIt>
OutputIt safe_copy_n(InputIt first, Size count, OutputIt result)
{
#if defined(_MSC_VER)
    // This loop and the method prototype is copy pasted
    // from "Possible implementation" on http://en.cppreference.com/w/cpp/algorithm/copy_n
    if (count > 0) {
        *result++ = *first;
        for (Size i = 1; i < count; ++i) {
            *result++ = *++first;
        }
    }
    return result;
#else
    return std::copy_n(first, count, result);
#endif
}


template <class T>
struct Wrap {
    Wrap(const T& v)
        : m_value(v)
    {
    }
    operator T() const
    {
        return m_value;
    }

private:
    T m_value;
};

// PlacementDelete is intended for use with std::unique_ptr when it holds an object allocated with
// placement new. It simply calls the object's destructor without freeing the memory.
struct PlacementDelete {
    template <class T>
    void operator()(T* v) const
    {
        v->~T();
    }
};

} // namespace realm

#endif // REALM_UTILITIES_HPP
