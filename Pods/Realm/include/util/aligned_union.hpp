////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or utilied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#ifndef REALM_OS_ALIGNED_UNION_HPP
#define REALM_OS_ALIGNED_UNION_HPP

#include <cstddef>
#include <initializer_list>
#include <type_traits>

namespace realm {

// Provide our own implementation of max as GCC 4.9's is not marked as constexpr.
namespace _impl {

template <typename T>
static constexpr const T& constexpr_max(const T& a, const T& b)
{
    return a > b ? a : b;
}

template <typename T>
static constexpr const T& constexpr_max(const T* begin, const T *end)
{
    return begin + 1 == end ? *begin : constexpr_max(*begin, constexpr_max(begin + 1, end));
}

template <typename T>
static constexpr const T& constexpr_max(std::initializer_list<T> list)
{
    return constexpr_max(list.begin(), list.end());
}

} // namespace _impl

namespace util {

// Provide our own implementation of `std::aligned_union` as it is missing from GCC 4.9.
template <size_t Len, typename... Types>
struct AlignedUnion
{
    static constexpr size_t alignment_value = _impl::constexpr_max({alignof(Types)...});
    static constexpr size_t storage_size = _impl::constexpr_max({Len, sizeof(Types)...});
    using type = typename std::aligned_storage<storage_size, alignment_value>::type;
};

} // namespace util
} // namespace realm

#endif // REALM_OS_ALIGNED_UNION_HPP
