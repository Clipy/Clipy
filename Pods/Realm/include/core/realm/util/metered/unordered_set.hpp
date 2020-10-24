/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2018] Realm Inc
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
#ifndef REALM_UTIL_METERED_UNORDERED_SET_HPP
#define REALM_UTIL_METERED_UNORDERED_SET_HPP

#include <unordered_set>
#include <realm/util/allocation_metrics.hpp>

namespace realm {
namespace util {
namespace metered {
/// Unordered set with metered allocation
template <class T,
          class Hash = std::hash<T>,
          class KeyEqual = std::equal_to<T>,
          class Alloc = MeteredSTLAllocator<T>>
using unordered_set = std::unordered_set<T, Hash, KeyEqual, Alloc>;
} // namespace metered
} // namespace util
} // namespace realm

#endif // REALM_UTIL_METERED_UNORDERED_SET_HPP

