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

#ifndef REALM_UTIL_SCOPE_EXIT_HPP
#define REALM_UTIL_SCOPE_EXIT_HPP

#include <type_traits>
#include <utility>

#include <realm/util/optional.hpp>

namespace realm {
namespace util {

template <class H>
class ScopeExit {
public:
    explicit ScopeExit(const H& handler) noexcept(std::is_nothrow_copy_constructible<H>::value)
        : m_handler(handler)
    {
    }

    explicit ScopeExit(H&& handler) noexcept(std::is_nothrow_move_constructible<H>::value)
        : m_handler(std::move(handler))
    {
    }

    ScopeExit(ScopeExit&& se) noexcept(std::is_nothrow_move_constructible<H>::value)
        : m_handler(std::move(se.m_handler))
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

template <class H>
ScopeExit<typename std::remove_reference<H>::type> make_scope_exit(H&& handler) noexcept(
    noexcept(ScopeExit<typename std::remove_reference<H>::type>(std::forward<H>(handler))))
{
    return ScopeExit<typename std::remove_reference<H>::type>(std::forward<H>(handler));
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_SCOPE_EXIT_HPP
