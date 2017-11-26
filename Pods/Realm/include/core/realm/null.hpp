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

#ifndef REALM_NULL_HPP
#define REALM_NULL_HPP

#include <cmath>
#include <cstring>

#include <realm/util/features.h>
#include <realm/util/optional.hpp>
#include <realm/utilities.hpp>
#include <realm/exceptions.hpp>

namespace realm {

/*
Represents null in Query, find(), get(), set(), etc.

Float/Double: Realm can both store user-given NaNs and null. Any user-given signaling NaN is converted to
0x7fa00000 (if float) or 0x7ff4000000000000 (if double). Any user-given quiet NaN is converted to
0x7fc00000 (if float) or 0x7ff8000000000000 (if double). So Realm does not preserve the optional bits in
user-given NaNs.

However, since both clang and gcc on x64 and ARM, and also Java on x64, return these bit patterns when
requesting NaNs, these will actually seem to roundtrip bit-exact for the end-user in most cases.

If set_null() is called, a null is stored in form of the bit pattern 0xffffffff (if float) or
0xffffffffffffffff (if double). These are quiet NaNs.

Executing a query that involves a float/double column that contains NaNs gives an undefined result. If
it contains signaling NaNs, it may throw an exception.

Notes on IEEE:

A NaN float is any bit pattern `s 11111111 S xxxxxxxxxxxxxxxxxxxxxx` where `s` and `x` are arbitrary, but at
least 1 `x` must be 1. If `S` is 1, it's a quiet NaN, else it's a signaling NaN.

A NaN doubule is the same as above, but for `s eeeeeeeeeee S xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx`

The `S` bit is at position 22 (float) or 51 (double).
*/

struct null {
    null()
    {
    }
    operator int64_t()
    {
        throw(LogicError::type_mismatch);
    }
    template <class T>
    operator util::Optional<T>()
    {
        return util::none;
    }

    template <class T>
    bool operator==(const T&) const
    {
        REALM_ASSERT(false);
        return false;
    }
    template <class T>
    bool operator!=(const T&) const
    {
        REALM_ASSERT(false);
        return false;
    }
    template <class T>
    bool operator>(const T&) const
    {
        REALM_ASSERT(false);
        return false;
    }
    template <class T>
    bool operator>=(const T&) const
    {
        REALM_ASSERT(false);
        return false;
    }
    template <class T>
    bool operator<=(const T&) const
    {
        REALM_ASSERT(false);
        return false;
    }
    template <class T>
    bool operator<(const T&) const
    {
        REALM_ASSERT(false);
        return false;
    }

    /// Returns whether `v` bitwise equals the null bit-pattern
    template <class T>
    static bool is_null_float(T v)
    {
        T i = null::get_null_float<T>();
        return std::memcmp(&i, &v, sizeof(T)) == 0;
    }

    /// Returns the quiet NaNs that represent null for floats/doubles in Realm in stored payload.
    template <class T>
    static T get_null_float()
    {
        typename std::conditional<std::is_same<T, float>::value, uint32_t, uint64_t>::type i;
        int64_t double_nan = 0x7ff80000000000aa;
        i = std::is_same<T, float>::value ? 0x7fc000aa : static_cast<decltype(i)>(double_nan);
        T d = type_punning<T, decltype(i)>(i);
        REALM_ASSERT_DEBUG(std::isnan(d));
        REALM_ASSERT_DEBUG(!is_signaling(d));
        return d;
    }

    /// Takes a NaN as argument and returns whether or not it's signaling
    template <class T>
    static bool is_signaling(T v)
    {
        REALM_ASSERT(std::isnan(static_cast<double>(v)));
        typename std::conditional<std::is_same<T, float>::value, uint32_t, uint64_t>::type i;
        size_t signal_bit = std::is_same<T, float>::value ? 22 : 51; // If this bit is set, it's quiet
        i = type_punning<decltype(i), T>(v);
        return !(i & (1ull << signal_bit));
    }

    /// Converts any signaling or quiet NaN to their their respective bit patterns that are used on x64 gcc+clang,
    /// ARM clang and x64 Java.
    template <class T>
    static T to_realm(T v)
    {
        if (std::isnan(static_cast<double>(v))) {
            typename std::conditional<std::is_same<T, float>::value, uint32_t, uint64_t>::type i;
            if (std::is_same<T, float>::value) {
                i = is_signaling(v) ? 0x7fa00000 : 0x7fc00000;
            }
            else {
                i = static_cast<decltype(i)>(is_signaling(v) ? 0x7ff4000000000000 : 0x7ff8000000000000);
            }
            return type_punning<T, decltype(i)>(i);
        }
        else {
            return v;
        }
    }
};

template <class OS>
OS& operator<<(OS& os, const null&)
{
    os << "(null)";
    return os;
}

} // namespace realm

#endif // REALM_NULL_HPP
