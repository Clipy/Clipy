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
#ifndef REALM_DISABLE_SYNC_TO_DISK_HPP
#define REALM_DISABLE_SYNC_TO_DISK_HPP

#include <realm/util/features.h>

namespace realm {

/// Completely disable synchronization with storage device to speed up unit
/// testing. This is an unsafe mode of operation, and should never be used in
/// production. This function is thread safe.
void disable_sync_to_disk();

/// Returns true after disable_sync_to_disk() has been called. This function is
/// thread safe.
bool get_disable_sync_to_disk() noexcept;

} // namespace realm

#endif // REALM_DISABLE_SYNC_TO_DISK_HPP
