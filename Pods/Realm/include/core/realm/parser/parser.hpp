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

#ifndef REALM_PARSER_HPP
#define REALM_PARSER_HPP

#include <memory>
#include <string>
#include <vector>

namespace realm {

namespace parser {

struct Predicate;

struct Expression
{
    enum class Type { None, Number, String, KeyPath, Argument, True, False, Null, Timestamp, Base64, SubQuery } type;
    enum class KeyPathOp { None, Min, Max, Avg, Sum, Count, SizeString, SizeBinary, BacklinkCount } collection_op;
    std::string s;
    std::vector<std::string> time_inputs;
    std::string op_suffix;
    std::string subquery_path, subquery_var;
    std::shared_ptr<Predicate> subquery;
    Expression(Type t = Type::None, std::string input = "") : type(t), collection_op(KeyPathOp::None), s(input) {}
    Expression(std::vector<std::string>&& timestamp) : type(Type::Timestamp), collection_op(KeyPathOp::None), time_inputs(timestamp) {}
    Expression(std::string prefix, KeyPathOp op, std::string suffix) : type(Type::KeyPath), collection_op(op), s(prefix), op_suffix(suffix) {}
};

struct Predicate
{
    enum class Type
    {
        Comparison,
        Or,
        And,
        True,
        False
    } type = Type::And;

    enum class Operator
    {
        None,
        Equal,
        NotEqual,
        LessThan,
        LessThanOrEqual,
        GreaterThan,
        GreaterThanOrEqual,
        BeginsWith,
        EndsWith,
        Contains,
        Like,
        In
    };

    enum class OperatorOption
    {
        None,
        CaseInsensitive,
    };

    enum class ComparisonType
    {
        Unspecified,
        Any,
        All,
        None,
    };

    struct Comparison
    {
        Operator op = Operator::None;
        OperatorOption option = OperatorOption::None;
        Expression expr[2] = {{Expression::Type::None, ""}, {Expression::Type::None, ""}};
        ComparisonType compare_type = ComparisonType::Unspecified;
    };

    struct Compound
    {
        std::vector<Predicate> sub_predicates;
    };

    Comparison cmpr;
    Compound   cpnd;

    bool negate = false;

    Predicate(Type t, bool n = false) : type(t), negate(n) {}
};

struct DescriptorOrderingState
{
    struct PropertyState
    {
        std::string key_path;
        bool ascending;
    };
    struct SingleOrderingState
    {
        std::vector<PropertyState> properties;
        bool is_distinct;
    };
    std::vector<SingleOrderingState> orderings;
};

struct ParserResult
{
    ParserResult(Predicate p, DescriptorOrderingState o)
    : predicate(p)
    , ordering(o) {}
    Predicate predicate;
    DescriptorOrderingState ordering;
};

ParserResult parse(const std::string &query);

// run the analysis tool to check for cycles in the grammar
// returns the number of problems found and prints some info to std::cout
size_t analyze_grammar();
}
}

#endif // REALM_PARSER_HPP
