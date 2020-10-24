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

#ifndef REALM_PARSER_UTILS_HPP
#define REALM_PARSER_UTILS_HPP

#include <realm/data_type.hpp>
#include <realm/util/to_string.hpp>
#include "parser.hpp"

#include <sstream>
#include <stdexcept>

namespace realm {

class Table;
class Timestamp;
struct Link;

namespace util {

// check a precondition and throw an exception if it is not met
// this should be used iff the condition being false indicates a bug in the caller
// of the function checking its preconditions
#define realm_precondition(condition, message)                                                                       \
    if (!REALM_LIKELY(condition)) {                                                                                  \
        throw std::logic_error(message);                                                                             \
    }


template <typename T>
const char* type_to_str();

template <>
const char* type_to_str<bool>();
template <>
const char* type_to_str<Int>();
template <>
const char* type_to_str<Float>();
template <>
const char* type_to_str<Double>();
template <>
const char* type_to_str<String>();
template <>
const char* type_to_str<Binary>();
template <>
const char* type_to_str<Timestamp>();
template <>
const char* type_to_str<Link>();

const char* data_type_to_str(DataType type);
const char* collection_operator_to_str(parser::Expression::KeyPathOp op);
const char* comparison_type_to_str(parser::Predicate::ComparisonType type);

using KeyPath = std::vector<std::string>;
KeyPath key_path_from_string(const std::string &s);
std::string key_path_to_string(const KeyPath& keypath);
StringData get_printable_table_name(StringData name);
StringData get_printable_table_name(const Table& table);

template<typename T>
T stot(std::string const& s) {
    std::istringstream iss(s);
    T value;
    iss >> value;
    if (iss.fail()) {
        throw std::invalid_argument(util::format("Cannot convert string '%1'", s));
    }
    return value;
}

} // namespace utils
} // namespace realm

#endif // REALM_PARSER_UTILS_HPP
