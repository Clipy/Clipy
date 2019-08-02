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
#ifndef REALM_UTIL_METERED_SET_HPP
#define REALM_UTIL_METERED_SET_HPP

#include <set>
#include <realm/util/allocation_metrics.hpp>

namespace realm {
namespace util {
namespace metered {
/// Set with metered allocation. Additionally, the default Compare is changed to
/// `std::less<>` instead of `std::less<K>`, which allows heterogenous lookup.
template <class T, class Compare = std::less<>, class Alloc = MeteredSTLAllocator<T>>
using set = std::set<T, Compare, Alloc>;
} // namespace metered
} // namespace util
} // namespace realm

#endif // REALM_UTIL_METERED_SET_HPP

