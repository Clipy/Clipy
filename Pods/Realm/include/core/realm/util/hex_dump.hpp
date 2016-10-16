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

#ifndef REALM_UTIL_HEX_DUMP_HPP
#define REALM_UTIL_HEX_DUMP_HPP

#include <cstddef>
#include <type_traits>
#include <limits>
#include <string>
#include <sstream>
#include <iomanip>

#include <realm/util/safe_int_ops.hpp>

namespace realm {
namespace util {

template <class T>
std::string hex_dump(const T* data, size_t size, const char* separator = " ", int min_digits = -1)
{
    using U = typename std::make_unsigned<T>::type;

    if (min_digits < 0)
        min_digits = (std::numeric_limits<U>::digits + 3) / 4;

    std::ostringstream out;
    for (const T* i = data; i != data + size; ++i) {
        if (i != data)
            out << separator;
        out << std::setw(min_digits) << std::setfill('0') << std::hex << std::uppercase << util::promote(U(*i));
    }
    return out.str();
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_HEX_DUMP_HPP
