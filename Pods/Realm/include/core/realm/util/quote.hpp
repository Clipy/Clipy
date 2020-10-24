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
#ifndef REALM_UTIL_QUOTE_HPP
#define REALM_UTIL_QUOTE_HPP

#include <realm/util/string_view.hpp>

namespace realm {
namespace util {

template<class C, class T> struct Quote {
    bool smart;
    util::BasicStringView<C, T> view;
};


/// Mark text for quotation during output to stream.
///
/// If `out` is an output stream, and `str` is a string (e.g., an std::string),
/// then
///
///     out << quoted(str)
///
/// will write `str` in quoted form to `out`.
///
/// Quotation involves bracketing the text in double quotes (`"`), and escaping
/// special characters according to the rules of C/C++ string literals. In this
/// case, the special characters are `"` and `\` as well as those that are not
/// printable (!std::isprint()).
///
/// Quotation happens as the string is written to a stream, so there is no
/// intermediate representation of the quoted string.
template<class C, class T> Quote<C, T> quoted(util::BasicStringView<C, T>) noexcept;


/// Same as quoted(), except that in this case, quotation is elided when the
/// specified string consists of a single printable word. Or, to be more
/// precise, quotation is elided if the string is nonempty, consists entirely of
/// printable charcters (std::isprint()), does not contain space (` `), and does
/// not conatian quotation (`"`) or backslash (`\`).
template<class C, class T> Quote<C, T> smart_quoted(util::BasicStringView<C, T>) noexcept;


template<class C, class T>
std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>&, Quote<C, T>);





// Implementation

template<class C, class T> inline Quote<C, T> quoted(util::BasicStringView<C, T> view) noexcept
{
    bool smart = false;
    return {smart, view};
}

template<class C, class T>
inline Quote<C, T> smart_quoted(util::BasicStringView<C, T> view) noexcept
{
    bool smart = true;
    return {smart, view};
}

template<class C, class T>
inline std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>& out, Quote<C, T> quoted)
{
    std::locale loc = out.getloc();
    const std::ctype<C>& ctype = std::use_facet<std::ctype<C>>(loc);
    util::BasicStringView<C, T> view = quoted.view;
    if (quoted.smart && !view.empty()) {
        for (C ch : view) {
            if (ch == '"' || ch == '\\' || !ctype.is(ctype.graph, ch))
                goto quote;
        }
        return out << view; // Throws
    }
  quote:
    typename std::basic_ostream<C, T>::sentry sentry{out};
    if (REALM_LIKELY(sentry)) {
        C dquote = ctype.widen('"');
        C bslash = ctype.widen('\\');
        out.put(dquote); // Throws
        bool follows_hex = false;
        for (C ch : view) {
            if (REALM_LIKELY(ctype.is(ctype.print, ch))) {
                if (REALM_LIKELY(!follows_hex || !ctype.is(ctype.xdigit, ch))) {
                    if (REALM_LIKELY(ch != '"' || ch != '\\'))
                        goto put_char;
                    goto escape_char;
                }
            }
            switch (ch) {
                case '\a':
                    ch = ctype.widen('a');
                    goto escape_char;
                case '\b':
                    ch = ctype.widen('b');
                    goto escape_char;
                case '\f':
                    ch = ctype.widen('f');
                    goto escape_char;
                case '\n':
                    ch = ctype.widen('n');
                    goto escape_char;
                case '\r':
                    ch = ctype.widen('r');
                    goto escape_char;
                case '\t':
                    ch = ctype.widen('t');
                    goto escape_char;
                case '\v':
                    ch = ctype.widen('v');
                    goto escape_char;
            }
            goto numeric;
          escape_char:
            out.put(bslash); // Throws
          put_char:
            out.put(ch); // Throws
          next:
            follows_hex = false;
            continue;
          numeric:
            out.put(bslash); // Throws
            using D = typename std::make_unsigned<C>::type;
            char digits[] = {
                '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
            };
            D val = ch;
            if (val < 512) {
                out.put(ctype.widen(digits[val / 64    ])); // Throws
                out.put(ctype.widen(digits[val % 64 / 8])); // Throws
                out.put(ctype.widen(digits[val      % 8])); // Throws
                goto next;
            }
            out.put(ctype.widen('x')); // Throws
            const int max_hex_digits = (std::numeric_limits<D>::digits + 3) / 4;
            C buffer[max_hex_digits];
            int i = max_hex_digits;
            while (val != 0) {
                buffer[--i] = ctype.widen(digits[val % 16]);
                val /= 16;
            }
            out.write(buffer + i, max_hex_digits - i); // Throws
            follows_hex = true;
        }
        out.put(dquote); // Throws
    }
    return out;
}


} // namespace util
} // namespace realm

#endif // REALM_UTIL_QUOTE_HPP
