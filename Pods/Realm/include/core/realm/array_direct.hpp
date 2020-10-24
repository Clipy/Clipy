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

#ifndef REALM_ARRAY_DIRECT_HPP
#define REALM_ARRAY_DIRECT_HPP

#include <realm/utilities.hpp>
#include <realm/alloc.hpp>

using namespace realm::util;

// clang-format off
/* wid == 16/32 likely when accessing offsets in B tree */
#define REALM_TEMPEX(fun, wid, arg) \
    if (wid == 16) {fun<16> arg;} \
    else if (wid == 32) {fun<32> arg;} \
    else if (wid == 0) {fun<0> arg;} \
    else if (wid == 1) {fun<1> arg;} \
    else if (wid == 2) {fun<2> arg;} \
    else if (wid == 4) {fun<4> arg;} \
    else if (wid == 8) {fun<8> arg;} \
    else if (wid == 64) {fun<64> arg;} \
    else {REALM_ASSERT_DEBUG(false); fun<0> arg;}

#define REALM_TEMPEX2(fun, targ, wid, arg) \
    if (wid == 16) {fun<targ, 16> arg;} \
    else if (wid == 32) {fun<targ, 32> arg;} \
    else if (wid == 0) {fun<targ, 0> arg;} \
    else if (wid == 1) {fun<targ, 1> arg;} \
    else if (wid == 2) {fun<targ, 2> arg;} \
    else if (wid == 4) {fun<targ, 4> arg;} \
    else if (wid == 8) {fun<targ, 8> arg;} \
    else if (wid == 64) {fun<targ, 64> arg;} \
    else {REALM_ASSERT_DEBUG(false); fun<targ, 0> arg;}

#define REALM_TEMPEX3(fun, targ1, targ2, wid, arg) \
    if (wid == 16) {fun<targ1, targ2, 16> arg;} \
    else if (wid == 32) {fun<targ1, targ2, 32> arg;} \
    else if (wid == 0) {fun<targ1, targ2, 0> arg;} \
    else if (wid == 1) {fun<targ1, targ2, 1> arg;} \
    else if (wid == 2) {fun<targ1, targ2, 2> arg;} \
    else if (wid == 4) {fun<targ1, targ2, 4> arg;} \
    else if (wid == 8) {fun<targ1, targ2, 8> arg;} \
    else if (wid == 64) {fun<targ1, targ2, 64> arg;} \
    else {REALM_ASSERT_DEBUG(false); fun<targ1, targ2, 0> arg;}

#define REALM_TEMPEX4(fun, targ1, targ2, wid, targ3, arg) \
    if (wid == 16) {fun<targ1, targ2, 16, targ3> arg;} \
    else if (wid == 32) {fun<targ1, targ2, 32, targ3> arg;} \
    else if (wid == 0) {fun<targ1, targ2, 0, targ3> arg;} \
    else if (wid == 1) {fun<targ1, targ2, 1, targ3> arg;} \
    else if (wid == 2) {fun<targ1, targ2, 2, targ3> arg;} \
    else if (wid == 4) {fun<targ1, targ2, 4, targ3> arg;} \
    else if (wid == 8) {fun<targ1, targ2, 8, targ3> arg;} \
    else if (wid == 64) {fun<targ1, targ2, 64, targ3> arg;} \
    else {REALM_ASSERT_DEBUG(false); fun<targ1, targ2, 0, targ3> arg;}

#define REALM_TEMPEX5(fun, targ1, targ2, targ3, targ4, wid, arg) \
    if (wid == 16) {fun<targ1, targ2, targ3, targ4, 16> arg;} \
    else if (wid == 32) {fun<targ1, targ2, targ3, targ4, 32> arg;} \
    else if (wid == 0) {fun<targ1, targ2, targ3, targ4, 0> arg;} \
    else if (wid == 1) {fun<targ1, targ2, targ3, targ4, 1> arg;} \
    else if (wid == 2) {fun<targ1, targ2, targ3, targ4, 2> arg;} \
    else if (wid == 4) {fun<targ1, targ2, targ3, targ4, 4> arg;} \
    else if (wid == 8) {fun<targ1, targ2, targ3, targ4, 8> arg;} \
    else if (wid == 64) {fun<targ1, targ2, targ3, targ4, 64> arg;} \
    else {REALM_ASSERT_DEBUG(false); fun<targ1, targ2, targ3, targ4, 0> arg;}
// clang-format on

