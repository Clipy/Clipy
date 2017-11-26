/*************************************************************************
 *
 * Copyright 2016 Realm Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 **************************************************************************/

#ifndef REALM_QUERY_INFO_HPP
#define REALM_QUERY_INFO_HPP

#include <memory>
#include <string>
#include <sstream>

#include <realm/array.hpp>
#include <realm/util/features.h>
#include <realm/metrics/metric_timer.hpp>

#if REALM_METRICS

namespace realm {

class Query; // forward declare in namespace realm

namespace metrics {

template <typename T>
std::string print_value(T value)
{
    std::stringstream ss;
    ss << value;
    return ss.str();
}

const std::string value_separator = ".";

class QueryInfo {
public:

    enum QueryType {
        type_Find,
        type_FindAll,
        type_Count,
        type_Sum,
        type_Average,
        type_Maximum,
        type_Minimum,
        type_Invalid
    };

    QueryInfo(const Query* query, QueryType type);
    ~QueryInfo() noexcept;

    std::string get_description() const;
    QueryType get_type() const;
    double get_query_time() const;

    static std::unique_ptr<MetricTimer> track(const Query* query, QueryType type);
    static QueryType type_from_action(Action action);

private:
    std::string m_description;
    QueryType m_type;
    std::shared_ptr<MetricTimerResult> m_query_time;
};

} // namespace metrics
} // namespace realm

#else

namespace realm {
namespace metrics {

template <typename T>
std::string print_value(T)
{
    return "";
}

const std::string value_separator = ".";

} // end namespace metrics
} // end namespace realm

#endif // REALM_METRICS


#endif // REALM_QUERY_INFO_HPP
