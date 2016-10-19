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

#ifndef REALM_UTIL_LOGGER_HPP
#define REALM_UTIL_LOGGER_HPP

#include <cstring>
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


/// All Logger objects store a reference to a LevelThreshold object which it
/// uses to efficiently query about the current log level threshold
/// (`level_threshold.get()`). All messages logged with a level that is lower
/// than the current threshold will be dropped. For the sake of efficiency, this
/// test happens before the message is formatted.
///
/// A logger is not inherently thread-safe, but specific implementations can be
/// (see ThreadSafeLogger). For a logger to be thread-safe, the implementation
/// of do_log() must be thread-safe and the referenced LevelThreshold object
/// must have a thread-safe get() method.
///
/// Examples:
///
///    logger.error("Overlong message from master coordinator");
///    logger.info("Listening for peers on %1:%2", listen_address, listen_port);
class Logger {
public:
    template <class... Params>
    void trace(const char* message, Params...);
    template <class... Params>
    void debug(const char* message, Params...);
    template <class... Params>
    void detail(const char* message, Params...);
    template <class... Params>
    void info(const char* message, Params...);
    template <class... Params>
    void warn(const char* message, Params...);
    template <class... Params>
    void error(const char* message, Params...);
    template <class... Params>
    void fatal(const char* message, Params...);

    /// Specifies criticality when passed to log(). Functions as a criticality
    /// threshold when returned from LevelThreshold::get().
    ///
    ///     error   Be silent unless when there is an error.
    ///     warn    Be silent unless when there is an error or a warning.
    ///     info    Reveal information about what is going on, but in a
    ///             minimalistic fashion to avoid general overhead from logging
    ///             and to keep volume down.
    ///     detail  Same as 'info', but prioritize completeness over minimalism.
    ///     debug   Reveal information that can aid debugging, no longer paying
    ///             attention to efficiency.
    ///     trace   A version of 'debug' that allows for very high volume
    ///             output.
    enum class Level { all, trace, debug, detail, info, warn, error, fatal, off };

    template <class... Params>
    void log(Level, const char* message, Params...);

    /// Shorthand for `int(level) >= int(level_threshold.get())`.
    bool would_log(Level level) const noexcept;

    class LevelThreshold;

    const LevelThreshold& level_threshold;

    virtual ~Logger() noexcept;

protected:
    Logger(const LevelThreshold&) noexcept;

    static void do_log(Logger&, Level, std::string message);

    virtual void do_log(Level, std::string message) = 0;

    static const char* get_level_prefix(Level) noexcept;

private:
    struct State;
    template <class>
    struct Subst;

    template <class... Params>
    REALM_NOINLINE void do_log(Level, const char* message, Params...);
    void log_impl(State&);
    template <class Param, class... Params>
    void log_impl(State&, const Param&, Params...);
};

template <class C, class T>
std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>&, Logger::Level);

template <class C, class T>
std::basic_istream<C, T>& operator>>(std::basic_istream<C, T>&, Logger::Level&);

class Logger::LevelThreshold {
public:
    virtual Level get() const noexcept = 0;
};


/// A root logger that is not thread-safe and allows for the log level threshold
/// to be changed over time. The initial log level threshold is
/// Logger::Level::info.
class RootLogger : private Logger::LevelThreshold, public Logger {
public:
    void set_level_threshold(Level) noexcept;

protected:
    RootLogger();

private:
    Level m_level_threshold = Level::info;
    Level get() const noexcept override final;
};


/// A logger that writes to STDERR. This logger is not thread-safe.
///
/// Since this class is a RootLogger, it contains modifiable a log level
/// threshold.
class StderrLogger : public RootLogger {
protected:
    void do_log(Level, std::string) override final;
};


/// A logger that writes to a stream. This logger is not thread-safe.
///
/// Since this class is a RootLogger, it contains modifiable a log level
/// threshold.
class StreamLogger : public RootLogger {
public:
    explicit StreamLogger(std::ostream&) noexcept;

protected:
    void do_log(Level, std::string) override final;

private:
    std::ostream& m_out;
};


