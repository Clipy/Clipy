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
#ifndef REALM_UTIL_NETWORK_HPP
#define REALM_UTIL_NETWORK_HPP

#include <cstddef>
#include <memory>
#include <chrono>
#include <tuple>
#include <string>
#include <system_error>
#include <ostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <realm/util/features.h>
#include <realm/util/assert.hpp>
#include <realm/util/buffer.hpp>
#include <realm/util/basic_system_errors.hpp>
#include <realm/util/call_with_tuple.hpp>


// FIXME: Unfinished business around `ip_address.m_ip_v6_scope_id`.


namespace realm {
namespace util {

/// \brief TCP/IP networking API.
///
/// The design of this networking API is heavily inspired by the ASIO C++
/// library (http://think-async.com).
///
///
/// ### Thread safety
///
/// A *service context* is a set of objects consisting of an instance of
/// io_service, and all the objects that are associated with that instance (\ref
/// resolver, \ref acceptor`, \ref socket`, \ref deadline_timer, and \ref
/// ssl::Stream).
///
/// In general, it is unsafe for two threads to call functions on the same
/// object, or on different objects in the same service context. This also
/// applies to destructors. Notable exceptions are the fully thread-safe
/// functions, such as io_service::post(), io_service::stop(), and
/// io_service::reset().
///
/// On the other hand, it is always safe for two threads to call functions on
/// objects belonging to different service contexts.
///
/// One implication of these rules is that at most one thread must execute run()
/// at any given time, and if one thread is executing run(), then no other
/// thread is allowed to access objects in the same service context (with the
/// mentioned exceptions).
///
/// Unless otherwise specified, free-staing objects, such as \ref
/// stream_protocol, \ref ip_address, \ref endpoint, and \ref endpoint::list are
/// fully thread-safe as long as they are not mutated. If one thread is mutating
/// such an object, no other thread may access it. Note that these free-standing
/// objects are not associcated with an instance of io_service, and are
/// therefore not part of a service context.
///
///
/// ### Comparison with ASIO
///
/// There is a crucial difference between the two libraries in regards to the
/// guarantees that are provided about the cancelability of asynchronous
/// operations. The Realm networking library (this library) considers an
/// asynchronous operation to be complete precisely when the completion handler
/// starts to execute, and it guarantees that such an operation is cancelable up
/// until that point in time. In particular, if `cancel()` is called on a socket
/// or a deadline timer object before the completion handler starts to execute,
/// then that operation will be canceled, and will receive
/// `error::operation_aborted`. This guarantee is possible (and free of
/// ambiguities) precisely because this library prohibits multiple threads from
/// executing the event loop concurrently, and because `cancel()` is allowed to
/// be called only from a completion handler (executed by the event loop thread)
/// or while no thread is executing the event loop. This guarantee allows for
/// safe destruction of sockets and deadline timers as long as the completion
/// handlers react appropriately to `error::operation_aborted`, in particular,
/// that they do not attempt to access the socket or deadline timer object in
/// such cases.
///
/// ASIO, on the other hand, allows for an asynchronous operation to complete
/// and become **uncancellable** before the completion handler starts to
/// execute. For this reason, it is possible with ASIO to get the completion
/// handler of an asynchronous wait operation to start executing and receive an
/// error code other than "operation aborted" at a point in time where
/// `cancel()` has already been called on the deadline timer object, or even at
/// a point in timer where the deadline timer has been destroyed. This seems
/// like an inevitable consequence of the fact that ASIO allows for multiple
/// threads to execute the event loop concurrently. This generally forces ASIO
/// applications to invent ways of extending the lifetime of deadline timer and
/// socket objects until the completion handler starts executing.
///
/// IMPORTANT: Even if ASIO is used in a way where at most one thread executes
/// the event loop, there is still no guarantee that an asynchronous operation
/// remains cancelable up until the point in timer where the completion handler
/// starts to execute.
namespace network {

std::string host_name();


class stream_protocol;
class ip_address;
class endpoint;
class io_service;
class resolver;
class socket_base;
class socket;
class acceptor;
class deadline_timer;
class ReadAheadBuffer;
namespace ssl {
class Stream;
} // namespace ssl


/// \brief An IP protocol descriptor.
class stream_protocol {
public:
    static stream_protocol ip_v4();
    static stream_protocol ip_v6();

    bool is_ip_v4() const;
    bool is_ip_v6() const;

    int protocol() const;
    int family() const;

    stream_protocol();
    ~stream_protocol() noexcept {}

private:
    int m_family;
    int m_socktype;
    int m_protocol;

    friend class resolver;
    friend class socket_base;
};


/// \brief An IP address (IPv4 or IPv6).
class ip_address {
public:
    bool is_ip_v4() const;
    bool is_ip_v6() const;

    template<class C, class T>
    friend std::basic_ostream<C,T>& operator<<(std::basic_ostream<C,T>&, const ip_address&);

    ip_address();
    ~ip_address() noexcept {}

private:
    typedef in_addr  ip_v4_type;
    typedef in6_addr ip_v6_type;
    union union_type {
        ip_v4_type m_ip_v4;
        ip_v6_type m_ip_v6;
    };
    union_type m_union;
    std::uint_least32_t m_ip_v6_scope_id = 0;
    bool m_is_ip_v6 = false;

    friend ip_address make_address(const char*, std::error_code&) noexcept;
    friend class endpoint;
};

ip_address make_address(const char* c_str);
ip_address make_address(const char* c_str, std::error_code& ec) noexcept;
ip_address make_address(const std::string&);
ip_address make_address(const std::string&, std::error_code& ec) noexcept;


/// \brief An IP endpoint.
///
/// An IP endpoint is a triplet (`protocol`, `address`, `port`).
class endpoint {
public:
    using port_type = uint_fast16_t;
    class list;

    stream_protocol protocol() const;
    ip_address address() const;
    port_type port() const;

    endpoint();
    endpoint(const stream_protocol&, port_type);
    endpoint(const ip_address&, port_type);
    ~endpoint() noexcept {}

    using data_type = sockaddr;
    data_type* data();
    const data_type* data() const;

private:
    stream_protocol m_protocol;

    using sockaddr_base_type  = sockaddr;
    using sockaddr_ip_v4_type = sockaddr_in;
    using sockaddr_ip_v6_type = sockaddr_in6;
    union sockaddr_union_type {
        sockaddr_base_type  m_base;
        sockaddr_ip_v4_type m_ip_v4;
        sockaddr_ip_v6_type m_ip_v6;
    };
    sockaddr_union_type m_sockaddr_union;

    friend class resolver;
    friend class socket_base;
    friend class socket;
    friend class acceptor;
};


/// \brief A list of IP endpoints.
class endpoint::list {
public:
    typedef const endpoint* iterator;

    iterator begin() const;
    iterator end() const;
    size_t size() const;

    list() = default;
    list(list&&) noexcept = default;
    ~list() noexcept = default;

private:
    Buffer<endpoint> m_endpoints;

    friend class resolver;
};


/// \brief TCP/IP networking service.
class io_service {
public:
    io_service();
    ~io_service() noexcept;

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
    ///
    /// Syncronous operations (e.g., socket::connect()) execute independently of
    /// the event loop, and do not require that any thread calls run().
    void run();

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
    void stop() noexcept;
    void reset() noexcept;
    /// @}

    /// \brief Submit a handler to be executed by the event loop thread.
    ///
    /// Register the sepcified completion handler for immediate asynchronous
    /// execution. The specified handler will be executed by an expression on
    /// the form `handler()`. If the the handler object is movable, it will
    /// never be copied. Otherwise, it will be copied as necessary.
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
    template<class H> void post(H handler);

private:
    enum class Want { nothing = 0, read, write };

    class async_oper;
    class wait_oper_base;
    class post_oper_base;
    template<class H> class post_oper;
    class IoOper;
    class UnusedOper; // Allocated, but currently unused memory
    template<class Oper> class OperQueue;

    template<class S> class BasicStreamOps;

    struct OwnersOperDeleter {
        void operator()(async_oper*) const noexcept;
    };
    struct LendersOperDeleter {
        void operator()(async_oper*) const noexcept;
    };
    using OwnersOperPtr      = std::unique_ptr<async_oper,     OwnersOperDeleter>;
    using LendersOperPtr     = std::unique_ptr<async_oper,     LendersOperDeleter>;
    using LendersWaitOperPtr = std::unique_ptr<wait_oper_base, LendersOperDeleter>;
    using LendersIoOperPtr   = std::unique_ptr<IoOper,         LendersOperDeleter>;

    class impl;
    const std::unique_ptr<impl> m_impl;

    template<class Oper, class... Args>
    static std::unique_ptr<Oper, LendersOperDeleter> alloc(OwnersOperPtr&, Args&&...);

    template<class Oper> static void execute(std::unique_ptr<Oper, LendersOperDeleter>&);

    template<class Oper, class... Args>
    static void initiate_io_oper(std::unique_ptr<Oper, LendersOperDeleter>, Args&&...);

    enum io_op { io_op_Read, io_op_Write };
    void add_io_oper(int fd, LendersIoOperPtr, io_op type);
    void add_wait_oper(LendersWaitOperPtr);
    void add_completed_oper(LendersOperPtr) noexcept;
    void cancel_incomplete_io_ops(int fd) noexcept;

    using PostOperConstr = post_oper_base*(void* addr, size_t size, impl&, void* cookie);
    void do_post(PostOperConstr, size_t size, void* cookie);
    template<class H>
    static post_oper_base* post_oper_constr(void* addr, size_t size, impl&, void* cookie);
    static void recycle_post_oper(impl&, post_oper_base*) noexcept;

    using clock = std::chrono::steady_clock;

    friend class socket_base;
    friend class socket;
    friend class acceptor;
    friend class deadline_timer;
    friend class ReadAheadBuffer;
    friend class ssl::Stream;
};


class resolver {
public:
    class query;

    resolver(io_service&);
    ~resolver() noexcept {}

    /// Thread-safe.
    io_service& get_io_service() noexcept;

    /// @{ \brief Resolve the specified query to one or more endpoints.
    void resolve(const query&, endpoint::list&);
    std::error_code resolve(const query&, endpoint::list&, std::error_code&);
    /// @}

private:
    io_service& m_service;
};


class resolver::query {
public:
    enum {
        /// Locally bound socket endpoint (server side)
        passive = AI_PASSIVE,

        /// Ignore families without a configured non-loopback address
        address_configured = AI_ADDRCONFIG
    };

    query(std::string service_port, int init_flags = passive|address_configured);
    query(const stream_protocol&, std::string service_port,
          int init_flags = passive|address_configured);
    query(std::string host_name, std::string service_port,
          int init_flags = address_configured);
    query(const stream_protocol&, std::string host_name, std::string service_port,
          int init_flags = address_configured);

    ~query() noexcept;

    int flags() const;
    stream_protocol protocol() const;
    std::string host() const;
    std::string service() const;

private:
    int m_flags;
    stream_protocol m_protocol;
    std::string m_host;    // hostname
    std::string m_service; // port

    friend class resolver;
};


class socket_base {
public:
    ~socket_base() noexcept;

    /// Thread-safe.
    io_service& get_io_service() noexcept;

    bool is_open() const noexcept;

    /// @{ \brief Open the socket for use with the specified protocol.
    ///
    /// It is an error to call open() on a socket that is already open.
    void open(const stream_protocol&);
    std::error_code open(const stream_protocol&, std::error_code&);
    /// @}

    /// \brief Close this socket.
    ///
    /// If the socket is open, it will be closed. If it is already closed (or
    /// never opened), this function does nothing (idempotency).
    ///
    /// A socket is automatically closed when destroyed.
    ///
    /// When the socket is closed, any incomplete asynchronous operation will be
    /// canceled (as if cancel() was called).
    void close() noexcept;

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
    void cancel() noexcept;

    template<class O>
    void get_option(O& opt) const;

    template<class O>
    std::error_code get_option(O& opt, std::error_code&) const;

    template<class O>
    void set_option(const O& opt);

