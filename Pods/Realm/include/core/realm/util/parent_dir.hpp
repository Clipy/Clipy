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
#ifndef REALM_UTIL_PARENT_DIR_HPP
#define REALM_UTIL_PARENT_DIR_HPP

#include <string>

namespace realm {
namespace util {

/// Same effect as std::filesystem::path::parent_path().
///
/// FIXME: This function ought to be moved to <realm/util/file.hpp> in the
/// realm-core repository.
std::string parent_dir(const std::string& path);

} // namespace util
} // namespace realm

#endif // REALM_UTIL_PARENT_DIR_HPP