/// A logger that writes to a file. This logger is not thread-safe.
///
/// Since this class is a RootLogger, it contains modifiable a log level
/// threshold.
class FileLogger : public StreamLogger {
public:
    explicit FileLogger(std::string path);
    explicit FileLogger(util::File);

private:
    util::File m_file;
    util::File::Streambuf m_streambuf;
    std::ostream m_out;
};


/// A thread-safe logger. This logger ignores the level threshold of the base
/// logger. Instead, it introduces new a LevelThreshold object with a fixed
/// value to achieve thread safety.
class ThreadSafeLogger : private Logger::LevelThreshold, public Logger {
public:
    explicit ThreadSafeLogger(Logger& base_logger, Level = Level::info);

protected:
    void do_log(Level, std::string) override final;

private:
    const Level m_level_threshold; // Immutable for thread safety
    Logger& m_base_logger;
    Mutex m_mutex;
    Level get() const noexcept override final;
};


/// A logger that adds a fixed prefix to each message. This logger inherits the
/// LevelThreshold object of the specified base logger. This logger is
/// thread-safe if, and only if the base logger is thread-safe.
class PrefixLogger : public Logger {
public:
    PrefixLogger(std::string prefix, Logger& base_logger) noexcept;

protected:
    void do_log(Level, std::string) override final;

private:
    const std::string m_prefix;
    Logger& m_base_logger;
};


// Implementation

struct Logger::State {
    Logger::Level m_level;
    std::string m_message;
    std::string m_search;
    int m_param_num = 1;
    std::ostringstream m_formatter;
    std::locale m_locale = std::locale::classic();
    State(Logger::Level level, const char* s)
        : m_level(level)
        , m_message(s)
        , m_search(m_message)
    {
        m_formatter.imbue(m_locale);
    }
};

template <class T>
struct Logger::Subst {
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

template <class... Params>
inline void Logger::trace(const char* message, Params... params)
{
    log(Level::trace, message, params...); // Throws
}

template <class... Params>
inline void Logger::debug(const char* message, Params... params)
{
    log(Level::debug, message, params...); // Throws
}

template <class... Params>
inline void Logger::detail(const char* message, Params... params)
{
    log(Level::detail, message, params...); // Throws
}

template <class... Params>
inline void Logger::info(const char* message, Params... params)
{
    log(Level::info, message, params...); // Throws
}

template <class... Params>
inline void Logger::warn(const char* message, Params... params)
{
    log(Level::warn, message, params...); // Throws
}

template <class... Params>
inline void Logger::error(const char* message, Params... params)
{
    log(Level::error, message, params...); // Throws
}

template <class... Params>
inline void Logger::fatal(const char* message, Params... params)
{
    log(Level::fatal, message, params...); // Throws
}

template <class... Params>
inline void Logger::log(Level level, const char* message, Params... params)
{
    if (would_log(level))
        do_log(level, message, params...); // Throws
}

inline bool Logger::would_log(Level level) const noexcept
{
    return int(level) >= int(level_threshold.get());
}

inline Logger::~Logger() noexcept
{
}

inline Logger::Logger(const LevelThreshold& lt) noexcept
    : level_threshold(lt)
{
}

inline void Logger::do_log(Logger& logger, Level level, std::string message)
{
    logger.do_log(level, std::move(message));
}

template <class... Params>
void Logger::do_log(Level level, const char* message, Params... params)
{
    State state(level, message);
    log_impl(state, params...);
}

inline void Logger::log_impl(State& state)
{
    do_log(state.m_level, std::move(state.m_message));
}

template <class Param, class... Params>
inline void Logger::log_impl(State& state, const Param& param, Params... params)
{
    Subst<Param>()(param, &state);
    log_impl(state, params...);
}

template <class C, class T>
std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>& out, Logger::Level level)
{
    switch (level) {
        case Logger::Level::all:
            out << "all";
            return out;
        case Logger::Level::trace:
            out << "trace";
            return out;
        case Logger::Level::debug:
            out << "debug";
            return out;
        case Logger::Level::detail:
            out << "detail";
            return out;
        case Logger::Level::info:
            out << "info";
            return out;
        case Logger::Level::warn:
            out << "warn";
            return out;
        case Logger::Level::error:
            out << "error";
            return out;
        case Logger::Level::fatal:
            out << "fatal";
            return out;
        case Logger::Level::off:
            out << "off";
            return out;
    }
    REALM_ASSERT(false);
    return out;
}

