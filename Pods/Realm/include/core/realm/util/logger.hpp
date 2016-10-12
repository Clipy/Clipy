/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2015] Realm Inc
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
#ifndef REALM_UTIL_LOGGER_HPP
#define REALM_UTIL_LOGGER_HPP

#include <utility>
#include <string>
#include <locale>
#include <sstream>
#include <iostream>

#include <realm/util/features.h>
#include <realm/util/tuple.hpp>
#include <realm/util/thread.hpp>
#include <realm/util/file.hpp>

namespace realm {
namespace util {


/// Examples:
///
///    logger.log("Overlong message from master coordinator");
///    logger.log("Listening for peers on %1:%2", listen_address, listen_port);
class Logger {
public:
    template<class... Params> void log(const char* message, Params...);

    Logger() noexcept;
    virtual ~Logger() noexcept;

protected:
    static void do_log(Logger&, std::string message);

    virtual void do_log(std::string message) = 0;

private:
    struct State;
    template<class> struct Subst;

    void log_impl(State&);
    template<class Param, class... Params> void log_impl(State&, const Param&, Params...);
};



class StderrLogger: public Logger {
protected:
    void do_log(std::string) override;
};



class StreamLogger: public Logger {
public:
    explicit StreamLogger(std::ostream&) noexcept;

protected:
    void do_log(std::string) override;

private:
    std::ostream& m_out;
};



class FileLogger: public StreamLogger {
public:
    explicit FileLogger(std::string path);
    explicit FileLogger(util::File);

private:
    util::File m_file;
    util::File::Streambuf m_streambuf;
    std::ostream m_out;
};



/// This makes Logger::log() thread-safe.
class ThreadSafeLogger: public Logger {
public:
    explicit ThreadSafeLogger(Logger& base_logger);

protected:
    void do_log(std::string) override;

private:
    Logger& m_base_logger;
    Mutex m_mutex;
};



class PrefixLogger: public Logger {
public:
    PrefixLogger(std::string prefix, Logger& base_logger);

protected:
    void do_log(std::string) override;

private:
    const std::string m_prefix;
    Logger& m_base_logger;
};




// Implementation

struct Logger::State {
    std::string m_message;
    std::string m_search;
    int m_param_num = 1;
    std::ostringstream m_formatter;
    State(const char* s):
        m_message(s),
        m_search(m_message)
    {
        m_formatter.imbue(std::locale::classic());
    }
};

template<class T> struct Logger::Subst {
    void operator()(const T& param, State* state)
    {
        state->m_formatter << "%" << state->m_param_num;
        std::string key = state->m_formatter.str();
        state->m_formatter.str(std::string());
        std::string::size_type j = state->m_search.find(key);
        if (j != std::string::npos) {
            state->m_formatter << param;
            std::string str = state->m_formatter.str();
            state->m_formatter.str(std::string());
            state->m_message.replace(j, key.size(), str);
            state->m_search.replace(j, key.size(), std::string(str.size(), '\0'));
        }
        ++state->m_param_num;
    }
};

inline Logger::Logger() noexcept
{
}

inline Logger::~Logger() noexcept
{
}

template<class... Params>
inline void Logger::log(const char* message, Params... params)
{
    State state(message);
    log_impl(state, params...);
}

inline void Logger::do_log(Logger& logger, std::string message)
{
    logger.do_log(std::move(message));
}

inline void Logger::log_impl(State& state)
{
    do_log(std::move(state.m_message));
}

template<class Param, class... Params>
inline void Logger::log_impl(State& state, const Param& param, Params... params)
{
    Subst<Param>()(param, &state);
    log_impl(state, params...);
}

inline void StderrLogger::do_log(std::string message)
{
    std::cerr << message << '\n'; // Throws
    std::cerr.flush(); // Throws
}

inline StreamLogger::StreamLogger(std::ostream& out) noexcept:
    m_out(out)
{
}

inline void StreamLogger::do_log(std::string message)
{
    m_out << message << '\n'; // Throws
    m_out.flush(); // Throws
}

inline FileLogger::FileLogger(std::string path):
    StreamLogger(m_out),
    m_file(path, util::File::mode_Write), // Throws
    m_streambuf(&m_file), // Throws
    m_out(&m_streambuf) // Throws
{
}

inline FileLogger::FileLogger(util::File file):
    StreamLogger(m_out),
    m_file(std::move(file)),
    m_streambuf(&m_file), // Throws
    m_out(&m_streambuf) // Throws
{
}

inline ThreadSafeLogger::ThreadSafeLogger(Logger& base_logger):
    m_base_logger(base_logger)
{
}

inline void ThreadSafeLogger::do_log(std::string message)
{
    LockGuard l(m_mutex);
    Logger::do_log(m_base_logger, message); // Throws
}

inline PrefixLogger::PrefixLogger(std::string prefix, Logger& base_logger):
    m_prefix(std::move(prefix)), // Throws
    m_base_logger(base_logger)
{
}

inline void PrefixLogger::do_log(std::string message)
{
    Logger::do_log(m_base_logger, m_prefix + message); // Throws
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_LOGGER_HPP
