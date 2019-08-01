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
#ifndef REALM_UTIL_PLATFORM_INFO_HPP
#define REALM_UTIL_PLATFORM_INFO_HPP

#include <string>


namespace realm {
namespace util {

/// Get a description of the current system platform.
///
/// Returns a space-separated concatenation of `osname`, `sysname`, `release`,
/// `version`, and `machine` as returned by get_platform_info(PlatformInfo&).
std::string get_platform_info();


struct PlatformInfo {
    std::string osname;  ///< Equivalent to `uname -o` (Linux).
    std::string sysname; ///< Equivalent to `uname -s`.
    std::string release; ///< Equivalent to `uname -r`.
    std::string version; ///< Equivalent to `uname -v`.
    std::string machine; ///< Equivalent to `uname -m`.
};

/// Get a description of the current system platform.
void get_platform_info(PlatformInfo&);




// Implementation

inline std::string get_platform_info()
{
    PlatformInfo info;
    get_platform_info(info); // Throws
    return (info.osname + " " + info.sysname + " " + info.release + " " + info.version + " " +
            info.machine); // Throws
}


} // namespace util
} // namespace realm

#endif // REALM_UTIL_PLATFORM_INFO_HPP
