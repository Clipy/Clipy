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

#ifndef REALM_UTIL_ENUM_HPP
#define REALM_UTIL_ENUM_HPP

#include <map>
#include <string>
#include <ios>
#include <locale>


namespace realm {
namespace util {

/// This template class allows you to endow a fundamental `enum` type with
/// information about how to print out the individual values, and how to parse
/// them.
///
/// Here is an example:
///
///     // Declaration
///
///     enum class Color { orange, purple, brown };
///
///     struct ColorSpec { static EnumAssoc map[]; };
///     using ColorEnum = Enum<Color, ColorSpec>;
///
///     // Implementation
///
///     EnumAssoc ColorSpec::map[] = {
///         { int(Color::orange), "orange" },
///         { int(Color::purple), "purple" },
///         { int(Color::brown),  "brown"  },
///         { 0, 0 }
///     };
///
///     // Application
///
///     ColorEnum color = Color::purple;
///
///     std::cout << color;  // Write a color
///     std::cin  >> color;  // Read a color
///
/// The current implementation is restricted to enumeration types whose values
/// can all be represented in a regular integer.
template<class E, class S, bool ignore_case = false> class Enum {
public:
    using base_enum_type = E;

    Enum(E = {}) noexcept;

    operator E() const noexcept;

    const std::string& str() const;

    bool str(const std::string*&) const noexcept;

    /// \return True if, and only if successful.
    static bool parse(const std::string& string, E& value);

private:
    E m_value = E{};
};

template<class C, class T, class E, class S, bool ignore_case>
std::basic_ostream<C,T>& operator<<(std::basic_ostream<C,T>&,
                                    const Enum<E, S, ignore_case>&);

template<class C, class T, class E, class S, bool ignore_case>
std::basic_istream<C,T>& operator>>(std::basic_istream<C,T>&,
                                    Enum<E, S, ignore_case>&);


struct EnumAssoc {
    const int value;
    const char* const name;
};




// Implementation

} // namespace util

namespace _impl {

class EnumMapper {
public:
    EnumMapper(const util::EnumAssoc*, bool ignore_case);

    bool parse(const std::string& string, int& value, bool ignore_case) const;

    std::map<int, std::string> value_to_name;
    std::map<std::string, int> name_to_value;
};

template<class S, bool ignore_case> const EnumMapper& get_enum_mapper()
{
    static EnumMapper mapper{S::map, ignore_case}; // Throws
    return mapper;
}

} // namespace _impl

namespace util {

template<class E, class S, bool ignore_case>
inline Enum<E, S, ignore_case>::Enum(E value) noexcept :
    m_value{value}
{
}

template<class E, class S, bool ignore_case>
inline Enum<E, S, ignore_case>::operator E() const noexcept
{
    return m_value;
}

template<class E, class S, bool ignore_case>
inline const std::string& Enum<E, S, ignore_case>::str() const
{
    return _impl::get_enum_mapper<S, ignore_case>().val_to_name.at(m_value); // Throws
}

template<class E, class S, bool ignore_case>
inline bool Enum<E, S, ignore_case>::str(const std::string*& string) const noexcept
{
    const auto& value_to_name = _impl::get_enum_mapper<S, ignore_case>().value_to_name;
    auto i = value_to_name.find(int(m_value));
    if (i == value_to_name.end())
        return false;
    string = &i->second;
    return true;
}

template<class E, class S, bool ignore_case>
inline bool Enum<E, S, ignore_case>::parse(const std::string& string, E& value)
{
    int value_2;
    if (!_impl::get_enum_mapper<S, ignore_case>().parse(string, value_2, ignore_case)) // Throws
        return false;
    value = E(value_2);
    return true;
}

template<class C, class T, class E, class S, bool ignore_case>
inline std::basic_ostream<C,T>& operator<<(std::basic_ostream<C,T>& out,
                                           const Enum<E, S, ignore_case>& e)
{
    const std::string* string;
    if (e.str(string)) {
        out << *string;
    }
    else {
        out << int(E(e));
    }
    return out;
}

template<class C, class T, class E, class S, bool ignore_case>
std::basic_istream<C,T>& operator>>(std::basic_istream<C,T>& in,
                                    Enum<E, S, ignore_case>& e)
{
    if (in.bad() || in.fail())
        return in;
    std::string string;
    const std::ctype<C>& ctype = std::use_facet<std::ctype<C>>(in.getloc());
    C underscore(ctype.widen('_'));
    for (;;) {
        C ch;
        // Allow white-spaces to be skipped when stream is configured
        // that way
        if (string.empty()) {
            in >> ch;
        }
        else {
            in.get(ch);
        }
        if (!in) {
            if (in.bad())
                return in;
            in.clear(in.rdstate() & ~std::ios_base::failbit);
            break;
        }
        if (!ctype.is(std::ctype_base::alnum, ch) && ch != underscore) {
            in.unget();
            break;
        }
        char ch_2 = ctype.narrow(ch, '\0');
        string += ch_2;
    }
    E value = E{};
    if (!Enum<E, S, ignore_case>::parse(string, value)) { // Throws
        in.setstate(std::ios_base::badbit);
    }
    else {
        e = value;
    }
    return in;
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_ENUM_HPP
