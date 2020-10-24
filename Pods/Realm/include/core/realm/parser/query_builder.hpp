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

#ifndef REALM_QUERY_BUILDER_HPP
#define REALM_QUERY_BUILDER_HPP

#include <string>
#include <memory>
#include <vector>

#include <realm/binary_data.hpp>
#include <realm/parser/keypath_mapping.hpp>
#include <realm/null.hpp>
#include <realm/string_data.hpp>
#include <realm/timestamp.hpp>
#include <realm/table.hpp>
#include <realm/util/any.hpp>
#include <realm/util/string_buffer.hpp>

namespace realm {
class Query;
class Realm;
class Table;
template<typename> class BasicRowExpr;
using RowExpr = BasicRowExpr<Table>;

namespace parser {
    struct Predicate;
    struct DescriptorOrderingState;
}

namespace query_builder {
class Arguments;

void apply_predicate(Query& query, const parser::Predicate& predicate, Arguments& arguments,
                     parser::KeyPathMapping mapping = parser::KeyPathMapping());

void apply_ordering(DescriptorOrdering& ordering, ConstTableRef target, const parser::DescriptorOrderingState& state,
                    Arguments& arguments, parser::KeyPathMapping mapping = parser::KeyPathMapping());
void apply_ordering(DescriptorOrdering& ordering, ConstTableRef target, const parser::DescriptorOrderingState& state,
                    parser::KeyPathMapping mapping = parser::KeyPathMapping());


struct AnyContext
{
    template<typename T>
    T unbox(const util::Any& wrapper) {
        return util::any_cast<T>(wrapper);
    }
    bool is_null(const util::Any& wrapper) {
        if (!wrapper.has_value()) {
            return true;
        }
        if (wrapper.type() == typeid(realm::null)) {
            return true;
        }
        return false;
    }
};

class Arguments {
public:
    virtual bool bool_for_argument(size_t argument_index) = 0;
    virtual long long long_for_argument(size_t argument_index) = 0;
    virtual float float_for_argument(size_t argument_index) = 0;
    virtual double double_for_argument(size_t argument_index) = 0;
    virtual StringData string_for_argument(size_t argument_index) = 0;
    virtual BinaryData binary_for_argument(size_t argument_index) = 0;
    virtual Timestamp timestamp_for_argument(size_t argument_index) = 0;
    virtual ObjKey object_index_for_argument(size_t argument_index) = 0;
    virtual bool is_argument_null(size_t argument_index) = 0;
    // dynamic conversion space with lifetime tied to this
    // it is used for storing literal binary/string data
    std::vector<util::StringBuffer> buffer_space;
};

template<typename ValueType, typename ContextType>
class ArgumentConverter : public Arguments {
public:
    ArgumentConverter(ContextType& context, const ValueType* arguments, size_t count)
    : m_ctx(context)
    , m_arguments(arguments)
    , m_count(count)
    {}

    bool bool_for_argument(size_t i) override { return get<bool>(i); }
    long long long_for_argument(size_t i) override { return get<int64_t>(i); }
    float float_for_argument(size_t i) override { return get<float>(i); }
    double double_for_argument(size_t i) override { return get<double>(i); }
    StringData string_for_argument(size_t i) override { return get<StringData>(i); }
    BinaryData binary_for_argument(size_t i) override { return get<BinaryData>(i); }
    Timestamp timestamp_for_argument(size_t i) override { return get<Timestamp>(i); }
    ObjKey object_index_for_argument(size_t i) override
    {
        return get<ObjKey>(i);
    }
    bool is_argument_null(size_t i) override { return m_ctx.is_null(at(i)); }

private:
    ContextType& m_ctx;
    const ValueType* m_arguments;
    size_t m_count;

    const ValueType& at(size_t index) const
    {
        if (index >= m_count) {
            std::string error_message;
            if (m_count) {
                error_message = util::format("Request for argument at index %1 but only %2 argument%3 provided",
                                             index, m_count, m_count == 1 ? " is" : "s are");
            }
            else {
                error_message = util::format("Request for argument at index %1 but no arguments are provided", index);
            }
            throw std::out_of_range(error_message);
        }
        return m_arguments[index];
    }

    template<typename T>
    T get(size_t index) const
    {
        return m_ctx.template unbox<T>(at(index));
    }
};

class NoArgsError : public std::runtime_error {
public:
    NoArgsError()
        : std::runtime_error("Attempt to retreive an argument when no arguments were given")
    {
    }
};

class NoArguments : public Arguments {
public:
    bool bool_for_argument(size_t)
    {
        throw NoArgsError();
    }
    long long long_for_argument(size_t)
    {
        throw NoArgsError();
    }
    float float_for_argument(size_t)
    {
        throw NoArgsError();
    }
    double double_for_argument(size_t)
    {
        throw NoArgsError();
    }
    StringData string_for_argument(size_t)
    {
        throw NoArgsError();
    }
    BinaryData binary_for_argument(size_t)
    {
        throw NoArgsError();
    }
    Timestamp timestamp_for_argument(size_t)
    {
        throw NoArgsError();
    }
    ObjKey object_index_for_argument(size_t)
    {
        throw NoArgsError();
    }
    bool is_argument_null(size_t)
    {
        throw NoArgsError();
    }
};

} // namespace query_builder
} // namespace realm

#endif // REALM_QUERY_BUILDER_HPP
