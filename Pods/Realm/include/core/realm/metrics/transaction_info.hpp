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

#ifndef REALM_TRANSACTION_INFO_HPP
#define REALM_TRANSACTION_INFO_HPP

#include <memory>
#include <string>

#include <realm/metrics/metric_timer.hpp>
#include <realm/util/features.h>

#if REALM_METRICS

namespace realm {
namespace metrics {

class Metrics;

class TransactionInfo {
public:
    enum TransactionType {
        read_transaction,
        write_transaction
    };
    TransactionInfo(TransactionType type);
    TransactionInfo(const TransactionInfo&) = default;
    ~TransactionInfo() noexcept;

    TransactionType get_transaction_type() const;
    // the transaction time is a total amount which includes fsync_time + write_time + user_time
    double get_transaction_time() const;
    double get_fsync_time() const;
    double get_write_time() const;
    size_t get_disk_size() const;
    size_t get_free_space() const;
    size_t get_total_objects() const;
    size_t get_num_available_versions() const;

private:
    MetricTimerResult m_transaction_time;
    std::shared_ptr<MetricTimerResult> m_fsync_time;
    std::shared_ptr<MetricTimerResult> m_write_time;
    MetricTimer m_transact_timer;

    size_t m_realm_disk_size;
    size_t m_realm_free_space;
    size_t m_total_objects;
    TransactionType m_type;
    size_t m_num_versions;

    friend class Metrics;
    void update_stats(size_t disk_size, size_t free_space, size_t total_objects, size_t available_versions);
    void finish_timer();
};

} // namespace metrics
} // namespace realm

#endif // REALM_METRICS

#endif // REALM_TRANSACTION_INFO_HPP