    template<class O>
    std::error_code set_option(const O& opt, std::error_code&);

    void bind(const endpoint&);
    std::error_code bind(const endpoint&, std::error_code&);

    endpoint local_endpoint() const;
    endpoint local_endpoint(std::error_code&) const;

private:
    enum opt_enum {
        opt_ReuseAddr, ///< `SOL_SOCKET`, `SO_REUSEADDR`
        opt_Linger,    ///< `SOL_SOCKET`, `SO_LINGER`
    };

    template<class, int, class> class option;

public:
    typedef option<bool, opt_ReuseAddr, int> reuse_address;

    // linger struct defined by POSIX sys/socket.h.
    struct linger_opt;
    typedef option<linger_opt, opt_Linger, struct linger> linger;

private:
    int m_sock_fd;
    bool m_in_blocking_mode; // Not in nonblocking mode
    stream_protocol m_protocol;

protected:
    io_service& m_service;
    io_service::OwnersOperPtr m_read_oper;  // Read or accept
    io_service::OwnersOperPtr m_write_oper; // Write or connect

    socket_base(io_service&);

    int get_sock_fd() const noexcept;
    const stream_protocol& get_protocol() const noexcept;
    std::error_code do_assign(const stream_protocol&, int sock_fd, std::error_code& ec);
    void do_close() noexcept;

    void get_option(opt_enum, void* value_data, size_t& value_size, std::error_code&) const;
    void set_option(opt_enum, const void* value_data, size_t value_size, std::error_code&);
    void map_option(opt_enum, int& level, int& option_name) const;

    // `ec` untouched on success
    std::error_code ensure_blocking_mode(std::error_code& ec) noexcept;
    std::error_code ensure_nonblocking_mode(std::error_code& ec) noexcept;

private:
    // `ec` untouched on success
    std::error_code set_nonblocking_mode(bool enable, std::error_code&) noexcept;

    friend class io_service;
    friend class acceptor;
};


template<class T, int opt, class U>
class socket_base::option {
public:
    option(T value = T());
    T value() const;

private:
    T m_value;

    void get(const socket_base&, std::error_code&);
    void set(socket_base&, std::error_code&) const;

    friend class socket_base;
};

struct socket_base::linger_opt {
    linger_opt(bool enable, int timeout_seconds = 0)
    {
        m_linger.l_onoff = enable ? 1 : 0;
        m_linger.l_linger = timeout_seconds;
    }

    ::linger m_linger;

    operator ::linger() const { return m_linger; }

    bool enabled() const { return m_linger.l_onoff != 0; }
    int  timeout() const { return m_linger.l_linger; }
};


/// Switching between synchronous and asynchronous operations is allowed, but
/// only in a nonoverlapping fashion. That is, a synchronous operation is not
/// allowed to run concurrently with an asynchronous one on the same
/// socket. Note that an asynchronous operation is considered to be running
/// until its completion handler starts executing.
class socket: public socket_base {
public:
    using native_handle_type = int;

    socket(io_service&);

    /// \brief Create a socket with an already-connected native socket handle.
    ///
    /// This constructor is shorthand for creating the socket with the
    /// one-argument constructor, and then calling the two-argument assign()
    /// with the specified protocol and native handle.
    socket(io_service&, const stream_protocol&, native_handle_type);

    ~socket() noexcept;

    void connect(const endpoint&);
    std::error_code connect(const endpoint&, std::error_code&);

    /// @{ \brief Perform a synchronous read operation.
    ///
    /// read() will not return until the specified buffer is full, or an error
    /// occurs. Reaching the end of input before the buffer is filled, is
    /// considered an error, and will cause the operation to fail with
    /// `network::end_of_input`.
    ///
    /// read_until() will not return until the specified buffer contains the
    /// specified delimiter, or an error occurs. If the buffer is filled before
    /// the delimiter is found, the operation fails with
    /// `network::delim_not_found`. Otherwise, if the end of input is reached
    /// before the delimiter is found, the operation fails with
    /// `network::end_of_input`. If the operation succeeds, the last byte placed
    /// in the buffer is the delimiter.
    ///
    /// The versions that take a ReadAheadBuffer argument will read through that
    /// buffer. This allows for fewer larger reads on the underlying
    /// socket. Since unconsumed data may be left in the read-ahead buffer after
    /// a read operation returns, it is important that the same read-ahead
    /// buffer is passed to the next read operation.
    ///
    /// The versions of read() and read_until() that do not take an
    /// `std::error_code&` argument will throw std::system_error on failure.
    ///
    /// The versions that do take an `std::error_code&` argument will set \a ec
    /// to `std::error_code()` on success, and to something else on failure. On
    /// failure they will return the number of bytes placed in the specified
    /// buffer before the error occured.
    ///
    /// \return The number of bytes places in the specified buffer upon return.
    std::size_t read(char* buffer, std::size_t size);
    std::size_t read(char* buffer, std::size_t size, std::error_code& ec) noexcept;
    std::size_t read(char* buffer, std::size_t size, ReadAheadBuffer&);
    std::size_t read(char* buffer, std::size_t size, ReadAheadBuffer&,
                     std::error_code& ec) noexcept;
    std::size_t read_until(char* buffer, std::size_t size, char delim, ReadAheadBuffer&);
    std::size_t read_until(char* buffer, std::size_t size, char delim, ReadAheadBuffer&,
                           std::error_code& ec) noexcept;
    /// @}

    /// @{ \brief Perform a synchronous write operation.
    ///
    /// write() will not return until all the specified bytes have been written
    /// to the socket, or an error occurs.
    ///
    /// The versions of write() that does not take an `std::error_code&`
    /// argument will throw std::system_error on failure. When it succeeds, it
    /// always returns \a size.
    ///
    /// The versions that does take an `std::error_code&` argument will set \a
    /// ec to `std::error_code()` on success, and to something else on
    /// failure. On success it returns \a size. On faulure it returns the number
    /// of bytes written before the failure occured.
    std::size_t write(const char* data, std::size_t size);
    std::size_t write(const char* data, std::size_t size, std::error_code& ec) noexcept;
    /// @}

    /// @{ \brief Read at least one byte from this socket.
    ///
    /// If \a size is zero, both versions of read_some() will return zero
    /// without blocking. Read errors may or may not be detected in this case.
    ///
    /// Otherwise, if \a size is greater than zero, and at least one byte is
    /// immediately available, that is, without blocking, then both versions
    /// will read at least one byte (but generally as many immediately available
    /// bytes as will fit into the specified buffer), and return without
    /// blocking.
    ///
    /// Otherwise, both versions will block the calling thread until at least one
    /// byte becomes available, or an error occurs.
    ///
    /// In this context, it counts as an error, if the end of input is reached
    /// before at least one byte becomes available (see
    /// `network::end_of_input`).
    ///
    /// If no error occurs, both versions will return the number of bytes placed
    /// in the specified buffer, which is generally as many as are immediately
    /// available at the time when the first byte becomes available, although
    /// never more than \a size.
    ///
    /// If no error occurs, the three-argument version will set \a ec to
    /// indicate success.
    ///
    /// If an error occurs, the two-argument version will throw
    /// `std::system_error`, while the three-argument version will set \a ec to
    /// indicate the error, and return zero.
    ///
    /// As long as \a size is greater than zero, the two argument version will
    /// always return a value that is greater than zero, while the three
    /// argument version will return a value greater than zero when, and only
    /// when \a ec is set to indicate success (no error, and no end of input).
    size_t read_some(char* buffer, size_t size);
    size_t read_some(char* buffer, size_t size, std::error_code& ec) noexcept;
    /// @}

    /// @{ \brief Write at least one byte to this socket.
    ///
    /// If \a size is zero, both versions of write_some() will return zero
    /// without blocking. Write errors may or may not be detected in this case.
    ///
    /// Otherwise, if \a size is greater than zero, and at least one byte can be
    /// written immediately, that is, without blocking, then both versions will
    /// write at least one byte (but generally as many as can be written
    /// immediately), and return without blocking.
    ///
    /// Otherwise, both versions will block the calling thread until at least one
    /// byte can be written, or an error occurs.
    ///
    /// If no error occurs, both versions will return the number of bytes
    /// written, which is generally as many as can be written immediately at the
    /// time when the first byte can be written.
    ///
    /// If no error occurs, the three-argument version will set \a ec to
    /// indicate success.
    ///
    /// If an error occurs, the two-argument version will throw
    /// `std::system_error`, while the three-argument version will set \a ec to
    /// indicate the error, and return zero.
    ///
    /// As long as \a size is greater than zero, the two argument version will
    /// always return a value that is greater than zero, while the three
    /// argument version will return a value greater than zero when, and only
    /// when \a ec is set to indicate success.
    size_t write_some(const char* data, size_t size);
    size_t write_some(const char* data, size_t size, std::error_code&) noexcept;
    /// @}

    /// \brief Perform an asynchronous connect operation.
    ///
    /// Initiate an asynchronous connect operation. The completion handler is
    /// called when the operation completes. The operation completes when the
    /// connection is established, or an error occurs.
    ///
    /// The completion handler is always executed by the event loop thread,
    /// i.e., by a thread that is executing io_service::run(). Conversely, the
    /// completion handler is guaranteed to not be called while no thread is
    /// executing io_service::run(). The execution of the completion handler is
    /// always deferred to the event loop, meaning that it never happens as a
    /// synchronous side effect of the execution of async_connect(), even when
    /// async_connect() is executed by the event loop thread. The completion
    /// handler is guaranteed to be called eventually, as long as there is time
    /// enough for the operation to complete or fail, and a thread is executing
    /// io_service::run() for long enough.
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
    /// If the socket is not already open, it will be opened as part of the
    /// connect operation as if by calling `open(ep.protocol())`. If the opening
    /// operation succeeds, but the connect operation fails, the socket will be
    /// left in the opened state.
    ///
    /// The specified handler will be executed by an expression on the form
    /// `handler(ec)` where `ec` is the error code. If the the handler object is
    /// movable, it will never be copied. Otherwise, it will be copied as
    /// necessary.
    ///
    /// It is an error to start a new connect operation (synchronous or
    /// asynchronous) while an asynchronous connect operation is in progress. An
    /// asynchronous connect operation is considered complete as soon as the
    /// completion handler starts to execute.
    ///
    /// \param ep The remote endpoint of the connection to be established.
    template<class H> void async_connect(const endpoint& ep, H handler);

    /// @{ \brief Perform an asynchronous read operation.
    ///
    /// Initiate an asynchronous buffered read operation on the associated
    /// socket. The completion handler will be called when the operation
    /// completes, or an error occurs.
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
    /// The versions that take a ReadAheadBuffer argument will read through that
    /// buffer. This allows for fewer larger reads on the underlying
    /// socket. Since unconsumed data may be left in the read-ahead buffer after
    /// a read operation completes, it is important that the same read-ahead
    /// buffer is passed to the next read operation.
    ///
    /// The completion handler is always executed by the event loop thread,
    /// i.e., by a thread that is executing io_service::run(). Conversely, the
    /// completion handler is guaranteed to not be called while no thread is
    /// executing io_service::run(). The execution of the completion handler is
    /// always deferred to the event loop, meaning that it never happens as a
    /// synchronous side effect of the execution of async_read() or
    /// async_read_until(), even when async_read() or async_read_until() is
    /// executed by the event loop thread. The completion handler is guaranteed
    /// to be called eventually, as long as there is time enough for the
    /// operation to complete or fail, and a thread is executing
    /// io_service::run() for long enough.
    ///
    /// The operation can be canceled by calling cancel() on the associated
    /// socket, and will be automatically canceled if the associated socket is
    /// closed. If the operation is canceled, it will fail with
    /// `error::operation_aborted`. The operation remains cancelable up until
    /// the point in time where the completion handler starts to execute. This
    /// means that if cancel() is called before the completion handler starts to
    /// execute, then the completion handler is guaranteed to have
    /// `error::operation_aborted` passed to it. This is true regardless of
    /// whether cancel() is called explicitly or implicitly, such as when the
    /// socket is destroyed.
    ///
    /// The specified handler will be executed by an expression on the form
    /// `handler(ec, n)` where `ec` is the error code, and `n` is the number of
    /// bytes placed in the buffer (of type `std::size_t`). `n` is guaranteed to
    /// be less than, or equal to \a size. If the the handler object is movable,
    /// it will never be copied. Otherwise, it will be copied as necessary.
    ///
    /// It is an error to start a read operation before the associated socket is
    /// connected.
    ///
    /// It is an error to start a new read operation (synchronous or
    /// asynchronous) while an asynchronous read operation is in progress. An
    /// asynchronous read operation is considered complete as soon as the
    /// completion handler starts executing. This means that a new read
    /// operation can be started from the completion handler of another
    /// asynchronous buffered read operation.
    template<class H> void async_read(char* buffer, std::size_t size, H handler);
    template<class H> void async_read(char* buffer, std::size_t size, ReadAheadBuffer&, H handler);
    template<class H> void async_read_until(char* buffer, std::size_t size, char delim,
                                            ReadAheadBuffer&, H handler);
    /// @}

