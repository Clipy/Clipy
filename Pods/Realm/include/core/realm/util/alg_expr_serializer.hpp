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

#ifndef REALM_UTIL_ALG_EXPR_SERIALIZE_HPP
#define REALM_UTIL_ALG_EXPR_SERIALIZE_HPP

#include <limits>

namespace realm {
namespace util {

class AlgExprSerializerBase {
public:
    enum class ParenMode { minimal, all };
};

/// Serialize an algebraic expressions that is represented as a tree whose inner
/// nodes represent operators, and whose leaf nodes represent values.
///
/// The `Context` type must define these types:
///
///  - `Node`, whose values represent nodes (inner nodes and leaf nodes) in the
///    expression tree. For example, it could be a pointer to a node object.
///  - `Operator`, whose values represent particular operators (prefix,
///    infix, and postfix). For example, it could be an index into a table of
///    operators.
///
/// The `Context` type must also define these functions:
///
///     bool expand_inner_or_serialize_leaf(Node, Operator& oper, Node& left, Node& right)
///     void output_operator(const Operator&)
///     void output_left_paren()
///     void output_left_paren()
///     bool is_prefix_operator(const Operator&) const noexcept
///     bool is_postfix_operator(const Operator&) const noexcept
///     int get_precedence_level(const Operator&) const noexcept
///     bool is_oper_associative(const Operator&) const noexcept
///     bool is_prec_right_associative(int precedence_level) const noexcept
///
/// For a leaf node, `expand_inner_or_serialize_leaf()` must output it and
/// return false. For an inner nodes, `expand_inner_or_serialize_leaf()` must
/// separate it into its constituent parts and return true. The operator
/// represented by the node must be assigned to `oper`. Additionally, one or two
/// operands must be extracted. For prefix operators,
/// `expand_inner_or_serialize_leaf()` need only update `right`. For postfix
/// operators, it need only update `left`. For binary infix operators, both
/// `left` and `right` must be updated.
///
/// FIXME: Replace with nonrecusive implementation.
template<class Context> class AlgExprSerializer: public AlgExprSerializerBase {
public:
    using Node     = typename Context::Node;
    using Operator = typename Context::Operator;

    void serialize(Node root, Context&, ParenMode paren_mode = ParenMode::minimal);

private:
    void do_serialize(Node, int parent_prec, Operator parent_oper, bool is_right_child);

    Context* m_context = nullptr;
    ParenMode m_paren_mode = ParenMode::minimal;
};




// Implementation

template<class Context>
inline void AlgExprSerializer<Context>::serialize(Node root, Context& context, ParenMode paren_mode)
{
    m_context = &context;
    m_paren_mode = paren_mode;
    int parent_prec = std::numeric_limits<int>::min(); // indicates that this is the root.
    Operator parent_oper = Operator{}; // Value is immaterial for the root.
    bool is_right_child  = false;      // Value is immaterial for the root.
    do_serialize(std::move(root), parent_prec, parent_oper, is_right_child); // Throws
}

template<class Context>
void AlgExprSerializer<Context>::do_serialize(Node node, int parent_prec, Operator parent_oper,
                                              bool is_right_child)
{
    Operator oper;
    Node left, right;
    if (m_context->expand_inner_or_serialize_leaf(std::move(node), oper, left, right)) {
        int prec = m_context->get_precedence_level(oper);
        bool elide_paren = false;
        if (m_paren_mode == ParenMode::minimal) {
            if (prec > parent_prec) {
                elide_paren = true;
            }
            else {
                // While processing the root, `parent_oper` and `is_right_child`
                // have no meaning. However, since `parent_prec` must be lower
                // than any real precedence level while processing the root, the
                // preceding check ensures that we only get to this point when
                // `parent_oper` and `is_right_child` have meaningful values.
                if (oper == parent_oper) {
                    if (m_context->is_oper_associative(oper))
                        elide_paren = true;
                }
                else if (prec == parent_prec) {
                    if (is_right_child == m_context->is_prec_right_associative(prec))
                        elide_paren = true;
                }
            }
        }
        if (!elide_paren)
            m_context->output_left_paren(); // Throws
        if (!m_context->is_prefix_operator(oper))
            do_serialize(std::move(left), prec, oper, false); // Throws
        m_context->output_operator(oper); // Throws
        if (!m_context->is_postfix_operator(oper))
            do_serialize(std::move(right), prec, oper, true); // Throws
        if (!elide_paren)
            m_context->output_right_paren(); // Throws
    }
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_ALG_EXPR_SERIALIZE_HPP
