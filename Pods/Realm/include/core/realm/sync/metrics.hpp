/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2012] Realm Inc
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of Realm Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Realm Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Realm Incorporated.
 *
 **************************************************************************/
#ifndef REALM_SYNC_METRICS_HPP
#define REALM_SYNC_METRICS_HPP

#if REALM_HAVE_DOGLESS
#  include <dogless.hpp>
#endif

namespace realm {
namespace sync {

// FIXME: Consider adding support for specification of sample rate. The Dogless
// API already supports this.
class Metrics {
public:
    /// Increment the counter identified by the specified key.
    virtual void increment(const char* key, int value = 1) = 0;

    /// Send the timing identified by the specified key.
    virtual void timing(const char* key, double value) = 0;

    /// Set value of the guage identified by the specified key.
    virtual void gauge(const char* key, double value) = 0;

    /// Add the specified value to the guage identified by the specified
    /// key. The value is allowed to be negative.
    virtual void gauge_relative(const char* key, double value) = 0;

    /// Allow the dogless library to send each metric to multiple endpoints, as
    /// required
    virtual void add_endpoint(const std::string& endpoint) = 0;

    virtual ~Metrics() {}
};

#if REALM_HAVE_DOGLESS

class DoglessMetrics: public sync::Metrics {
public:
    DoglessMetrics():
        m_dogless(dogless::hostname_prefix("realm")) // Throws
    {
        m_dogless.loop_interval(1);
    }

    void increment(const char* key, int value = 1) override
    {
        const char* metric = key;
        m_dogless.increment(metric, value); // Throws
    }

    void timing(const char* key, double value) override
    {
        const char* metric = key;
        m_dogless.timing(metric, value); // Throws
    }

    void gauge(const char* key, double value) override
    {
        const char* metric = key;
        m_gauges[key] = value;
        m_dogless.gauge(metric, m_gauges[key]); // Throws
    }

    void gauge_relative(const char* key, double value) override
    {
        const char* metric = key;
        m_gauges[key] += value;
        m_dogless.gauge(metric, m_gauges[key]); // Throws
    }

    void add_endpoint(const std::string& endpoint) override
    {
        m_dogless.add_endpoint(endpoint);
    }

private:
    dogless::BufferedStatsd m_dogless;
    std::map<std::string, double> m_gauges;
};

#endif

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_METRICS_HPP
