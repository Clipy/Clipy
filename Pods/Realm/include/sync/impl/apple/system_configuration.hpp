////////////////////////////////////////////////////////////////////////////
//
// Copyright 2017 Realm Inc.
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

#ifndef REALM_OS_SYSTEM_CONFIGURATION_HPP
#define REALM_OS_SYSTEM_CONFIGURATION_HPP

#include "sync/impl/network_reachability.hpp"

#if NETWORK_REACHABILITY_AVAILABLE

#include <SystemConfiguration/SystemConfiguration.h>

namespace realm {
namespace _impl {

class SystemConfiguration {
public:
    static SystemConfiguration& shared();

    SCNetworkReachabilityRef network_reachability_create_with_name(CFAllocatorRef allocator,
                                                                   const char *hostname);
    SCNetworkReachabilityRef network_reachability_create_with_address(CFAllocatorRef allocator,
                                                                      const sockaddr *address);
    bool network_reachability_set_dispatch_queue(SCNetworkReachabilityRef target,
                                                 dispatch_queue_t queue);
    bool network_reachability_set_callback(SCNetworkReachabilityRef target,
                                           SCNetworkReachabilityCallBack callback,
                                           SCNetworkReachabilityContext *context);
    bool network_reachability_get_flags(SCNetworkReachabilityRef target,
                                        SCNetworkReachabilityFlags *flags);

private:
    using create_with_name_t = SCNetworkReachabilityRef(*)(CFAllocatorRef, const char*);
    using create_with_address_t = SCNetworkReachabilityRef(*)(CFAllocatorRef, const sockaddr*);
    using set_dispatch_queue_t = bool(*)(SCNetworkReachabilityRef, dispatch_queue_t);
    using set_callback_t = bool(*)(SCNetworkReachabilityRef,
                                   SCNetworkReachabilityCallBack,
                                   SCNetworkReachabilityContext*);
    using get_flags_t = bool(*)(SCNetworkReachabilityRef, SCNetworkReachabilityFlags*);

    SystemConfiguration();

    void* m_framework_handle;

    create_with_name_t m_create_with_name = nullptr;
    create_with_address_t m_create_with_address = nullptr;
    set_dispatch_queue_t m_set_dispatch_queue = nullptr;
    set_callback_t m_set_callback = nullptr;
    get_flags_t m_get_flags = nullptr;
};

} // namespace _impl
} // namespace realm

#endif // NETWORK_REACHABILITY_AVAILABLE

#endif // REALM_OS_SYSTEM_CONFIGURATION_HPP
