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

#ifndef REALM_IMPL_QUERY_TOKENIZER_HPP
#define REALM_IMPL_QUERY_TOKENIZER_HPP

#include <cstddef>
#include <cstdint>
#include <system_error>
#include <utility>
#include <vector>

#include <realm/util/buffer.hpp>
#include <realm/string_data.hpp>

namespace realm {
namespace _impl {

class QueryTokenizer {
public:
    using Boolean    = bool;
    using Integer    = std::int_fast64_t;
    using Fractional = double;
    using String     = std::size_t; ///< As produced by Context::add_string()
    using Location   = std::size_t; ///< Offset from beginning of query string

    enum class Operator { not_, and_, or_, eq, not_eq_, lt, lt_eq, gt, gt_eq, dot };
    struct Value;
    struct Token;

    class Context;

    QueryTokenizer(Context& context):
        m_context{context},
        m_keyword_map{} // Throws
    {
    }

    void reset_input(StringData query)
    {
        const char* data = query.data();
        std::size_t size = query.size();
        m_begin = data;
        m_lex   = data;
        m_curr  = data;
        m_end   = data + size;
    }

    /// Returns false if tokenization was terminated. Otherwise, sets `token`
    /// and `loc` to appropriate values and returns true. If `token` is set to
    /// `Token::Type::end_of_input`, `loc` is set to the end of input, otherwise
    /// `loc` is set to point to the first character of the extracted
    /// token. Tokenization is terminated if Context::tokenizer_error() returns
    /// false.
    ///
    /// If this function returns false, or sets `token` to
    /// `Token::Type::end_of_input`, it must not be called again until after a
    /// call to reset_input().
    bool next(Token& token, Location& loc);

    bool is_keyword(StringData) const noexcept;

    enum class Error;
    static const std::error_category& error_category() noexcept;

private:
    class KeywordMap {
    public:
        KeywordMap();
        bool contains(StringData string)  const noexcept;
        bool lookup(StringData string, Token& token) const noexcept;
    private:
        std::vector<std::pair<StringData, Token>> m_map;
        void add(StringData keyword, Token);
        std::size_t find(StringData string) const noexcept;
    };

    Context& m_context;

    const char* m_begin = nullptr;
    const char* m_lex   = nullptr;
    const char* m_curr  = nullptr;
    const char* m_end   = nullptr;

    util::AppendBuffer<char> m_buffer;

    KeywordMap m_keyword_map;

    bool do_next(Token&);

    // Assumes digit at m_curr.
    bool get_number(Token&);

    // Assumes digit at m_curr preceeded by '.', or '.' at m_curr preceeded by
    // digit.
    bool get_fractional(Token&);

    // Assumes opening single or double quote at m_curr.
    bool get_string(Token&);

    // Assumes first name character at m_curr.
    bool get_name_or_keyword(Token&, bool escaped);

    // Returns false on overflow
    bool parse_integer(const char* begin, const char* end, Integer&);
    void parse_fractional(const char* begin, const char* end, Fractional&);

    bool error(Error, const Location&);
};


struct QueryTokenizer::Value {
    enum class Type { boolean, integer, fractional, string, name };
    Type type;
    union {
        Boolean    boolean;
        Integer    integer;
        Fractional fractional;
        String     string;
        String     name;
    };
    Value() noexcept:
        type{Type::boolean}
    {
        boolean = false;
    }
    static Value make_boolean(Boolean value) noexcept
    {
        Value val;
        val.type = Type::boolean;
        val.boolean = value;
        return val;
    }
    static Value make_integer(Integer value) noexcept
    {
        Value val;
        val.type = Type::integer;
        val.integer = value;
        return val;
    }
    static Value make_fractional(Fractional value) noexcept
    {
        Value val;
        val.type = Type::fractional;
        val.fractional = value;
        return val;
    }
    static Value make_string(String string) noexcept
    {
        Value val;
        val.type = Type::string;
        val.string = string;
        return val;
    }
    static Value make_name(String name) noexcept
    {
        Value val;
        val.type = Type::name;
        val.name = name;
        return val;
    }
};


struct QueryTokenizer::Token {
    enum class Type {
        error, // value with parse error
        value,
        oper,
        true_predicate,
        false_predicate,
        left_paren,
        right_paren,
        end_of_input
    };
    Type type;
    union {
        Value    value;
        Operator oper;
    };
    struct ErrorTag{};
    struct TruePredicateTag {};
    struct FalsePredicateTag {};
    struct LeftParenTag {};
    struct RightParenTag {};
    struct EndOfInputTag {};
    Token() noexcept:
        type{Type::error}
    {
    }
    Token(ErrorTag) noexcept:
        type{Type::error}
    {
    }
    Token(Value val) noexcept:
        type{Type::value}
    {
        value = val;
    }
    Token(Operator op) noexcept:
        type{Type::oper}
    {
        oper = op;
    }
    Token(TruePredicateTag) noexcept:
        type{Type::true_predicate}
    {
    }
    Token(FalsePredicateTag) noexcept:
        type{Type::false_predicate}
    {
    }
    Token(LeftParenTag) noexcept:
        type{Type::left_paren}
    {
    }
    Token(RightParenTag) noexcept:
        type{Type::right_paren}
    {
    }
    Token(EndOfInputTag) noexcept:
        type{Type::end_of_input}
    {
    }
};


class QueryTokenizer::Context {
public:
    virtual String add_string(StringData) = 0;

    /// If this function returns false, tokenization is terminated immediately.
    virtual bool tokenizer_error(std::error_code, const Location&) = 0;
};


enum class QueryTokenizer::Error {
    unrecognized_token,
    bad_chars_in_integer,
    bad_chars_in_fractional,
    bad_escape_seq_in_string,
    unterminated_string_literal,
    integer_overflow,
};

std::error_code make_error_code(QueryTokenizer::Error) noexcept;




// Implementation

inline bool QueryTokenizer::next(Token& token, Location& loc)
{
    if (!do_next(token)) // Throws
        return false;
    loc = std::size_t(m_lex - m_begin);
    return true;
}

inline bool QueryTokenizer::KeywordMap::contains(StringData string)  const noexcept
{
    std::size_t i = find(string);
    return (i != std::size_t(-1));
}

inline bool QueryTokenizer::KeywordMap::lookup(StringData string, Token& token) const noexcept
{
    std::size_t i = find(string);
    if (i == std::size_t(-1))
        return false;
    token = m_map[i].second;
    return true;
}

} // namespace _impl
} // namespace realm

namespace std {

template<> struct is_error_code_enum<realm::_impl::QueryTokenizer::Error> {
    static const bool value = true;
};

} // namespace std

#endif // REALM_IMPL_QUERY_TOKENIZER_HPP
