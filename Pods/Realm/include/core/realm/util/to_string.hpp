/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2016] Realm Inc
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
#ifndef REALM_UTIL_TO_STRING_HPP
#define REALM_UTIL_TO_STRING_HPP

#include <iosfwd>
#include <string>

namespace realm {
namespace util {

class Printable {
public:
    Printable(bool value) : m_type(Type::Bool), m_uint(value) { }
    Printable(unsigned char value) : m_type(Type::Uint), m_uint(value) { }
    Printable(unsigned int value) : m_type(Type::Uint), m_uint(value) { }
    Printable(unsigned long value) : m_type(Type::Uint), m_uint(value) { }
    Printable(unsigned long long value) : m_type(Type::Uint), m_uint(value) { }
    Printable(char value) : m_type(Type::Int), m_int(value) { }
    Printable(int value) : m_type(Type::Int), m_int(value) { }
    Printable(long value) : m_type(Type::Int), m_int(value) { }
    Printable(long long value) : m_type(Type::Int), m_int(value) { }
    Printable(const char* value) : m_type(Type::String), m_string(value) { }

    void print(std::ostream& out, bool quote) const;
    std::string str() const;

    static void print_all(std::ostream& out, const std::initializer_list<Printable>& values, bool quote);

private:
    enum class Type {
        Bool,
        Int,
        Uint,
        String
    } m_type;

    union {
        uintmax_t m_uint;
        intmax_t m_int;
        const char* m_string;
    };
};


template<class T>
std::string to_string(const T& v)
{
    return Printable(v).str();
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_TO_STRING_HPP
