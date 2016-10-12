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

#include <stddef.h>
#include <memory>
#include <functional>
#include <chrono>
#include <exception>
#include <string>
#include <vector>
#include <system_error>

#ifndef REALM_UTIL_EVENT_LOOP_HPP
#define REALM_UTIL_EVENT_LOOP_HPP

namespace realm {
namespace util {

class EventLoop;
class Socket;
class DeadlineTimer;


/// \brief Create an event loop using the default implementation
/// (EventLoop::Implementation::get_default()).
std::unique_ptr<EventLoop> make_event_loop();


/// Event Loops are an abstraction over asynchronous I/O.
///
/// The interface described by EventLoop is a "proactor pattern" approach to
/// asynchronous I/O. All operations are started with a completion handler,
/// which is invoked once the operation "completes", i.e. succeeds, fails, or is
/// cancelled.
///
/// In general, completion handlers are always invoked, regardless of whether or
/// not the operation was successful.
///
/// Most operations return an abstract handle through a smart pointer, which can
/// be used to cancel the operation or reschedule a new operation. In general,
/// if the handle (socket or timer) is destroyed and an operation is in
/// progress, the operation is cancelled.
///
/// Operations on an event-loop are generally **not thread-safe** (exceptions
/// are post(), stop(), and reset(), which are thread-safe).
///
/// \sa Socket
/// \sa DeadlineTimer
class EventLoop {
public:
    using PostCompletionHandler = std::function<void()>;

    virtual std::unique_ptr<Socket> make_socket() = 0;
    virtual std::unique_ptr<DeadlineTimer> make_timer() = 0;

    /// \brief Submit a handler to be executed by the event loop thread.
    ///
    /// Register the sepcified completion handler for immediate asynchronous
    /// execution. The specified handler object will be copied as necessary, and
    /// will be executed by an expression on the form `handler()`.
    ///
    /// This function is thread-safe, that is, it may be called by any
    /// thread. It may also be called from other completion handlers.
    ///
    /// The handler will never be called as part of the execution of post(). It
    /// will always be called by a thread that is executing run(). If no thread
    /// is currently executing run(), the handler will not be executed until a
    /// thread starts executing run(). If post() is called while another thread
    /// is executing run(), the handler may be called before post() returns. If
    /// post() is called from another completion handler, the submitted handler
    /// is guaranteed to not be called during the execution of post().
    ///
    /// Completion handlers added through post() will be executed in the order
    /// that they are added. More precisely, if post() is called twice to add
    /// two handlers, A and B, and the execution of post(A) ends before the
    /// beginning of the execution of post(B), then A is guaranteed to execute
    /// before B.
    virtual void post(PostCompletionHandler) = 0;

    /// \brief Execute the event loop.
    ///
    /// Execute completion handlers of completed asynchronous operations, or
    /// wait for more completion handlers to become ready for
    /// execution. Handlers submitted via post() are considered immeditely
    /// ready. If there are no completion handlers ready for execution, and
    /// there are no asynchronous operations in progress, run() returns.
    ///
    /// All completion handlers, including handlers submitted via post() will be
    /// executed from run(), that is by the thread that executes run(). If no
    /// thread executes run(), then the completion handlers will not be
    /// executed.
    ///
    /// Exceptions thrown by completion handlers will always propagate back
    /// through run().
    virtual void run() = 0;

    /// @{ \brief Stop event loop execution.
    ///
    /// stop() puts the event loop into the stopped mode. If a thread is currently
    /// executing run(), it will be made to return in a timely fashion, that is,
    /// without further blocking. If a thread is currently blocked in run(), it
    /// will be unblocked. Handlers that can be executed immediately, may, or
    /// may not be executed before run() returns, but new handlers submitted by
    /// these, will not be executed.
    ///
    /// The event loop will remain in the stopped mode until reset() is
    /// called. If reset() is called before run() returns, it may, or may not
    /// cause run() to continue normal operation without returning.
    ///
    /// Both stop() and reset() are thread-safe, that is, they may be called by
    /// any thread. Also, both of these function may be called from completion
    /// handlers (including posted handlers).
    virtual void stop() noexcept = 0;
    virtual void reset() noexcept = 0;
    /// @}

    virtual ~EventLoop() {}

