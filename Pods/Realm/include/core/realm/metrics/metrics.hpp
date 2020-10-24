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

#ifndef REALM_METRICS_HPP
#define REALM_METRICS_HPP

#include <memory>

#include <realm/metrics/query_info.hpp>
#include <realm/metrics/transaction_info.hpp>
#include <realm/util/features.h>
#include "realm/util/fixed_size_buffer.hpp"

namespace realm {

class Group;

namespace metrics {

class Metrics {
public:
    Metrics(size_t max_history_size);
    ~Metrics() noexcept;
    size_t num_query_metrics() const;
    size_t num_transaction_metrics() const;

    void add_query(QueryInfo info);
    void add_transaction(TransactionInfo info);

    void start_read_transaction();
    void start_write_transaction();
    void end_read_transaction(size_t total_size, size_t free_space, size_t num_objects, size_t num_versions,
                              size_t num_decrypted_pages);
    void end_write_transaction(size_t total_size, size_t free_space, size_t num_objects, size_t num_versions,
                               size_t num_decrypted_pages);
    static std::unique_ptr<MetricTimer> report_fsync_time(const Group& g);
    static std::unique_ptr<MetricTimer> report_write_time(const Group& g);

    using QueryInfoList = util::FixedSizeBuffer<QueryInfo>;
    using TransactionInfoList = util::FixedSizeBuffer<TransactionInfo>;

    // Get the list of metric objects tracked since the last take
    std::unique_ptr<QueryInfoList> take_queries();
    std::unique_ptr<TransactionInfoList> take_transactions();

private:
    std::unique_ptr<QueryInfoList> m_query_info;
    std::unique_ptr<TransactionInfoList> m_transaction_info;

    std::unique_ptr<TransactionInfo> m_pending_read;
    std::unique_ptr<TransactionInfo> m_pending_write;

    size_t m_max_num_queries;
    size_t m_max_num_transactions;
};

} // namespace metrics
} // namespace realm


#endif // REALM_METRICS_HPP