template <class C, class T>
std::basic_istream<C, T>& operator>>(std::basic_istream<C, T>& in, Logger::Level& level)
{
    std::basic_string<C, T> str;
    auto check = [&](const char* name) {
        size_t n = strlen(name);
        if (n != str.size())
            return false;
        for (size_t i = 0; i < n; ++i) {
            if (in.widen(name[i]) != str[i])
                return false;
        }
        return true;
    };
    if (in >> str) {
        if (check("all")) {
            level = Logger::Level::all;
        }
        else if (check("trace")) {
            level = Logger::Level::trace;
        }
        else if (check("debug")) {
            level = Logger::Level::debug;
        }
        else if (check("detail")) {
            level = Logger::Level::detail;
        }
        else if (check("info")) {
            level = Logger::Level::info;
        }
        else if (check("warn")) {
            level = Logger::Level::warn;
        }
        else if (check("error")) {
            level = Logger::Level::error;
        }
        else if (check("fatal")) {
            level = Logger::Level::fatal;
        }
        else if (check("off")) {
            level = Logger::Level::off;
        }
        else {
            in.setstate(std::ios_base::failbit);
        }
    }
    return in;
}

inline void RootLogger::set_level_threshold(Level new_level_threshold) noexcept
{
    m_level_threshold = new_level_threshold;
}

inline RootLogger::RootLogger()
    : Logger::LevelThreshold()
    , Logger(static_cast<Logger::LevelThreshold&>(*this))
{
}

inline Logger::Level RootLogger::get() const noexcept
{
    return m_level_threshold;
}

inline void StderrLogger::do_log(Level level, std::string message)
{
    std::cerr << get_level_prefix(level) << message << '\n'; // Throws
    std::cerr.flush();                                       // Throws
}

inline StreamLogger::StreamLogger(std::ostream& out) noexcept
    : m_out(out)
{
}

inline void StreamLogger::do_log(Level level, std::string message)
{
    m_out << get_level_prefix(level) << message << '\n'; // Throws
    m_out.flush();                                       // Throws
}

inline FileLogger::FileLogger(std::string path)
    : StreamLogger(m_out)
    , m_file(path, util::File::mode_Write) // Throws
    , m_streambuf(&m_file)                 // Throws
    , m_out(&m_streambuf)                  // Throws
{
}

inline FileLogger::FileLogger(util::File file)
    : StreamLogger(m_out)
    , m_file(std::move(file))
    , m_streambuf(&m_file) // Throws
    , m_out(&m_streambuf)  // Throws
{
}

inline ThreadSafeLogger::ThreadSafeLogger(Logger& base_logger, Level threshold)
    : Logger::LevelThreshold()
    , Logger(static_cast<Logger::LevelThreshold&>(*this))
    , m_level_threshold(threshold)
    , m_base_logger(base_logger)
{
}

inline void ThreadSafeLogger::do_log(Level level, std::string message)
{
    LockGuard l(m_mutex);
    Logger::do_log(m_base_logger, level, message); // Throws
}

inline Logger::Level ThreadSafeLogger::get() const noexcept
{
    return m_level_threshold;
}

inline PrefixLogger::PrefixLogger(std::string prefix, Logger& base_logger) noexcept
    : Logger(base_logger.level_threshold)
    , m_prefix(std::move(prefix))
    , m_base_logger(base_logger)
{
}

inline void PrefixLogger::do_log(Level level, std::string message)
{
    Logger::do_log(m_base_logger, level, m_prefix + message); // Throws
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_LOGGER_HPP
