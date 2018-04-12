////////////////////////////////////////////////////////////////////////////
//
// Copyright 2015 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#ifndef REALM_COLLECTION_OPERATOR_EXPRESSION_HPP
#define REALM_COLLECTION_OPERATOR_EXPRESSION_HPP

#include "property_expression.hpp"
#include "parser.hpp"
#include "parser_utils.hpp"

#include <realm/query_expression.hpp>

namespace realm {
namespace parser {

template <typename RetType, parser::Expression::KeyPathOp AggOpType, class Enable = void>
struct CollectionOperatorGetter;

template <parser::Expression::KeyPathOp OpType>
struct CollectionOperatorExpression
{
    static constexpr parser::Expression::KeyPathOp operation_type = OpType;
    std::function<Table *()> table_getter;
    PropertyExpression pe;
    size_t post_link_col_ndx;
    DataType post_link_col_type;

    CollectionOperatorExpression(PropertyExpression&& exp, std::string suffix_path, parser::KeyPathMapping& mapping)
    : pe(std::move(exp))
    , post_link_col_ndx(realm::not_found)
    {
        table_getter = std::bind(&PropertyExpression::table_getter, pe);

        const bool requires_suffix_path = !(OpType == parser::Expression::KeyPathOp::SizeString
                                            || OpType == parser::Expression::KeyPathOp::SizeBinary
                                            || OpType == parser::Expression::KeyPathOp::Count);

        if (requires_suffix_path) {
            Table* pre_link_table = pe.table_getter();
            REALM_ASSERT(pre_link_table);
            StringData list_property_name;
            if (pe.dest_type_is_backlink()) {
                list_property_name = "linking object";
            } else {
                list_property_name = pre_link_table->get_column_name(pe.get_dest_ndx());
            }
            realm_precondition(pe.get_dest_type() == type_LinkList || pe.dest_type_is_backlink(),
                         util::format("The '%1' operation must be used on a list property, but '%2' is not a list",
                                      util::collection_operator_to_str(OpType), list_property_name));

            ConstTableRef post_link_table;
            if (pe.dest_type_is_backlink()) {
                post_link_table = pe.get_dest_table();
            } else {
                post_link_table = pe.get_dest_table()->get_link_target(pe.get_dest_ndx());
            }
            REALM_ASSERT(post_link_table);
            StringData printable_post_link_table_name = get_printable_table_name(*post_link_table);

            KeyPath suffix_key_path = key_path_from_string(suffix_path);

            realm_precondition(suffix_path.size() > 0 && suffix_key_path.size() > 0,
                         util::format("A property from object '%1' must be provided to perform operation '%2'",
                                      printable_post_link_table_name, util::collection_operator_to_str(OpType)));
            size_t index = 0;
            KeyPathElement element = mapping.process_next_path(post_link_table, suffix_key_path, index);

            realm_precondition(suffix_key_path.size() == 1,
                         util::format("Unable to use '%1' because collection aggreate operations are only supported "
                                      "for direct properties at this time", suffix_path));

            post_link_col_ndx = element.col_ndx;
            post_link_col_type = element.col_type;
        }
        else {  // !requires_suffix_path
            post_link_col_type = pe.get_dest_type();

            realm_precondition(suffix_path.empty(),
                         util::format("An extraneous property '%1' was found for operation '%2'",
                                      suffix_path, util::collection_operator_to_str(OpType)));
        }
    }
    template <typename T>
    auto value_of_type_for_query() const
    {
        return CollectionOperatorGetter<T, OpType>::convert(*this);
    }
};


// Certain operations are disabled for some types (eg. a sum of timestamps is invalid).
// The operations that are supported have a specialisation with std::enable_if for that type below
// any type/operation combination that is not specialised will get the runtime error from the following
// default implementation. The return type is just a dummy to make things compile.
template <typename RetType, parser::Expression::KeyPathOp AggOpType, class Enable>
struct CollectionOperatorGetter {
    static Columns<RetType> convert(const CollectionOperatorExpression<AggOpType>& op) {
        throw std::runtime_error(util::format("Predicate error: comparison of type '%1' with result of '%2' is not supported.",
                                              type_to_str<RetType>(),
                                              collection_operator_to_str(op.operation_type)));
    }
};

template <typename RetType>
struct CollectionOperatorGetter<RetType, parser::Expression::KeyPathOp::Min,
typename std::enable_if_t<realm::is_any<RetType, Int, Float, Double>::value> >{
    static SubColumnAggregate<RetType, aggregate_operations::Minimum<RetType> > convert(const CollectionOperatorExpression<parser::Expression::KeyPathOp::Min>& expr)
    {
        if (expr.pe.dest_type_is_backlink()) {
            return expr.table_getter()->template column<Link>(*expr.pe.get_dest_table(), expr.pe.get_dest_ndx()).template column<RetType>(expr.post_link_col_ndx).min();
        } else {
            return expr.table_getter()->template column<Link>(expr.pe.get_dest_ndx()).template column<RetType>(expr.post_link_col_ndx).min();
        }
    }
};

template <typename RetType>
struct CollectionOperatorGetter<RetType, parser::Expression::KeyPathOp::Max,
typename std::enable_if_t<realm::is_any<RetType, Int, Float, Double>::value> >{
    static SubColumnAggregate<RetType, aggregate_operations::Maximum<RetType> > convert(const CollectionOperatorExpression<parser::Expression::KeyPathOp::Max>& expr)
    {
        if (expr.pe.dest_type_is_backlink()) {
            return expr.table_getter()->template column<Link>(*expr.pe.get_dest_table(), expr.pe.get_dest_ndx()).template column<RetType>(expr.post_link_col_ndx).max();
        } else {
            return expr.table_getter()->template column<Link>(expr.pe.get_dest_ndx()).template column<RetType>(expr.post_link_col_ndx).max();
        }
    }
};

template <typename RetType>
struct CollectionOperatorGetter<RetType, parser::Expression::KeyPathOp::Sum,
typename std::enable_if_t<realm::is_any<RetType, Int, Float, Double>::value> >{
    static SubColumnAggregate<RetType, aggregate_operations::Sum<RetType> > convert(const CollectionOperatorExpression<parser::Expression::KeyPathOp::Sum>& expr)
    {
        if (expr.pe.dest_type_is_backlink()) {
            return expr.table_getter()->template column<Link>(*expr.pe.get_dest_table(), expr.pe.get_dest_ndx()).template column<RetType>(expr.post_link_col_ndx).sum();
        } else {
            return expr.table_getter()->template column<Link>(expr.pe.get_dest_ndx()).template column<RetType>(expr.post_link_col_ndx).sum();
        }
    }
};

template <typename RetType>
struct CollectionOperatorGetter<RetType, parser::Expression::KeyPathOp::Avg,
typename std::enable_if_t<realm::is_any<RetType, Int, Float, Double>::value> >{
    static SubColumnAggregate<RetType, aggregate_operations::Average<RetType> > convert(const CollectionOperatorExpression<parser::Expression::KeyPathOp::Avg>& expr)
    {
        if (expr.pe.dest_type_is_backlink()) {
            return expr.table_getter()->template column<Link>(*expr.pe.get_dest_table(), expr.pe.get_dest_ndx()).template column<RetType>(expr.post_link_col_ndx).average();
        } else {
            return expr.table_getter()->template column<Link>(expr.pe.get_dest_ndx()).template column<RetType>(expr.post_link_col_ndx).average();
        }
    }
};

template <typename RetType>
struct CollectionOperatorGetter<RetType, parser::Expression::KeyPathOp::Count,
    typename std::enable_if_t<realm::is_any<RetType, Int, Float, Double>::value> >{
    static LinkCount convert(const CollectionOperatorExpression<parser::Expression::KeyPathOp::Count>& expr)
    {
        if (expr.pe.dest_type_is_backlink()) {
            return expr.table_getter()->template column<Link>(*expr.pe.get_dest_table(), expr.pe.get_dest_ndx()).count();
        } else {
            return expr.table_getter()->template column<Link>(expr.pe.get_dest_ndx()).count();
        }
    }
};

template <>
struct CollectionOperatorGetter<Int, parser::Expression::KeyPathOp::SizeString>{
    static SizeOperator<Size<String> > convert(const CollectionOperatorExpression<parser::Expression::KeyPathOp::SizeString>& expr)
    {
        return expr.table_getter()->template column<String>(expr.pe.get_dest_ndx()).size();
    }
};

template <>
struct CollectionOperatorGetter<Int, parser::Expression::KeyPathOp::SizeBinary>{
    static SizeOperator<Size<Binary> > convert(const CollectionOperatorExpression<parser::Expression::KeyPathOp::SizeBinary>& expr)
    {
        return expr.table_getter()->template column<Binary>(expr.pe.get_dest_ndx()).size();
    }
};

} // namespace parser
} // namespace realm

#endif // REALM_COLLECTION_OPERATOR_EXPRESSION_HPP
