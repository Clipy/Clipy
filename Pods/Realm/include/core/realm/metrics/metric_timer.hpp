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

#if REALM_METRICS

namespace realm {
namespace metrics {


class MetricTimerResult
{
public:
    MetricTimerResult();
    ~MetricTimerResult();
    double get_elapsed_seconds() const;
    void report_seconds(double time);
protected:
    double m_elapsed_seconds;
};


class MetricTimer {
public:
    MetricTimer(std::shared_ptr<MetricTimerResult> destination = nullptr);
    ~MetricTimer();

    void reset();

    /// Returns elapsed time in seconds since last call to reset().
    double get_elapsed_time() const;
    /// Same as get_elapsed_time().
    operator double() const;

    /// Format the elapsed time on the form 0h00m, 00m00s, 00.00s, or
    /// 000.0ms depending on magnitude.
    static void format(double seconds, std::ostream&);

    static std::string format(double seconds);

private:
    using clock_type = std::chrono::high_resolution_clock;
    using time_point = std::chrono::time_point<clock_type>;
    time_point m_start;
    time_point m_paused_at;
    std::shared_ptr<MetricTimerResult> m_dest;

    time_point get_timer_ticks() const;
    double calc_elapsed_seconds(time_point begin, time_point end) const;
};


inline void MetricTimer::reset()
{
    m_start = get_timer_ticks();
}

inline double MetricTimer::get_elapsed_time() const
{
    return calc_elapsed_seconds(m_start, get_timer_ticks());
}

inline MetricTimer::operator double() const
{
    return get_elapsed_time();
}

inline std::ostream& operator<<(std::ostream& out, const MetricTimer& timer)
{
    MetricTimer::format(timer, out);
    return out;
}


} // namespace metrics
} // namespace realm

#endif // REALM_METRICS

#endif // REALM_METRIC_TIMER_HPP
