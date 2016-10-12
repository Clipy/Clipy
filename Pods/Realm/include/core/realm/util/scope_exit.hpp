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
#ifndef REALM_UTIL_SCOPE_EXIT_HPP
#define REALM_UTIL_SCOPE_EXIT_HPP

#include <type_traits>
#include <utility>

#include <realm/util/optional.hpp>

namespace realm {
namespace util {

template<class H>
class ScopeExit {
public:
    explicit ScopeExit(const H& handler) noexcept(std::is_nothrow_copy_constructible<H>::value):
        m_handler(handler)
    {
    }

    explicit ScopeExit(H&& handler) noexcept(std::is_nothrow_move_constructible<H>::value):
        m_handler(std::move(handler))
    {
    }

    ScopeExit(ScopeExit&& se) noexcept(std::is_nothrow_move_constructible<H>::value):
        m_handler(std::move(se.m_handler))
    {
        se.m_handler = none;
    }

    ~ScopeExit() noexcept
    {
        if (m_handler)
            (*m_handler)();
    }

    static_assert(noexcept(std::declval<H>()()), "Handler must be nothrow executable");
    static_assert(std::is_nothrow_destructible<H>::value, "Handler must be nothrow destructible");

private:
    util::Optional<H> m_handler;
};

template<class H>
ScopeExit<typename std::remove_reference<H>::type> make_scope_exit(H&& handler)
    noexcept(noexcept(ScopeExit<typename std::remove_reference<H>::type>(std::forward<H>(handler))))
{
    return ScopeExit<typename std::remove_reference<H>::type>(std::forward<H>(handler));
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_SCOPE_EXIT_HPP