    class Implementation;
};


enum class SocketSecurity {
    None,  ///< No socket security (cleartext).
    TLSv1, ///< Transport Layer Security v1 (encrypted).
};


/// Socket describes an event handler for socket operations.
///
/// It is also used to schedule individual I/O operations on a socket.
class Socket {
public:
    using port_type = uint_fast16_t;
    using ConnectCompletionHandler =
        std::function<void(std::error_code)>;
    using ReadCompletionHandler =
        std::function<void(std::error_code, size_t num_bytes_transferred)>;
    using WriteCompletionHandler =
        std::function<void(std::error_code, size_t num_bytes_transferred)>;

    /// \brief Perform an asynchronous connect operation.
    ///
    /// Initiate an asynchronous connect operation. The completion handler is
    /// called when the operation completes. The operation completes when the
    /// connection is established, or an error occurs.
    ///
    /// The completion handler is always executed by the event loop thread,
    /// i.e., by a thread that is executing EventLoop::run(). Conversely, the
    /// completion handler is guaranteed to not be called while no thread is
    /// executing EventLoop::run(). The execution of the completion handler is
    /// always deferred to the event loop, meaning that it never happens as a
    /// synchronous side effect of the execution of async_connect(), even when
    /// async_connect() is executed by the event loop thread. The completion
    /// handler is guaranteed to be called eventually, as long as there is time
    /// enough for the operation to complete or fail, and a thread is executing
    /// EventLoop::run() for long enough.
    ///
    /// The operation can be canceled by calling cancel(), and will be
    /// automatically canceled if the socket is closed. If the operation is
    /// canceled, it will fail with `error::operation_aborted`. The operation
    /// remains cancelable up until the point in time where the completion
    /// handler starts to execute. This means that if cancel() is called before
    /// the completion handler starts to execute, then the completion handler is
    /// guaranteed to have `error::operation_aborted` passed to it. This is true
    /// regardless of whether cancel() is called explicitly or implicitly, such
    /// as when the socket is destroyed.
    ///
    /// The specified handler object will be copied as necessary, and will be
    /// executed by an expression on the form `handler(ec)` where `ec` is the
    /// error code.
    ///
    /// It is an error to start a new connect operation while another connect
    /// operation is in progress. A connect operation is considered complete as
    /// soon as the completion handler starts to execute.
    virtual void async_connect(std::string host, port_type port, SocketSecurity security,
                               ConnectCompletionHandler handler) = 0;

    /// @{ \brief Perform an asynchronous read operation.
    ///
    /// Initiate an asynchronous buffered read operation. The completion handler
    /// will be called when the operation completes, or an error occurs.
    ///
    /// async_read() will continue reading until the specified buffer is full,
    /// or an error occurs. If the end of input is reached before the buffer is
    /// filled, the operation fails with `network::end_of_input`.
    ///
    /// async_read_until() will continue reading until the specified buffer
    /// contains the specified delimiter, or an error occurs. If the buffer is
    /// filled before a delimiter is found, the operation fails with
    /// `network::delim_not_found`. Otherwise, if the end of input is reached
    /// before a delimiter is found, the operation fails with
    /// `network::end_of_input`. Otherwise, if the operation succeeds, the last
    /// byte placed in the buffer is the delimiter.
    ///
    /// The completion handler is always executed by the event loop thread,
    /// i.e., by a thread that is executing EventLoop::run(). Conversely, the
    /// completion handler is guaranteed to not be called while no thread is
    /// executing EventLoop::run(). The execution of the completion handler is
    /// always deferred to the event loop, meaning that it never happens as a
    /// synchronous side effect of the execution of async_read() or
    /// async_read_until(), even when async_read() or async_read_until() is
    /// executed by the event loop thread. The completion handler is guaranteed
    /// to be called eventually, as long as there is time enough for the
    /// operation to complete or fail, and a thread is executing
    /// EventLoop::run() for long enough.
    ///
    /// The operation can be canceled by calling cancel(), and will be
    /// automatically canceled if the socket is closed. If the operation is
    /// canceled, it will fail with `error::operation_aborted`. The operation
    /// remains cancelable up until the point in time where the completion
    /// handler starts to execute. This means that if cancel() is called before
    /// the completion handler starts to execute, then the completion handler is
    /// guaranteed to have `error::operation_aborted` passed to it. This is true
    /// regardless of whether cancel() is called explicitly or implicitly, such
    /// as when the socket is destroyed.
    ///
    /// The specified handler object will be copied as necessary, and will be
    /// executed by an expression on the form `handler(ec, n)` where `ec` is the
    /// error code, and `n` is the number of bytes placed in the buffer. `n` is
    /// guaranteed to be less than, or equal to \a size.
    ///
    /// It is an error to start a read operation before the socket is connected.
    ///
    /// It is an error to start a new read operation while another read
    /// operation is in progress. A read operation is considered complete as
    /// soon as the completion handler starts executing. This means that a new
    /// read operation can be started from the completion handler of another
    /// read operation.
    virtual void async_read(char* buffer, size_t size, ReadCompletionHandler handler) = 0;
    virtual void async_read_until(char* buffer, size_t size, char delim,
                                  ReadCompletionHandler handler) = 0;
    /// @}

