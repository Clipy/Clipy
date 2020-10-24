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
#ifndef REALM_UTIL_COPY_DIR_RECURSIVE_HPP
#define REALM_UTIL_COPY_DIR_RECURSIVE_HPP

#include <string>

namespace realm {
namespace util {

/// Recursively copy the specified directory.
///
/// It is not an error if the target directory already exists. It is also not an
/// error if the target directory is not empty. If the origin and target
/// directories contain a file of the same name, the one in the target directory
/// will be overwitten. Other files that already exist in the target directory
/// will be left alone.
///
/// \param skip_special_files If true, entries in the origin directory that are
/// neither regular files nor subdirectories will be skipped (not copied). If
/// false (the default), this function will fail if such an entry is encoutered.
///
/// FIXME: This function ought to be moved to <realm/util/file.hpp> in the
/// realm-core repository.
void copy_dir_recursive(const std::string& origin_path, const std::string& target_path,
                        bool skip_special_files = false);

} // namespace util
} // namespace realm

#endif // REALM_UTIL_COPY_DIR_RECURSIVE_HPP
