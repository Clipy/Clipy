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

#ifndef REALM_VALUE_EXPRESSION_HPP
#define REALM_VALUE_EXPRESSION_HPP

#include "parser.hpp"
#include "query_builder.hpp"

namespace realm {
namespace parser {

struct ValueExpression
{
    const parser::Expression* value;
    query_builder::Arguments* arguments;

    ValueExpression(query_builder::Arguments* args, const parser::Expression* v);
    bool is_null();
    template <typename RetType>
    RetType value_of_type_for_query();
};

} // namespace parser
} // namespace realm

#endif // REALM_VALUE_EXPRESSION_HPP
