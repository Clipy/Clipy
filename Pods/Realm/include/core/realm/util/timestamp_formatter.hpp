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

#ifndef REALM_UTIL_TIMESTAMP_FORMATTER_HPP
#define REALM_UTIL_TIMESTAMP_FORMATTER_HPP

#include <ctime>
#include <chrono>
#include <utility>
#include <string>

#include <realm/util/features.h>
#include <realm/util/assert.hpp>
#include <realm/util/string_view.hpp>
#include <realm/util/memory_stream.hpp>


namespace realm {
namespace util {

class TimestampFormatter {
public:
    using char_type = char;
    using string_view_type = util::BasicStringView<char_type>;

    enum class Precision { seconds, milliseconds, microseconds, nanoseconds };

    /// Default configuration for corresponds to local time in ISO 8601 date and
    /// time format.
    struct Config {
        Config() {}

        bool utc_time = false;

        Precision precision = Precision::seconds;

        /// The format of the timestamp as understood by std::put_time(), except
        /// that the first occurrence of `%S` (also taking into account the `%S`
        /// that is an implicit part of `%T`) is expanded to `SS.fff` if \ref
        /// precision is Precision::milliseconds, or to `SS.ffffff` if \ref
        /// precision is Precision::microseconds, or to `SS.fffffffff` if \ref
        /// precision is Precision::nanoseconds, where `SS` is what `%S` expands
        /// to conventionally.
        const char* format = "%FT%T%z";
    };

    TimestampFormatter(Config = {});

    // FIXME: Use std::timespec in C++17.
    string_view_type format(std::time_t time, long nanoseconds);

    template<class B> string_view_type format(std::chrono::time_point<B>);

private:
    using memory_output_stream_type = util::MemoryOutputStream;
    using format_segments_type = std::pair<std::string, const char*>;

    const bool m_utc_time;
    const Precision m_precision;
    const format_segments_type m_format_segments;
    char_type m_buffer[64];
    memory_output_stream_type m_out;

    static format_segments_type make_format_segments(const Config&);
};





// Implementation

template<class B>
inline auto TimestampFormatter::format(std::chrono::time_point<B> time) -> string_view_type
{
    using clock_type = B;
    using time_point_type = std::chrono::time_point<B>;
    std::time_t time_2 = clock_type::to_time_t(time);
    time_point_type time_3 = clock_type::from_time_t(time_2);
    if (REALM_UNLIKELY(time_3 > time)) {
        --time_2;
        time_3 = clock_type::from_time_t(time_2);
    }
    long nanoseconds =
        int(std::chrono::duration_cast<std::chrono::nanoseconds>(time - time_3).count());
    REALM_ASSERT(nanoseconds >= 0 && nanoseconds < 1000000000);
    return format(time_2, nanoseconds); // Throws
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_TIMESTAMP_FORMATTER_HPP