    /// \brief Perform an asynchronous write operation.
    ///
    /// Initiate an asynchronous write operation. The completion handler is
    /// called when the operation completes. The operation completes when all
    /// the specified bytes have been written to the socket, or an error occurs.
    ///
    /// The completion handler is always executed by the event loop thread,
    /// i.e., by a thread that is executing io_service::run(). Conversely, the
    /// completion handler is guaranteed to not be called while no thread is
    /// executing io_service::run(). The execution of the completion handler is
    /// always deferred to the event loop, meaning that it never happens as a
    /// synchronous side effect of the execution of async_write(), even when
    /// async_write() is executed by the event loop thread. The completion
    /// handler is guaranteed to be called eventually, as long as there is time
    /// enough for the operation to complete or fail, and a thread is executing
    /// io_service::run() for long enough.
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
    /// The specified handler will be executed by an expression on the form
    /// `handler(ec, n)` where `ec` is the error code, and `n` is the number of
    /// bytes written (of type `size_t`). If the the handler object is movable,
    /// it will never be copied. Otherwise, it will be copied as necessary.
    ///
    /// It is an error to start an asynchronous write operation before the
    /// socket is connected.
    ///
    /// It is an error to start a new write operation (synchronous or
    /// asynchronous) while an asynchronous write operation is in progress. An
    /// asynchronous write operation is considered complete as soon as the
    /// completion handler starts to execute. This means that a new write
    /// operation can be started from the completion handler of another
    /// asynchronous write operation.
    template<class H> void async_write(const char* data, size_t size, H handler);

    template<class H> void async_read_some(char* buffer, std::size_t size, H handler);
    template<class H> void async_write_some(const char* data, std::size_t size, H handler);

    enum shutdown_type {
        /// Shutdown the receive side of the socket.
        shutdown_receive = SHUT_RD,

        /// Shutdown the send side of the socket.
        shutdown_send = SHUT_WR,

        /// Shutdown both send and receive on the socket.
        shutdown_both = SHUT_RDWR
    };

    /// @{ \brief Shut down the connected sockets sending and/or receiving
    /// side.
    ///
    /// It is an error to call this function when the socket is not both open
    /// and connected.
    void shutdown(shutdown_type);
    std::error_code shutdown(shutdown_type, std::error_code&) noexcept;
    /// @}

    /// @{ \brief Initialize socket with an already-connected native socket
    /// handle.
    ///
    /// The specified native handle must refer to a socket that is already fully
    /// open and connected.
    ///
    /// If the assignment operation succeeds, this socket object has taken
    /// ownership of the specified native handle, and the handle will be closed
    /// when the socket object is destroyed, (or when close() is called). If the
    /// operation fails, the caller still owns the specified native handle.
    ///
    /// It is an error to call connect() or async_connect() on a socket object
    /// that is initialized this way (unless it is first closed).
    ///
    /// It is an error to call this function on a socket object that is already
    /// open.
    void assign(const stream_protocol&, native_handle_type);
    std::error_code assign(const stream_protocol&, native_handle_type, std::error_code&);
    /// @}

    /// Returns a reference to this socket, as this socket is the lowest layer
    /// of a stream.
    socket& lowest_layer() noexcept;

private:
    using Want = io_service::Want;
    using StreamOps = io_service::BasicStreamOps<socket>;

    class ConnectOperBase;
    template<class H> class ConnectOper;

    using LendersConnectOperPtr =
        std::unique_ptr<ConnectOperBase, io_service::LendersOperDeleter>;

    // `ec` untouched on success, but no immediate completion
    bool initiate_async_connect(const endpoint&, std::error_code& ec) noexcept;
    // `ec` untouched on success
    std::error_code finalize_async_connect(std::error_code& ec) noexcept;

    // See io_service::BasicStreamOps for details on these these 8 functions.
    void do_init_read_sync(std::error_code&) noexcept;
    void do_init_write_sync(std::error_code&) noexcept;
    void do_init_read_async(std::error_code&, Want&) noexcept;
    void do_init_write_async(std::error_code&, Want&) noexcept;
    std::size_t do_read_some_sync(char* buffer, std::size_t size,
                                  std::error_code&) noexcept;
    std::size_t do_write_some_sync(const char* data, std::size_t size,
                                   std::error_code&) noexcept;
    std::size_t do_read_some_async(char* buffer, std::size_t size,
                                   std::error_code&, Want&) noexcept;
    std::size_t do_write_some_async(const char* data, std::size_t size,
                                    std::error_code&, Want&) noexcept;

    friend class io_service::BasicStreamOps<socket>;
    friend class io_service::BasicStreamOps<ssl::Stream>;
    friend class ReadAheadBuffer;
    friend class ssl::Stream;
};


/// Switching between synchronous and asynchronous operations is allowed, but
/// only in a nonoverlapping fashion. That is, a synchronous operation is not
/// allowed to run concurrently with an asynchronous one on the same
/// acceptor. Note that an asynchronous operation is considered to be running
/// until its completion handler starts executing.
class acceptor: public socket_base {
public:
    acceptor(io_service&);
    ~acceptor() noexcept;

    static constexpr int max_connections = SOMAXCONN;

    void listen(int backlog = max_connections);
    std::error_code listen(int backlog, std::error_code&);

    void accept(socket&);
    void accept(socket&, endpoint&);
    std::error_code accept(socket&, std::error_code&);
    std::error_code accept(socket&, endpoint&, std::error_code&);

    /// @{ \brief Perform an asynchronous accept operation.
    ///
    /// Initiate an asynchronous accept operation. The completion handler will
    /// be called when the operation completes. The operation completes when the
    /// connection is accepted, or an error occurs. If the operation succeeds,
    /// the specified local socket will have become connected to a remote
    /// socket.
    ///
    /// The completion handler is always executed by the event loop thread,
    /// i.e., by a thread that is executing io_service::run(). Conversely, the
    /// completion handler is guaranteed to not be called while no thread is
    /// executing io_service::run(). The execution of the completion handler is
    /// always deferred to the event loop, meaning that it never happens as a
    /// synchronous side effect of the execution of async_accept(), even when
    /// async_accept() is executed by the event loop thread. The completion
    /// handler is guaranteed to be called eventually, as long as there is time
    /// enough for the operation to complete or fail, and a thread is executing
    /// io_service::run() for long enough.
    ///
    /// The operation can be canceled by calling cancel(), and will be
    /// automatically canceled if the acceptor is closed. If the operation is
    /// canceled, it will fail with `error::operation_aborted`. The operation
    /// remains cancelable up until the point in time where the completion
    /// handler starts to execute. This means that if cancel() is called before
    /// the completion handler starts to execute, then the completion handler is
    /// guaranteed to have `error::operation_aborted` passed to it. This is true
    /// regardless of whether cancel() is called explicitly or implicitly, such
    /// as when the acceptor is destroyed.
    ///
    /// The specified handler will be executed by an expression on the form
    /// `handler(ec)` where `ec` is the error code. If the the handler object is
    /// movable, it will never be copied. Otherwise, it will be copied as
    /// necessary.
    ///
    /// It is an error to start a new accept operation (synchronous or
    /// asynchronous) while an asynchronous accept operation is in progress. An
    /// asynchronous accept operation is considered complete as soon as the
    /// completion handler starts executing. This means that a new accept
    /// operation can be started from the completion handler.
    ///
    /// \param sock This is the local socket, that upon successful completion
    /// will have become connected to the remote socket. It must be in the
    /// closed state (socket::is_open()) when async_accept() is called.
    ///
    /// \param ep Upon completion, the remote peer endpoint will have been
    /// assigned to this variable.
    template<class H> void async_accept(socket& sock, H handler);
    template<class H> void async_accept(socket& sock, endpoint& ep, H handler);
    /// @}

private:
    using Want = io_service::Want;

    class AcceptOperBase;
    template<class H> class AcceptOper;

    using LendersAcceptOperPtr = std::unique_ptr<AcceptOperBase, io_service::LendersOperDeleter>;

    std::error_code accept(socket&, endpoint*, std::error_code&);
    void do_accept_sync(socket&, endpoint*, std::error_code&) noexcept;
    Want do_accept_async(socket&, endpoint*, std::error_code&) noexcept;

    template<class H> void async_accept(socket&, endpoint*, H);
};


/// \brief A timer object supporting asynchronous wait operations.
class deadline_timer {
public:
    deadline_timer(io_service&);
    ~deadline_timer() noexcept;

    /// Thread-safe.
    io_service& get_io_service() noexcept;

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
    /// i.e., by a thread that is executing io_service::run(). Conversely, the
    /// completion handler is guaranteed to not be called while no thread is
    /// executing io_service::run(). The execution of the completion handler is
    /// always deferred to the event loop, meaning that it never happens as a
    /// synchronous side effect of the execution of async_wait(), even when
    /// async_wait() is executed by the event loop thread. The completion
    /// handler is guaranteed to be called eventually, as long as there is time
    /// enough for the operation to complete or fail, and a thread is executing
    /// io_service::run() for long enough.
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
    /// The specified handler will be executed by an expression on the form
    /// `handler(ec)` where `ec` is the error code. If the the handler object is
    /// movable, it will never be copied. Otherwise, it will be copied as
    /// necessary.
    ///
    /// It is an error to start a new asynchronous wait operation while an
    /// another one is in progress. An asynchronous wait operation is in
    /// progress until its completion handler starts executing.
    template<class R, class P, class H>
    void async_wait(std::chrono::duration<R,P> delay, H handler);

    /// \brief Cancel an asynchronous wait operation.
    ///
    /// If an asynchronous wait operation, that is associated with this deadline
    /// timer, is in progress, cause it to fail with
    /// `error::operation_aborted`. An asynchronous wait operation is in
    /// progress until its completion handler starts executing.
    ///
    /// Completion handlers of canceled operations will become immediately ready
    /// to execute, but will never be executed directly as part of the execution
    /// of cancel().
    void cancel() noexcept;

private:
    template<class H> class wait_oper;

    using clock = io_service::clock;

    io_service& m_service;
    io_service::OwnersOperPtr m_wait_oper;
};


class ReadAheadBuffer {
public:
    ReadAheadBuffer();

    /// Discard any buffered data.
    void clear() noexcept;

private:
    using Want = io_service::Want;

    char* m_begin = nullptr;
    char* m_end   = nullptr;
    static constexpr std::size_t s_size = 1024;
    const std::unique_ptr<char[]> m_buffer;

    bool empty() const noexcept;
    bool read(char*& begin, char* end, int delim, std::error_code&) noexcept;
    template<class S> void refill_sync(S& stream, std::error_code&) noexcept;
    template<class S> bool refill_async(S& stream, std::error_code&, Want&) noexcept;

    template<class> friend class io_service::BasicStreamOps;
};


enum errors {
    /// End of input.
    end_of_input = 1,

    /// Delimiter not found.
    delim_not_found,

    /// Host not found (authoritative).
    host_not_found,

