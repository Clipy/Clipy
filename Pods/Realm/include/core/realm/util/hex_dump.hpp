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
#ifndef REALM_UTIL_HEX_DUMP_HPP
#define REALM_UTIL_HEX_DUMP_HPP

#include <stddef.h>
#include <type_traits>
#include <limits>
#include <string>
#include <sstream>
#include <iomanip>

#include <realm/util/safe_int_ops.hpp>

namespace realm {
namespace util {

template<class T>
std::string hex_dump(const T* data, size_t size, const char* separator = " ", int min_digits = -1)
{
    using U = typename std::make_unsigned<T>::type;

    if (min_digits < 0)
        min_digits = (std::numeric_limits<U>::digits+3) / 4;

    std::ostringstream out;
    for (const T* i = data; i != data+size; ++i) {
        if (i != data)
            out << separator;
        out << std::setw(min_digits)<<std::setfill('0')<<std::hex<<std::uppercase <<
            util::promote(U(*i));
    }
    return out.str();
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_HEX_DUMP_HPP
