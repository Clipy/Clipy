/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2016] Realm Inc
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
#ifndef REALM_UTIL_TO_STRING_HPP
#define REALM_UTIL_TO_STRING_HPP

#include <locale>
#include <string>
#include <sstream>

namespace realm {
namespace util {

template<class T>
std::string to_string(const T& v)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out.exceptions(std::ios_base::failbit | std::ios_base::badbit);
    out << v; // Throws
    return out.str();
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_TO_STRING_HPP