    /// Host not found (non-authoritative).
    host_not_found_try_again,

    /// The query is valid but does not have associated address data.
    no_data,

    /// A non-recoverable error occurred.
    no_recovery,

    /// The service is not supported for the given socket type.
    service_not_found,

    /// The socket type is not supported.
    socket_type_not_supported,

    /// Premature end of input (e.g., end of input before reception of SSL
    /// shutdown alert).
    premature_end_of_input
};

std::error_code make_error_code(errors);

} // namespace network
} // namespace util
} // namespace realm

namespace std {

template<>
struct is_error_code_enum<realm::util::network::errors>
{
public:
    static const bool value = true;
};

} // namespace std

namespace realm {
namespace util {
namespace network {





// Implementation

// ---------------- stream_protocol ----------------

inline stream_protocol stream_protocol::ip_v4()
{
    stream_protocol prot;
    prot.m_family = AF_INET;
    return prot;
}

inline stream_protocol stream_protocol::ip_v6()
{
    stream_protocol prot;
    prot.m_family = AF_INET6;
    return prot;
}

inline bool stream_protocol::is_ip_v4() const
{
    return m_family == AF_INET;
}

inline bool stream_protocol::is_ip_v6() const
{
    return m_family == AF_INET6;
}

inline int stream_protocol::family() const
{
    return m_family;
}

inline int stream_protocol::protocol() const
{
    return m_protocol;
}

inline stream_protocol::stream_protocol():
    m_family(AF_UNSPEC),     // Allow both IPv4 and IPv6
    m_socktype(SOCK_STREAM), // Or SOCK_DGRAM for UDP
    m_protocol(0)            // Any protocol
{
}

// ---------------- ip_address ----------------

inline bool ip_address::is_ip_v4() const
{
    return !m_is_ip_v6;
}

inline bool ip_address::is_ip_v6() const
{
    return m_is_ip_v6;
}

template<class C, class T>
inline std::basic_ostream<C,T>& operator<<(std::basic_ostream<C,T>& out, const ip_address& addr)
{
    // FIXME: Not taking `addr.m_ip_v6_scope_id` into account. What does ASIO
    // do?
    union buffer_union {
        char ip_v4[INET_ADDRSTRLEN];
        char ip_v6[INET6_ADDRSTRLEN];
    };
    char buffer[sizeof (buffer_union)];
    int af = addr.m_is_ip_v6 ? AF_INET6 : AF_INET;
    const char* ret = ::inet_ntop(af, &addr.m_union, buffer, sizeof buffer);
    if (ret == 0) {
        std::error_code ec = make_basic_system_error_code(errno);
        throw std::system_error(ec);
    }
    out << ret;
    return out;
}

inline ip_address::ip_address()
{
    m_union.m_ip_v4 = ip_v4_type();
}

inline ip_address make_address(const char* c_str)
{
    std::error_code ec;
    ip_address addr = make_address(c_str, ec);
    if (ec)
        throw std::system_error(ec);
    return addr;
}

inline ip_address make_address(const std::string& str)
{
    std::error_code ec;
    ip_address addr = make_address(str, ec);
    if (ec)
        throw std::system_error(ec);
    return addr;
}

inline ip_address make_address(const std::string& str, std::error_code& ec) noexcept
{
    return make_address(str.c_str(), ec);
}

// ---------------- endpoint ----------------

inline stream_protocol endpoint::protocol() const
{
    return m_protocol;
}

inline ip_address endpoint::address() const
{
    ip_address addr;
    if (m_protocol.is_ip_v4()) {
        addr.m_union.m_ip_v4 = m_sockaddr_union.m_ip_v4.sin_addr;
    }
    else {
        addr.m_union.m_ip_v6 = m_sockaddr_union.m_ip_v6.sin6_addr;
        addr.m_ip_v6_scope_id = m_sockaddr_union.m_ip_v6.sin6_scope_id;
        addr.m_is_ip_v6 = true;
    }
    return addr;
}

inline endpoint::port_type endpoint::port() const
{
    return ntohs(m_protocol.is_ip_v4() ? m_sockaddr_union.m_ip_v4.sin_port :
                 m_sockaddr_union.m_ip_v6.sin6_port);
}

inline endpoint::data_type* endpoint::data()
{
    return &m_sockaddr_union.m_base;
}

inline const endpoint::data_type* endpoint::data() const
{
    return &m_sockaddr_union.m_base;
}

inline endpoint::endpoint():
    endpoint(stream_protocol::ip_v4(), 0)
{
}

inline endpoint::endpoint(const stream_protocol& protocol, port_type port):
    m_protocol(protocol)
{
    int family = m_protocol.family();
    if (family == AF_INET) {
        m_sockaddr_union.m_ip_v4 = sockaddr_ip_v4_type(); // Clear
        m_sockaddr_union.m_ip_v4.sin_family = AF_INET;
        m_sockaddr_union.m_ip_v4.sin_port = htons(port);
        m_sockaddr_union.m_ip_v4.sin_addr.s_addr = INADDR_ANY;
    }
    else if (family == AF_INET6) {
        m_sockaddr_union.m_ip_v6 = sockaddr_ip_v6_type(); // Clear
        m_sockaddr_union.m_ip_v6.sin6_family = AF_INET6;
        m_sockaddr_union.m_ip_v6.sin6_port = htons(port);
    }
    else {
        m_sockaddr_union.m_ip_v4 = sockaddr_ip_v4_type(); // Clear
        m_sockaddr_union.m_ip_v4.sin_family = AF_UNSPEC;
        m_sockaddr_union.m_ip_v4.sin_port = htons(port);
        m_sockaddr_union.m_ip_v4.sin_addr.s_addr = INADDR_ANY;
    }
}

inline endpoint::endpoint(const ip_address& addr, port_type port)
{
    if (addr.m_is_ip_v6) {
        m_protocol = stream_protocol::ip_v6();
        m_sockaddr_union.m_ip_v6.sin6_family = AF_INET6;
        m_sockaddr_union.m_ip_v6.sin6_port = htons(port);
        m_sockaddr_union.m_ip_v6.sin6_flowinfo = 0;
        m_sockaddr_union.m_ip_v6.sin6_addr = addr.m_union.m_ip_v6;
        m_sockaddr_union.m_ip_v6.sin6_scope_id = addr.m_ip_v6_scope_id;
    }
    else {
        m_protocol = stream_protocol::ip_v4();
        m_sockaddr_union.m_ip_v4.sin_family = AF_INET;
        m_sockaddr_union.m_ip_v4.sin_port = htons(port);
        m_sockaddr_union.m_ip_v4.sin_addr = addr.m_union.m_ip_v4;
    }
}

inline endpoint::list::iterator endpoint::list::begin() const
{
    return m_endpoints.data();
}

inline endpoint::list::iterator endpoint::list::end() const
{
    return m_endpoints.data() + m_endpoints.size();
}

inline size_t endpoint::list::size() const
{
    return m_endpoints.size();
}

// ---------------- io_service ----------------

class io_service::async_oper {
public:
    bool in_use() const noexcept;
    bool is_complete() const noexcept;
    bool is_uncanceled() const noexcept;
    void cancel() noexcept;
    /// Every object of type \ref async_oper must be desroyed either by a call
    /// to this function or to recycle(). This function recycles the operation
    /// object (commits suicide), even if it throws.
    virtual void recycle_and_execute() = 0;
    /// Every object of type \ref async_oper must be destroyed either by a call
    /// to recycle_and_execute() or to this function. This function destroys the
    /// object (commits suicide).
    virtual void recycle() noexcept = 0;
    /// Must be called when the owner dies, and the object is in use (not an
    /// instance of UnusedOper).
    virtual void orphan()  noexcept = 0;
protected:
    async_oper(size_t size, bool in_use) noexcept;
    virtual ~async_oper() noexcept {}
    bool is_canceled() const noexcept;
    void set_is_complete(bool value) noexcept;
    template<class H, class... Args>
    void do_recycle_and_execute(bool orphaned, H& handler, Args&&...);
    void do_recycle(bool orphaned) noexcept;
private:
    size_t m_size; // Allocated number of bytes
    bool m_in_use   = false;
    bool m_complete = false;      // Always false when not in use
    bool m_canceled = false;      // Always false when not in use
    async_oper* m_next = nullptr; // Always null when not in use
    friend class io_service;
};

class io_service::wait_oper_base: public async_oper {
public:
    wait_oper_base(size_t size, deadline_timer& timer, clock::time_point expiration_time):
        async_oper(size, true), // Second argument is `in_use`
        m_timer(&timer),
        m_expiration_time(expiration_time)
    {
    }
    void recycle() noexcept override final
    {
        bool orphaned = !m_timer;
        // Note: do_recycle() commits suicide.
        do_recycle(orphaned);
    }
    void orphan() noexcept override final
    {
        m_timer = 0;
    }
protected:
    deadline_timer* m_timer;
    clock::time_point m_expiration_time;
    friend class io_service;
};

class io_service::post_oper_base:
        public async_oper {
public:
    post_oper_base(size_t size, impl& serv):
        async_oper(size, true), // Second argument is `in_use`
        m_service(serv)
    {
    }
    void recycle() noexcept override final
    {
        // io_service::recycle_post_oper() destroys this operation object
        io_service::recycle_post_oper(m_service, this);
    }
    void orphan() noexcept override final
    {
        REALM_ASSERT(false); // Never called
    }
protected:
    impl& m_service;
};

template<class H> class io_service::post_oper: public post_oper_base {
public:
    post_oper(size_t size, impl& serv, H handler):
        post_oper_base(size, serv),
        m_handler(std::move(handler))
    {
    }
    void recycle_and_execute() override final
    {
        // Recycle the operation object before the handler is exceuted, such
        // that the memory is available for a new post operation that might be
        // initiated during the execution of the handler.
        bool was_recycled = false;
        try {
            H handler = std::move(m_handler); // Throws
            // io_service::recycle_post_oper() destroys this operation object
            io_service::recycle_post_oper(m_service, this);
            was_recycled = true;
            handler(); // Throws
        }
        catch (...) {
            if (!was_recycled) {
                // io_service::recycle_post_oper() destroys this operation object
                io_service::recycle_post_oper(m_service, this);
            }
            throw;
        }
    }
private:
    H m_handler;
};

class io_service::IoOper: public async_oper {
public:
    IoOper(std::size_t size) noexcept:
        async_oper(size, true) // Second argument is `in_use`
    {
    }
    virtual Want proceed() noexcept = 0;
};

class io_service::UnusedOper: public async_oper {
public:
    UnusedOper(size_t size) noexcept:
        async_oper(size, false) // Second argument is `in_use`
    {
    }
    void recycle_and_execute() override final
    {
        // Must never be called
        REALM_ASSERT(false);
    }
    void recycle() noexcept override final
    {
        // Must never be called
        REALM_ASSERT(false);
    }
    void orphan() noexcept override final
    {
        // Must never be called
        REALM_ASSERT(false);
    }
};

// `S` must be a stream class with the following member functions:
//
//    void do_init_read_sync(std::error_code& ec) noexcept;
//    void do_init_write_sync(std::error_code& ec) noexcept;
//    void do_init_read_async(std::error_code& ec, Want& want) noexcept;
//    void do_init_write_async(std::error_code& ec, Want& want) noexcept;
//
//    std::size_t do_read_some_sync(char* buffer, std::size_t size,
//                                  std::error_code& ec) noexcept;
//    std::size_t do_write_some_sync(const char* data, std::size_t size,
//                                   std::error_code& ec) noexcept;
//    std::size_t do_read_some_async(char* buffer, std::size_t size,
//                                   std::error_code& ec, Want& want) noexcept;
//    std::size_t do_write_some_async(const char* data, std::size_t size,
//                                    std::error_code& ec, Want& want) noexcept;
//
// Additionally, `S` must have members `m_read_oper` and `m_write_oper`, which
// both must be of type `LendersIoOperPtr` (or equivalent smart pointer to a
// derivative of IoOper).
//
// The do_init_*_sync() functions must enable blocking mode. The
// do_init_*_async() function must disable blocking mode.
//
// If an error occurs during any of the 8 functions, the `ec` argument must be
// set accordingly. Otherwise the `ec` argument must be set to
// `std::error_code()`.
//
// The do_init_*_async() functions must update the `want` argument to indicate
// how the operation must be initiated:
//
//    Want::read      Wait for read readiness, then call do_*_some_async().
//    Want::write     Wait for write readiness, then call do_*_some_async().
//    Want::nothing   Call do_*_some_async() immediately without waiting for
//                    read or write readyness.
//
// If end-of-input occurs while reading, do_read_some_*() must fail, set `ec` to
// `network::end_of_input`, and return zero.
//
// If an error occurs during reading or writing, do_*_some_sync() must set `ec`
// accordingly (to something other than `std::system_error()`) and return
// zero. Otherwise they must set `ec` to `std::system_error()` and return the
// number of bytes read or written, which **must** be at least 1. If the
// underlying socket is in nonblocking mode, and no bytes could be immediately
// read or written these functinos must fail with
// `error::resource_unavailable_try_again`.
//
// If an error occurs during reading or writing, do_*_some_async() must set `ec`
// accordingly (to something other than `std::system_error()`), `want` to
// `Want::nothing`, and return zero. Otherwise they must set `ec` to
// `std::system_error()` and return the number of bytes read or written, which
// must be zero if no bytes could be immediately read or written. Note, in this
// case it is not an error if the underlying socket is in nonblocking mode, and
// no bytes could be immediately read or written. When these functions succeed,
// but return zero because no bytes could be immediately read or written, they
// must set `want` to something other than `Want::nothing`.
//
// If no error occurs, do_*_some_async() must set `want` to indicate how the
// operation should proceed if additional data needs to be read or written, or
// if no bytes were transferred:
//
//    Want::read      Wait for read readiness, then call do_*_some_async() again.
//    Want::write     Wait for write readiness, then call do_*_some_async() again.
//    Want::nothing   Call do_*_some_async() again without waiting for read or
//                    write readyness.
//
// NOTE: If, for example, do_read_some_async() sets `want` to `Want::write`, it
// means that the stream needs to write data to the underlying TCP socket before
// it is able to deliver any additional data to the caller. While such a
// situation will never occur on a raw TCP socket, it can occur on an SSL stream
// (Secure Socket Layer).
//
// When do_*_some_async() returns `n`, at least one of the following conditions
// must be true:
//
//    n > 0                     Bytes were transferred.
//    ec != std::error_code()   An error occured.
//    want != Want::nothing     Wait for read/write readyness.
//
// This is of critical importance, as it is the only way we can avoid falling
// into a busy loop of repeated invocations of do_*_some_async().
//
// NOTE: do_*_some_async() are allowed to set `want` to `Want::read` or
// `Want::write`, even when they succesfully transfer a nonzero number of bytes.
template<class S> class io_service::BasicStreamOps {
public:
    class StreamOper;
    class ReadOperBase;
    class WriteOperBase;
    class BufferedReadOperBase;
    template<class H> class ReadOper;
    template<class H> class WriteOper;
    template<class H> class BufferedReadOper;

    using LendersReadOperPtr          = std::unique_ptr<ReadOperBase,         LendersOperDeleter>;
    using LendersWriteOperPtr         = std::unique_ptr<WriteOperBase,        LendersOperDeleter>;
    using LendersBufferedReadOperPtr  = std::unique_ptr<BufferedReadOperBase, LendersOperDeleter>;

    static std::size_t read(S& stream, char* buffer, std::size_t size, std::error_code& ec) noexcept
    {
        REALM_ASSERT(!stream.m_read_oper || !stream.m_read_oper->in_use());
        stream.do_init_read_sync(ec);
        if (REALM_UNLIKELY(ec))
            return 0;
        char* begin = buffer;
        char* end   = buffer + size;
        char* curr  = begin;
        for (;;) {
            if (curr == end) {
                ec = std::error_code(); // Success
                break;
            }
            char* buffer_2 = curr;
            std::size_t size_2 = std::size_t(end - curr);
            std::size_t n = stream.do_read_some_sync(buffer_2, size_2, ec);
            if (REALM_UNLIKELY(ec))
                break;
            REALM_ASSERT(n > 0);
            REALM_ASSERT(n <= size_2);
            curr += n;
        }
        std::size_t n = std::size_t(curr - begin);
        return n;
    }

    static std::size_t write(S& stream, const char* data, std::size_t size, std::error_code& ec) noexcept
    {
        REALM_ASSERT(!stream.m_write_oper || !stream.m_write_oper->in_use());
        stream.do_init_write_sync(ec);
        if (REALM_UNLIKELY(ec))
            return 0;
        const char* begin = data;
        const char* end   = data + size;
        const char* curr  = begin;
        for (;;) {
            if (curr == end) {
                ec = std::error_code(); // Success
                break;
            }
            const char* data_2 = curr;
            std::size_t size_2 = std::size_t(end - curr);
            std::size_t n = stream.do_write_some_sync(data_2, size_2, ec);
            if (REALM_UNLIKELY(ec))
                break;
            REALM_ASSERT(n > 0);
            REALM_ASSERT(n <= size_2);
            curr += n;
        }
        std::size_t n = std::size_t(curr - begin);
        return n;
    }

    static std::size_t buffered_read(S& stream, char* buffer, std::size_t size, int delim,
                                     ReadAheadBuffer& rab, std::error_code& ec) noexcept
    {
        REALM_ASSERT(!stream.m_read_oper || !stream.m_read_oper->in_use());
        stream.do_init_read_sync(ec);
        if (REALM_UNLIKELY(ec))
            return 0;

        char* begin = buffer;
        char* end   = buffer + size;
        char* curr  = begin;
        for (;;) {
            bool complete = rab.read(curr, end, delim, ec);
            if (complete)
                break;

            rab.refill_sync(stream, ec);
            if (REALM_UNLIKELY(ec))
                break;
        }
        std::size_t n = (curr - begin);
        return n;
    }

    static std::size_t read_some(S& stream, char* buffer, std::size_t size,
                                 std::error_code& ec) noexcept
    {
        REALM_ASSERT(!stream.m_read_oper || !stream.m_read_oper->in_use());
        stream.do_init_read_sync(ec);
        if (REALM_UNLIKELY(ec))
            return 0;
        return stream.do_read_some_sync(buffer, size, ec);
    }

    static std::size_t write_some(S& stream, const char* data, std::size_t size,
                                  std::error_code& ec) noexcept
    {
        REALM_ASSERT(!stream.m_write_oper || !stream.m_write_oper->in_use());
        stream.do_init_write_sync(ec);
        if (REALM_UNLIKELY(ec))
            return 0;
        return stream.do_write_some_sync(data, size, ec);
    }

    template<class H>
    static void async_read(S& stream, char* buffer, std::size_t size, bool is_read_some, H handler)
    {
        char* begin = buffer;
        char* end   = buffer + size;
        LendersReadOperPtr op =
            io_service::alloc<ReadOper<H>>(stream.m_read_oper, stream, is_read_some, begin, end,
                                           std::move(handler)); // Throws
        initiate_io_oper(std::move(op)); // Throws
    }

    template<class H>
    static void async_write(S& stream, const char* data, std::size_t size, bool is_write_some,
                            H handler)
    {
        const char* begin = data;
        const char* end   = data + size;
        LendersWriteOperPtr op =
            io_service::alloc<WriteOper<H>>(stream.m_write_oper, stream, is_write_some, begin, end,
                                            std::move(handler)); // Throws
        initiate_io_oper(std::move(op)); // Throws
    }

    template<class H>
    static void async_buffered_read(S& stream, char* buffer, std::size_t size, int delim,
                                    ReadAheadBuffer& rab, H handler)
    {
        char* begin = buffer;
        char* end   = buffer + size;
        LendersBufferedReadOperPtr op =
            io_service::alloc<BufferedReadOper<H>>(stream.m_read_oper, stream, begin, end, delim,
                                                   rab, std::move(handler)); // Throws
        initiate_io_oper(std::move(op)); // Throws
    }
};

template<class S> class io_service::BasicStreamOps<S>::StreamOper: public IoOper {
public:
    StreamOper(std::size_t size, S& stream) noexcept:
        IoOper(size),
        m_stream(&stream)
    {
    }
    void recycle() noexcept override final
    {
        bool orphaned = !m_stream;
        // Note: do_recycle() commits suicide.
        do_recycle(orphaned);
    }
    void orphan() noexcept override final
    {
        m_stream = nullptr;
    }
    socket_base& get_socket() noexcept
    {
        return m_stream->lowest_layer();
    }
protected:
    S* m_stream;
    std::error_code m_error_code;
};

template<class S> class io_service::BasicStreamOps<S>::ReadOperBase: public StreamOper {
public:
    ReadOperBase(std::size_t size, S& stream, bool is_read_some, char* begin, char* end) noexcept:
        StreamOper(size, stream),
        m_is_read_some(is_read_some),
        m_begin(begin),
        m_end(end)
    {
    }
    Want initiate() noexcept
    {
        auto& s = *this;
        REALM_ASSERT(this == s.m_stream->m_read_oper.get());
        REALM_ASSERT(!s.is_complete());
        REALM_ASSERT(s.m_curr <= s.m_end);
        Want want = Want::nothing;
        if (REALM_UNLIKELY(s.m_curr == s.m_end)) {
            s.set_is_complete(true); // Success
        }
        else {
            s.m_stream->do_init_read_async(s.m_error_code, want);
            if (want == Want::nothing) {
                if (REALM_UNLIKELY(s.m_error_code)) {
                    s.set_is_complete(true); // Failure
                }
                else {
                    want = proceed();
                }
            }
        }
        return want;
    }
    Want proceed() noexcept override final
    {
        auto& s = *this;
        REALM_ASSERT(!s.is_complete());
        REALM_ASSERT(!s.is_canceled());
        REALM_ASSERT(!s.m_error_code);
        REALM_ASSERT(s.m_curr < s.m_end);
        REALM_ASSERT(!s.m_is_read_some || s.m_curr == m_begin);
        for (;;) {
            // Read into callers buffer
            char* buffer = s.m_curr;
            std::size_t size = std::size_t(s.m_end - s.m_curr);
            Want want = Want::nothing;
            std::size_t n = s.m_stream->do_read_some_async(buffer, size, s.m_error_code, want);
            REALM_ASSERT(n > 0 || s.m_error_code || want != Want::nothing); // No busy loop, please
            bool got_nothing = (n == 0);
            if (got_nothing) {
                if (REALM_UNLIKELY(s.m_error_code)) {
                    s.set_is_complete(true); // Failure
                    return Want::nothing;
                }
                // Got nothing, but want something
                return want;
            }
            // Check for completion
            REALM_ASSERT(n <= size);
            s.m_curr += n;
            if (s.m_is_read_some || s.m_curr == s.m_end) {
                s.set_is_complete(true); // Success
                return Want::nothing;
            }
            if (want != Want::nothing)
                return want;
        }
    }
protected:
    const bool m_is_read_some;
    char* const m_begin;    // May be dangling after cancellation
    char* const m_end;      // May be dangling after cancellation
    char* m_curr = m_begin; // May be dangling after cancellation
};

template<class S> class io_service::BasicStreamOps<S>::WriteOperBase: public StreamOper {
public:
    WriteOperBase(std::size_t size, S& stream, bool is_write_some,
                  const char* begin, const char* end) noexcept:
        StreamOper(size, stream),
        m_is_write_some(is_write_some),
        m_begin(begin),
        m_end(end)
    {
    }
    Want initiate() noexcept
    {
        auto& s = *this;
        REALM_ASSERT(this == s.m_stream->m_write_oper.get());
        REALM_ASSERT(!s.is_complete());
        REALM_ASSERT(s.m_curr <= s.m_end);
        Want want = Want::nothing;
        if (REALM_UNLIKELY(s.m_curr == s.m_end)) {
            s.set_is_complete(true); // Success
        }
        else {
            s.m_stream->do_init_write_async(s.m_error_code, want);
            if (want == Want::nothing) {
                if (REALM_UNLIKELY(s.m_error_code)) {
                    s.set_is_complete(true); // Failure
                }
                else {
                    want = proceed();
                }
            }
        }
        return want;
    }
    Want proceed() noexcept override final
    {
        auto& s = *this;
        REALM_ASSERT(!s.is_complete());
        REALM_ASSERT(!s.is_canceled());
        REALM_ASSERT(!s.m_error_code);
        REALM_ASSERT(s.m_curr < s.m_end);
        REALM_ASSERT(!s.m_is_write_some || s.m_curr == s.m_begin);
        for (;;) {
            // Write from callers buffer
            const char* data = s.m_curr;
            std::size_t size = std::size_t(s.m_end - s.m_curr);
            Want want = Want::nothing;
            std::size_t n = s.m_stream->do_write_some_async(data, size, s.m_error_code, want);
            REALM_ASSERT(n > 0 || s.m_error_code || want != Want::nothing); // No busy loop, please
            bool wrote_nothing = (n == 0);
            if (wrote_nothing) {
                if (REALM_UNLIKELY(s.m_error_code)) {
                    s.set_is_complete(true); // Failure
                    return Want::nothing;
                }
                // Wrote nothing, but want something written
                return want;
            }
            // Check for completion
            REALM_ASSERT(n <= size);
            s.m_curr += n;
            if (s.m_is_write_some || s.m_curr == s.m_end) {
                s.set_is_complete(true); // Success
                return Want::nothing;
            }
            if (want != Want::nothing)
                return want;
        }
    }
protected:
    const bool m_is_write_some;
    const char* const m_begin;    // May be dangling after cancellation
    const char* const m_end;      // May be dangling after cancellation
    const char* m_curr = m_begin; // May be dangling after cancellation
};

template<class S> class io_service::BasicStreamOps<S>::BufferedReadOperBase: public StreamOper {
public:
    BufferedReadOperBase(std::size_t size, S& stream, char* begin, char* end, int delim,
                         ReadAheadBuffer& rab) noexcept:
        StreamOper(size, stream),
        m_read_ahead_buffer(rab),
        m_begin(begin),
        m_end(end),
        m_delim(delim)
    {
    }
    Want initiate() noexcept
    {
        auto& s = *this;
        REALM_ASSERT(this == s.m_stream->m_read_oper.get());
        REALM_ASSERT(!s.is_complete());
        Want want = Want::nothing;
        bool complete = s.m_read_ahead_buffer.read(s.m_curr, s.m_end, s.m_delim, s.m_error_code);
        if (complete) {
            s.set_is_complete(true); // Success or failure
        }
        else {
            s.m_stream->do_init_read_async(s.m_error_code, want);
            if (want == Want::nothing) {
                if (REALM_UNLIKELY(s.m_error_code)) {
                    s.set_is_complete(true); // Failure
                }
                else {
                    want = proceed();
                }
            }
        }
        return want;
    }
    Want proceed() noexcept override final
    {
        auto& s = *this;
        REALM_ASSERT(!s.is_complete());
        REALM_ASSERT(!s.is_canceled());
        REALM_ASSERT(!s.m_error_code);
        REALM_ASSERT(s.m_read_ahead_buffer.empty());
        REALM_ASSERT(s.m_curr < s.m_end);
        for (;;) {
            // Fill read-ahead buffer from stream (is empty now)
            Want want = Want::nothing;
            bool nonempty = s.m_read_ahead_buffer.refill_async(*s.m_stream, s.m_error_code, want);
            REALM_ASSERT(nonempty || s.m_error_code ||
                         want != Want::nothing); // No busy loop, please
            bool got_nothing = !nonempty;
            if (got_nothing) {
                if (REALM_UNLIKELY(s.m_error_code)) {
                    s.set_is_complete(true); // Failure
                    return Want::nothing;
                }
                // Got nothing, but want something
                return want;
            }
            // Transfer buffered data to callers buffer
            bool complete =
                s.m_read_ahead_buffer.read(s.m_curr, s.m_end, s.m_delim, s.m_error_code);
            if (complete) {
                s.set_is_complete(true); // Success or failure (delim_not_found)
                return Want::nothing;
            }
            if (want != Want::nothing)
                return want;
        }
    }
protected:
    ReadAheadBuffer& m_read_ahead_buffer; // May be dangling after cancellation
    char* const m_begin;                  // May be dangling after cancellation
    char* const m_end;                    // May be dangling after cancellation
    char* m_curr = m_begin;               // May be dangling after cancellation
    const int m_delim;
};

template<class S> template<class H>
class io_service::BasicStreamOps<S>::ReadOper: public ReadOperBase {
public:
    ReadOper(std::size_t size, S& stream, bool is_read_some, char* begin, char* end, H handler):
        ReadOperBase(size, stream, is_read_some, begin, end),
        m_handler(std::move(handler))
    {
    }
    void recycle_and_execute() override final
    {
        auto& s = *this;
        REALM_ASSERT(s.is_complete() || s.is_canceled());
        REALM_ASSERT(s.is_complete() == (s.m_error_code || s.m_curr == s.m_end ||
                                         (s.m_is_read_some && s.m_curr != s.m_begin)));
        REALM_ASSERT(s.m_curr >= s.m_begin);
        bool orphaned = !s.m_stream;
        std::error_code ec = s.m_error_code;
        if (s.is_canceled())
            ec = error::operation_aborted;
        std::size_t num_bytes_transferred = std::size_t(s.m_curr - s.m_begin);
        // Note: do_recycle_and_execute() commits suicide.
        s.template do_recycle_and_execute<H>(orphaned, s.m_handler, ec,
                                             num_bytes_transferred); // Throws
    }
private:
    H m_handler;
};

template<class S> template<class H>
class io_service::BasicStreamOps<S>::WriteOper: public WriteOperBase {
public:
    WriteOper(std::size_t size, S& stream, bool is_write_some,
              const char* begin, const char* end, H handler):
        WriteOperBase(size, stream, is_write_some, begin, end),
        m_handler(std::move(handler))
    {
    }
    void recycle_and_execute() override final
    {
        auto& s = *this;
        REALM_ASSERT(s.is_complete() || s.is_canceled());
        REALM_ASSERT(s.is_complete() == (s.m_error_code || s.m_curr == s.m_end ||
                                         (s.m_is_write_some && s.m_curr != s.m_begin)));
        REALM_ASSERT(s.m_curr >= s.m_begin);
        bool orphaned = !s.m_stream;
        std::error_code ec = s.m_error_code;
        if (s.is_canceled())
            ec = error::operation_aborted;
        std::size_t num_bytes_transferred = std::size_t(s.m_curr - s.m_begin);
        // Note: do_recycle_and_execute() commits suicide.
        s.template do_recycle_and_execute<H>(orphaned, s.m_handler, ec,
                                             num_bytes_transferred); // Throws
    }
private:
    H m_handler;
};

template<class S> template<class H>
class io_service::BasicStreamOps<S>::BufferedReadOper: public BufferedReadOperBase {
public:
    BufferedReadOper(std::size_t size, S& stream, char* begin, char* end, int delim,
                     ReadAheadBuffer& rab, H handler):
        BufferedReadOperBase(size, stream, begin, end, delim, rab),
        m_handler(std::move(handler))
    {
    }
    void recycle_and_execute() override final
    {
        auto& s = *this;
        REALM_ASSERT(s.is_complete() || (s.is_canceled() && !s.m_error_code));
        REALM_ASSERT(s.is_canceled() || s.m_error_code ||
                     (s.m_delim != std::char_traits<char>::eof() ?
                      s.m_curr > s.m_begin && s.m_curr[-1] ==
                      std::char_traits<char>::to_char_type(s.m_delim) :
                      s.m_curr == s.m_end));
        REALM_ASSERT(s.m_curr >= s.m_begin);
        bool orphaned = !s.m_stream;
        std::error_code ec = s.m_error_code;
        if (s.is_canceled())
            ec = error::operation_aborted;
        std::size_t num_bytes_transferred = std::size_t(s.m_curr - s.m_begin);
        // Note: do_recycle_and_execute() commits suicide.
        s.template do_recycle_and_execute<H>(orphaned, s.m_handler, ec,
                                             num_bytes_transferred); // Throws
    }
private:
    H m_handler;
};

template<class H> inline void io_service::post(H handler)
{
    do_post(&io_service::post_oper_constr<H>, sizeof (post_oper<H>), &handler);
}

inline void io_service::OwnersOperDeleter::operator()(async_oper* op) const noexcept
{
    if (op->in_use()) {
        op->orphan();
    }
    else {
        void* addr = op;
        op->~async_oper();
        delete[] static_cast<char*>(addr);
    }
}

inline void io_service::LendersOperDeleter::operator()(async_oper* op) const noexcept
{
    op->recycle(); // Suicide
}

template<class Oper, class... Args> std::unique_ptr<Oper, io_service::LendersOperDeleter>
io_service::alloc(OwnersOperPtr& owners_ptr, Args&&... args)
{
    void* addr = owners_ptr.get();
    size_t size;
    if (REALM_LIKELY(addr)) {
        REALM_ASSERT(!owners_ptr->in_use());
        size = owners_ptr->m_size;
        // We can use static dispatch in the destructor call here, since an
        // object, that is not in use, is always an instance of UnusedOper.
        REALM_ASSERT(dynamic_cast<UnusedOper*>(owners_ptr.get()));
        static_cast<UnusedOper*>(owners_ptr.get())->UnusedOper::~UnusedOper();
        if (REALM_UNLIKELY(size < sizeof (Oper))) {
            owners_ptr.release();
            delete[] static_cast<char*>(addr);
            goto no_object;
        }
    }
    else {
      no_object:
        addr = new char[sizeof (Oper)]; // Throws
        size = sizeof (Oper);
        owners_ptr.reset(static_cast<async_oper*>(addr));
    }
    std::unique_ptr<Oper, LendersOperDeleter> lenders_ptr;
    try {
        lenders_ptr.reset(new (addr) Oper(size, std::forward<Args>(args)...)); // Throws
    }
    catch (...) {
        new (addr) UnusedOper(size); // Does not throw
        throw;
    }
    return lenders_ptr;
}

template<class Oper>
inline void io_service::execute(std::unique_ptr<Oper, LendersOperDeleter>& lenders_ptr)
{
    lenders_ptr.release()->recycle_and_execute(); // Throws
}

template<class Oper, class... Args>
inline void io_service::initiate_io_oper(std::unique_ptr<Oper, LendersOperDeleter> op,
                                         Args&&... args)
{
    socket_base& sock = op->get_socket();
    io_service& service = sock.get_io_service();
    Want want = op->initiate(std::forward<Args>(args)...);
    switch (want) {
        case Want::nothing:
            service.add_completed_oper(std::move(op));
            return;
        case Want::read:
            service.add_io_oper(sock.get_sock_fd(), std::move(op), io_op_Read); // Throws
            return;
        case Want::write:
            service.add_io_oper(sock.get_sock_fd(), std::move(op), io_op_Write); // Throws
            return;
    }
    REALM_ASSERT(false);
}

template<class H> inline io_service::post_oper_base*
io_service::post_oper_constr(void* addr, size_t size, impl& serv, void* cookie)
{
    H& handler = *static_cast<H*>(cookie);
    return new (addr) post_oper<H>(size, serv, std::move(handler)); // Throws
}

inline bool io_service::async_oper::in_use() const noexcept
{
    return m_in_use;
}

inline bool io_service::async_oper::is_complete() const noexcept
{
    return m_complete;
}

inline bool io_service::async_oper::is_uncanceled() const noexcept
{
    return m_in_use && !m_canceled;
}

inline void io_service::async_oper::cancel() noexcept
{
    REALM_ASSERT(m_in_use);
    REALM_ASSERT(!m_canceled);
    m_canceled = true;
}

inline io_service::async_oper::async_oper(size_t size, bool is_in_use) noexcept:
    m_size(size),
    m_in_use(is_in_use)
{
}

inline bool io_service::async_oper::is_canceled() const noexcept
{
    return m_canceled;
}

inline void io_service::async_oper::set_is_complete(bool value) noexcept
{
    REALM_ASSERT(!m_complete);
    REALM_ASSERT(!value || m_in_use);
    m_complete = value;
}

template<class H, class... Args>
inline void io_service::async_oper::do_recycle_and_execute(bool orphaned, H& handler,
                                                           Args&&... args)
{
    // Recycle the operation object before the handler is exceuted, such that
    // the memory is available for a new post operation that might be initiated
    // during the execution of the handler.
    bool was_recycled = false;
    try {
        H handler_2 = std::move(handler); // Throws
        // The caller (various subclasses of `async_oper`) must not pass any
        // arguments to the completion handler by reference if they refer to
        // this operation object, or parts of it. Due to the recycling of the
        // operation object (`do_recycle()`), such references would become
        // dangling before the invocation of the completion handler. Due to
        // `std::decay`, the following tuple will introduce a copy of all
        // nonconst lvalue reference arguments, preventing such references from
        // being passed through.
        std::tuple<typename std::decay<Args>::type...> copy_of_args(args...); // Throws
        do_recycle(orphaned);
        was_recycled = true;
        util::call_with_tuple(std::move(handler_2), std::move(copy_of_args)); // Throws
    }
    catch (...) {
        if (!was_recycled)
            do_recycle(orphaned);
        throw;
    }
}

inline void io_service::async_oper::do_recycle(bool orphaned) noexcept
{
    REALM_ASSERT(in_use());
    void* addr = this;
    size_t size = m_size;
    this->~async_oper(); // Suicide
    if (orphaned) {
        delete[] static_cast<char*>(addr);
    }
    else {
        new (addr) UnusedOper(size);
    }
}

// ---------------- resolver ----------------

inline resolver::resolver(io_service& serv):
    m_service(serv)
{
}

inline io_service& resolver::get_io_service() noexcept
{
    return m_service;
}

inline void resolver::resolve(const query& q, endpoint::list& l)
{
    std::error_code ec;
    if (resolve(q, l, ec))
        throw std::system_error(ec);
}

inline resolver::query::query(std::string service_port, int init_flags):
    m_flags(init_flags),
    m_service(service_port)
{
}

inline resolver::query::query(const stream_protocol& prot, std::string service_port,
                              int init_flags):
    m_flags(init_flags),
    m_protocol(prot),
    m_service(service_port)
{
}

inline resolver::query::query(std::string host_name, std::string service_port, int init_flags):
    m_flags(init_flags),
    m_host(host_name),
    m_service(service_port)
{
}

inline resolver::query::query(const stream_protocol& prot, std::string host_name,
                              std::string service_port, int init_flags):
    m_flags(init_flags),
    m_protocol(prot),
    m_host(host_name),
    m_service(service_port)
{
}

inline resolver::query::~query() noexcept
{
}

inline int resolver::query::flags() const
{
    return m_flags;
}

inline stream_protocol resolver::query::protocol() const
{
    return m_protocol;
}

inline std::string resolver::query::host() const
{
    return m_host;
}

inline std::string resolver::query::service() const
{
    return m_service;
}

// ---------------- socket_base ----------------

inline socket_base::socket_base(io_service& s):
    m_sock_fd(-1),
    m_service(s)
{
}

inline socket_base::~socket_base() noexcept
{
    close();
}

inline io_service& socket_base::get_io_service() noexcept
{
    return m_service;
}

inline bool socket_base::is_open() const noexcept
{
    return m_sock_fd != -1;
}

inline void socket_base::open(const stream_protocol& prot)
{
    std::error_code ec;
    if (open(prot, ec))
        throw std::system_error(ec);
}

inline void socket_base::close() noexcept
{
    if (!is_open())
        return;
    cancel();
    do_close();
}

template<class O>
inline void socket_base::get_option(O& opt) const
{
    std::error_code ec;
    if (get_option(opt, ec))
        throw std::system_error(ec);
}

template<class O>
inline std::error_code socket_base::get_option(O& opt, std::error_code& ec) const
{
    opt.get(*this, ec);
    return ec;
}

template<class O>
inline void socket_base::set_option(const O& opt)
{
    std::error_code ec;
    if (set_option(opt, ec))
        throw std::system_error(ec);
}

template<class O>
inline std::error_code socket_base::set_option(const O& opt, std::error_code& ec)
{
    opt.set(*this, ec);
    return ec;
}

inline void socket_base::bind(const endpoint& ep)
{
    std::error_code ec;
    if (bind(ep, ec))
        throw std::system_error(ec);
}

inline endpoint socket_base::local_endpoint() const
{
    std::error_code ec;
    endpoint ep = local_endpoint(ec);
    if (ec)
        throw std::system_error(ec);
    return ep;
}

inline int socket_base::get_sock_fd() const noexcept
{
    return m_sock_fd;
}

inline const stream_protocol& socket_base::get_protocol() const noexcept
{
    return m_protocol;
}

inline std::error_code socket_base::ensure_blocking_mode(std::error_code& ec) noexcept
{
    // Assuming that sockets are either used mostly in blocking mode, or mostly
    // in nonblocking mode.
    if (REALM_UNLIKELY(!m_in_blocking_mode)) {
        bool enable = false;
        if (set_nonblocking_mode(enable, ec))
            return ec;
        m_in_blocking_mode = true;
    }
    return std::error_code(); // Success
}

inline std::error_code socket_base::ensure_nonblocking_mode(std::error_code& ec) noexcept
{
    // Assuming that sockets are either used mostly in blocking mode, or mostly
    // in nonblocking mode.
    if (REALM_UNLIKELY(m_in_blocking_mode)) {
        bool enable = true;
        if (set_nonblocking_mode(enable, ec))
            return ec;
        m_in_blocking_mode = false;
    }
    return std::error_code(); // Success
}

template<class T, int opt, class U>
inline socket_base::option<T, opt, U>::option(T init_value):
    m_value(init_value)
{
}

template<class T, int opt, class U>
inline T socket_base::option<T, opt, U>::value() const
{
    return m_value;
}

template<class T, int opt, class U>
inline void socket_base::option<T, opt, U>::get(const socket_base& sock, std::error_code& ec)
{
    union {
        U value;
        char strut[sizeof (U) + 1];
    };
    size_t value_size = sizeof strut;
    sock.get_option(opt_enum(opt), &value, value_size, ec);
    if (!ec) {
        REALM_ASSERT(value_size == sizeof value);
        m_value = T(value);
    }
}

template<class T, int opt, class U>
inline void socket_base::option<T, opt, U>::set(socket_base& sock, std::error_code& ec) const
{
    U value_to_set = U(m_value);
    sock.set_option(opt_enum(opt), &value_to_set, sizeof value_to_set, ec);
}

// ---------------- socket ----------------

class socket::ConnectOperBase: public io_service::IoOper {
public:
    ConnectOperBase(std::size_t size, socket& sock) noexcept:
        IoOper(size),
        m_socket(&sock)
    {
    }
    Want initiate(const endpoint& ep) noexcept
    {
        REALM_ASSERT(this == m_socket->m_write_oper.get());
        if (m_socket->initiate_async_connect(ep, m_error_code)) {
            set_is_complete(true); // Failure, or immediate completion
            return Want::nothing;
        }
        return Want::write;
    }
    Want proceed() noexcept override final
    {
        REALM_ASSERT(!is_complete());
        REALM_ASSERT(!is_canceled());
        REALM_ASSERT(!m_error_code);
        m_socket->finalize_async_connect(m_error_code);
        set_is_complete(true);
        return Want::nothing;
    }
    void recycle() noexcept override final
    {
        bool orphaned = !m_socket;
        // Note: do_recycle() commits suicide.
        do_recycle(orphaned);
    }
    void orphan() noexcept override final
    {
        m_socket = nullptr;
    }
    socket_base& get_socket() noexcept
    {
        return *m_socket;
    }
protected:
    socket* m_socket;
    std::error_code m_error_code;
};

template<class H> class socket::ConnectOper: public ConnectOperBase {
public:
    ConnectOper(size_t size, socket& sock, H handler):
        ConnectOperBase(size, sock),
        m_handler(std::move(handler))
    {
    }
    void recycle_and_execute() override final
    {
        REALM_ASSERT(is_complete() || (is_canceled() && !m_error_code));
        bool orphaned = !m_socket;
        std::error_code ec = m_error_code;
        if (is_canceled())
            ec = error::operation_aborted;
        // Note: do_recycle_and_execute() commits suicide.
        do_recycle_and_execute<H>(orphaned, m_handler, ec); // Throws
    }
private:
    H m_handler;
};

inline socket::socket(io_service& s):
    socket_base(s)
{
}

inline socket::socket(io_service& s, const stream_protocol& prot, native_handle_type native_socket):
    socket_base(s)
{
    assign(prot, native_socket); // Throws
}

inline socket::~socket() noexcept
{
}

inline void socket::connect(const endpoint& ep)
{
    std::error_code ec;
    if (connect(ep, ec))
        throw std::system_error(ec);
}

inline std::size_t socket::read(char* buffer, std::size_t size)
{
    std::error_code ec;
    read(buffer, size, ec);
    if (ec)
        throw std::system_error(ec);
    return size;
}

inline std::size_t socket::read(char* buffer, std::size_t size, std::error_code& ec) noexcept
{
    return StreamOps::read(*this, buffer, size, ec);
}

inline std::size_t socket::read(char* buffer, std::size_t size, ReadAheadBuffer& rab)
{
    std::error_code ec;
    read(buffer, size, rab, ec);
    if (ec)
        throw std::system_error(ec);
    return size;
}

inline std::size_t socket::read(char* buffer, std::size_t size, ReadAheadBuffer& rab,
                                std::error_code& ec) noexcept
{
    int delim = std::char_traits<char>::eof();
    return StreamOps::buffered_read(*this, buffer, size, delim, rab, ec);
}

inline std::size_t socket::read_until(char* buffer, std::size_t size, char delim,
                                      ReadAheadBuffer& rab)
{
    std::error_code ec;
    std::size_t n = read_until(buffer, size, delim, rab, ec);
    if (ec)
        throw std::system_error(ec);
    return n;
}

inline std::size_t socket::read_until(char* buffer, std::size_t size, char delim,
                                      ReadAheadBuffer& rab, std::error_code& ec) noexcept
{
    int delim_2 = std::char_traits<char>::to_int_type(delim);
    return StreamOps::buffered_read(*this, buffer, size, delim_2, rab, ec);
}

inline std::size_t socket::write(const char* data, std::size_t size)
{
    std::error_code ec;
    write(data, size, ec);
    if (ec)
        throw std::system_error(ec);
    return size;
}

inline std::size_t socket::write(const char* data, std::size_t size, std::error_code& ec) noexcept
{
    return StreamOps::write(*this, data, size, ec);
}

inline std::size_t socket::read_some(char* buffer, std::size_t size)
{
    std::error_code ec;
    std::size_t n = read_some(buffer, size, ec);
    if (ec)
        throw std::system_error(ec);
    return n;
}

inline std::size_t socket::read_some(char* buffer, std::size_t size, std::error_code& ec) noexcept
{
    return StreamOps::read_some(*this, buffer, size, ec);
}

inline std::size_t socket::write_some(const char* data, std::size_t size)
{
    std::error_code ec;
    std::size_t n = write_some(data, size, ec);
    if (ec)
        throw std::system_error(ec);
    return n;
}

inline std::size_t socket::write_some(const char* data, std::size_t size,
                                      std::error_code& ec) noexcept
{
    return StreamOps::write_some(*this, data, size, ec);
}

template<class H> inline void socket::async_connect(const endpoint& ep, H handler)
{
    LendersConnectOperPtr op =
        io_service::alloc<ConnectOper<H>>(m_write_oper, *this, std::move(handler)); // Throws
    io_service::initiate_io_oper(std::move(op), ep); // Throws
}

template<class H> inline void socket::async_read(char* buffer, std::size_t size, H handler)
{
    bool is_read_some = false;
    StreamOps::async_read(*this, buffer, size, is_read_some, std::move(handler)); // Throws
}

template<class H>
inline void socket::async_read(char* buffer, std::size_t size, ReadAheadBuffer& rab, H handler)
{
    int delim = std::char_traits<char>::eof();
    StreamOps::async_buffered_read(*this, buffer, size, delim, rab, std::move(handler)); // Throws
}

template<class H>
inline void socket::async_read_until(char* buffer, std::size_t size, char delim,
                                     ReadAheadBuffer& rab, H handler)
{
    int delim_2 = std::char_traits<char>::to_int_type(delim);
    StreamOps::async_buffered_read(*this, buffer, size, delim_2, rab, std::move(handler)); // Throws
}

template<class H> inline void socket::async_write(const char* data, std::size_t size, H handler)
{
    bool is_write_some = false;
    StreamOps::async_write(*this, data, size, is_write_some, std::move(handler)); // Throws
}

template<class H> inline void socket::async_read_some(char* buffer, size_t size, H handler)
{
    bool is_read_some = true;
    StreamOps::async_read(*this, buffer, size, is_read_some, std::move(handler)); // Throws
}

template<class H>
inline void socket::async_write_some(const char* data, std::size_t size, H handler)
{
    bool is_write_some = true;
    StreamOps::async_write(*this, data, size, is_write_some, std::move(handler)); // Throws
}

inline void socket::shutdown(shutdown_type what)
{
    std::error_code ec;
    if (shutdown(what, ec))
        throw std::system_error(ec);
}

inline void socket::assign(const stream_protocol& prot, native_handle_type native_socket)
{
    std::error_code ec;
    if (assign(prot, native_socket, ec)) // Throws
        throw std::system_error(ec);
}

inline std::error_code socket::assign(const stream_protocol& prot,
                                      native_handle_type native_socket, std::error_code& ec)
{
    return do_assign(prot, native_socket, ec); // Throws
}

inline socket& socket::lowest_layer() noexcept
{
    return *this;
}

inline void socket::do_init_read_sync(std::error_code& ec) noexcept
{
    ensure_blocking_mode(ec);
}

inline void socket::do_init_write_sync(std::error_code& ec) noexcept
{
    ensure_blocking_mode(ec);
}

inline void socket::do_init_read_async(std::error_code& ec, Want& want) noexcept
{
    if (REALM_UNLIKELY(ensure_nonblocking_mode(ec))) {
        want = Want::nothing; // Failure
        return;
    }
    want = Want::read; // Wait for read readiness before proceeding
}

inline void socket::do_init_write_async(std::error_code& ec, Want& want) noexcept
{
    if (REALM_UNLIKELY(ensure_nonblocking_mode(ec))) {
        want = Want::nothing; // Failure
        return;
    }
    want = Want::write; // Wait for write readiness before proceeding
}

inline std::size_t socket::do_read_some_async(char* buffer, std::size_t size,
                                              std::error_code& ec, Want& want) noexcept
{
    std::error_code ec_2;
    std::size_t n = do_read_some_sync(buffer, size, ec_2);
    if (REALM_UNLIKELY(ec_2 && ec_2 != error::resource_unavailable_try_again)) {
        ec = ec_2;
        want = Want::nothing; // Failure
        return 0;
    }
    ec = std::error_code();
    want = Want::read; // Success
    return n;
}

inline std::size_t socket::do_write_some_async(const char* data, std::size_t size,
                                               std::error_code& ec, Want& want) noexcept
{
    std::error_code ec_2;
    std::size_t n = do_write_some_sync(data, size, ec_2);
    if (REALM_UNLIKELY(ec_2 && ec_2 != error::resource_unavailable_try_again)) {
        ec = ec_2;
        want = Want::nothing; // Failure
        return 0;
    }
    ec = std::error_code();
    want = Want::write; // Success
    return n;
}

// ---------------- acceptor ----------------

class acceptor::AcceptOperBase: public io_service::IoOper {
public:
    AcceptOperBase(std::size_t size, acceptor& a, socket& s, endpoint* e):
        IoOper(size),
        m_acceptor(&a),
        m_socket(s),
        m_endpoint(e)
    {
    }
    Want initiate() noexcept
    {
        REALM_ASSERT(this == m_acceptor->m_read_oper.get());
        REALM_ASSERT(!is_complete());
        if (m_acceptor->ensure_nonblocking_mode(m_error_code)) {
            set_is_complete(true); // Failure
            return Want::nothing;
        }
        return Want::read;
    }
    Want proceed() noexcept override final
    {
        REALM_ASSERT(!is_complete());
        REALM_ASSERT(!is_canceled());
        REALM_ASSERT(!m_error_code);
        REALM_ASSERT(!m_socket.is_open());
        Want want = m_acceptor->do_accept_async(m_socket, m_endpoint, m_error_code);
        if (want == Want::nothing)
            set_is_complete(true); // Success or failure
        return want;
    }
    void recycle() noexcept override final
    {
        bool orphaned = !m_acceptor;
        // Note: do_recycle() commits suicide.
        do_recycle(orphaned);
    }
    void orphan() noexcept override final
    {
        m_acceptor = 0;
    }
    socket_base& get_socket() noexcept
    {
        return *m_acceptor;
    }
protected:
    acceptor* m_acceptor;
    socket& m_socket;           // May be dangling after cancellation
    endpoint* const m_endpoint; // May be dangling after cancellation
    std::error_code m_error_code;
};

template<class H> class acceptor::AcceptOper: public AcceptOperBase {
public:
    AcceptOper(size_t size, acceptor& a, socket& s, endpoint* e, H handler):
        AcceptOperBase(size, a, s, e),
        m_handler(std::move(handler))
    {
    }
    void recycle_and_execute() override final
    {
        REALM_ASSERT(is_complete() || (is_canceled() && !m_error_code));
        REALM_ASSERT(is_canceled() || m_error_code || m_socket.is_open());
        bool orphaned = !m_acceptor;
        std::error_code ec = m_error_code;
        if (is_canceled())
            ec = error::operation_aborted;
        // Note: do_recycle_and_execute() commits suicide.
        do_recycle_and_execute<H>(orphaned, m_handler, ec); // Throws
    }
private:
    H m_handler;
};

inline acceptor::acceptor(io_service& s):
    socket_base(s)
{
}

inline acceptor::~acceptor() noexcept
{
}

inline void acceptor::listen(int backlog)
{
    std::error_code ec;
    if (listen(backlog, ec))
        throw std::system_error(ec);
}

inline void acceptor::accept(socket& sock)
{
    std::error_code ec;
    if (accept(sock, ec)) // Throws
        throw std::system_error(ec);
}

inline void acceptor::accept(socket& sock, endpoint& ep)
{
    std::error_code ec;
    if (accept(sock, ep, ec)) // Throws
        throw std::system_error(ec);
}

inline std::error_code acceptor::accept(socket& sock, std::error_code& ec)
{
    endpoint* ep = nullptr;
    return accept(sock, ep, ec); // Throws
}

inline std::error_code acceptor::accept(socket& sock, endpoint& ep, std::error_code& ec)
{
    return accept(sock, &ep, ec); // Throws
}

template<class H> inline void acceptor::async_accept(socket& sock, H handler)
{
    endpoint* ep = nullptr;
    async_accept(sock, ep, std::move(handler)); // Throws
}

template<class H> inline void acceptor::async_accept(socket& sock, endpoint& ep, H handler)
{
    async_accept(sock, &ep, std::move(handler)); // Throws
}

inline std::error_code acceptor::accept(socket& sock, endpoint* ep, std::error_code& ec)
{
    REALM_ASSERT(!m_read_oper || !m_read_oper->in_use());
    if (REALM_UNLIKELY(sock.is_open()))
        throw std::runtime_error("Socket is already open");
    if (ensure_blocking_mode(ec))
        return ec;
    do_accept_sync(sock, ep, ec);
    return ec;
}

inline acceptor::Want acceptor::do_accept_async(socket& socket, endpoint* ep, std::error_code& ec) noexcept
{
    std::error_code ec_2;
    do_accept_sync(socket, ep, ec_2);
    if (ec_2 == error::resource_unavailable_try_again)
        return Want::read;
    ec = ec_2;
    return Want::nothing;
}

template<class H> inline void acceptor::async_accept(socket& sock, endpoint* ep, H handler)
{
    if (REALM_UNLIKELY(sock.is_open()))
        throw std::runtime_error("Socket is already open");
    LendersAcceptOperPtr op = io_service::alloc<AcceptOper<H>>(m_read_oper, *this, sock, ep,
                                                               std::move(handler)); // Throws
    io_service::initiate_io_oper(std::move(op)); // Throws
}

// ---------------- deadline_timer ----------------

template<class H>
class deadline_timer::wait_oper:
        public io_service::wait_oper_base {
public:
    wait_oper(size_t size, deadline_timer& timer, clock::time_point expiration_time, H handler):
        io_service::wait_oper_base(size, timer, expiration_time),
        m_handler(std::move(handler))
    {
    }
    void recycle_and_execute() override final
    {
        bool orphaned = !m_timer;
        std::error_code ec;
        if (is_canceled())
            ec = error::operation_aborted;
        // Note: do_recycle_and_execute() commits suicide.
        do_recycle_and_execute<H>(orphaned, m_handler, ec); // Throws
    }
private:
    H m_handler;
};

inline deadline_timer::deadline_timer(io_service& serv):
    m_service(serv)
{
}

inline deadline_timer::~deadline_timer() noexcept
{
    cancel();
}

inline io_service& deadline_timer::get_io_service() noexcept
{
    return m_service;
}

template<class R, class P, class H>
inline void deadline_timer::async_wait(std::chrono::duration<R,P> delay, H handler)
{
    clock::time_point now = clock::now();
    auto max_add = clock::time_point::max() - now;
    if (delay > max_add)
        throw std::runtime_error("Expiration time overflow");
    clock::time_point expiration_time = now + delay;
    io_service::LendersWaitOperPtr op =
        io_service::alloc<wait_oper<H>>(m_wait_oper, *this, expiration_time,
                                        std::move(handler)); // Throws
    m_service.add_wait_oper(std::move(op)); // Throws
}

// ---------------- ReadAheadBuffer ----------------

inline ReadAheadBuffer::ReadAheadBuffer():
    m_buffer(new char[s_size]) // Throws
{
}

inline void ReadAheadBuffer::clear() noexcept
{
    m_begin = nullptr;
    m_end   = nullptr;
}

inline bool ReadAheadBuffer::empty() const noexcept
{
    return (m_begin == m_end);
}

template<class S> inline void ReadAheadBuffer::refill_sync(S& stream, std::error_code& ec) noexcept
{
    char* buffer = m_buffer.get();
    std::size_t size = s_size;
    static_assert(noexcept(stream.do_read_some_sync(buffer, size, ec)), "");
    std::size_t n = stream.do_read_some_sync(buffer, size, ec);
    if (REALM_UNLIKELY(n == 0))
        return;
    REALM_ASSERT(!ec);
    REALM_ASSERT(n <= size);
    m_begin = m_buffer.get();
    m_end   = m_begin + n;
}

template<class S>
inline bool ReadAheadBuffer::refill_async(S& stream, std::error_code& ec, Want& want) noexcept
{
    char* buffer = m_buffer.get();
    std::size_t size = s_size;
    static_assert(noexcept(stream.do_read_some_async(buffer, size, ec, want)), "");
    std::size_t n = stream.do_read_some_async(buffer, size, ec, want);
    if (n == 0)
        return false;
    REALM_ASSERT(!ec);
    REALM_ASSERT(n <= size);
    m_begin = m_buffer.get();
    m_end   = m_begin + n;
    return true;
}

} // namespace network
} // namespace util
} // namespace realm

#endif // REALM_UTIL_NETWORK_HPP
