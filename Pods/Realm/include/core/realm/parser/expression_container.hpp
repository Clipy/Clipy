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

#ifndef REALM_EXPRESSION_CONTAINER_HPP
#define REALM_EXPRESSION_CONTAINER_HPP

#include <realm/util/any.hpp>

#include "collection_operator_expression.hpp"
#include "parser.hpp"
#include "property_expression.hpp"
#include "query_builder.hpp"
#include "subquery_expression.hpp"
#include "value_expression.hpp"

namespace realm {
namespace parser {

class ExpressionContainer
{
public:
    ExpressionContainer(Query& query, const parser::Expression& e, query_builder::Arguments& args,
                        parser::KeyPathMapping& mapping);

    bool is_null();

    PropertyExpression& get_property();
    ValueExpression& get_value();
    CollectionOperatorExpression<parser::Expression::KeyPathOp::Min>& get_min();
    CollectionOperatorExpression<parser::Expression::KeyPathOp::Max>& get_max();
    CollectionOperatorExpression<parser::Expression::KeyPathOp::Sum>& get_sum();
    CollectionOperatorExpression<parser::Expression::KeyPathOp::Avg>& get_avg();
    CollectionOperatorExpression<parser::Expression::KeyPathOp::Count>& get_count();
    CollectionOperatorExpression<parser::Expression::KeyPathOp::BacklinkCount>& get_backlink_count();
    CollectionOperatorExpression<parser::Expression::KeyPathOp::SizeString>& get_size_string();
    CollectionOperatorExpression<parser::Expression::KeyPathOp::SizeBinary>& get_size_binary();
    SubqueryExpression& get_subexpression();

    DataType check_type_compatibility(DataType type);
    DataType get_comparison_type(ExpressionContainer& rhs);

    enum class ExpressionInternal {
        exp_Value,
        exp_Property,
        exp_OpMin,
        exp_OpMax,
        exp_OpSum,
        exp_OpAvg,
        exp_OpCount,
        exp_OpSizeString,
        exp_OpSizeBinary,
        exp_OpBacklinkCount,
        exp_SubQuery
    };

    ExpressionInternal type;
private:
    util::Any storage;
};

} // namespace parser
} // namespace realm

#endif // REALM_EXPRESSION_CONTAINER_HPP
