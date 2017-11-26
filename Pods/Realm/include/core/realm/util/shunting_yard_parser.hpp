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

#ifndef REALM_UTIL_SHUNTING_YARD_PARSER_HPP
#define REALM_UTIL_SHUNTING_YARD_PARSER_HPP

#include <utility>
#include <vector>

#include <realm/util/assert.hpp>

namespace realm {
namespace util {

class ShuntingYardParserBase {
public:
    enum class Token {
        error,       // value parsed with errors
        value,
        oper,        // operator
        left_paren,
        right_paren,
        end_of_input
    };
    enum class Side { left, right };
};

/// Edsger Dijkstra's shunting yard algorithm.
///
/// The `Context` type must define these types:
///
///  - `Operator`, whose values must represent particular operators (prefix,
///    infix, and postfix). For example, it could be an index into a table of
///    operators.
///  - `Value`, which is the type of the operands for operators, and the type of
///    the result of performing the associated operations.
///  - `Location`, which is taken to represent a location (of a token) in the
///    input.
///
/// The `Context` type must also define these functions:
///
///     bool next_token(Token& tok, Value& val, Operator& oper, Location& loc)
///     bool is_prefix_operator(const Operator&) const noexcept
///     bool is_postfix_operator(const Operator&) const noexcept
///     int get_precedence_level(const Operator&) const noexcept
///     bool is_prec_right_associative(int precedence_level) const noexcept
///     bool perform_unop(Operator, Value value, Value& result, bool& error)
///     bool perform_binop(Operator, Value left, Value right, Value& result, bool& error)
///     bool check_result(const Value& result, const Location&)
///     bool missing_operand(const Operator&, Side, const Location&)
///     bool missing_operator_before(const Value& preceding_value, const Location&)
///     bool unmatched_paren(Side, const Location&)
///     bool empty_parentheses(const Location&) // Location is closing parenthesis
///     bool empty_input(const Location&) // Location is end of input
///
/// If `next_token()` returns false, parsing will be terminated
/// immediately. Otherwise, `next_token()` must set `tok` to the appropriate
/// value, and `loc` to the location of the token in the input, or to the end of
/// input if `tok` is set to `Token::end_of_input`. If the extracted token is
/// `Token::value` (operand), then it must set `value` to the appropriate
/// value. If the extracted token is `Token::oper` (operator), then it must set
/// `oper` to the appropriate value.
///
/// If `perform_unop()` or `perform_binop()` returns false, parsing will be
/// terminated immediately. Otherwise, on success, they must set `result` to the
/// appropriate value, and on failure, they must set `error` to true. If they
/// set `error` to true, it does not matter whether they also modify
/// `result`. The prior value of `error` is guaranteed to be false.
///
/// If `check_result()` returns false, parsing will be terminated immediately.
///
/// If `missing_operand()`, `missing_operator_before()`, `unmatched_paren()`,
/// `empty_parentheses()`, or `empty_input()` returns false, parsing will be
/// terminated immediately.
template<class Context> class ShuntingYardParser: public ShuntingYardParserBase {
public:
    using Operator = typename Context::Operator;
    using Value    = typename Context::Value;
    using Location = typename Context::Location;

    /// If an error occurred and/or parsing was terminated (see class-level
    /// documentation), this function returns false. Otherwise, it sets `result`
    /// and returns true.
    bool parse(Context&, Value& result);

private:
    struct ErrorTag {};
    struct LeftParenTag {};

    struct ValueSlot {
        bool error;
        Value value;
        ValueSlot(Value);
        ValueSlot(ErrorTag);
    };

    struct OperSlot {
        enum class Type { normal, left_paren, error };
        Type type;
        Operator oper;
        Location loc;
        OperSlot(Operator, const Location&);
        OperSlot(LeftParenTag, const Location&);
        OperSlot(ErrorTag);
    };

    // The operator stack contains only prefix and infix operators.
    std::vector<ValueSlot> m_value_stack;
    std::vector<OperSlot> m_operator_stack;

    bool do_parse(Context&, Value&);

    bool perform_prefix_or_infix_oper(Operator, const Location&, Context&);
    bool perform_prefix_or_postfix_oper(Operator, const Location&, Context&);
    void perform_error_operation() noexcept;

    void clear_stacks() noexcept;
};




// Implementation

template<class Context> inline bool ShuntingYardParser<Context>::parse(Context& context, Value& result)
{
    try {
        bool unterminated = do_parse(context, result); // Throws
        clear_stacks();
        return unterminated;
    }
    catch (...) {
        clear_stacks();
        throw;
    }
}

template<class Context> inline ShuntingYardParser<Context>::ValueSlot::ValueSlot(Value val):
    error{false},
    value{std::move(val)}
{
}

template<class Context> inline ShuntingYardParser<Context>::ValueSlot::ValueSlot(ErrorTag):
    error{true},
    value{Value{}}
{
}

template<class Context> inline ShuntingYardParser<Context>::OperSlot::OperSlot(Operator op, const Location& loc):
    type{Type::normal},
    oper{std::move(op)},
    loc{loc}
{
}

template<class Context> inline ShuntingYardParser<Context>::OperSlot::OperSlot(LeftParenTag, const Location& loc):
    type{Type::left_paren},
    oper{Operator{}},
    loc{loc}
{
}

template<class Context> inline ShuntingYardParser<Context>::OperSlot::OperSlot(ErrorTag):
    type{Type::error},
    oper{Operator{}},
    loc{Location{}}
{
}

template<class Context> bool ShuntingYardParser<Context>::do_parse(Context& context, Value& result)
{
    Token token   = Token{};
    Value value   = Value{};
    Operator oper = Operator{};
    Location loc  = Location{};

  want_operand:
    if (!context.next_token(token, value, oper, loc)) // Throws
        return false;

  want_operand_2:
    switch (token) {
        case Token::error:
            m_value_stack.emplace_back(ErrorTag{}); // Throws
            goto have_operand;
        case Token::value:
            m_value_stack.emplace_back(std::move(value)); // Throws
            goto have_operand;
        case Token::oper:
            if (context.is_prefix_operator(oper)) {
                m_operator_stack.emplace_back(std::move(oper), loc); // Throws
                goto want_operand;
            }
            if (!context.missing_operand(oper, Side::left, loc)) // Throws
                return false;
            m_value_stack.emplace_back(ErrorTag{}); // Throws
            goto have_operand_2;
        case Token::left_paren:
            m_operator_stack.emplace_back(LeftParenTag{}, loc); // Throws
            goto want_operand;
        case Token::right_paren:
            if (!m_operator_stack.empty()) {
                const OperSlot& slot = m_operator_stack.back();
                // This cannot be an injected error operator, because such an
                // operator would not have been injected when the right-hand
                // side operand is missing.
                REALM_ASSERT(slot.type != OperSlot::Type::error);
                if (slot.type == OperSlot::Type::normal) {
                    if (!context.missing_operand(slot.oper, Side::right, slot.loc)) // Throws
                        return false;
                }
                else {
                    if (!context.empty_parentheses(loc)) // Throws
                        return false;
                }
            }
            m_value_stack.emplace_back(ErrorTag{}); // Throws
            goto have_operand_2;
        case Token::end_of_input:
            if (m_operator_stack.empty()) {
                // Assume `loc` was set to point to end of input
                if (!context.empty_input(loc)) // Throws
                    return false;
            }
            else {
                const OperSlot& slot = m_operator_stack.back();
                // This cannot be an injected error operator, because such an
                // operator would not have been injected when the right-hand
                // side operand is missing.
                REALM_ASSERT(slot.type != OperSlot::Type::error);
                if (slot.type == OperSlot::Type::normal)
                    if (!context.missing_operand(slot.oper, Side::right, slot.loc)) // Throws
                        return false;
                // If the operator slot contains a left parenthesis,
                // unmatched_paren() will be called later.
            }
            m_value_stack.emplace_back(ErrorTag{}); // Throws
            goto end_of_input;
    }
    REALM_ASSERT(false);
    return false;

  have_operand:
    if (!context.next_token(token, value, oper, loc)) // Throws
        return false;

  have_operand_2:
    switch (token) {
        case Token::error:
            goto missing_operator_2;
        case Token::value:
            goto missing_operator;
        case Token::oper:
            if (!context.is_prefix_operator(oper)) { // infix or postfix
                while (!m_operator_stack.empty()) {
                    OperSlot& slot = m_operator_stack.back();
                    if (slot.type == OperSlot::Type::left_paren)
                        break;
                    if (slot.type == OperSlot::Type::error)
                        break;
                    const Operator& left_oper  = slot.oper;
                    const Operator& right_oper = oper;
                    int left_prec  = context.get_precedence_level(left_oper);
                    int right_prec = context.get_precedence_level(right_oper);
                    if (left_prec < right_prec)
                        break;
                    if (left_prec == right_prec) {
                        int precedence = left_prec;
                        if (context.is_prec_right_associative(precedence))
                            break;
                    }
                    OperSlot slot_2 = std::move(slot);
                    m_operator_stack.pop_back();
                    perform_prefix_or_infix_oper(std::move(slot_2.oper), slot_2.loc,
                                                 context); // Throws
                }
                if (context.is_postfix_operator(oper)) {
                    if (!perform_prefix_or_postfix_oper(std::move(oper), loc, context)) // Throws
                        return false;
                    goto have_operand;
                }
                m_operator_stack.emplace_back(std::move(oper), loc); // Throws
                goto want_operand;
            }
            goto missing_operator;
        case Token::left_paren:
            goto missing_operator;
        case Token::right_paren:
            for (;;) {
                if (m_operator_stack.empty()) {
                    if (!context.unmatched_paren(Side::right, loc)) // Throws
                        return false;
                    break;
                }
                OperSlot slot = std::move(m_operator_stack.back());
                m_operator_stack.pop_back();
                if (slot.type == OperSlot::Type::left_paren)
                    break;
                if (slot.type == OperSlot::Type::error) {
                    perform_error_operation();
                    continue;
                }
                perform_prefix_or_infix_oper(std::move(slot.oper), slot.loc, context); // Throws
            }
            goto have_operand;
        case Token::end_of_input:
            goto end_of_input;
    }
    REALM_ASSERT(false);
    return false;

  missing_operator:
    REALM_ASSERT(!m_value_stack.empty());
    {
        const ValueSlot& slot = m_value_stack.back();
        if (!slot.error) {
            if (!context.missing_operator_before(slot.value, loc)) // Throws
                return false;
        }
    }
  missing_operator_2:
    m_operator_stack.emplace_back(ErrorTag{}); // Throws
    goto want_operand_2;

  end_of_input:
    while (!m_operator_stack.empty()) {
        OperSlot slot = std::move(m_operator_stack.back());
        m_operator_stack.pop_back();
        if (slot.type == OperSlot::Type::left_paren) {
            if (!context.unmatched_paren(Side::left, slot.loc)) // Throws
                return false;
            continue;
        }
        if (slot.type == OperSlot::Type::error) {
            perform_error_operation();
            continue;
        }
        perform_prefix_or_infix_oper(std::move(slot.oper), slot.loc, context); // Throws
    }

    REALM_ASSERT(!m_value_stack.empty());
    ValueSlot slot = std::move(m_value_stack.back());
    m_value_stack.pop_back();
    if (slot.error)
        return false;
    if (!context.check_result(slot.value, loc)) // Throws
        return false;
    result = std::move(slot.value);
    return true;
}

template<class Context>
inline bool ShuntingYardParser<Context>::perform_prefix_or_infix_oper(Operator oper,
                                                                      const Location& loc,
                                                                      Context& context)
{
    if (context.is_prefix_operator(oper))
        return perform_prefix_or_postfix_oper(std::move(oper), loc, context); // Throws
    REALM_ASSERT(m_value_stack.size() >= 2);
    ValueSlot right = std::move(m_value_stack.back());
    m_value_stack.pop_back();
    ValueSlot left = std::move(m_value_stack.back());
    m_value_stack.pop_back();
    if (left.error || left.error) {
      error:
        m_value_stack.emplace_back(ErrorTag{}); // Throws
        return true;
    }
    Value result = Value{};
    bool error = false;
    if (!context.perform_binop(std::move(oper), loc, std::move(left.value), std::move(right.value),
                               result, error)) // Throws
        return false;
    if (error)
        goto error;
    m_value_stack.emplace_back(std::move(result)); // Throws
    return true;
}

template<class Context>
inline bool ShuntingYardParser<Context>::perform_prefix_or_postfix_oper(Operator oper,
                                                                        const Location& loc,
                                                                        Context& context)
{
    REALM_ASSERT(!m_value_stack.empty());
    ValueSlot slot = std::move(m_value_stack.back());
    m_value_stack.pop_back();
    if (slot.error) {
      error:
        m_value_stack.emplace_back(ErrorTag{}); // Throws
        return true;
    }
    Value result = Value{};
    bool error = false;
    if (!context.perform_unop(std::move(oper), loc, std::move(slot.value),
                              result, error)) // Throws
        return false;
    if (error)
        goto error;
    m_value_stack.emplace_back(std::move(result)); // Throws
    return true;
}

template<class Context> inline void ShuntingYardParser<Context>::perform_error_operation() noexcept
{
    REALM_ASSERT(m_value_stack.size() >= 2);
    m_value_stack.pop_back();
    m_value_stack.pop_back();
    m_value_stack.emplace_back(ErrorTag{}); // Throws
}

template<class Context> inline void ShuntingYardParser<Context>::clear_stacks() noexcept
{
    m_value_stack.clear();
    m_operator_stack.clear();
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_SHUNTING_YARD_PARSER_HPP
