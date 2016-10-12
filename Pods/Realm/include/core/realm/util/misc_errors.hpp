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
#ifndef REALM_UTIL_MISC_ERRORS_HPP
#define REALM_UTIL_MISC_ERRORS_HPP

#include <system_error>


namespace realm {
namespace util {
namespace error {

enum misc_errors {
    unknown = 1
};

std::error_code make_error_code(misc_errors);

} // namespace error
} // namespace util
} // namespace realm

namespace std {

template<>
class is_error_code_enum<realm::util::error::misc_errors>
{
public:
    static const bool value = true;
};

} // namespace std

#endif // REALM_UTIL_MISC_ERRORS_HPP
