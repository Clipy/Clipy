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

#include "sync/impl/apple/system_configuration.hpp"

#if NETWORK_REACHABILITY_AVAILABLE

#include <asl.h>
#include "dlfcn.h"

using namespace realm;
using namespace realm::_impl;

SystemConfiguration::SystemConfiguration()
{
    m_framework_handle = dlopen("/System/Library/Frameworks/SystemConfiguration.framework/SystemConfiguration", RTLD_LAZY);

    if (m_framework_handle) {
        m_create_with_name = (create_with_name_t)dlsym(m_framework_handle, "SCNetworkReachabilityCreateWithName");
        m_create_with_address = (create_with_address_t)dlsym(m_framework_handle, "SCNetworkReachabilityCreateWithAddress");
        m_set_dispatch_queue = (set_dispatch_queue_t)dlsym(m_framework_handle, "SCNetworkReachabilitySetDispatchQueue");
        m_set_callback = (set_callback_t)dlsym(m_framework_handle, "SCNetworkReachabilitySetCallback");
        m_get_flags = (get_flags_t)dlsym(m_framework_handle, "SCNetworkReachabilityGetFlags");
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        asl_log(nullptr, nullptr, ASL_LEVEL_WARNING, "network reachability is not available");
#pragma clang diagnostic pop
    }
}

SystemConfiguration& SystemConfiguration::shared()
{
    static SystemConfiguration system_configuration;

    return system_configuration;
}

SCNetworkReachabilityRef SystemConfiguration::network_reachability_create_with_name(CFAllocatorRef allocator,
                                                                                    const char *hostname)
{
    if (m_create_with_name)
        return m_create_with_name(allocator, hostname);

    return nullptr;
}

SCNetworkReachabilityRef SystemConfiguration::network_reachability_create_with_address(CFAllocatorRef allocator,
                                                                                       const sockaddr *address)
{
    if (m_create_with_address)
        return m_create_with_address(allocator, address);

    return nullptr;
}

bool SystemConfiguration::network_reachability_set_dispatch_queue(SCNetworkReachabilityRef target, dispatch_queue_t queue)
{
    if (m_set_dispatch_queue)
        return m_set_dispatch_queue(target, queue);

    return false;
}

bool SystemConfiguration::network_reachability_set_callback(SCNetworkReachabilityRef target,
                                                            SCNetworkReachabilityCallBack callback,
                                                            SCNetworkReachabilityContext *context)
{
    if (m_set_callback)
        return m_set_callback(target, callback, context);

    return false;
}

bool SystemConfiguration::network_reachability_get_flags(SCNetworkReachabilityRef target, SCNetworkReachabilityFlags *flags)
{
    if (m_get_flags)
        return m_get_flags(target, flags);

    return false;
}

#endif // NETWORK_REACHABILITY_AVAILABLE
