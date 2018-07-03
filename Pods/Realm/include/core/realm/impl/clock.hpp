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

#ifndef REALM_IMPL_CLOCK_HPP
#define REALM_IMPL_CLOCK_HPP

#include <cstdint>
#include <chrono>

#include <realm/sync/protocol.hpp>

namespace realm {
namespace _impl {

inline sync::milliseconds_type realtime_clock_now() noexcept
{
    using clock = std::chrono::system_clock;
    auto time_since_epoch = clock::now().time_since_epoch();
    auto millis_since_epoch =
        std::chrono::duration_cast<std::chrono::milliseconds>(time_since_epoch).count();
    return sync::milliseconds_type(millis_since_epoch);
}


inline sync::milliseconds_type monotonic_clock_now() noexcept
{
    using clock = std::chrono::steady_clock;
    auto time_since_epoch = clock::now().time_since_epoch();
    auto millis_since_epoch =
        std::chrono::duration_cast<std::chrono::milliseconds>(time_since_epoch).count();
    return sync::milliseconds_type(millis_since_epoch);
}

} // namespace _impl
} // namespace realm

#endif // REALM_IMPL_CLOCK_HPP
