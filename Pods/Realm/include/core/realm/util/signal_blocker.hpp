/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2016] Realm Inc
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

#ifndef REALM_UTIL_SIGNAL_BLOCKER_HPP
#define REALM_UTIL_SIGNAL_BLOCKER_HPP

#include <csignal>

#include <realm/util/config.h>


namespace realm {
namespace util {

/// \brief Block all signals from being delivered to the instantiating thread.
///
/// On platforms that support POSIX signals, the constructor will set the signal
/// mask such that all signals are blocked from being delivered to the calling
/// thread, and the destructor will restore the signal mask to its original
/// value.
///
/// This scheme assumes that it is always the same thread that constructs and
/// destroys a particular instance of SignalBlocker, and that, for a particular
/// thread, two SignalBlocker objects never overlap in time, and the signal mask
/// is never modified by other means while a SignalBlocker object exists.
class SignalBlocker {
public:
    SignalBlocker() noexcept;
    ~SignalBlocker() noexcept;

private:
#ifndef _WIN32
    ::sigset_t m_orig_mask;
#endif
};



// Implementation

inline SignalBlocker::SignalBlocker() noexcept
{
#ifndef _WIN32
    ::sigset_t mask;
    sigfillset(&mask);
    int ret = ::pthread_sigmask(SIG_BLOCK, &mask, &m_orig_mask);
    REALM_ASSERT(ret == 0);
#endif
}

inline SignalBlocker::~SignalBlocker() noexcept
{
#ifndef _WIN32
    int ret = ::pthread_sigmask(SIG_SETMASK, &m_orig_mask, nullptr);
    REALM_ASSERT(ret == 0);
#endif
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_SIGNAL_BLOCKER_HPP
