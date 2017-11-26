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

#ifndef REALM_QUERY_PARSER_HPP
#define REALM_QUERY_PARSER_HPP

#include <type_traits>
#include <memory>
#include <utility>
#include <system_error>
#include <ostream>

#include <realm/util/logger.hpp>
#include <realm/string_data.hpp>
#include <realm/table.hpp>
#include <realm/query.hpp>
#include <realm/impl/query_tokenizer.hpp>

namespace realm {


/// A parser for queries that approximates the syntax used in NSPredicate. This
/// parser is experimental. A different one exists at the object store level.
///
/// ### Grammar
///
///     query          = disjunction
///     disjunction    = [disjunction OR] conjunction
///     conjunction    = [conjunction AND] negation
///     negation       = [NOT] predicate
///     predicate      = compare_pred   |
///                      contains_pred  |
///                      TRUEPREDICATE  |
///                      FALSEPREDICATE |
///                      LPAR disjunction RPAR
///
///     compare_pred   = value compare value
///     compare        = EQ [CMP_MODIF] | NOT_EQ | LT | LT_EQ | GT | GT_EQ
///
///     contains_pred  = key_path contains [CMP_MODIF] value
///     contains       = CONTAINS | BEGINSWITH | ENDSWITH
///
///     value          = literal | argument | key_path | LPAR value RPAR
///     literal        = INTEGER | FRACTIONAL | STRING | TRUE | FALSE | NULL
///     argument       = ARGUMENT
///
///     key_path       = [key_path DOT] key_path_2
///     key_path_2     = NAME | LPAR key_path RPAR
///
/// ### Tokens
///
///     OR             = '||' | 'OR'
///     AND            = '&&' | 'AND'
///     NOT            = '!'  | 'NOT'
///
///     TRUEPREDICATE  = 'TRUEPREDICATE'
///     FALSEPREDICATE = 'FALSEPREDICATE'
///
///     LPAR           = '('
///     RPAR           = ')'
///
///     TRUE           = 'TRUE'
///     FALSE          = 'FALSE'
///     NULL           = 'NULL'
///
///     EQ             = '='  | '=='
///     NOT_EQ         = '!=' | '<>'
///     LT             = '<'
///     LT_EQ          = '<=' | '=<'
///     GT             = '>'
///     GT_EQ          = '>=' | '=>'
///
///     CMP_MODIF      = '[C]'
///
///     CONTAINS       = 'CONTAINS'
///     BEGINSWITH     = 'BEGINSWITH'
///     ENDSWITH       = 'ENDSWITH'
///
///     INTEGER        = /-?[[:digit:]]+\b/ |
///                      /-?0x[[:xdigit:]]+\b/
///     FRACTIONAL     = /-?[[:digit:]]+\.[[:digit:]]*(?!\w)/ |
///                      /-?[[:digit:]]*\.[[:digit:]]+\b/
///     STRING         = /'([^'\\]|{ESC_SEQ})*'/ |
///                      /"([^"\\]|{ESC_SEQ})*"/
///     ARGUMENT       = /\$[[:digit:]]+\b/
///     NAME           = /#?([[:alpha:]_][[:alnum:]_]*)/
///
/// Note: `/.../` are ECMAScript-style regular expressions.
///
/// All matching is case insensitive.
///
/// When two token patterns match, the one that matches the longest prefix of the
/// input takes precedence. If two matches are of equal length, the one that is
/// listed first, takes precedence.
///
/// Any amount of white space is allowed between tokens.
///
/// ### Regular subexpressions
///
///     ESC_SEQ        = /\\[\\'"bfnrt0]/ |
///                      /(\\u[[:xdigit:]]{4})+/
///
/// Note that the purpose of the optional `#` in `NAME` is to escape
/// keywords. The hashmark itself is not considered to be part of the name.
///
/// FIXME: The `CONTAINS`, `BEGINSWITH`, and `ENDSWITH` operations are not yet
/// supported, and the corresponding tokens are not yet recognized.
///
/// FIXME: Case-insensitive comparisons are not yet supported, and the `[c]`
/// token is not yet recognized.
///
/// FIXME: Null-value literals are not yet supported, and the `NULL` token is
/// not yet recognized.
///
/// FIXME: Arguments are not yet supported, and the `ARGUMENT` token is not yet
/// recognized.
///
/// FIXME: Hexadecimal integer literals are not yet recognized by the tokenizer.
///
/// FIXME: Unicode identifiers are not yet recognized by the tokenizer.
///
/// FIXME: Unicode escape sequences in string are not yet supported.
class QueryParser {
public:
    using Location = _impl::QueryTokenizer::Location;
    enum class Error;
    class ErrorHandler;

