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

#ifndef REALM_TIMESTAMP_HPP
#define REALM_TIMESTAMP_HPP

#include <cstdint>
#include <ostream>
#include <chrono>
#include <ctime>
#include <realm/util/assert.hpp>
#include <realm/null.hpp>

namespace realm {

class Timestamp {
public:
    // Construct from the number of seconds and nanoseconds since the UNIX epoch: 00:00:00 UTC on 1 January 1970
    //
    // To split a native nanosecond representation, only division and modulo are necessary:
    //
    //     s = native_nano / nanoseconds_per_second
    //     n = native_nano % nanoseconds_per_second
    //     Timestamp ts(s, n);
    //
    // To convert back into native nanosecond representation, simple multiply and add:
    //
    //     native_nano = ts.s * nanoseconds_per_second + ts.n
    //
    // Specifically this allows the nanosecond part to become negative (only) for Timestamps before the UNIX epoch.
    // Usually this will not need special attention, but for reference, valid Timestamps will have one of the
    // following sign combinations:
    //
    //     s | n
    //     -----
    //     + | +
    //     + | 0
    //     0 | +
    //     0 | 0
    //     0 | -
    //     - | 0
    //     - | -
    //
    // Examples:
    //     The UNIX epoch is constructed by Timestamp(0, 0)
    //     Relative times are constructed as follows:
    //       +1 second is constructed by Timestamp(1, 0)
    //       +1 nanosecond is constructed by Timestamp(0, 1)
    //       +1.1 seconds (1100 milliseconds after the epoch) is constructed by Timestamp(1, 100000000)
    //       -1.1 seconds (1100 milliseconds before the epoch) is constructed by Timestamp(-1, -100000000)
    //
    Timestamp(int64_t seconds, int32_t nanoseconds)
        : m_seconds(seconds)
        , m_nanoseconds(nanoseconds)
        , m_is_null(false)
    {
        REALM_ASSERT_EX(-nanoseconds_per_second < nanoseconds && nanoseconds < nanoseconds_per_second, nanoseconds);
        const bool both_non_negative = seconds >= 0 && nanoseconds >= 0;
        const bool both_non_positive = seconds <= 0 && nanoseconds <= 0;
        REALM_ASSERT_EX(both_non_negative || both_non_positive, both_non_negative, both_non_positive);
    }
    Timestamp(realm::null)
        : m_is_null(true)
    {
    }
    template <typename C = std::chrono::system_clock, typename D = typename C::duration>
    Timestamp(std::chrono::time_point<C, D> tp)
        : m_is_null(false)
    {
        int64_t native_nano = std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count();
        m_seconds = native_nano / nanoseconds_per_second;
        m_nanoseconds = static_cast<int32_t>(native_nano % nanoseconds_per_second);
    }
    Timestamp()
        : Timestamp(null{})
    {
    }

    bool is_null() const
    {
        return m_is_null;
    }

    int64_t get_seconds() const noexcept
    {
        REALM_ASSERT(!m_is_null);
        return m_seconds;
    }

    int32_t get_nanoseconds() const noexcept
    {
        REALM_ASSERT(!m_is_null);
        return m_nanoseconds;
    }

    template <typename C = std::chrono::system_clock, typename D = typename C::duration>
    std::chrono::time_point<C, D> get_time_point()
    {
        REALM_ASSERT(!m_is_null);

        int64_t native_nano = m_seconds * nanoseconds_per_second + m_nanoseconds;
        auto duration = std::chrono::duration_cast<D>(std::chrono::duration<int64_t, std::nano>{native_nano});

        return std::chrono::time_point<C, D>(duration);
    }

    bool operator==(const Timestamp& rhs) const
    {
        if (is_null() && rhs.is_null())
            return true;

        if (is_null() != rhs.is_null())
            return false;

        return m_seconds == rhs.m_seconds && m_nanoseconds == rhs.m_nanoseconds;
    }
    bool operator!=(const Timestamp& rhs) const
    {
        return !(*this == rhs);
    }
    bool operator>(const Timestamp& rhs) const
    {
        if (is_null()) {
            return false;
        }
        if (rhs.is_null()) {
            return true;
        }
        return (m_seconds > rhs.m_seconds) || (m_seconds == rhs.m_seconds && m_nanoseconds > rhs.m_nanoseconds);
    }
    bool operator<(const Timestamp& rhs) const
    {
        if (rhs.is_null()) {
            return false;
        }
        if (is_null()) {
            return true;
        }
        return (m_seconds < rhs.m_seconds) || (m_seconds == rhs.m_seconds && m_nanoseconds < rhs.m_nanoseconds);
    }
    bool operator<=(const Timestamp& rhs) const
    {
        if (is_null()) {
            return true;
        }
        if (rhs.is_null()) {
            return false;
        }
        return *this < rhs || *this == rhs;
    }
    bool operator>=(const Timestamp& rhs) const
    {
        if (rhs.is_null()) {
            return true;
        }
        if (is_null()) {
            return false;
        }
        return *this > rhs || *this == rhs;
    }
    Timestamp& operator=(const Timestamp& rhs) = default;

    template <class Ch, class Tr>
    friend std::basic_ostream<Ch, Tr>& operator<<(std::basic_ostream<Ch, Tr>& out, const Timestamp&);
    static constexpr int32_t nanoseconds_per_second = 1000000000;

private:
    int64_t m_seconds;
    int32_t m_nanoseconds;
    bool m_is_null;
};

// LCOV_EXCL_START
template <class C, class T>
inline std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>& out, const Timestamp& d)
{
    auto seconds = time_t(d.get_seconds());
    struct tm buf;
#ifdef _MSC_VER
    bool success = gmtime_s(&buf, &seconds) == 0;
#else
    bool success = gmtime_r(&seconds, &buf) != nullptr;
#endif
    if (success) {
        // We need a buffer for formatting dates.
        // Max size is 20 bytes (incl terminating zero) "YYYY-MM-DD HH:MM:SS"\0
        char buffer[30];
        if (strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &buf)) {
            out << buffer;
        }
    }

    return out;
}
// LCOV_EXCL_STOP

} // namespace realm

namespace std {
template <>
struct numeric_limits<realm::Timestamp> {
    static constexpr bool is_integer = false;
    static realm::Timestamp min()
    {
        return realm::Timestamp(numeric_limits<int64_t>::min(), 0);
    }
    static realm::Timestamp lowest()
    {
        return realm::Timestamp(numeric_limits<int64_t>::lowest(), 0);
    }
    static realm::Timestamp max()
    {
        return realm::Timestamp(numeric_limits<int64_t>::max(), 0);
    }
};
}

#endif // REALM_TIMESTAMP_HPP
