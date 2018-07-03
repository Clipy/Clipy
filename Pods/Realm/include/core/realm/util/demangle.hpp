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
#ifndef REALM_UTIL_DEMANGLE_HPP
#define REALM_UTIL_DEMANGLE_HPP

#include <typeinfo>
#include <string>

namespace realm {
namespace util {


/// Demangle the specified C++ ABI identifier.
///
/// See for example
/// http://gcc.gnu.org/onlinedocs/libstdc++/latest-doxygen/namespaceabi.html
std::string demangle(const std::string&);


/// Get the demangled name of the specified type.
template<class T> inline std::string get_type_name()
{
    return demangle(typeid(T).name());
}


/// Get the demangled name of the type of the specified argument.
template<class T> inline std::string get_type_name(const T& v)
{
    return demangle(typeid(v).name());
}


} // namespace util
} // namespace realm

#endif // REALM_UTIL_DEMANGLE_HPP
