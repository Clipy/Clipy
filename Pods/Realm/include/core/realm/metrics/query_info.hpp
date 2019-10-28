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

namespace realm {

class Query; // forward declare in namespace realm

namespace metrics {

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
    std::string get_table_name() const;
    QueryType get_type() const;
    nanosecond_storage_t get_query_time_nanoseconds() const;

    static std::unique_ptr<MetricTimer> track(const Query* query, QueryType type);
    static QueryType type_from_action(Action action);

private:
    std::string m_description;
    std::string m_table_name;
    QueryType m_type;
    std::shared_ptr<MetricTimerResult> m_query_time;
};

} // namespace metrics
} // namespace realm

#endif // REALM_QUERY_INFO_HPP
