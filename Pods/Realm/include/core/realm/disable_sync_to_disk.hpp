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
