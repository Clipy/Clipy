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
#ifndef REALM_UTIL_METERED_UNORDERED_MAP_HPP
#define REALM_UTIL_METERED_UNORDERED_MAP_HPP

#include <unordered_map>
#include <realm/util/allocation_metrics.hpp>

namespace realm {
namespace util {
namespace metered {
/// Unordered map with metered allocation
template <class K,
          class V,
          class Hash = std::hash<K>,
          class KeyEqual = std::equal_to<K>,
          class Alloc = MeteredSTLAllocator<std::pair<const K, V>>>
using unordered_map = std::unordered_map<K, V, Hash, KeyEqual, Alloc>;
} // namespace metered
} // namespace util
} // namespace realm

#endif // REALM_UTIL_METERED_UNORDERED_MAP_HPP

