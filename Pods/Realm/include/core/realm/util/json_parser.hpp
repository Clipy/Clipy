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

#ifndef REALM_UTIL_JSON_PARSER_HPP
#define REALM_UTIL_JSON_PARSER_HPP

#include <system_error>
#include <algorithm>
#include <cstdlib>

#include <realm/string_data.hpp>

namespace realm {
namespace util {

/// A JSON parser that neither allocates heap memory nor throws exceptions.
///
/// The parser takes as input a range of characters, and emits a stream of events
/// representing the structure of the JSON document.
///
/// Parser errors are represented as `std::error_condition`s.
class JSONParser {
public:
    using InputIterator = const char*;

    enum class EventType {
        number,
        string,
        boolean,
        null,
        array_begin,
        array_end,
        object_begin,
        object_end
    };

    using Range = StringData;

    struct Event {
        EventType type;
        Range range;
        Event(EventType type): type(type) {}

        union {
            bool boolean;
            double number;
        };

        StringData escaped_string_value() const noexcept;

        /// Unescape the string value into \a buffer.
        /// The type of this event must be EventType::string.
        ///
        /// \param buffer is a pointer to a buffer big enough to hold the
        /// unescaped string value. The unescaped string is guaranteed to be
        /// shorter than the escaped string, so escaped_string_value().size() can
        /// be used as an upper bound. Unicode sequences of the form "\uXXXX"
        /// will be converted to UTF-8 sequences. Note that the escaped form of
        /// a unicode point takes exactly 6 bytes, which is also the maximum
        /// possible length of a UTF-8 encoded codepoint.
        StringData unescape_string(char* buffer) const noexcept;
    };

    enum class Error {
        unexpected_token = 1,
        unexpected_end_of_stream = 2
    };

    JSONParser(StringData);

    /// Parse the input data, and call f repeatedly with an argument of type Event
    /// representing the token that the parser encountered.
    ///
    /// The stream of events is "flat", which is to say that it is the responsibility
    /// of the function f to keep track of any nested object structures as it deems
    /// appropriate.
    ///
    /// This function is guaranteed to never throw, as long as f never throws.
    template<class F>
    std::error_condition parse(F&& f) noexcept(noexcept(f(std::declval<Event>())));

    class ErrorCategory: public std::error_category {
    public:
        const char* name() const noexcept final;
        std::string message(int) const final;
    };
    static const ErrorCategory error_category;
private:
    enum Token: char {
        object_begin = '{',
        object_end   = '}',
        array_begin  = '[',
        array_end    = ']',
        colon        = ':',
        comma        = ',',
        dquote       = '"',
        escape       = '\\',
        minus        = '-',
        space        = ' ',
        tab          = '\t',
        cr           = '\r',
        lf           = '\n',
    };

    InputIterator m_current;
    InputIterator m_end;

    template<class F>
    std::error_condition parse_object(F&& f) noexcept(noexcept(f(std::declval<Event>())));
    template<class F>
    std::error_condition parse_pair(F&& f) noexcept(noexcept(f(std::declval<Event>())));
    template<class F>
    std::error_condition parse_array(F&& f) noexcept(noexcept(f(std::declval<Event>())));
    template<class F>
    std::error_condition parse_number(F&& f) noexcept(noexcept(f(std::declval<Event>())));
    template<class F>
    std::error_condition parse_string(F&& f) noexcept(noexcept(f(std::declval<Event>())));
    template<class F>
    std::error_condition parse_value(F&& f) noexcept(noexcept(f(std::declval<Event>())));
    template<class F>
    std::error_condition parse_boolean(F&& f) noexcept(noexcept(f(std::declval<Event>())));
    template<class F>
    std::error_condition parse_null(F&& f) noexcept(noexcept(f(std::declval<Event>())));

    std::error_condition expect_token(char, Range& out_range) noexcept;
    std::error_condition expect_token(Token, Range& out_range) noexcept;

    // Returns true unless EOF was reached.
    bool peek_char(char& out_c) noexcept;
    bool peek_token(Token& out_t) noexcept;
    bool is_whitespace(Token t) noexcept;
    void skip_whitespace() noexcept;
};

std::error_condition make_error_condition(JSONParser::Error e);

} // namespace util
} // namespace realm

namespace std {
template<>
struct is_error_condition_enum<realm::util::JSONParser::Error> {
    static const bool value = true;
};
}

