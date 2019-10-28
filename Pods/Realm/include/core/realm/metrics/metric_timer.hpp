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

#ifndef REALM_METRIC_TIMER_HPP
#define REALM_METRIC_TIMER_HPP

#include <realm/util/features.h>

#include <chrono>
#include <memory>
#include <ostream>

namespace realm {
namespace metrics {

using nanosecond_storage_t = int64_t;

class MetricTimerResult
{
public:
    MetricTimerResult();
    ~MetricTimerResult();
    nanosecond_storage_t get_elapsed_nanoseconds() const;
    void report_nanoseconds(nanosecond_storage_t time);
protected:
    nanosecond_storage_t m_elapsed_nanoseconds;
};


class MetricTimer {
public:
    MetricTimer(std::shared_ptr<MetricTimerResult> destination = nullptr);
    ~MetricTimer();

    void reset();

    /// Returns elapsed time in nanoseconds since last call to reset().
    nanosecond_storage_t get_elapsed_nanoseconds() const;
    /// Same as get_elapsed_time().
    operator nanosecond_storage_t() const;

    /// Format the elapsed time on the form 0h00m, 00m00s, 00.00s, or
    /// 000.0ms depending on magnitude.
    static void format(nanosecond_storage_t nanoseconds, std::ostream&);

    static std::string format(nanosecond_storage_t nanoseconds);

private:
    using clock_type = std::chrono::high_resolution_clock;
    using time_point = std::chrono::time_point<clock_type>;
    time_point m_start;
    time_point m_paused_at;
    std::shared_ptr<MetricTimerResult> m_dest;

    time_point get_timer_ticks() const;
    nanosecond_storage_t calc_elapsed_nanoseconds(time_point begin, time_point end) const;
};


inline void MetricTimer::reset()
{
    m_start = get_timer_ticks();
}

inline nanosecond_storage_t MetricTimer::get_elapsed_nanoseconds() const
{
    return calc_elapsed_nanoseconds(m_start, get_timer_ticks());
}

inline MetricTimer::operator nanosecond_storage_t() const
{
    return get_elapsed_nanoseconds();
}

inline std::ostream& operator<<(std::ostream& out, const MetricTimer& timer)
{
    MetricTimer::format(timer, out);
    return out;
}


} // namespace metrics
} // namespace realm

#endif // REALM_METRIC_TIMER_HPP
