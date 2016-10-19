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

#ifndef REALM_UTIL_INSPECT_HPP
#define REALM_UTIL_INSPECT_HPP

#include <string>

namespace realm {
namespace util {

// LCOV_EXCL_START
//
// Because these are templated functions, every combination of output stream
// type and value(s) type(s) generates a new function.  This makes LCOV/GCOVR
// report over 70 functions in this file, with only 6.6% function coverage,
// even though line coverage is at 100%.

template <class OS, class T>
void inspect_value(OS& os, const T& value)
{
    os << value;
}

template <class OS>
void inspect_value(OS& os, const std::string& value)
{
    // FIXME: Escape the string.
    os << "\"" << value << "\"";
}

template <class OS>
void inspect_value(OS& os, const char* value)
{
    // FIXME: Escape the string.
    os << "\"" << value << "\"";
}

template <class OS>
void inspect_all(OS&)
{
    // No-op
}

/// Convert all arguments to strings, and quote string arguments.
template <class OS, class First, class... Args>
void inspect_all(OS& os, First&& first, Args&&... args)
{
    inspect_value(os, std::forward<First>(first));
    if (sizeof...(Args) != 0) {
        os << ", ";
    }
    inspect_all(os, std::forward<Args>(args)...);
}

// LCOV_EXCL_STOP

} // namespace util
} // namespace realm

#endif // REALM_UTIL_INSPECT_HPP
