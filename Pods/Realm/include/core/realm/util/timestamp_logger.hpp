/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2016] Realm Inc
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

#ifndef REALM_UTIL_TIMESTAMP_LOGGER_HPP
#define REALM_UTIL_TIMESTAMP_LOGGER_HPP

#include <realm/util/logger.hpp>
#include <realm/util/timestamp_formatter.hpp>


namespace realm {
namespace util {

class TimestampStderrLogger : public RootLogger {
public:
    using Precision = TimestampFormatter::Precision;
    using Config    = TimestampFormatter::Config;

    explicit TimestampStderrLogger(Config = {});

protected:
    void do_log(Logger::Level, std::string message) override;

private:
    TimestampFormatter m_formatter;
};


} // namespace util
} // namespace realm

#endif // REALM_UTIL_TIMESTAMP_LOGGER_HPP
