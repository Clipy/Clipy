////////////////////////////////////////////////////////////////////////////
//
// Copyright 2018 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#ifndef REALM_OS_SUBSCRIPTION_STATE_HPP
#define REALM_OS_SUBSCRIPTION_STATE_HPP

#include <cstdint>

namespace realm {
namespace partial_sync {

// Enum describing the various states a partial sync subscription can have.
enum class SubscriptionState : int8_t {
    Error = -1,      // An error occurred while creating or processing the partial sync subscription.
    Creating = 2,    // The subscription is being created.
    Pending = 0,     // The subscription was created, but has not yet been processed by the sync server.
    Complete = 1,    // The subscription has been processed by the sync server and data is being synced to the device.
    Invalidated = 3, // The subscription has been removed.
};

}
}

#endif // REALM_OS_SUBSCRIPTION_STATE_HPP