    /// \brief Perform an asynchronous write operation.
    ///
    /// Initiate an asynchronous write operation. The completion handler is
    /// called when the operation completes. The operation completes when all
    /// the specified bytes have been written to the socket, or an error occurs.
    ///
    /// The completion handler is always executed by the event loop thread,
    /// i.e., by a thread that is executing EventLoop::run(). Conversely, the
    /// completion handler is guaranteed to not be called while no thread is
    /// executing EventLoop::run(). The execution of the completion handler is
    /// always deferred to the event loop, meaning that it never happens as a
    /// synchronous side effect of the execution of async_write(), even when
    /// async_write() is executed by the event loop thread. The completion
    /// handler is guaranteed to be called eventually, as long as there is time
    /// enough for the operation to complete or fail, and a thread is executing
    /// EventLoop::run() for long enough.
    ///
    /// The operation can be canceled by calling cancel(), and will be
    /// automatically canceled if the socket is closed. If the operation is
    /// canceled, it will fail with `error::operation_aborted`. The operation
    /// remains cancelable up until the point in time where the completion
    /// handler starts to execute. This means that if cancel() is called before
    /// the completion handler starts to execute, then the completion handler is
    /// guaranteed to have `error::operation_aborted` passed to it. This is true
    /// regardless of whether cancel() is called explicitly or implicitly, such
    /// as when the socket is destroyed.
    ///
    /// The specified handler object will be copied as necessary, and will be
    /// executed by an expression on the form `handler(ec, n)` where `ec` is the
    /// error code, and `n` is the number of bytes written (of type `size_t`).
    ///
    /// It is an error to start a write operation before the socket is
    /// connected.
    ///
    /// It is an error to start a new write operation while another write
    /// operation is in progress. A write operation is considered complete as
    /// soon as the completion handler starts to execute. This means that a new
    /// write operation can be started from the completion handler of another
    /// write operation.
    virtual void async_write(const char* data, size_t size, WriteCompletionHandler handler) = 0;

    /// \brief Close this socket.
    ///
    /// If the socket is connected, it will be disconnected. If it is already
    /// disconnected (or never connected), this function does nothing
    /// (idempotency).
    ///
    /// A socket is automatically closed when destroyed.
    ///
    /// When the socket is closed, any incomplete asynchronous operation will be
    /// canceled (as if cancel() was called).
    virtual void close() noexcept = 0;

    /// \brief Cancel all asynchronous operations.
    ///
    /// Cause all incomplete asynchronous operations, that are associated with
    /// this socket, to fail with `error::operation_aborted`. An asynchronous
    /// operation is complete precisely when its completion handler starts
    /// executing.
    ///
    /// Completion handlers of canceled operations will become immediately ready
    /// to execute, but will never be executed directly as part of the execution
    /// of cancel().
    virtual void cancel() noexcept = 0;

    virtual EventLoop& get_event_loop() noexcept = 0;

    virtual ~Socket() {}
};


class DeadlineTimer {
public:
    using Duration = std::chrono::milliseconds;
    using WaitCompletionHandler = std::function<void(std::error_code)>;