namespace realm {

/// Takes a 64-bit value and returns the minimum number of bits needed
/// to fit the value. For alignment this is rounded up to nearest
/// log2. Posssible results {0, 1, 2, 4, 8, 16, 32, 64}
size_t bit_width(int64_t value);

// Direct access methods

template <size_t width>
void set_direct(char* data, size_t ndx, int_fast64_t value) noexcept
{
    if (width == 0) {
        REALM_ASSERT_DEBUG(value == 0);
        return;
    }
    else if (width == 1) {
        REALM_ASSERT_DEBUG(0 <= value && value <= 0x01);
        size_t byte_ndx = ndx / 8;
        size_t bit_ndx = ndx % 8;
        typedef unsigned char uchar;
        uchar* p = reinterpret_cast<uchar*>(data) + byte_ndx;
        *p = uchar((*p & ~(0x01 << bit_ndx)) | (int(value) & 0x01) << bit_ndx);
    }
    else if (width == 2) {
        REALM_ASSERT_DEBUG(0 <= value && value <= 0x03);
        size_t byte_ndx = ndx / 4;
        size_t bit_ndx = ndx % 4 * 2;
        typedef unsigned char uchar;
        uchar* p = reinterpret_cast<uchar*>(data) + byte_ndx;
        *p = uchar((*p & ~(0x03 << bit_ndx)) | (int(value) & 0x03) << bit_ndx);
    }
    else if (width == 4) {
        REALM_ASSERT_DEBUG(0 <= value && value <= 0x0F);
        size_t byte_ndx = ndx / 2;
        size_t bit_ndx = ndx % 2 * 4;
        typedef unsigned char uchar;
        uchar* p = reinterpret_cast<uchar*>(data) + byte_ndx;
        *p = uchar((*p & ~(0x0F << bit_ndx)) | (int(value) & 0x0F) << bit_ndx);
    }
    else if (width == 8) {
        REALM_ASSERT_DEBUG(std::numeric_limits<int8_t>::min() <= value &&
                           value <= std::numeric_limits<int8_t>::max());
        *(reinterpret_cast<int8_t*>(data) + ndx) = int8_t(value);
    }
    else if (width == 16) {
        REALM_ASSERT_DEBUG(std::numeric_limits<int16_t>::min() <= value &&
                           value <= std::numeric_limits<int16_t>::max());
        *(reinterpret_cast<int16_t*>(data) + ndx) = int16_t(value);
    }
    else if (width == 32) {
        REALM_ASSERT_DEBUG(std::numeric_limits<int32_t>::min() <= value &&
                           value <= std::numeric_limits<int32_t>::max());
        *(reinterpret_cast<int32_t*>(data) + ndx) = int32_t(value);
    }
    else if (width == 64) {
        REALM_ASSERT_DEBUG(std::numeric_limits<int64_t>::min() <= value &&
                           value <= std::numeric_limits<int64_t>::max());
        *(reinterpret_cast<int64_t*>(data) + ndx) = int64_t(value);
    }
    else {
        REALM_ASSERT_DEBUG(false);
    }
}

inline void set_direct(char* data, size_t width, size_t ndx, int_fast64_t value) noexcept
{
    REALM_TEMPEX(set_direct, width, (data, ndx, value));
}

template <size_t width>
void fill_direct(char* data, size_t begin, size_t end, int_fast64_t value) noexcept
{
    for (size_t i = begin; i != end; ++i)
        set_direct<width>(data, i, value);
}

template <int w>
int64_t get_direct(const char* data, size_t ndx) noexcept
{
    if (w == 0) {
        return 0;
    }
    if (w == 1) {
        size_t offset = ndx >> 3;
        return (data[offset] >> (ndx & 7)) & 0x01;
    }
    if (w == 2) {
        size_t offset = ndx >> 2;
        return (data[offset] >> ((ndx & 3) << 1)) & 0x03;
    }
    if (w == 4) {
        size_t offset = ndx >> 1;
        return (data[offset] >> ((ndx & 1) << 2)) & 0x0F;
    }
    if (w == 8) {
        return *reinterpret_cast<const signed char*>(data + ndx);
    }
    if (w == 16) {
        size_t offset = ndx * 2;
        return *reinterpret_cast<const int16_t*>(data + offset);
    }
    if (w == 32) {
        size_t offset = ndx * 4;
        return *reinterpret_cast<const int32_t*>(data + offset);
    }
    if (w == 64) {
        size_t offset = ndx * 8;
        return *reinterpret_cast<const int64_t*>(data + offset);
    }
    REALM_ASSERT_DEBUG(false);
    return int64_t(-1);
}

inline int64_t get_direct(const char* data, size_t width, size_t ndx) noexcept
{
    REALM_TEMPEX(return get_direct, width, (data, ndx));
}


template <int width>
inline std::pair<int64_t, int64_t> get_two(const char* data, size_t ndx) noexcept
{
    return std::make_pair(to_size_t(get_direct<width>(data, ndx + 0)), to_size_t(get_direct<width>(data, ndx + 1)));
}

inline std::pair<int64_t, int64_t> get_two(const char* data, size_t width, size_t ndx) noexcept
{
    REALM_TEMPEX(return get_two, width, (data, ndx));
}


template <int width>
inline void get_three(const char* data, size_t ndx, ref_type& v0, ref_type& v1, ref_type& v2) noexcept
{
    v0 = to_ref(get_direct<width>(data, ndx + 0));
    v1 = to_ref(get_direct<width>(data, ndx + 1));
    v2 = to_ref(get_direct<width>(data, ndx + 2));
}

inline void get_three(const char* data, size_t width, size_t ndx, ref_type& v0, ref_type& v1, ref_type& v2) noexcept
{
    REALM_TEMPEX(get_three, width, (data, ndx, v0, v1, v2));
}


// Lower/upper bound in sorted sequence
// ------------------------------------
//
//   3 3 3 4 4 4 5 6 7 9 9 9
//   ^     ^     ^     ^     ^
//   |     |     |     |     |
//   |     |     |     |      -- Lower and upper bound of 15
//   |     |     |     |
//   |     |     |      -- Lower and upper bound of 8
//   |     |     |
//   |     |      -- Upper bound of 4
//   |     |
//   |      -- Lower bound of 4
//   |
//    -- Lower and upper bound of 1
//
// These functions are semantically identical to std::lower_bound() and
// std::upper_bound().
//
// We currently use binary search. See for example
// http://www.tbray.org/ongoing/When/200x/2003/03/22/Binary.
template <int width>
inline size_t lower_bound(const char* data, size_t size, int64_t value) noexcept
{
    // The binary search used here is carefully optimized. Key trick is to use a single
    // loop controlling variable (size) instead of high/low pair, and to keep updates
    // to size done inside the loop independent of comparisons. Further key to speed
    // is to avoid branching inside the loop, using conditional moves instead. This
    // provides robust performance for random searches, though predictable searches
    // might be slightly faster if we used branches instead. The loop unrolling yields
    // a final 5-20% speedup depending on circumstances.

    size_t low = 0;

    while (size >= 8) {
        // The following code (at X, Y and Z) is 3 times manually unrolled instances of (A) below.
        // These code blocks must be kept in sync. Meassurements indicate 3 times unrolling to give
        // the best performance. See (A) for comments on the loop body.
        // (X)
        size_t half = size / 2;
        size_t other_half = size - half;
        size_t probe = low + half;
        size_t other_low = low + other_half;
        int64_t v = get_direct<width>(data, probe);
        size = half;
        low = (v < value) ? other_low : low;

        // (Y)
        half = size / 2;
        other_half = size - half;
        probe = low + half;
        other_low = low + other_half;
        v = get_direct<width>(data, probe);
        size = half;
        low = (v < value) ? other_low : low;

        // (Z)
        half = size / 2;
        other_half = size - half;
        probe = low + half;
        other_low = low + other_half;
        v = get_direct<width>(data, probe);
        size = half;
        low = (v < value) ? other_low : low;
    }
    while (size > 0) {
        // (A)
        // To understand the idea in this code, please note that
        // for performance, computation of size for the next iteration
        // MUST be INDEPENDENT of the conditional. This allows the
        // processor to unroll the loop as fast as possible, and it
        // minimizes the length of dependence chains leading up to branches.
        // Making the unfolding of the loop independent of the data being
        // searched, also minimizes the delays incurred by branch
        // mispredictions, because they can be determined earlier
        // and the speculation corrected earlier.

        // Counterintuitive:
        // To make size independent of data, we cannot always split the
        // range at the theoretical optimal point. When we determine that
        // the key is larger than the probe at some index K, and prepare
        // to search the upper part of the range, you would normally start
        // the search at the next index, K+1, to get the shortest range.
        // We can only do this when splitting a range with odd number of entries.
        // If there is an even number of entries we search from K instead of K+1.
        // This potentially leads to redundant comparisons, but in practice we
        // gain more performance by making the changes to size predictable.

        // if size is even, half and other_half are the same.
        // if size is odd, half is one less than other_half.
        size_t half = size / 2;
        size_t other_half = size - half;
        size_t probe = low + half;
        size_t other_low = low + other_half;
        int64_t v = get_direct<width>(data, probe);
        size = half;
        // for max performance, the line below should compile into a conditional
        // move instruction. Not all compilers do this. To maximize chance
        // of succes, no computation should be done in the branches of the
        // conditional.
        low = (v < value) ? other_low : low;
    };

    return low;
}

// See lower_bound()
template <int width>
inline size_t upper_bound(const char* data, size_t size, int64_t value) noexcept
{
    size_t low = 0;
    while (size >= 8) {
        size_t half = size / 2;
        size_t other_half = size - half;
        size_t probe = low + half;
        size_t other_low = low + other_half;
        int64_t v = get_direct<width>(data, probe);
        size = half;
        low = (value >= v) ? other_low : low;

        half = size / 2;
        other_half = size - half;
        probe = low + half;
        other_low = low + other_half;
        v = get_direct<width>(data, probe);
        size = half;
        low = (value >= v) ? other_low : low;

        half = size / 2;
        other_half = size - half;
        probe = low + half;
        other_low = low + other_half;
        v = get_direct<width>(data, probe);
        size = half;
        low = (value >= v) ? other_low : low;
    }

    while (size > 0) {
        size_t half = size / 2;
        size_t other_half = size - half;
        size_t probe = low + half;
        size_t other_low = low + other_half;
        int64_t v = get_direct<width>(data, probe);
        size = half;
        low = (value >= v) ? other_low : low;
    };

    return low;
}
}

#endif /* ARRAY_TPL_HPP_ */
