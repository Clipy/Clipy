/*************************************************************************
 *
 * Copyright 2018 Realm Inc.
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

#ifndef REALM_UTIL_BACKTRACE_HPP
#define REALM_UTIL_BACKTRACE_HPP

#include <string>
#include <iosfwd>
#include <stdexcept>

namespace realm {
namespace util {

/// Backtrace encapsulates a stack trace, usually as captured by `backtrace()`
/// and `backtrace_symbols()` (or platform-specific equivalents).
struct Backtrace {
    /// Capture a symbolicated stack trace, excluding the call to `capture()`
    /// itself. If any error occurs while capturing the stack trace or
    /// translating symbol names, a `Backtrace` object is returned containing a
    /// single line describing the error.
    ///
    /// This function only allocates memory as part of calling
    /// `backtrace_symbols()` (or the current platform's equivalent).
    static Backtrace capture() noexcept;

    /// Print the backtrace to the stream. Each line is separated by a newline.
    /// The format of the output is unspecified.
    void print(std::ostream&) const;

    /// Construct an empty stack trace.
    Backtrace() noexcept
    {
    }

    /// Move constructor. This operation cannot fail.
    Backtrace(Backtrace&&) noexcept;

    /// Copy constructor. See the copy assignment operator.
    Backtrace(const Backtrace&) noexcept;

    ~Backtrace();

    /// Move assignment operator. This operation cannot fail.
    Backtrace& operator=(Backtrace&&) noexcept;

    /// Copy assignment operator. Copying a `Backtrace` object may result in a
    /// memory allocation. If such an allocation fails, the backtrace is
    /// replaced with a single line describing the error.
    Backtrace& operator=(const Backtrace&) noexcept;

private:
    Backtrace(void* memory, const char* const* strs, size_t len)
        : m_memory(memory)
        , m_strs(strs)
        , m_len(len)
    {
    }
    Backtrace(void* memory, size_t len)
        : m_memory(memory)
        , m_strs(static_cast<char* const*>(memory))
        , m_len(len)
    {
    }

    // m_memory is a pointer to the memory block returned by
    // `backtrace_symbols()`. It is usually equal to `m_strs`, except in the
    // case where an error has occurred and `m_strs` points to statically
    // allocated memory describing the error.
    //
    // When `m_memory` is non-null, the memory is owned by this object.
    void* m_memory = nullptr;

    // A pointer to a list of string pointers describing the stack trace (same
    // format as returned by `backtrace_symbols()`).
    const char* const* m_strs = nullptr;

    // Number of entries in this stack trace.
    size_t m_len = 0;
};

namespace detail {

class ExceptionWithBacktraceBase {
public:
    ExceptionWithBacktraceBase()
        : m_backtrace(util::Backtrace::capture())
    {
    }
    const util::Backtrace& backtrace() const noexcept
    {
        return m_backtrace;
    }
    virtual const char* message() const noexcept = 0;

protected:
    util::Backtrace m_backtrace;
    // Cannot use Optional here, because Optional wants to use
    // ExceptionWithBacktrace.
    mutable bool m_has_materialized_message = false;
    mutable std::string m_materialized_message;

    // Render the message and the backtrace into m_message_with_backtrace. If an
    // exception is thrown while rendering the message, the message without the
    // backtrace will be returned.
    const char* materialize_message() const noexcept;
};

} // namespace detail

/// Base class for exceptions that record a stack trace of where they were
/// thrown.
///
/// The template argument is expected to be an exception type conforming to the
/// standard library exception API (`std::exception` and friends).
///
/// It is possible to opt in to exception backtraces in two ways, (a) as part of
/// the exception type, in which case the backtrace will always be included for
/// all exceptions of that type, or (b) at the call-site of an opaque exception
/// type, in which case it is up to the throw-site to decide whether a backtrace
/// should be included.
///
/// Example (a):
/// ```
///     class MyException : ExceptionWithBacktrace<std::exception> {
///     public:
///         const char* message() const noexcept override
///         {
///             return "MyException error message";
///         }
///     };
///
///     ...
///
///     try {
///         throw MyException{};
///     }
///     catch (const MyException& ex) {
///         // Print the backtrace without the message:
///         std::cerr << ex.backtrace() << "\n";
///         // Print the exception message and the backtrace:
///         std::cerr << ex.what() << "\n";
///         // Print the exception message without the backtrace:
///         std::cerr << ex.message() << "\n";
///     }
/// ```
///
/// Example (b):
/// ```
///     class MyException : std::exception {
///     public:
///         const char* what() const noexcept override
///         {
///             return "MyException error message";
///         }
///     };
///
///     ...
///
///     try {
///         throw ExceptionWithBacktrace<MyException>{};
///     }
///     catch (const MyException& ex) {
///         // Print the exception message and the backtrace:
///         std::cerr << ex.what() << "\n";
///     }
/// ```
template <class Base = std::runtime_error>
class ExceptionWithBacktrace : public Base, public detail::ExceptionWithBacktraceBase {
public:
    template <class... Args>
    inline ExceptionWithBacktrace(Args&&... args)
        : Base(std::forward<Args>(args)...)
        , detail::ExceptionWithBacktraceBase() // backtrace captured here
    {
    }

    /// Return the message of the exception, including the backtrace of where
    /// the exception was thrown.
    const char* what() const noexcept final
    {
        return materialize_message();
    }

    /// Return the message of the exception without the backtrace. The default
    /// implementation calls `Base::what()`.
    const char* message() const noexcept override
    {
        return Base::what();
    }
};

// Wrappers for standard exception types with backtrace support
using runtime_error = ExceptionWithBacktrace<std::runtime_error>;
using range_error = ExceptionWithBacktrace<std::range_error>;
using overflow_error = ExceptionWithBacktrace<std::overflow_error>;
using underflow_error = ExceptionWithBacktrace<std::underflow_error>;
using bad_alloc = ExceptionWithBacktrace<std::bad_alloc>;
using invalid_argument = ExceptionWithBacktrace<std::invalid_argument>;
using out_of_range = ExceptionWithBacktrace<std::out_of_range>;
using logic_error = ExceptionWithBacktrace<std::logic_error>;

} // namespace util
} // namespace realm

inline std::ostream& operator<<(std::ostream& os, const realm::util::Backtrace& bt)
{
    bt.print(os);
    return os;
}

#endif // REALM_UTIL_BACKTRACE_HPP