    /// \brief Perform an asynchronous wait operation.
    ///
    /// Initiate an asynchronous wait operation. The completion handler becomes
    /// ready to execute when the expiration time is reached, or an error occurs
    /// (cancellation counts as an error here). The expiration time is the time
    /// of initiation plus the specified delay. The error code passed to the
    /// complition handler will **never** indicate success, unless the
    /// expiration time was reached.
    ///
    /// The completion handler is always executed by the event loop thread,
    /// i.e., by a thread that is executing EventLoop::run(). Conversely, the
    /// completion handler is guaranteed to not be called while no thread is
    /// executing EventLoop::run(). The execution of the completion handler is
    /// always deferred to the event loop, meaning that it never happens as a
    /// synchronous side effect of the execution of async_wait(), even when
    /// async_wait() is executed by the event loop thread. The completion
    /// handler is guaranteed to be called eventually, as long as there is time
    /// enough for the operation to complete or fail, and a thread is executing
    /// EventLoop::run() for long enough.
    ///
    /// The operation can be canceled by calling cancel(), and will be
    /// automatically canceled if the timer is destroyed. If the operation is
    /// canceled, it will fail with `error::operation_aborted`. The operation
    /// remains cancelable up until the point in time where the completion
    /// handler starts to execute. This means that if cancel() is called before
    /// the completion handler starts to execute, then the completion handler is
    /// guaranteed to have `error::operation_aborted` passed to it. This is true
    /// regardless of whether cancel() is called explicitly or implicitly, such
    /// as when the timer is destroyed.
    ///
    /// The specified handler object will be copied as necessary, and will be
    /// executed by an expression on the form `handler(ec)` where `ec` is the
    /// error code.
    ///
    /// It is an error to start a new wait operation while an another one is in
    /// progress. A wait operation is in progress until its completion handler
    /// starts to execute.
    ///
    /// \param delay The amount of time to wait, represented as anumber of
    /// milliseconds. If the delay is zero or negative, the wait is considered
    /// complete immediately.
    virtual void async_wait(Duration delay, WaitCompletionHandler handler) = 0;

    /// \brief Cancel an asynchronous wait operation.
    ///
    /// If an asynchronous wait operation, that is associated with this deadline
    /// timer, is in progress, cause it to fail with
    /// `error::operation_aborted`. An asynchronous wait operation is in
    /// progress until its completion handler starts to execute.
    ///
    /// Completion handlers of canceled operations will become immediately ready
    /// to execute, but will never be executed directly as part of the execution
    /// of cancel().
    virtual void cancel() noexcept = 0;

    virtual EventLoop& get_event_loop() noexcept = 0;

    virtual ~DeadlineTimer() {}
};


class EventLoop::Implementation {
public:
    class NotAvailable;

    /// \brief Get the default event loop implementation.
    ///
    /// In general, the best implementation is chosen when several are
    /// available, On Apple iOS, this will be the implementation returned by
    /// get_apple_cf(). On most other platforms (including Linux), it will be
    /// the implementation returned by get_posix().
    ///
    /// \throw NotAvailable if no implementation is available on this platform.
    static Implementation& get_default();

    /// \brief Get an implementations by name.
    ///
    /// \throw NotAvailable if no implementation is available with the specified
    /// name on this platform.
    static Implementation& get(const std::string& name);

    /// \brief Get all the available implementations on this platform.
    ///
    /// If no implementations are available on this platform, this function
    /// returns an empty vector.
    static std::vector<Implementation*> get_all();

    /// \brief Get an implementation base on the POSIX level networking API.
    ///
    /// The name of this implementation is `posix`.
    ///
    /// This function returns null on platforms where this implementation is not
    /// available.
    ///
    /// This implementation is guaranteed to be available on Linux, Android, Mac
    /// OS X, and iOS.
    ///
    /// \throw NotAvailable if this implementation is not available on this
    /// platform.
    static Implementation& get_posix();

    /// \brief Get an implementation base on the networking API provided by the
    /// Apple Core Foundation library (`CFRunLoop`).
    ///
    /// The name of this implementation is `apple-cf`.
    ///
    /// This implementation is guaranteed to be available on Mac OS X and
    /// iOS. This is the default implementation on iOS, because according to
    /// Apple's documentaion, POSIX level socket operations are not guaranteed
    /// to properly activate the radio antenna.
    ///
    /// \throw NotAvailable if this implementation is not available on this
    /// platform.
    static Implementation& get_apple_cf();

    /// \brief Get the name of this implementation.
    virtual std::string name() const = 0;

    /// \brief Create an event loop that uses this implementation.
    virtual std::unique_ptr<EventLoop> make_event_loop() = 0;

    virtual ~Implementation() {}
};




// Implementation

inline std::unique_ptr<EventLoop> make_event_loop()
{
    return EventLoop::Implementation::get_default().make_event_loop(); // Throws
}

class EventLoop::Implementation::NotAvailable: public std::exception {
    const char* what() const noexcept override
    {
        return "No such event loop implementation on this platform";
    }
};

} // namespace util
} // namespace realm


#endif // REALM_UTIL_EVENT_LOOP_HPP

