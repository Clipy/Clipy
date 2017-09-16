////////////////////////////////////////////////////////////////////////////
//
// Copyright 2017 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#ifndef REALM_OS_UTIL_TAGGED_BOOL_HPP
#define REALM_OS_UTIL_TAGGED_BOOL_HPP

#include <type_traits>

namespace realm {
namespace util {
// A type factory which defines a type which is implicitly convertable to and
// from `bool`, but not to other TaggedBool types
//
// Usage:
// using IsIndexed = util::TaggedBool<class IsIndexedTag>;
// using IsPrimary = util::TaggedBool<class IsPrimaryTag>;
// void foo(IsIndexed is_indexed, IsPrimary is_primary);
//
// foo(IsIndexed{true}, IsPrimary{false}); // compiles
// foo(IsPrimary{true}, IsIndexed{false}); // doesn't compile
template <typename Tag>
struct TaggedBool {
    // Allow explicit construction from anything convertible to bool
    constexpr explicit TaggedBool(bool v) : m_value(v) { }

    // Allow implicit construction from *just* bool and not things convertible
    // to bool (such as other types of tagged bools)
    template <typename Bool, typename = typename std::enable_if<std::is_same<Bool, bool>::value>::type>
    constexpr TaggedBool(Bool v) : m_value(v) {}

    constexpr TaggedBool(TaggedBool const& v) : m_value(v.m_value) {}

    constexpr operator bool() const { return m_value; }
    constexpr TaggedBool operator!() const { return TaggedBool{!m_value}; }

    friend constexpr bool operator==(TaggedBool l, TaggedBool r) { return l.m_value == r.m_value; }
    friend constexpr bool operator!=(TaggedBool l, TaggedBool r) { return l.m_value != r.m_value; }

private:
    bool m_value;
};

}
}
#endif // REALM_OS_UTIL_TAGGED_BOOL_HPP