namespace realm {
namespace util {

/// Implementation:


inline JSONParser::JSONParser(StringData input):
    m_current(input.data()), m_end(input.data() + input.size())
{
}

template<class F>
std::error_condition JSONParser::parse(F&& f) noexcept(noexcept(f(std::declval<Event>())))
{
    return parse_value(f);
}

template<class F>
std::error_condition JSONParser::parse_object(F&& f) noexcept(noexcept(f(std::declval<Event>())))
{
    Event event{EventType::object_begin};
    auto ec = expect_token(Token::object_begin, event.range);
    if (ec)
        return ec;
    ec = f(event);
    if (ec)
        return ec;

    while (true) {
        ec = expect_token(Token::object_end, event.range);
        if (!ec) {
            // End of object
            event.type = EventType::object_end;
            ec = f(event);
            if (ec)
                return ec;
            break;
        }

        if (ec != Error::unexpected_token)
            return ec;

        ec = parse_pair(f);
        if (ec)
            return ec;

        skip_whitespace();

        Token t;
        if (peek_token(t)) {
            if (t == Token::object_end) {
                // Fine, will terminate on next iteration
            }
            else if (t == Token::comma)
                ++m_current; // OK, because peek_char returned true
            else
                return Error::unexpected_token;
        }
        else {
            return Error::unexpected_end_of_stream;
        }
    }

    return std::error_condition{};
}

template<class F>
std::error_condition JSONParser::parse_pair(F&& f) noexcept(noexcept(f(std::declval<Event>())))
{
    skip_whitespace();

    auto ec = parse_string(f);
    if (ec)
        return ec;

    skip_whitespace();

    Token t;
    if (peek_token(t)) {
        if (t == Token::colon) {
            ++m_current;
        }
        else {
            return Error::unexpected_token;
        }
    }

    return parse_value(f);
}

template<class F>
std::error_condition JSONParser::parse_array(F&& f) noexcept(noexcept(f(std::declval<Event>())))
{
    Event event{EventType::array_begin};
    auto ec = expect_token(Token::array_begin, event.range);
    if (ec)
        return ec;
    ec = f(event);
    if (ec)
        return ec;

    while (true) {
        ec = expect_token(Token::array_end, event.range);
        if (!ec) {
            // End of array
            event.type = EventType::array_end;
            ec = f(event);
            if (ec)
                return ec;
            break;
        }

        if (ec != Error::unexpected_token)
            return ec;

        ec = parse_value(f);
        if (ec)
            return ec;

        skip_whitespace();

        Token t;
        if (peek_token(t)) {
            if (t == Token::array_end) {
                // Fine, will terminate next iteration.
            }
            else if (t == Token::comma)
                ++m_current; // OK, because peek_char returned true
            else
                return Error::unexpected_token;
        }
        else {
            return Error::unexpected_end_of_stream;
        }
    }

    return std::error_condition{};
}

template<class F>
std::error_condition JSONParser::parse_number(F&& f) noexcept(noexcept(f(std::declval<Event>())))
{
    static const size_t buffer_size = 64;
    char buffer[buffer_size] = {0};
    size_t bytes_to_copy = std::min<size_t>(m_end - m_current, buffer_size - 1);
    if (bytes_to_copy == 0)
        return Error::unexpected_end_of_stream;

    if (std::isspace(*m_current)) {
        // JSON has a different idea of what constitutes whitespace than isspace(),
        // but strtod() uses isspace() to skip initial whitespace. We have already
        // skipped whitespace that JSON considers valid, so if there is any whitespace
        // at m_current now, it is invalid according to JSON, and so is an error.
        return Error::unexpected_token;
    }

    switch (m_current[0]) {
        case 'N':
            // strtod() parses "NAN", JSON does not.
        case 'I':
            // strtod() parses "INF", JSON does not.
        case 'p':
        case 'P':
            // strtod() may parse exponent notation, JSON does not.
            return Error::unexpected_token;
        case '0':
            if (bytes_to_copy > 2 && (m_current[1] == 'x' || m_current[1] == 'X')) {
                // strtod() parses hexadecimal, JSON does not.
                return Error::unexpected_token;
            }
    }

    std::copy(m_current, m_current + bytes_to_copy, buffer);

    char* endp = nullptr;
    Event event{EventType::number};
    event.number = std::strtod(buffer, &endp);

    if (endp == buffer) {
        return Error::unexpected_token;
    }
    size_t num_bytes_consumed = endp - buffer;
    m_current += num_bytes_consumed;
    return f(event);
}

template<class F>
std::error_condition JSONParser::parse_string(F&& f) noexcept(noexcept(f(std::declval<Event>())))
{
    InputIterator p = m_current;
    if (p >= m_end)
        return Error::unexpected_end_of_stream;

    auto count_num_escapes_backwards = [](const char* p, const char* begin) -> size_t {
        size_t result = 0;
        for (; p > begin && *p == Token::escape; ++p)
            ++result;
        return result;
    };

    Token t = static_cast<Token>(*p);
    InputIterator inner_end;
    if (t == Token::dquote) {
        inner_end = m_current;
        do {
            inner_end = std::find(inner_end + 1, m_end, Token::dquote);
            if (inner_end == m_end)
                return Error::unexpected_end_of_stream;
        } while (count_num_escapes_backwards(inner_end - 1, m_current) % 2 == 1);

        Event event{EventType::string};
        event.range = Range(m_current, inner_end - m_current + 1);
        m_current = inner_end + 1;
        return f(event);
    }
    return Error::unexpected_token;
}

template<class F>
std::error_condition JSONParser::parse_boolean(F&& f) noexcept(noexcept(f(std::declval<Event>())))
{
    auto first_nonalpha = std::find_if_not(m_current, m_end, [](auto c) { return std::isalpha(c); });

    Event event{EventType::boolean};
    event.range = Range(m_current, first_nonalpha - m_current);
    if (event.range == "true") {
        event.boolean = true;
        m_current += 4;
        return f(event);
    }
    else if (event.range == "false") {
        event.boolean = false;
        m_current += 5;
        return f(event);
    }

    return Error::unexpected_token;
}

template<class F>
std::error_condition JSONParser::parse_null(F&& f) noexcept(noexcept(f(std::declval<Event>())))
{
    auto first_nonalpha = std::find_if_not(m_current, m_end, [](auto c) { return std::isalpha(c); });

    Event event{EventType::null};
    event.range = Range(m_current, first_nonalpha - m_current);
    if (event.range == "null") {
        m_current += 4;
        return f(event);
    }

    return Error::unexpected_token;
}

template<class F>
std::error_condition JSONParser::parse_value(F&& f) noexcept(noexcept(f(std::declval<Event>())))
{
    skip_whitespace();

    if (m_current >= m_end)
        return Error::unexpected_end_of_stream;

    if (*m_current == Token::object_begin)
        return parse_object(f);

    if (*m_current == Token::array_begin)
        return parse_array(f);

    if (*m_current == 't' || *m_current == 'f')
        return parse_boolean(f);

    if (*m_current == 'n')
        return parse_null(f);

    if (*m_current == Token::dquote)
        return parse_string(f);

    return parse_number(f);
}

inline
bool JSONParser::is_whitespace(Token t) noexcept
{
    switch (t) {
        case Token::space:
        case Token::tab:
        case Token::cr:
        case Token::lf:
            return true;
        default:
            return false;
    }
}

inline
void JSONParser::skip_whitespace() noexcept
{
    while (m_current < m_end && is_whitespace(static_cast<Token>(*m_current)))
        ++m_current;
}

inline
std::error_condition JSONParser::expect_token(char c, Range& out_range) noexcept
{
    skip_whitespace();
    if (m_current == m_end)
        return Error::unexpected_end_of_stream;
    if (*m_current == c) {
        out_range = Range(m_current, 1);
        ++m_current;
        return std::error_condition{};
    }
    return Error::unexpected_token;
}

inline
std::error_condition JSONParser::expect_token(Token t, Range& out_range) noexcept
{
    return expect_token(static_cast<char>(t), out_range);
}

inline
bool JSONParser::peek_char(char& out_c) noexcept
{
    if (m_current < m_end) {
        out_c = *m_current;
        return true;
    }
    return false;
}

inline
bool JSONParser::peek_token(Token& out_t) noexcept
{
    if (m_current < m_end) {
        out_t = static_cast<Token>(*m_current);
        return true;
    }
    return false;
}

inline
StringData JSONParser::Event::escaped_string_value() const noexcept
{
    REALM_ASSERT(type == EventType::string);
    REALM_ASSERT(range.size() >= 2);
    return StringData(range.data() + 1, range.size() - 2);
}

template<class OS>
OS& operator<<(OS& os, JSONParser::EventType type)
{
    switch (type) {
        case JSONParser::EventType::number:       os << "number"; return os;
        case JSONParser::EventType::string:       os << "string"; return os;
        case JSONParser::EventType::boolean:      os << "boolean"; return os;
        case JSONParser::EventType::null:         os << "null"; return os;
        case JSONParser::EventType::array_begin:  os << "["; return os;
        case JSONParser::EventType::array_end:    os << "]"; return os;
        case JSONParser::EventType::object_begin: os << "{"; return os;
        case JSONParser::EventType::object_end:   os << "}"; return os;
    }
    REALM_UNREACHABLE();
}

template<class OS>
OS& operator<<(OS& os, const JSONParser::Event& e) {
    os << e.type;
    switch (e.type) {
        case JSONParser::EventType::number:       return os << "(" << e.number << ")";
        case JSONParser::EventType::string:       return os << "(" << e.range << ")";
        case JSONParser::EventType::boolean:      return os << "(" << e.boolean << ")"; 
        default: return os;
    }
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_JSON_PARSER_HPP

