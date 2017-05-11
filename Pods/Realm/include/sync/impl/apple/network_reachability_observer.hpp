////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
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

#ifndef REALM_OS_NETWORK_REACHABILITY_OBSERVER_HPP
#define REALM_OS_NETWORK_REACHABILITY_OBSERVER_HPP

#include <functional>
#include <string>

#include <realm/util/cf_ptr.hpp>
#include <realm/util/optional.hpp>

#include "sync/impl/network_reachability.hpp"

#if NETWORK_REACHABILITY_AVAILABLE

#include "sync/impl/apple/system_configuration.hpp"

namespace realm {
namespace _impl {

enum NetworkReachabilityStatus {
    NotReachable,
    ReachableViaWiFi,
    ReachableViaWWAN
};

class NetworkReachabilityObserver {
public:
    NetworkReachabilityObserver(util::Optional<std::string> hostname,
                                std::function<void (const NetworkReachabilityStatus)> handler);

    ~NetworkReachabilityObserver();

    NetworkReachabilityStatus reachability_status() const;

    bool start_observing();
    void stop_observing();

private:
    void reachability_changed();

    util::CFPtr<SCNetworkReachabilityRef> m_reachability_ref;
    NetworkReachabilityStatus m_previous_status;
    dispatch_queue_t m_callback_queue;
    std::function<void (const NetworkReachabilityStatus)> m_change_handler;
};

} // namespace _impl
} // namespace realm

#endif // NETWORK_REACHABILITY_AVAILABLE

#endif // REALM_OS_NETWORK_REACHABILITY_OBSERVER_HPP