    /// Returns true if parsing was successful, otherwise returns false. If
    /// parsing was not successful, one or more errors will have been reported
    /// through the error handler.
    virtual bool parse(StringData query, Query&, ErrorHandler&) = 0;

    template<class H, class = std::enable_if_t<!std::is_base_of<ErrorHandler, H>::value &&
                                               !std::is_base_of<util::Logger, H>::value &&
                                               !std::is_base_of<std::ostream, H>::value>>
    bool parse(StringData query, Query&, H error_handler);

    bool parse(StringData query, Query&, util::Logger&);

    /// Print errors to the specified output stream.
    bool parse(StringData query, Query&, std::ostream&);

    virtual ~QueryParser() {}

    static const std::error_category& error_category() noexcept;
};

std::unique_ptr<QueryParser> make_query_parser();


class QueryParser::ErrorHandler {
public:
    /// If this function returns false, parsing is terminated immediately.
    ///
    /// The handler must be prepared to recieve error codes from either of the
    /// following categories:
    ///
    ///     QueryTokenizer::error_category()
    ///     QueryParser::error_category()
    virtual bool handle(std::error_code, const Location&) = 0;
};


enum class QueryParser::Error {
    missing_compare_operator_before,
    missing_logical_operator_before,
    missing_left_kp_construct_operand,
    missing_right_kp_construct_operand,
    missing_left_compare_operand,
    missing_right_compare_operand,
    missing_left_logical_operand,
    missing_right_logical_operand,
    bad_left_kp_construct_operand,
    bad_right_kp_construct_operand,
    bad_left_compare_operand,
    bad_right_compare_operand,
    bad_left_logical_operand,
    bad_right_logical_operand,
    key_path_lookup_error,
    not_a_link_column,
    multivalued_key_path_on_both_sides,
    unsupported_compare_datatype,
    unsupported_string_comparison,
    comparison_datatype_mismatch,
    need_key_path_compare_operand,
    unmatched_left_paren,
    unmatched_right_paren,
    empty_parentheses, // Location is closing parenthesis
    empty_input,
};

std::error_code make_error_code(QueryParser::Error) noexcept;




// Implementation

template<class H, class> bool QueryParser::parse(StringData query, Query& query_2, H error_handler)
{
    class ErrorHandlerImpl: public ErrorHandler {
    public:
        ErrorHandlerImpl(H error_handler):
            m_error_handler{std::move(error_handler)}
        {
        }
        bool handle(std::error_code ec, const Location& loc) override
        {
            return m_error_handler(ec, loc); // Throws
        }
    private:
        H m_error_handler;
    };
    ErrorHandlerImpl error_handler_2{std::move(error_handler)};
    return parse(query, query_2, error_handler_2); // Throws
}

inline bool QueryParser::parse(StringData query, Query& query_2, util::Logger& logger)
{
    class ErrorHandlerImpl: public ErrorHandler {
    public:
        ErrorHandlerImpl(util::Logger& logger):
            m_logger{logger}
        {
        }
        bool handle(std::error_code ec, const Location&) override
        {
            m_logger.error("%1", ec.message()); // Throws
            return true;
        }
    private:
        util::Logger& m_logger;
    };
    ErrorHandlerImpl error_handler{logger};
    return parse(query, query_2, error_handler); // Throws
}

inline bool QueryParser::parse(StringData query, Query& query_2, std::ostream& out)
{
    class ErrorHandlerImpl: public ErrorHandler {
    public:
        ErrorHandlerImpl(StringData query, std::ostream& out):
            m_query{query},
            m_out{out}
        {
        }
        bool handle(std::error_code ec, const QueryParser::Location& loc) override
        {
            m_out << "ERROR: "<<ec.category().name()<<": "<<ec.message()<<"\n"; // Throws
            m_out << "> "<<m_query<<"\n"; // Throws
            m_out << "> "<<std::setw(int(loc))<<""<<"^\n"; // Throws
            return true;
        }
    private:
        const StringData m_query;
        std::ostream& m_out;
    };
    ErrorHandlerImpl error_handler{query, out};
    return parse(query, query_2, error_handler); // Throws
}

} // namespace realm

namespace std {

template<> struct is_error_code_enum<realm::QueryParser::Error> {
    static const bool value = true;
};

} // namespace std

#endif // REALM_QUERY_PARSER_HPP
