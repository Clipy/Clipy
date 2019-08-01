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

#ifndef REALM_UTIL_TIME_HPP
#define REALM_UTIL_TIME_HPP

#include <cstring>
#include <ctime>
#include <iterator>
#include <locale>
#include <ostream>
#include <sstream>


namespace realm {
namespace util {

/// Thread safe version of std::localtime(). Uses localtime_r() on POSIX.
std::tm localtime(std::time_t);

/// Thread safe version of std::gmtime(). Uses gmtime_r() on POSIX.
std::tm gmtime(std::time_t);

/// Similar to std::put_time() from <iomanip>. See std::put_time() for
/// information about the format string. This function is provided because
/// std::put_time() is unavailable in GCC 4. This function is thread safe.
///
/// The default format is ISO 8601 date and time.
template<class C, class T>
void put_time(std::basic_ostream<C,T>&, const std::tm&, const C* format = "%FT%T%z");

// @{
/// These functions combine localtime() or gmtime() with put_time() and
/// std::ostringstream. For detals on the format string, see
/// std::put_time(). These function are thread safe.
std::string format_local_time(std::time_t, const char* format = "%FT%T%z");
std::string format_utc_time(std::time_t, const char* format = "%FT%T%z");
// @}

/// The local time since the epoch in microseconds.
///
/// FIXME: This function has nothing to do with local time.
double local_time_microseconds();




// Implementation

template<class C, class T>
inline void put_time(std::basic_ostream<C,T>& out, const std::tm& tm, const C* format)
{
    const auto& facet = std::use_facet<std::time_put<C>>(out.getloc()); // Throws
    facet.put(std::ostreambuf_iterator<C>(out), out, ' ', &tm,
              format, format + T::length(format)); // Throws
}

inline std::string format_local_time(std::time_t time, const char* format)
{
    std::tm tm = util::localtime(time);
    std::ostringstream out;
    util::put_time(out, tm, format); // Throws
    return out.str(); // Throws
}

inline std::string format_utc_time(std::time_t time, const char* format)
{
    std::tm tm = util::gmtime(time);
    std::ostringstream out;
    util::put_time(out, tm, format); // Throws
    return out.str(); // Throws
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_TIME_HPP
