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
#include <string>
#include <system_error>
#include <ostream>

#include <sys/types.h>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <stdio.h>
#  include <Ws2def.h>
#  pragma comment(lib, "Ws2_32.lib")
#else
#  include <sys/socket.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#endif

#include <realm/util/features.h>
#include <realm/util/assert.hpp>
#include <realm/util/buffer.hpp>
#include <realm/util/basic_system_errors.hpp>

// Linux epoll
//
// Require Linux kernel version >= 2.6.27 such that we have epoll_create1(),
// `O_CLOEXEC`, and `EPOLLRDHUP`.
#if defined(__linux__)
#  include <linux/version.h>
#  if !defined(REALM_HAVE_EPOLL)
#    if !defined(REALM_DISABLE_UTIL_NETWORK_EPOLL)
#      if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#        define REALM_HAVE_EPOLL 1
#      endif
#    endif
#  endif
#endif
#if !defined(REALM_HAVE_EPOLL)
#  define REALM_HAVE_EPOLL 0
#endif

// FreeBSD Kqueue.
//
// Available on Mac OS X, FreeBSD, NetBSD, OpenBSD
#if (defined(__MACH__) && defined(__APPLE__)) || defined(__FreeBSD__) || \
    defined(__NetBSD__) || defined(__OpenBSD__)
#  if !defined(REALM_HAVE_KQUEUE)
#    if !defined(REALM_DISABLE_UTIL_NETWORK_KQUEUE)
#      define REALM_HAVE_KQUEUE 1
#    endif
#  endif
#endif
#if !defined(REALM_HAVE_KQUEUE)
#  define REALM_HAVE_KQUEUE 0
#endif



// FIXME: Unfinished business around `Address::m_ip_v6_scope_id`.


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
/// Service, and all the objects that are associated with that instance (\ref
/// Resolver, \ref Socket`, \ref Acceptor`, \ref DeadlineTimer, and \ref
/// ssl::Stream).
///
/// In general, it is unsafe for two threads to call functions on the same
/// object, or on different objects in the same service context. This also
/// applies to destructors. Notable exceptions are the fully thread-safe
/// functions, such as Service::post(), Service::stop(), and Service::reset().
///
/// On the other hand, it is always safe for two threads to call functions on
/// objects belonging to different service contexts.
///
/// One implication of these rules is that at most one thread must execute run()
/// at any given time, and if one thread is executing run(), then no other
/// thread is allowed to access objects in the same service context (with the
/// mentioned exceptions).
///
/// Unless otherwise specified, free-standing objects, such as \ref
/// StreamProtocol, \ref Address, \ref Endpoint, and \ref Endpoint::List are
/// fully thread-safe as long as they are not mutated. If one thread is mutating
/// such an object, no other thread may access it. Note that these free-standing
/// objects are not associcated with an instance of Service, and are therefore
/// not part of a service context.
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
/// `error::operation_aborted`. This guarantee is possible to provide (and free
/// of ambiguities) precisely because this library prohibits multiple threads
/// from executing the event loop concurrently, and because `cancel()` is
/// allowed to be called only from a completion handler (executed by the event
/// loop thread) or while no thread is executing the event loop. This guarantee
/// allows for safe destruction of sockets and deadline timers as long as the
/// completion handlers react appropriately to `error::operation_aborted`, in
/// particular, that they do not attempt to access the socket or deadline timer
/// object in such cases.
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
/// remains cancelable up until the point in time where the completion handler
/// starts to execute.
namespace network {

std::string host_name();


class StreamProtocol;
class Address;
class Endpoint;
class Service;
class Resolver;
class SocketBase;
class Socket;
class Acceptor;
class DeadlineTimer;
class ReadAheadBuffer;
namespace ssl {
class Stream;
} // namespace ssl


/// \brief An IP protocol descriptor.
class StreamProtocol {
public:
    static StreamProtocol ip_v4();
    static StreamProtocol ip_v6();

    bool is_ip_v4() const;
    bool is_ip_v6() const;

    int protocol() const;
    int family() const;

    StreamProtocol();
    ~StreamProtocol() noexcept {}

private:
    int m_family;
    int m_socktype;
    int m_protocol;

    friend class Resolver;
    friend class SocketBase;
};


/// \brief An IP address (IPv4 or IPv6).
class Address {
public:
    bool is_ip_v4() const;
    bool is_ip_v6() const;

    template<class C, class T>
    friend std::basic_ostream<C,T>& operator<<(std::basic_ostream<C,T>&, const Address&);

    Address();
    ~Address() noexcept {}

private:
    using ip_v4_type = in_addr;
    using ip_v6_type = in6_addr;
    union union_type {
        ip_v4_type m_ip_v4;
        ip_v6_type m_ip_v6;
    };
    union_type m_union;
    std::uint_least32_t m_ip_v6_scope_id = 0;
    bool m_is_ip_v6 = false;

    friend Address make_address(const char*, std::error_code&) noexcept;
    friend class Endpoint;
};

Address make_address(const char* c_str);
Address make_address(const char* c_str, std::error_code& ec) noexcept;
Address make_address(const std::string&);
Address make_address(const std::string&, std::error_code& ec) noexcept;


/// \brief An IP endpoint.
///
/// An IP endpoint is a triplet (`protocol`, `address`, `port`).
class Endpoint {
public:
    using port_type = std::uint_fast16_t;
    class List;

    StreamProtocol protocol() const;
    Address address() const;
    port_type port() const;

    Endpoint();
    Endpoint(const StreamProtocol&, port_type);
    Endpoint(const Address&, port_type);
    ~Endpoint() noexcept {}

    using data_type = sockaddr;
    data_type* data();
    const data_type* data() const;

private:
    StreamProtocol m_protocol;

    using sockaddr_base_type  = sockaddr;
    using sockaddr_ip_v4_type = sockaddr_in;
    using sockaddr_ip_v6_type = sockaddr_in6;
    union sockaddr_union_type {
        sockaddr_base_type  m_base;
        sockaddr_ip_v4_type m_ip_v4;
        sockaddr_ip_v6_type m_ip_v6;
    };
    sockaddr_union_type m_sockaddr_union;

    friend class Service;
    friend class Resolver;
    friend class SocketBase;
    friend class Socket;
};


/// \brief A list of IP endpoints.
class Endpoint::List {
public:
    using iterator = const Endpoint*;

    iterator begin() const noexcept;
    iterator end() const noexcept;
    std::size_t size() const noexcept;
    bool empty() const noexcept;

    List() noexcept = default;
    List(List&&) noexcept = default;
    ~List() noexcept = default;

    List& operator=(List&&) noexcept = default;

private:
    Buffer<Endpoint> m_endpoints;

    friend class Resolver;
};


/// \brief TCP/IP networking service.
class Service {
public:
    Service();
    ~Service() noexcept;

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
    /// Syncronous operations (e.g., Socket::connect()) execute independently of
    /// the event loop, and do not require that any thread calls run().
    void run();

    /// @{ \brief Stop event loop execution.
    ///
    /// stop() puts the event loop into the stopped mode. If a thread is
    /// currently executing run(), it will be made to return in a timely
    /// fashion, that is, without further blocking. If a thread is currently
    /// blocked in run(), it will be unblocked. Handlers that can be executed
    /// immediately, may, or may not be executed before run() returns, but new
    /// handlers submitted by these, will not be executed before run()
    /// returns. Also, if a handler is submitted by a call to post, and that
    /// call happens after stop() returns, then that handler is guaranteed to
    /// not be executed before run() returns.
    ///
    /// The event loop will remain in the stopped mode until reset() is
    /// called. If reset() is called before run() returns, it may, or may not
    /// cause run() to resume normal operation without returning.
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

    template<class Oper> class OperQueue;
    class Descriptor;
    class AsyncOper;
    class WaitOperBase;
    class PostOperBase;
    template<class H> class PostOper;
    class IoOper;
    class UnusedOper; // Allocated, but currently unused memory

    template<class S> class BasicStreamOps;

    struct OwnersOperDeleter {
        void operator()(AsyncOper*) const noexcept;
    };
    struct LendersOperDeleter {
        void operator()(AsyncOper*) const noexcept;
    };
    using OwnersOperPtr      = std::unique_ptr<AsyncOper,    OwnersOperDeleter>;
    using LendersOperPtr     = std::unique_ptr<AsyncOper,    LendersOperDeleter>;
    using LendersWaitOperPtr = std::unique_ptr<WaitOperBase, LendersOperDeleter>;
    using LendersIoOperPtr   = std::unique_ptr<IoOper,       LendersOperDeleter>;

    class IoReactor;
    class Impl;
    const std::unique_ptr<Impl> m_impl;

    template<class Oper, class... Args>
    static std::unique_ptr<Oper, LendersOperDeleter> alloc(OwnersOperPtr&, Args&&...);

    template<class Oper> static void execute(std::unique_ptr<Oper, LendersOperDeleter>&);

    using PostOperConstr = PostOperBase*(void* addr, std::size_t size, Impl&, void* cookie);
    void do_post(PostOperConstr, std::size_t size, void* cookie);
    template<class H>
    static PostOperBase* post_oper_constr(void* addr, std::size_t size, Impl&, void* cookie);
    static void recycle_post_oper(Impl&, PostOperBase*) noexcept;

    using clock = std::chrono::steady_clock;

    friend class Resolver;
    friend class SocketBase;
    friend class Socket;
    friend class Acceptor;
    friend class DeadlineTimer;
    friend class ReadAheadBuffer;
    friend class ssl::Stream;
};


template<class Oper> class Service::OperQueue {
public:
    using LendersOperPtr = std::unique_ptr<Oper, LendersOperDeleter>;
    bool empty() const noexcept;
    void push_back(LendersOperPtr) noexcept;
    template<class Oper2> void push_back(OperQueue<Oper2>&) noexcept;
    LendersOperPtr pop_front() noexcept;
    void clear() noexcept;
    OperQueue() noexcept = default;
    OperQueue(OperQueue&&) noexcept;
    ~OperQueue() noexcept;
private:
    Oper* m_back = nullptr;
    template<class> friend class OperQueue;
};


class Service::Descriptor {
public:
#ifdef _WIN32
    using native_handle_type = SOCKET;
#else
    using native_handle_type = int;
#endif

    Impl& service_impl;

    Descriptor(Impl& service) noexcept;
    ~Descriptor() noexcept;

    /// \param in_blocking_mode Must be true if, and only if the passed file
    /// descriptor refers to a file description in which the file status flag
    /// O_NONBLOCK is not set.
    ///
    /// The passed file descriptor must have the file descriptor flag FD_CLOEXEC
    /// set.
    void assign(native_handle_type fd, bool in_blocking_mode) noexcept;
    void close() noexcept;

    bool is_open() const noexcept;

    native_handle_type native_handle() const noexcept;
    bool in_blocking_mode() const noexcept;

    void accept(Descriptor&, StreamProtocol, Endpoint*, std::error_code&) noexcept;
    std::size_t read_some(char* buffer, std::size_t size, std::error_code&) noexcept;
    std::size_t write_some(const char* data, std::size_t size, std::error_code&) noexcept;

    /// \tparam Oper An operation type inherited from IoOper with an initate()
    /// function that initiates the operation and figures out whether it needs
    /// to read from, or write to the underlying descriptor to
    /// proceed. `initiate()` must return Want::read if the operation needs to
    /// read, or Want::write if the operation needs to write. If the operation
    /// completes immediately (e.g. due to a failure during initialization),
    /// `initiate()` must return Want::nothing.
    template<class Oper, class... Args>
    void initiate_oper(std::unique_ptr<Oper, LendersOperDeleter>, Args&&...);

    void ensure_blocking_mode();
    void ensure_nonblocking_mode();

private:
    native_handle_type m_fd = -1;
    bool m_in_blocking_mode; // Not in nonblocking mode

#if REALM_HAVE_EPOLL || REALM_HAVE_KQUEUE
    bool m_read_ready;
    bool m_write_ready;
    bool m_imminent_end_of_input; // Kernel has seen the end of input
    bool m_is_registered;
    OperQueue<IoOper> m_suspended_read_ops, m_suspended_write_ops;

    void deregister_for_async() noexcept;
#endif

    bool assume_read_would_block() const noexcept;
    bool assume_write_would_block() const noexcept;

    void set_read_ready(bool) noexcept;
    void set_write_ready(bool) noexcept;

    void set_nonblock_flag(bool value);
    void add_initiated_oper(LendersIoOperPtr, Want);

    void do_close() noexcept;

    friend class IoReactor;
};


class Resolver {
public:
    class Query;

    Resolver(Service&);
    ~Resolver() noexcept;

    /// Thread-safe.
    Service& get_service() noexcept;

    /// @{ \brief Resolve the specified query to one or more endpoints.
    Endpoint::List resolve(const Query&);
    Endpoint::List resolve(const Query&, std::error_code&);
    /// @}

    /// \brief Perform an asynchronous resolve operation.
    ///
    /// Initiate an asynchronous resolve operation. The completion handler will
    /// be called when the operation completes. The operation completes when it
    /// succeeds, or an error occurs.
    ///
    /// The completion handler is always executed by the event loop thread,
    /// i.e., by a thread that is executing Service::run(). Conversely, the
    /// completion handler is guaranteed to not be called while no thread is
    /// executing Service::run(). The execution of the completion handler is
    /// always deferred to the event loop, meaning that it never happens as a
    /// synchronous side effect of the execution of async_resolve(), even when
    /// async_resolve() is executed by the event loop thread. The completion
    /// handler is guaranteed to be called eventually, as long as there is time
    /// enough for the operation to complete or fail, and a thread is executing
    /// Service::run() for long enough.
    ///
    /// The operation can be canceled by calling cancel(), and will be
    /// automatically canceled if the resolver object is destroyed. If the
    /// operation is canceled, it will fail with `error::operation_aborted`. The
    /// operation remains cancelable up until the point in time where the
    /// completion handler starts to execute. This means that if cancel() is
    /// called before the completion handler starts to execute, then the
    /// completion handler is guaranteed to have `error::operation_aborted`
    /// passed to it. This is true regardless of whether cancel() is called
    /// explicitly or implicitly, such as when the resolver is destroyed.
    ///
    /// The specified handler will be executed by an expression on the form
    /// `handler(ec, endpoints)` where `ec` is the error code and `endpoints` is
    /// an object of type `Endpoint::List`. If the the handler object is
    /// movable, it will never be copied. Otherwise, it will be copied as
    /// necessary.
    ///
    /// It is an error to start a new resolve operation (synchronous or
    /// asynchronous) while an asynchronous resolve operation is in progress via
    /// the same resolver object. An asynchronous resolve operation is
    /// considered complete as soon as the completion handler starts to
    /// execute. This means that a new resolve operation can be started from the
    /// completion handler.
    template<class H> void async_resolve(Query, H handler);

    /// \brief Cancel all asynchronous operations.
    ///
    /// Cause all incomplete asynchronous operations, that are associated with
    /// this resolver (at most one), to fail with `error::operation_aborted`. An
    /// asynchronous operation is complete precisely when its completion handler
    /// starts executing.
    ///
    /// Completion handlers of canceled operations will become immediately ready
    /// to execute, but will never be executed directly as part of the execution
    /// of cancel().
    ///
    /// Cancellation happens automatically when the resolver object is destroyed.
    void cancel() noexcept;

private:
    class ResolveOperBase;
    template<class H> class ResolveOper;

    using LendersResolveOperPtr = std::unique_ptr<ResolveOperBase, Service::LendersOperDeleter>;

    Service::Impl& m_service_impl;

    Service::OwnersOperPtr m_resolve_oper;

    void initiate_oper(LendersResolveOperPtr);
};


class Resolver::Query {
public:
    enum {
        /// Locally bound socket endpoint (server side)
        passive = AI_PASSIVE,

        /// Ignore families without a configured non-loopback address
        address_configured = AI_ADDRCONFIG
    };

    Query(std::string service_port, int init_flags = passive|address_configured);
    Query(const StreamProtocol&, std::string service_port,
          int init_flags = passive|address_configured);
    Query(std::string host_name, std::string service_port,
          int init_flags = address_configured);
    Query(const StreamProtocol&, std::string host_name, std::string service_port,
          int init_flags = address_configured);

    ~Query() noexcept;

    int flags() const;
    StreamProtocol protocol() const;
    std::string host() const;
    std::string service() const;

private:
    int m_flags;
    StreamProtocol m_protocol;
    std::string m_host;    // hostname
    std::string m_service; // port

    friend class Resolver;
};


class SocketBase {
public:
    using native_handle_type = Service::Descriptor::native_handle_type;

    ~SocketBase() noexcept;

    /// Thread-safe.
    Service& get_service() noexcept;

    bool is_open() const noexcept;
    native_handle_type native_handle() const noexcept;

    /// @{ \brief Open the socket for use with the specified protocol.
    ///
    /// It is an error to call open() on a socket that is already open.
    void open(const StreamProtocol&);
    std::error_code open(const StreamProtocol&, std::error_code&);
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

    void bind(const Endpoint&);
    std::error_code bind(const Endpoint&, std::error_code&);

    Endpoint local_endpoint() const;
    Endpoint local_endpoint(std::error_code&) const;

private:
    enum opt_enum {
        opt_ReuseAddr, ///< `SOL_SOCKET`, `SO_REUSEADDR`
        opt_Linger,    ///< `SOL_SOCKET`, `SO_LINGER`
        opt_NoDelay,   ///< `IPPROTO_TCP`, `TCP_NODELAY` (disable the Nagle algorithm)
    };

    template<class, int, class> class Option;

public:
    using reuse_address = Option<bool, opt_ReuseAddr, int>;
    using no_delay      = Option<bool, opt_NoDelay,   int>;

    // linger struct defined by POSIX sys/socket.h.
    struct linger_opt;
    using linger = Option<linger_opt, opt_Linger, struct linger>;

protected:
    Service::Descriptor m_desc;

private:
    StreamProtocol m_protocol;

protected:
    Service::OwnersOperPtr m_read_oper;  // Read or accept
    Service::OwnersOperPtr m_write_oper; // Write or connect

    SocketBase(Service&);

    const StreamProtocol& get_protocol() const noexcept;
    std::error_code do_assign(const StreamProtocol&, native_handle_type, std::error_code&);
    void do_close() noexcept;

    void get_option(opt_enum, void* value_data, std::size_t& value_size, std::error_code&) const;
    void set_option(opt_enum, const void* value_data, std::size_t value_size, std::error_code&);
    void map_option(opt_enum, int& level, int& option_name) const;

    friend class Acceptor;
};


template<class T, int opt, class U> class SocketBase::Option {
public:
    Option(T value = T());
    T value() const;

private:
    T m_value;

    void get(const SocketBase&, std::error_code&);
    void set(SocketBase&, std::error_code&) const;

    friend class SocketBase;
};

struct SocketBase::linger_opt {
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
class Socket: public SocketBase {
public:
    Socket(Service&);

    /// \brief Create a socket with an already-connected native socket handle.
    ///
    /// This constructor is shorthand for creating the socket with the
    /// one-argument constructor, and then calling the two-argument assign()
    /// with the specified protocol and native handle.
    Socket(Service&, const StreamProtocol&, native_handle_type);

    ~Socket() noexcept;

    void connect(const Endpoint&);
    std::error_code connect(const Endpoint&, std::error_code&);

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
    std::size_t read(char* buffer, std::size_t size, std::error_code& ec);
    std::size_t read(char* buffer, std::size_t size, ReadAheadBuffer&);
    std::size_t read(char* buffer, std::size_t size, ReadAheadBuffer&, std::error_code& ec);
    std::size_t read_until(char* buffer, std::size_t size, char delim, ReadAheadBuffer&);
    std::size_t read_until(char* buffer, std::size_t size, char delim, ReadAheadBuffer&,
                           std::error_code& ec);
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
    std::size_t write(const char* data, std::size_t size, std::error_code& ec);
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
    std::size_t read_some(char* buffer, std::size_t size);
    std::size_t read_some(char* buffer, std::size_t size, std::error_code& ec);
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
    std::size_t write_some(const char* data, std::size_t size);
    std::size_t write_some(const char* data, std::size_t size, std::error_code&);
    /// @}

    /// \brief Perform an asynchronous connect operation.
    ///
    /// Initiate an asynchronous connect operation. The completion handler is
    /// called when the operation completes. The operation completes when the
    /// connection is established, or an error occurs.
    ///
    /// The completion handler is always executed by the event loop thread,
    /// i.e., by a thread that is executing Service::run(). Conversely, the
    /// completion handler is guaranteed to not be called while no thread is
    /// executing Service::run(). The execution of the completion handler is
    /// always deferred to the event loop, meaning that it never happens as a
    /// synchronous side effect of the execution of async_connect(), even when
    /// async_connect() is executed by the event loop thread. The completion
    /// handler is guaranteed to be called eventually, as long as there is time
    /// enough for the operation to complete or fail, and a thread is executing
    /// Service::run() for long enough.
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
    template<class H> void async_connect(const Endpoint& ep, H handler);

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
    /// i.e., by a thread that is executing Service::run(). Conversely, the
    /// completion handler is guaranteed to not be called while no thread is
    /// executing Service::run(). The execution of the completion handler is
    /// always deferred to the event loop, meaning that it never happens as a
    /// synchronous side effect of the execution of async_read() or
    /// async_read_until(), even when async_read() or async_read_until() is
    /// executed by the event loop thread. The completion handler is guaranteed
    /// to be called eventually, as long as there is time enough for the
    /// operation to complete or fail, and a thread is executing Service::run()
    /// for long enough.
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
    /// i.e., by a thread that is executing Service::run(). Conversely, the
    /// completion handler is guaranteed to not be called while no thread is
    /// executing Service::run(). The execution of the completion handler is
    /// always deferred to the event loop, meaning that it never happens as a
    /// synchronous side effect of the execution of async_write(), even when
    /// async_write() is executed by the event loop thread. The completion
    /// handler is guaranteed to be called eventually, as long as there is time
    /// enough for the operation to complete or fail, and a thread is executing
    /// Service::run() for long enough.
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
    /// bytes written (of type `std::size_t`). If the the handler object is
    /// movable, it will never be copied. Otherwise, it will be copied as
    /// necessary.
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
    template<class H> void async_write(const char* data, std::size_t size, H handler);

    template<class H> void async_read_some(char* buffer, std::size_t size, H handler);
    template<class H> void async_write_some(const char* data, std::size_t size, H handler);

    enum shutdown_type {
#ifdef _WIN32
        /// Shutdown the receiving side of the socket.
        shutdown_receive = SD_RECEIVE,

        /// Shutdown the sending side of the socket.
        shutdown_send = SD_SEND,

        /// Shutdown both sending and receiving side of the socket.
        shutdown_both = SD_BOTH
#else
        shutdown_receive = SHUT_RD,
        shutdown_send = SHUT_WR,
        shutdown_both = SHUT_RDWR
#endif
    };

    /// @{ \brief Shut down the connected sockets sending and/or receiving
    /// side.
    ///
    /// It is an error to call this function when the socket is not both open
    /// and connected.
    void shutdown(shutdown_type);
    std::error_code shutdown(shutdown_type, std::error_code&);
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
    void assign(const StreamProtocol&, native_handle_type);
    std::error_code assign(const StreamProtocol&, native_handle_type, std::error_code&);
    /// @}

    /// Returns a reference to this socket, as this socket is the lowest layer
    /// of a stream.
    Socket& lowest_layer() noexcept;

private:
    using Want = Service::Want;
    using StreamOps = Service::BasicStreamOps<Socket>;

    class ConnectOperBase;
    template<class H> class ConnectOper;

    using LendersConnectOperPtr = std::unique_ptr<ConnectOperBase, Service::LendersOperDeleter>;

    // `ec` untouched on success, but no immediate completion
    bool initiate_async_connect(const Endpoint&, std::error_code& ec);
    // `ec` untouched on success
    std::error_code finalize_async_connect(std::error_code& ec) noexcept;

    // See Service::BasicStreamOps for details on these these 6 functions.
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

    friend class Service::BasicStreamOps<Socket>;
    friend class Service::BasicStreamOps<ssl::Stream>;
    friend class ReadAheadBuffer;
    friend class ssl::Stream;
};


/// Switching between synchronous and asynchronous operations is allowed, but
/// only in a nonoverlapping fashion. That is, a synchronous operation is not
/// allowed to run concurrently with an asynchronous one on the same
/// acceptor. Note that an asynchronous operation is considered to be running
/// until its completion handler starts executing.
class Acceptor: public SocketBase {
public:
    Acceptor(Service&);
    ~Acceptor() noexcept;

    static constexpr int max_connections = SOMAXCONN;

    void listen(int backlog = max_connections);
    std::error_code listen(int backlog, std::error_code&);

    void accept(Socket&);
    void accept(Socket&, Endpoint&);
    std::error_code accept(Socket&, std::error_code&);
    std::error_code accept(Socket&, Endpoint&, std::error_code&);

    /// @{ \brief Perform an asynchronous accept operation.
    ///
    /// Initiate an asynchronous accept operation. The completion handler will
    /// be called when the operation completes. The operation completes when the
    /// connection is accepted, or an error occurs. If the operation succeeds,
    /// the specified local socket will have become connected to a remote
    /// socket.
    ///
    /// The completion handler is always executed by the event loop thread,
    /// i.e., by a thread that is executing Service::run(). Conversely, the
    /// completion handler is guaranteed to not be called while no thread is
    /// executing Service::run(). The execution of the completion handler is
    /// always deferred to the event loop, meaning that it never happens as a
    /// synchronous side effect of the execution of async_accept(), even when
    /// async_accept() is executed by the event loop thread. The completion
    /// handler is guaranteed to be called eventually, as long as there is time
    /// enough for the operation to complete or fail, and a thread is executing
    /// Service::run() for long enough.
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
    /// closed state (Socket::is_open()) when async_accept() is called.
    ///
    /// \param ep Upon completion, the remote peer endpoint will have been
    /// assigned to this variable.
    template<class H> void async_accept(Socket& sock, H handler);
    template<class H> void async_accept(Socket& sock, Endpoint& ep, H handler);
    /// @}

private:
    using Want = Service::Want;

    class AcceptOperBase;
    template<class H> class AcceptOper;

    using LendersAcceptOperPtr = std::unique_ptr<AcceptOperBase, Service::LendersOperDeleter>;

    std::error_code accept(Socket&, Endpoint*, std::error_code&);
    Want do_accept_async(Socket&, Endpoint*, std::error_code&) noexcept;

    template<class H> void async_accept(Socket&, Endpoint*, H);
};


/// \brief A timer object supporting asynchronous wait operations.
class DeadlineTimer {
public:
    DeadlineTimer(Service&);
    ~DeadlineTimer() noexcept;

    /// Thread-safe.
    Service& get_service() noexcept;

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
    /// i.e., by a thread that is executing Service::run(). Conversely, the
    /// completion handler is guaranteed to not be called while no thread is
    /// executing Service::run(). The execution of the completion handler is
    /// always deferred to the event loop, meaning that it never happens as a
    /// synchronous side effect of the execution of async_wait(), even when
    /// async_wait() is executed by the event loop thread. The completion
    /// handler is guaranteed to be called eventually, as long as there is time
    /// enough for the operation to complete or fail, and a thread is executing
    /// Service::run() for long enough.
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
    ///
    /// Cancellation happens automatically when the timer object is destroyed.
    void cancel() noexcept;

private:
    template<class H> class WaitOper;

    using clock = Service::clock;

    Service::Impl& m_service_impl;
    Service::OwnersOperPtr m_wait_oper;

    void add_oper(Service::LendersWaitOperPtr);
};


class ReadAheadBuffer {
public:
    ReadAheadBuffer();

    /// Discard any buffered data.
    void clear() noexcept;

private:
    using Want = Service::Want;

    char* m_begin = nullptr;
    char* m_end   = nullptr;
    static constexpr std::size_t s_size = 1024;
    const std::unique_ptr<char[]> m_buffer;

    bool empty() const noexcept;
    bool read(char*& begin, char* end, int delim, std::error_code&) noexcept;
    template<class S> void refill_sync(S& stream, std::error_code&) noexcept;
    template<class S> bool refill_async(S& stream, std::error_code&, Want&) noexcept;

    template<class> friend class Service::BasicStreamOps;
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

template<> class is_error_code_enum<realm::util::network::errors> {
public:
    static const bool value = true;
};

} // namespace std

namespace realm {
namespace util {
namespace network {





// Implementation

// ---------------- StreamProtocol ----------------

inline StreamProtocol StreamProtocol::ip_v4()
{
    StreamProtocol prot;
    prot.m_family = AF_INET;
    return prot;
}

inline StreamProtocol StreamProtocol::ip_v6()
{
    StreamProtocol prot;
    prot.m_family = AF_INET6;
    return prot;
}

inline bool StreamProtocol::is_ip_v4() const
{
    return m_family == AF_INET;
}

inline bool StreamProtocol::is_ip_v6() const
{
    return m_family == AF_INET6;
}

inline int StreamProtocol::family() const
{
    return m_family;
}

inline int StreamProtocol::protocol() const
{
    return m_protocol;
}

inline StreamProtocol::StreamProtocol():
    m_family{AF_UNSPEC},     // Allow both IPv4 and IPv6
    m_socktype{SOCK_STREAM}, // Or SOCK_DGRAM for UDP
    m_protocol{0}            // Any protocol
{
}

// ---------------- Address ----------------

inline bool Address::is_ip_v4() const
{
    return !m_is_ip_v6;
}

inline bool Address::is_ip_v6() const
{
    return m_is_ip_v6;
}

template<class C, class T>
inline std::basic_ostream<C,T>& operator<<(std::basic_ostream<C,T>& out, const Address& addr)
{
    // FIXME: Not taking `addr.m_ip_v6_scope_id` into account. What does ASIO
    // do?
    union buffer_union {
        char ip_v4[INET_ADDRSTRLEN];
        char ip_v6[INET6_ADDRSTRLEN];
    };
    char buffer[sizeof (buffer_union)];
    int af = addr.m_is_ip_v6 ? AF_INET6 : AF_INET;
#ifdef _WIN32
    void* src = const_cast<void*>(reinterpret_cast<const void*>(&addr.m_union));
#else
    const void* src = &addr.m_union;
#endif
    const char* ret = ::inet_ntop(af, src, buffer, sizeof buffer);
    if (ret == 0) {
        std::error_code ec = make_basic_system_error_code(errno);
        throw std::system_error(ec);
    }
    out << ret;
    return out;
}

inline Address::Address()
{
    m_union.m_ip_v4 = ip_v4_type();
}

inline Address make_address(const char* c_str)
{
    std::error_code ec;
    Address addr = make_address(c_str, ec);
    if (ec)
        throw std::system_error(ec);
    return addr;
}

inline Address make_address(const std::string& str)
{
    std::error_code ec;
    Address addr = make_address(str, ec);
    if (ec)
        throw std::system_error(ec);
    return addr;
}

inline Address make_address(const std::string& str, std::error_code& ec) noexcept
{
    return make_address(str.c_str(), ec);
}

// ---------------- Endpoint ----------------

inline StreamProtocol Endpoint::protocol() const
{
    return m_protocol;
}

inline Address Endpoint::address() const
{
    Address addr;
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

inline Endpoint::port_type Endpoint::port() const
{
    return ntohs(m_protocol.is_ip_v4() ? m_sockaddr_union.m_ip_v4.sin_port :
                 m_sockaddr_union.m_ip_v6.sin6_port);
}

inline Endpoint::data_type* Endpoint::data()
{
    return &m_sockaddr_union.m_base;
}

inline const Endpoint::data_type* Endpoint::data() const
{
    return &m_sockaddr_union.m_base;
}

inline Endpoint::Endpoint():
    Endpoint{StreamProtocol::ip_v4(), 0}
{
}

inline Endpoint::Endpoint(const StreamProtocol& protocol, port_type port):
    m_protocol{protocol}
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

inline Endpoint::Endpoint(const Address& addr, port_type port)
{
    if (addr.m_is_ip_v6) {
        m_protocol = StreamProtocol::ip_v6();
        m_sockaddr_union.m_ip_v6.sin6_family = AF_INET6;
        m_sockaddr_union.m_ip_v6.sin6_port = htons(port);
        m_sockaddr_union.m_ip_v6.sin6_flowinfo = 0;
        m_sockaddr_union.m_ip_v6.sin6_addr = addr.m_union.m_ip_v6;
        m_sockaddr_union.m_ip_v6.sin6_scope_id = addr.m_ip_v6_scope_id;
    }
    else {
        m_protocol = StreamProtocol::ip_v4();
        m_sockaddr_union.m_ip_v4.sin_family = AF_INET;
        m_sockaddr_union.m_ip_v4.sin_port = htons(port);
        m_sockaddr_union.m_ip_v4.sin_addr = addr.m_union.m_ip_v4;
    }
}

inline Endpoint::List::iterator Endpoint::List::begin() const noexcept
{
    return m_endpoints.data();
}

inline Endpoint::List::iterator Endpoint::List::end() const noexcept
{
    return m_endpoints.data() + m_endpoints.size();
}

inline std::size_t Endpoint::List::size() const noexcept
{
    return m_endpoints.size();
}

inline bool Endpoint::List::empty() const noexcept
{
    return m_endpoints.size() == 0;
}

// ---------------- Service::OperQueue ----------------

template<class Oper> inline bool Service::OperQueue<Oper>::empty() const noexcept
{
    return !m_back;
}

template<class Oper> inline void Service::OperQueue<Oper>::push_back(LendersOperPtr op) noexcept
{
    REALM_ASSERT(!op->m_next);
    if (m_back) {
        op->m_next = m_back->m_next;
        m_back->m_next = op.get();
    }
    else {
        op->m_next = op.get();
    }
    m_back = op.release();
}

template<class Oper> template<class Oper2>
inline void Service::OperQueue<Oper>::push_back(OperQueue<Oper2>& q) noexcept
{
    if (!q.m_back)
        return;
    if (m_back)
        std::swap(m_back->m_next, q.m_back->m_next);
    m_back = q.m_back;
    q.m_back = nullptr;
}

template<class Oper> inline auto Service::OperQueue<Oper>::pop_front() noexcept -> LendersOperPtr
{
    Oper* op = nullptr;
    if (m_back) {
        op = static_cast<Oper*>(m_back->m_next);
        if (op != m_back) {
            m_back->m_next = op->m_next;
        }
        else {
            m_back = nullptr;
        }
        op->m_next = nullptr;
    }
    return LendersOperPtr(op);
}

template<class Oper> inline void Service::OperQueue<Oper>::clear() noexcept
{
    if (m_back) {
        LendersOperPtr op(m_back);
        while (op->m_next != m_back)
            op.reset(static_cast<Oper*>(op->m_next));
        m_back = nullptr;
    }
}

template<class Oper> inline Service::OperQueue<Oper>::OperQueue(OperQueue&& q) noexcept:
    m_back{q.m_back}
{
    q.m_back = nullptr;
}

template<class Oper> inline Service::OperQueue<Oper>::~OperQueue() noexcept
{
    clear();
}

// ---------------- Service::Descriptor ----------------

inline Service::Descriptor::Descriptor(Impl& s) noexcept:
    service_impl{s}
{
}

inline Service::Descriptor::~Descriptor() noexcept
{
    if (is_open())
        close();
}

inline void Service::Descriptor::assign(native_handle_type fd, bool in_blocking_mode) noexcept
{
    REALM_ASSERT(!is_open());
    m_fd = fd;
    m_in_blocking_mode = in_blocking_mode;
#if REALM_HAVE_EPOLL || REALM_HAVE_KQUEUE
    m_read_ready  = false;
    m_write_ready = false;
    m_imminent_end_of_input = false;
    m_is_registered = false;
#endif
}

inline void Service::Descriptor::close() noexcept
{
    REALM_ASSERT(is_open());
#if REALM_HAVE_EPOLL || REALM_HAVE_KQUEUE
    if (m_is_registered)
        deregister_for_async();
    m_is_registered = false;
#endif
    do_close();
}

inline bool Service::Descriptor::is_open() const noexcept
{
    return (m_fd != -1);
}

inline auto Service::Descriptor::native_handle() const noexcept -> native_handle_type
{
    return m_fd;
}

inline bool Service::Descriptor::in_blocking_mode() const noexcept
{
    return m_in_blocking_mode;
}

template<class Oper, class... Args>
inline void Service::Descriptor::initiate_oper(std::unique_ptr<Oper, LendersOperDeleter> op,
                                               Args&&... args)
{
    Service::Want want = op->initiate(std::forward<Args>(args)...); // Throws
    add_initiated_oper(std::move(op), want); // Throws
}

inline void Service::Descriptor::ensure_blocking_mode()
{
    // Assuming that descriptors are either used mostly in blocking mode, or
    // mostly in nonblocking mode.
    if (REALM_UNLIKELY(!m_in_blocking_mode)) {
        bool value = false;
        set_nonblock_flag(value); // Throws
        m_in_blocking_mode = true;
    }
}

inline void Service::Descriptor::ensure_nonblocking_mode()
{
    // Assuming that descriptors are either used mostly in blocking mode, or
    // mostly in nonblocking mode.
    if (REALM_UNLIKELY(m_in_blocking_mode)) {
        bool value = true;
        set_nonblock_flag(value); // Throws
        m_in_blocking_mode = false;
    }
}

inline bool Service::Descriptor::assume_read_would_block() const noexcept
{
#if REALM_HAVE_EPOLL || REALM_HAVE_KQUEUE
    return !m_in_blocking_mode && !m_read_ready;
#else
    return false;
#endif
}

inline bool Service::Descriptor::assume_write_would_block() const noexcept
{
#if REALM_HAVE_EPOLL || REALM_HAVE_KQUEUE
    return !m_in_blocking_mode && !m_write_ready;
#else
    return false;
#endif
}

inline void Service::Descriptor::set_read_ready(bool value) noexcept
{
#if REALM_HAVE_EPOLL || REALM_HAVE_KQUEUE
    m_read_ready = value;
#else
    // No-op
    static_cast<void>(value);
#endif
}

inline void Service::Descriptor::set_write_ready(bool value) noexcept
{
#if REALM_HAVE_EPOLL || REALM_HAVE_KQUEUE
    m_write_ready = value;
#else
    // No-op
    static_cast<void>(value);
#endif
}

// ---------------- Service ----------------

class Service::AsyncOper {
public:
    bool in_use() const noexcept;
    bool is_complete() const noexcept;
    bool is_canceled() const noexcept;
    void cancel() noexcept;
    /// Every object of type \ref AsyncOper must be destroyed either by a call
    /// to this function or to recycle(). This function recycles the operation
    /// object (commits suicide), even if it throws.
    virtual void recycle_and_execute() = 0;
    /// Every object of type \ref AsyncOper must be destroyed either by a call
    /// to recycle_and_execute() or to this function. This function destroys the
    /// object (commits suicide).
    virtual void recycle() noexcept = 0;
    /// Must be called when the owner dies, and the object is in use (not an
    /// instance of UnusedOper).
    virtual void orphan()  noexcept = 0;
protected:
    AsyncOper(std::size_t size, bool in_use) noexcept;
    virtual ~AsyncOper() noexcept {}
    void set_is_complete(bool value) noexcept;
    template<class H, class... Args>
    void do_recycle_and_execute(bool orphaned, H& handler, Args&&...);
    void do_recycle(bool orphaned) noexcept;
private:
    std::size_t m_size; // Allocated number of bytes
    bool m_in_use   = false;
    // Set to true when the operation completes successfully or fails. If the
    // operation is canceled before this happens, it will never be set to
    // true. Always false when not in use
    bool m_complete = false;
    // Set to true when the operation is canceled. Always false when not in use.
    bool m_canceled = false;
    AsyncOper* m_next = nullptr; // Always null when not in use
    template<class H, class... Args>
    void do_recycle_and_execute_helper(bool orphaned, bool& was_recycled, H handler, Args...);
    friend class Service;
};

class Service::WaitOperBase: public AsyncOper {
public:
    WaitOperBase(std::size_t size, DeadlineTimer& timer, clock::time_point expiration_time):
        AsyncOper{size, true}, // Second argument is `in_use`
        m_timer{&timer},
        m_expiration_time{expiration_time}
    {
    }
    void expired() noexcept
    {
        set_is_complete(true);
    }
    void recycle() noexcept override final
    {
        bool orphaned = !m_timer;
        // Note: do_recycle() commits suicide.
        do_recycle(orphaned);
    }
    void orphan() noexcept override final
    {
        m_timer = nullptr;
    }
protected:
    DeadlineTimer* m_timer;
    clock::time_point m_expiration_time;
    friend class Service;
};

class Service::PostOperBase: public AsyncOper {
public:
    PostOperBase(std::size_t size, Impl& service):
        AsyncOper{size, true}, // Second argument is `in_use`
        m_service{service}
    {
    }
    void recycle() noexcept override final
    {
        // Service::recycle_post_oper() destroys this operation object
        Service::recycle_post_oper(m_service, this);
    }
    void orphan() noexcept override final
    {
        REALM_ASSERT(false); // Never called
    }
protected:
    Impl& m_service;
};

template<class H> class Service::PostOper: public PostOperBase {
public:
    PostOper(std::size_t size, Impl& service, H handler):
        PostOperBase{size, service},
        m_handler{std::move(handler)}
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
            // Service::recycle_post_oper() destroys this operation object
            Service::recycle_post_oper(m_service, this);
            was_recycled = true;
            handler(); // Throws
        }
        catch (...) {
            if (!was_recycled) {
                // Service::recycle_post_oper() destroys this operation object
                Service::recycle_post_oper(m_service, this);
            }
            throw;
        }
    }
private:
    H m_handler;
};

class Service::IoOper: public AsyncOper {
public:
    IoOper(std::size_t size) noexcept:
        AsyncOper{size, true} // Second argument is `in_use`
    {
    }
    virtual Descriptor& descriptor() noexcept = 0;
    /// Advance this operation and figure out out whether it needs to read from,
    /// or write to the underlying descriptor to advance further. This function
    /// must return Want::read if the operation needs to read, or Want::write if
    /// the operation needs to write to advance further. If the operation
    /// completes (due to success or failure), this function must return
    /// Want::nothing.
    virtual Want advance() noexcept = 0;
};

class Service::UnusedOper: public AsyncOper {
public:
    UnusedOper(std::size_t size) noexcept:
        AsyncOper{size, false} // Second argument is `in_use`
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
//    Socket& lowest_layer() noexcept;
//
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
// If an error occurs during any of these 6 functions, the `ec` argument must be
// set accordingly. Otherwise the `ec` argument must be set to
// `std::error_code()`.
//
// The do_init_*_async() functions must update the `want` argument to indicate
// how the operation must be initiated:
//
//    Want::read      Wait for read readiness, then call do_*_some_async().
//    Want::write     Wait for write readiness, then call do_*_some_async().
//    Want::nothing   Call do_*_some_async() immediately without waiting for
//                    read or write readiness.
//
// If end-of-input occurs while reading, do_read_some_*() must fail, set `ec` to
// `network::end_of_input`, and return zero.
//
// If an error occurs during reading or writing, do_*_some_sync() must set `ec`
// accordingly (to something other than `std::system_error()`) and return
// zero. Otherwise they must set `ec` to `std::system_error()` and return the
// number of bytes read or written, which **must** be at least 1. If the
// underlying socket is in nonblocking mode, and no bytes could be immediately
// read or written these functions must fail with
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
//                    write readiness.
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
//    want != Want::nothing     Wait for read/write readiness.
//
// This is of critical importance, as it is the only way we can avoid falling
// into a busy loop of repeated invocations of do_*_some_async().
//
// NOTE: do_*_some_async() are allowed to set `want` to `Want::read` or
// `Want::write`, even when they succesfully transfer a nonzero number of bytes.
template<class S> class Service::BasicStreamOps {
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

    // Synchronous read
    static std::size_t read(S& stream, char* buffer, std::size_t size,
                            std::error_code& ec)
    {
        REALM_ASSERT(!stream.lowest_layer().m_read_oper ||
                     !stream.lowest_layer().m_read_oper->in_use());
        stream.lowest_layer().m_desc.ensure_blocking_mode(); // Throws
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

    // Synchronous write
    static std::size_t write(S& stream, const char* data, std::size_t size,
                             std::error_code& ec)
    {
        REALM_ASSERT(!stream.lowest_layer().m_write_oper ||
                     !stream.lowest_layer().m_write_oper->in_use());
        stream.lowest_layer().m_desc.ensure_blocking_mode(); // Throws
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

    // Synchronous read
    static std::size_t buffered_read(S& stream, char* buffer, std::size_t size, int delim,
                                     ReadAheadBuffer& rab, std::error_code& ec)
    {
        REALM_ASSERT(!stream.lowest_layer().m_read_oper ||
                     !stream.lowest_layer().m_read_oper->in_use());
        stream.lowest_layer().m_desc.ensure_blocking_mode(); // Throws
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

    // Synchronous read
    static std::size_t read_some(S& stream, char* buffer, std::size_t size,
                                 std::error_code& ec)
    {
        REALM_ASSERT(!stream.lowest_layer().m_read_oper ||
                     !stream.lowest_layer().m_read_oper->in_use());
        stream.lowest_layer().m_desc.ensure_blocking_mode(); // Throws
        return stream.do_read_some_sync(buffer, size, ec);
    }

    // Synchronous write
    static std::size_t write_some(S& stream, const char* data, std::size_t size,
                                  std::error_code& ec)
    {
        REALM_ASSERT(!stream.lowest_layer().m_write_oper ||
                     !stream.lowest_layer().m_write_oper->in_use());
        stream.lowest_layer().m_desc.ensure_blocking_mode(); // Throws
        return stream.do_write_some_sync(data, size, ec);
    }

    template<class H>
    static void async_read(S& stream, char* buffer, std::size_t size, bool is_read_some, H handler)
    {
        char* begin = buffer;
        char* end   = buffer + size;
        LendersReadOperPtr op =
            Service::alloc<ReadOper<H>>(stream.lowest_layer().m_read_oper, stream, is_read_some,
                                        begin, end, std::move(handler)); // Throws
        stream.lowest_layer().m_desc.initiate_oper(std::move(op)); // Throws
    }

    template<class H>
    static void async_write(S& stream, const char* data, std::size_t size, bool is_write_some,
                            H handler)
    {
        const char* begin = data;
        const char* end   = data + size;
        LendersWriteOperPtr op =
            Service::alloc<WriteOper<H>>(stream.lowest_layer().m_write_oper, stream, is_write_some,
                                         begin, end, std::move(handler)); // Throws
        stream.lowest_layer().m_desc.initiate_oper(std::move(op)); // Throws
    }

    template<class H>
    static void async_buffered_read(S& stream, char* buffer, std::size_t size, int delim,
                                    ReadAheadBuffer& rab, H handler)
    {
        char* begin = buffer;
        char* end   = buffer + size;
        LendersBufferedReadOperPtr op =
            Service::alloc<BufferedReadOper<H>>(stream.lowest_layer().m_read_oper, stream,
                                                begin, end, delim, rab,
                                                std::move(handler)); // Throws
        stream.lowest_layer().m_desc.initiate_oper(std::move(op)); // Throws
    }
};

template<class S> class Service::BasicStreamOps<S>::StreamOper: public IoOper {
public:
    StreamOper(std::size_t size, S& stream) noexcept:
        IoOper{size},
        m_stream{&stream}
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
    Descriptor& descriptor() noexcept override final
    {
        return m_stream->lowest_layer().m_desc;
    }
protected:
    S* m_stream;
    std::error_code m_error_code;
};

template<class S> class Service::BasicStreamOps<S>::ReadOperBase: public StreamOper {
public:
    ReadOperBase(std::size_t size, S& stream, bool is_read_some, char* begin, char* end) noexcept:
        StreamOper{size, stream},
        m_is_read_some{is_read_some},
        m_begin{begin},
        m_end{end}
    {
    }
    Want initiate()
    {
        auto& s = *this;
        REALM_ASSERT(this == s.m_stream->lowest_layer().m_read_oper.get());
        REALM_ASSERT(!s.is_complete());
        REALM_ASSERT(s.m_curr <= s.m_end);
        Want want = Want::nothing;
        if (REALM_UNLIKELY(s.m_curr == s.m_end)) {
            s.set_is_complete(true); // Success
        }
        else {
            s.m_stream->lowest_layer().m_desc.ensure_nonblocking_mode(); // Throws
            s.m_stream->do_init_read_async(s.m_error_code, want);
            if (want == Want::nothing) {
                if (REALM_UNLIKELY(s.m_error_code)) {
                    s.set_is_complete(true); // Failure
                }
                else {
                    want = advance();
                }
            }
        }
        return want;
    }
    Want advance() noexcept override final
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
            REALM_ASSERT(!s.m_error_code);
            // Check for completion
            REALM_ASSERT(n <= size);
            s.m_curr += n;
            if (s.m_is_read_some || s.m_curr == s.m_end) {
                s.set_is_complete(true); // Success
                return Want::nothing;
            }
            if (want != Want::nothing)
                return want;
            REALM_ASSERT(n < size);
        }
    }
protected:
    const bool m_is_read_some;
    char* const m_begin;    // May be dangling after cancellation
    char* const m_end;      // May be dangling after cancellation
    char* m_curr = m_begin; // May be dangling after cancellation
};

template<class S> class Service::BasicStreamOps<S>::WriteOperBase: public StreamOper {
public:
    WriteOperBase(std::size_t size, S& stream, bool is_write_some,
                  const char* begin, const char* end) noexcept:
        StreamOper{size, stream},
        m_is_write_some{is_write_some},
        m_begin{begin},
        m_end{end}
    {
    }
    Want initiate()
    {
        auto& s = *this;
        REALM_ASSERT(this == s.m_stream->lowest_layer().m_write_oper.get());
        REALM_ASSERT(!s.is_complete());
        REALM_ASSERT(s.m_curr <= s.m_end);
        Want want = Want::nothing;
        if (REALM_UNLIKELY(s.m_curr == s.m_end)) {
            s.set_is_complete(true); // Success
        }
        else {
            s.m_stream->lowest_layer().m_desc.ensure_nonblocking_mode(); // Throws
            s.m_stream->do_init_write_async(s.m_error_code, want);
            if (want == Want::nothing) {
                if (REALM_UNLIKELY(s.m_error_code)) {
                    s.set_is_complete(true); // Failure
                }
                else {
                    want = advance();
                }
            }
        }
        return want;
    }
    Want advance() noexcept override final
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
            REALM_ASSERT(!s.m_error_code);
            // Check for completion
            REALM_ASSERT(n <= size);
            s.m_curr += n;
            if (s.m_is_write_some || s.m_curr == s.m_end) {
                s.set_is_complete(true); // Success
                return Want::nothing;
            }
            if (want != Want::nothing)
                return want;
            REALM_ASSERT(n < size);
        }
    }
protected:
    const bool m_is_write_some;
    const char* const m_begin;    // May be dangling after cancellation
    const char* const m_end;      // May be dangling after cancellation
    const char* m_curr = m_begin; // May be dangling after cancellation
};

template<class S> class Service::BasicStreamOps<S>::BufferedReadOperBase: public StreamOper {
public:
    BufferedReadOperBase(std::size_t size, S& stream, char* begin, char* end, int delim,
                         ReadAheadBuffer& rab) noexcept:
        StreamOper{size, stream},
        m_read_ahead_buffer{rab},
        m_begin{begin},
        m_end{end},
        m_delim{delim}
    {
    }
    Want initiate()
    {
        auto& s = *this;
        REALM_ASSERT(this == s.m_stream->lowest_layer().m_read_oper.get());
        REALM_ASSERT(!s.is_complete());
        Want want = Want::nothing;
        bool complete = s.m_read_ahead_buffer.read(s.m_curr, s.m_end, s.m_delim, s.m_error_code);
        if (complete) {
            s.set_is_complete(true); // Success or failure
        }
        else {
            s.m_stream->lowest_layer().m_desc.ensure_nonblocking_mode(); // Throws
            s.m_stream->do_init_read_async(s.m_error_code, want);
            if (want == Want::nothing) {
                if (REALM_UNLIKELY(s.m_error_code)) {
                    s.set_is_complete(true); // Failure
                }
                else {
                    want = advance();
                }
            }
        }
        return want;
    }
    Want advance() noexcept override final
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
class Service::BasicStreamOps<S>::ReadOper: public ReadOperBase {
public:
    ReadOper(std::size_t size, S& stream, bool is_read_some, char* begin, char* end, H handler):
        ReadOperBase{size, stream, is_read_some, begin, end},
        m_handler{std::move(handler)}
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
class Service::BasicStreamOps<S>::WriteOper: public WriteOperBase {
public:
    WriteOper(std::size_t size, S& stream, bool is_write_some,
              const char* begin, const char* end, H handler):
        WriteOperBase{size, stream, is_write_some, begin, end},
        m_handler{std::move(handler)}
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
class Service::BasicStreamOps<S>::BufferedReadOper: public BufferedReadOperBase {
public:
    BufferedReadOper(std::size_t size, S& stream, char* begin, char* end, int delim,
                     ReadAheadBuffer& rab, H handler):
        BufferedReadOperBase{size, stream, begin, end, delim, rab},
        m_handler{std::move(handler)}
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

template<class H> inline void Service::post(H handler)
{
    do_post(&Service::post_oper_constr<H>, sizeof (PostOper<H>), &handler);
}

inline void Service::OwnersOperDeleter::operator()(AsyncOper* op) const noexcept
{
    if (op->in_use()) {
        op->orphan();
    }
    else {
        void* addr = op;
        op->~AsyncOper();
        delete[] static_cast<char*>(addr);
    }
}

inline void Service::LendersOperDeleter::operator()(AsyncOper* op) const noexcept
{
    op->recycle(); // Suicide
}

template<class Oper, class... Args> std::unique_ptr<Oper, Service::LendersOperDeleter>
Service::alloc(OwnersOperPtr& owners_ptr, Args&&... args)
{
    void* addr = owners_ptr.get();
    std::size_t size;
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
        owners_ptr.reset(static_cast<AsyncOper*>(addr));
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
inline void Service::execute(std::unique_ptr<Oper, LendersOperDeleter>& lenders_ptr)
{
    lenders_ptr.release()->recycle_and_execute(); // Throws
}

template<class H> inline Service::PostOperBase*
Service::post_oper_constr(void* addr, std::size_t size, Impl& service, void* cookie)
{
    H& handler = *static_cast<H*>(cookie);
    return new (addr) PostOper<H>(size, service, std::move(handler)); // Throws
}

inline bool Service::AsyncOper::in_use() const noexcept
{
    return m_in_use;
}

inline bool Service::AsyncOper::is_complete() const noexcept
{
    return m_complete;
}

inline void Service::AsyncOper::cancel() noexcept
{
    REALM_ASSERT(m_in_use);
    REALM_ASSERT(!m_canceled);
    m_canceled = true;
}

inline Service::AsyncOper::AsyncOper(std::size_t size, bool is_in_use) noexcept:
    m_size{size},
    m_in_use{is_in_use}
{
}

inline bool Service::AsyncOper::is_canceled() const noexcept
{
    return m_canceled;
}

inline void Service::AsyncOper::set_is_complete(bool value) noexcept
{
    REALM_ASSERT(!m_complete);
    REALM_ASSERT(!value || m_in_use);
    m_complete = value;
}

template<class H, class... Args>
inline void Service::AsyncOper::do_recycle_and_execute(bool orphaned, H& handler, Args&&... args)
{
    // Recycle the operation object before the handler is exceuted, such that
    // the memory is available for a new post operation that might be initiated
    // during the execution of the handler.
    bool was_recycled = false;
    try {
        // We need to copy or move all arguments to be passed to the handler,
        // such that there is no risk of references to the recycled operation
        // object being passed to the handler (the passed arguments may be
        // references to members of the recycled operation object). The easiest
        // way to achive this, is by forwarding the reference arguments (passed
        // to this function) to a helper function whose arguments have
        // nonreference type (`Args...` rather than `Args&&...`).
        //
        // Note that the copying and moving of arguments may throw, and it is
        // important that the operation is still recycled even if that
        // happens. For that reason, copying and moving of arguments must not
        // happen until we are in a scope (this scope) that catches and deals
        // correctly with such exceptions.
        do_recycle_and_execute_helper(orphaned, was_recycled, std::move(handler),
                                      std::forward<Args>(args)...); // Throws
    }
    catch (...) {
        if (!was_recycled)
            do_recycle(orphaned);
        throw;
    }
}

template<class H, class... Args>
inline void Service::AsyncOper::do_recycle_and_execute_helper(bool orphaned, bool& was_recycled,
                                                              H handler, Args... args)
{
    do_recycle(orphaned);
    was_recycled = true;
    handler(std::move(args)...); // Throws
}

inline void Service::AsyncOper::do_recycle(bool orphaned) noexcept
{
    REALM_ASSERT(in_use());
    void* addr = this;
    std::size_t size = m_size;
    this->~AsyncOper(); // Suicide
    if (orphaned) {
        delete[] static_cast<char*>(addr);
    }
    else {
        new (addr) UnusedOper(size);
    }
}

// ---------------- Resolver ----------------

class Resolver::ResolveOperBase: public Service::AsyncOper {
public:
    ResolveOperBase(std::size_t size, Resolver& r, Query q) noexcept:
        AsyncOper{size, true},
        m_resolver{&r},
        m_query{std::move(q)}
    {
    }
    void perform()
    {
        // FIXME: Temporary hack until we get a true asynchronous resolver
        m_endpoints = m_resolver->resolve(std::move(m_query), m_error_code); // Throws
        set_is_complete(true);
    }
    void recycle() noexcept override final
    {
        bool orphaned = !m_resolver;
        // Note: do_recycle() commits suicide.
        do_recycle(orphaned);
    }
    void orphan() noexcept override final
    {
        m_resolver = nullptr;
    }
protected:
    Resolver* m_resolver;
    Query m_query;
    Endpoint::List m_endpoints;
    std::error_code m_error_code;
};

template<class H> class Resolver::ResolveOper: public ResolveOperBase {
public:
    ResolveOper(std::size_t size, Resolver& r, Query q, H handler):
        ResolveOperBase{size, r, std::move(q)},
        m_handler{std::move(handler)}
    {
    }
    void recycle_and_execute() override final
    {
        REALM_ASSERT(is_complete() || (is_canceled() && !m_error_code));
        REALM_ASSERT(is_canceled() || m_error_code || !m_endpoints.empty());
        bool orphaned = !m_resolver;
        std::error_code ec = m_error_code;
        if (is_canceled())
            ec = error::operation_aborted;
        // Note: do_recycle_and_execute() commits suicide.
        do_recycle_and_execute<H>(orphaned, m_handler, ec, std::move(m_endpoints)); // Throws
    }
private:
    H m_handler;
};

inline Resolver::Resolver(Service& service):
    m_service_impl{*service.m_impl}
{
}

inline Resolver::~Resolver() noexcept
{
    cancel();
}

inline Endpoint::List Resolver::resolve(const Query& q)
{
    std::error_code ec;
    Endpoint::List list = resolve(q, ec);
    if (REALM_UNLIKELY(ec))
        throw std::system_error(ec);
    return list;
}

template<class H> void Resolver::async_resolve(Query query, H handler)
{
    LendersResolveOperPtr op = Service::alloc<ResolveOper<H>>(m_resolve_oper, *this,
                                                              std::move(query),
                                                              std::move(handler)); // Throws
    initiate_oper(std::move(op)); // Throws
}

inline Resolver::Query::Query(std::string service_port, int init_flags):
    m_flags{init_flags},
    m_service{service_port}
{
}

inline Resolver::Query::Query(const StreamProtocol& prot, std::string service_port,
                              int init_flags):
    m_flags{init_flags},
    m_protocol{prot},
    m_service{service_port}
{
}

inline Resolver::Query::Query(std::string host_name, std::string service_port, int init_flags):
    m_flags{init_flags},
    m_host{host_name},
    m_service{service_port}
{
}

inline Resolver::Query::Query(const StreamProtocol& prot, std::string host_name,
                              std::string service_port, int init_flags):
    m_flags{init_flags},
    m_protocol{prot},
    m_host{host_name},
    m_service{service_port}
{
}

inline Resolver::Query::~Query() noexcept
{
}

inline int Resolver::Query::flags() const
{
    return m_flags;
}

inline StreamProtocol Resolver::Query::protocol() const
{
    return m_protocol;
}

inline std::string Resolver::Query::host() const
{
    return m_host;
}

inline std::string Resolver::Query::service() const
{
    return m_service;
}

// ---------------- SocketBase ----------------

inline SocketBase::SocketBase(Service& service):
    m_desc{*service.m_impl}
{
}

inline SocketBase::~SocketBase() noexcept
{
    close();
}

inline bool SocketBase::is_open() const noexcept
{
    return m_desc.is_open();
}

inline auto SocketBase::native_handle() const noexcept -> native_handle_type
{
    return m_desc.native_handle();
}

inline void SocketBase::open(const StreamProtocol& prot)
{
    std::error_code ec;
    if (open(prot, ec))
        throw std::system_error(ec);
}

inline void SocketBase::close() noexcept
{
    if (!is_open())
        return;
    cancel();
    m_desc.close();
}

template<class O>
inline void SocketBase::get_option(O& opt) const
{
    std::error_code ec;
    if (get_option(opt, ec))
        throw std::system_error(ec);
}

template<class O>
inline std::error_code SocketBase::get_option(O& opt, std::error_code& ec) const
{
    opt.get(*this, ec);
    return ec;
}

template<class O>
inline void SocketBase::set_option(const O& opt)
{
    std::error_code ec;
    if (set_option(opt, ec))
        throw std::system_error(ec);
}

template<class O>
inline std::error_code SocketBase::set_option(const O& opt, std::error_code& ec)
{
    opt.set(*this, ec);
    return ec;
}

inline void SocketBase::bind(const Endpoint& ep)
{
    std::error_code ec;
    if (bind(ep, ec))
        throw std::system_error(ec);
}

inline Endpoint SocketBase::local_endpoint() const
{
    std::error_code ec;
    Endpoint ep = local_endpoint(ec);
    if (ec)
        throw std::system_error(ec);
    return ep;
}

inline const StreamProtocol& SocketBase::get_protocol() const noexcept
{
    return m_protocol;
}

template<class T, int opt, class U>
inline SocketBase::Option<T, opt, U>::Option(T init_value):
    m_value{init_value}
{
}

template<class T, int opt, class U>
inline T SocketBase::Option<T, opt, U>::value() const
{
    return m_value;
}

template<class T, int opt, class U>
inline void SocketBase::Option<T, opt, U>::get(const SocketBase& sock, std::error_code& ec)
{
    union {
        U value;
        char strut[sizeof (U) + 1];
    };
    std::size_t value_size = sizeof strut;
    sock.get_option(opt_enum(opt), &value, value_size, ec);
    if (!ec) {
        REALM_ASSERT(value_size == sizeof value);
        m_value = T(value);
    }
}

template<class T, int opt, class U>
inline void SocketBase::Option<T, opt, U>::set(SocketBase& sock, std::error_code& ec) const
{
    U value_to_set = U(m_value);
    sock.set_option(opt_enum(opt), &value_to_set, sizeof value_to_set, ec);
}

// ---------------- Socket ----------------

class Socket::ConnectOperBase: public Service::IoOper {
public:
    ConnectOperBase(std::size_t size, Socket& sock) noexcept:
        IoOper{size},
        m_socket{&sock}
    {
    }
    Want initiate(const Endpoint& ep)
    {
        REALM_ASSERT(this == m_socket->m_write_oper.get());
        if (m_socket->initiate_async_connect(ep, m_error_code)) { // Throws
            set_is_complete(true); // Failure, or immediate completion
            return Want::nothing;
        }
        return Want::write;
    }
    Want advance() noexcept override final
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
    Service::Descriptor& descriptor() noexcept override final
    {
        return m_socket->m_desc;
    }
protected:
    Socket* m_socket;
    std::error_code m_error_code;
};

template<class H> class Socket::ConnectOper: public ConnectOperBase {
public:
    ConnectOper(std::size_t size, Socket& sock, H handler):
        ConnectOperBase{size, sock},
        m_handler{std::move(handler)}
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

inline Socket::Socket(Service& service):
    SocketBase{service}
{
}

inline Socket::Socket(Service& service, const StreamProtocol& prot,
                      native_handle_type native_socket):
    SocketBase{service}
{
    assign(prot, native_socket); // Throws
}

inline Socket::~Socket() noexcept
{
}

inline void Socket::connect(const Endpoint& ep)
{
    std::error_code ec;
    if (connect(ep, ec)) // Throws
        throw std::system_error(ec);
}

inline std::size_t Socket::read(char* buffer, std::size_t size)
{
    std::error_code ec;
    read(buffer, size, ec); // Throws
    if (ec)
        throw std::system_error(ec);
    return size;
}

inline std::size_t Socket::read(char* buffer, std::size_t size, std::error_code& ec)
{
    return StreamOps::read(*this, buffer, size, ec); // Throws
}

inline std::size_t Socket::read(char* buffer, std::size_t size, ReadAheadBuffer& rab)
{
    std::error_code ec;
    read(buffer, size, rab, ec); // Throws
    if (ec)
        throw std::system_error(ec);
    return size;
}

inline std::size_t Socket::read(char* buffer, std::size_t size, ReadAheadBuffer& rab,
                                std::error_code& ec)
{
    int delim = std::char_traits<char>::eof();
    return StreamOps::buffered_read(*this, buffer, size, delim, rab, ec); // Throws
}

inline std::size_t Socket::read_until(char* buffer, std::size_t size, char delim,
                                      ReadAheadBuffer& rab)
{
    std::error_code ec;
    std::size_t n = read_until(buffer, size, delim, rab, ec); // Throws
    if (ec)
        throw std::system_error(ec);
    return n;
}

inline std::size_t Socket::read_until(char* buffer, std::size_t size, char delim,
                                      ReadAheadBuffer& rab, std::error_code& ec)
{
    int delim_2 = std::char_traits<char>::to_int_type(delim);
    return StreamOps::buffered_read(*this, buffer, size, delim_2, rab, ec); // Throws
}

inline std::size_t Socket::write(const char* data, std::size_t size)
{
    std::error_code ec;
    write(data, size, ec); // Throws
    if (ec)
        throw std::system_error(ec);
    return size;
}

inline std::size_t Socket::write(const char* data, std::size_t size, std::error_code& ec)
{
    return StreamOps::write(*this, data, size, ec); // Throws
}

inline std::size_t Socket::read_some(char* buffer, std::size_t size)
{
    std::error_code ec;
    std::size_t n = read_some(buffer, size, ec); // Throws
    if (ec)
        throw std::system_error(ec);
    return n;
}

inline std::size_t Socket::read_some(char* buffer, std::size_t size, std::error_code& ec)
{
    return StreamOps::read_some(*this, buffer, size, ec); // Throws
}

inline std::size_t Socket::write_some(const char* data, std::size_t size)
{
    std::error_code ec;
    std::size_t n = write_some(data, size, ec); // Throws
    if (ec)
        throw std::system_error(ec);
    return n;
}

inline std::size_t Socket::write_some(const char* data, std::size_t size, std::error_code& ec)
{
    return StreamOps::write_some(*this, data, size, ec); // Throws
}

template<class H> inline void Socket::async_connect(const Endpoint& ep, H handler)
{
    LendersConnectOperPtr op =
        Service::alloc<ConnectOper<H>>(m_write_oper, *this, std::move(handler)); // Throws
    m_desc.initiate_oper(std::move(op), ep); // Throws
}

template<class H> inline void Socket::async_read(char* buffer, std::size_t size, H handler)
{
    bool is_read_some = false;
    StreamOps::async_read(*this, buffer, size, is_read_some, std::move(handler)); // Throws
}

template<class H>
inline void Socket::async_read(char* buffer, std::size_t size, ReadAheadBuffer& rab, H handler)
{
    int delim = std::char_traits<char>::eof();
    StreamOps::async_buffered_read(*this, buffer, size, delim, rab, std::move(handler)); // Throws
}

template<class H>
inline void Socket::async_read_until(char* buffer, std::size_t size, char delim,
                                     ReadAheadBuffer& rab, H handler)
{
    int delim_2 = std::char_traits<char>::to_int_type(delim);
    StreamOps::async_buffered_read(*this, buffer, size, delim_2, rab, std::move(handler)); // Throws
}

template<class H> inline void Socket::async_write(const char* data, std::size_t size, H handler)
{
    bool is_write_some = false;
    StreamOps::async_write(*this, data, size, is_write_some, std::move(handler)); // Throws
}

template<class H> inline void Socket::async_read_some(char* buffer, std::size_t size, H handler)
{
    bool is_read_some = true;
    StreamOps::async_read(*this, buffer, size, is_read_some, std::move(handler)); // Throws
}

template<class H>
inline void Socket::async_write_some(const char* data, std::size_t size, H handler)
{
    bool is_write_some = true;
    StreamOps::async_write(*this, data, size, is_write_some, std::move(handler)); // Throws
}

inline void Socket::shutdown(shutdown_type what)
{
    std::error_code ec;
    if (shutdown(what, ec)) // Throws
        throw std::system_error(ec);
}

inline void Socket::assign(const StreamProtocol& prot, native_handle_type native_socket)
{
    std::error_code ec;
    if (assign(prot, native_socket, ec)) // Throws
        throw std::system_error(ec);
}

inline std::error_code Socket::assign(const StreamProtocol& prot,
                                      native_handle_type native_socket, std::error_code& ec)
{
    return do_assign(prot, native_socket, ec); // Throws
}

inline Socket& Socket::lowest_layer() noexcept
{
    return *this;
}

inline void Socket::do_init_read_async(std::error_code&, Want& want) noexcept
{
    want = Want::read; // Wait for read readiness before proceeding
}

inline void Socket::do_init_write_async(std::error_code&, Want& want) noexcept
{
    want = Want::write; // Wait for write readiness before proceeding
}

inline std::size_t Socket::do_read_some_sync(char* buffer, std::size_t size,
                                             std::error_code& ec) noexcept
{
    return m_desc.read_some(buffer, size, ec);
}

inline std::size_t Socket::do_write_some_sync(const char* data, std::size_t size,
                                              std::error_code& ec) noexcept
{
    return m_desc.write_some(data, size, ec);
}

inline std::size_t Socket::do_read_some_async(char* buffer, std::size_t size,
                                              std::error_code& ec, Want& want) noexcept
{
    std::error_code ec_2;
    std::size_t n = m_desc.read_some(buffer, size, ec_2);
    bool success = (!ec_2 || ec_2 == error::resource_unavailable_try_again);
    if (REALM_UNLIKELY(!success)) {
        ec = ec_2;
        want = Want::nothing; // Failure
        return 0;
    }
    ec = std::error_code();
    want = Want::read; // Success
    return n;
}

inline std::size_t Socket::do_write_some_async(const char* data, std::size_t size,
                                               std::error_code& ec, Want& want) noexcept
{
    std::error_code ec_2;
    std::size_t n = m_desc.write_some(data, size, ec_2);
    bool success = (!ec_2 || ec_2 == error::resource_unavailable_try_again);
    if (REALM_UNLIKELY(!success)) {
        ec = ec_2;
        want = Want::nothing; // Failure
        return 0;
    }
    ec = std::error_code();
    want = Want::write; // Success
    return n;
}

// ---------------- Acceptor ----------------

class Acceptor::AcceptOperBase: public Service::IoOper {
public:
    AcceptOperBase(std::size_t size, Acceptor& a, Socket& s, Endpoint* e):
        IoOper{size},
        m_acceptor{&a},
        m_socket{s},
        m_endpoint{e}
    {
    }
    Want initiate()
    {
        REALM_ASSERT(this == m_acceptor->m_read_oper.get());
        REALM_ASSERT(!is_complete());
        m_acceptor->m_desc.ensure_nonblocking_mode(); // Throws
        return Want::read;
    }
    Want advance() noexcept override final
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
        m_acceptor = nullptr;
    }
    Service::Descriptor& descriptor() noexcept override final
    {
        return m_acceptor->m_desc;
    }
protected:
    Acceptor* m_acceptor;
    Socket& m_socket;           // May be dangling after cancellation
    Endpoint* const m_endpoint; // May be dangling after cancellation
    std::error_code m_error_code;
};

template<class H> class Acceptor::AcceptOper: public AcceptOperBase {
public:
    AcceptOper(std::size_t size, Acceptor& a, Socket& s, Endpoint* e, H handler):
        AcceptOperBase{size, a, s, e},
        m_handler{std::move(handler)}
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

inline Acceptor::Acceptor(Service& service):
    SocketBase{service}
{
}

inline Acceptor::~Acceptor() noexcept
{
}

inline void Acceptor::listen(int backlog)
{
    std::error_code ec;
    if (listen(backlog, ec)) // Throws
        throw std::system_error(ec);
}

inline void Acceptor::accept(Socket& sock)
{
    std::error_code ec;
    if (accept(sock, ec)) // Throws
        throw std::system_error(ec);
}

inline void Acceptor::accept(Socket& sock, Endpoint& ep)
{
    std::error_code ec;
    if (accept(sock, ep, ec)) // Throws
        throw std::system_error(ec);
}

inline std::error_code Acceptor::accept(Socket& sock, std::error_code& ec)
{
    Endpoint* ep = nullptr;
    return accept(sock, ep, ec); // Throws
}

inline std::error_code Acceptor::accept(Socket& sock, Endpoint& ep, std::error_code& ec)
{
    return accept(sock, &ep, ec); // Throws
}

template<class H> inline void Acceptor::async_accept(Socket& sock, H handler)
{
    Endpoint* ep = nullptr;
    async_accept(sock, ep, std::move(handler)); // Throws
}

template<class H> inline void Acceptor::async_accept(Socket& sock, Endpoint& ep, H handler)
{
    async_accept(sock, &ep, std::move(handler)); // Throws
}

inline std::error_code Acceptor::accept(Socket& socket, Endpoint* ep, std::error_code& ec)
{
    REALM_ASSERT(!m_read_oper || !m_read_oper->in_use());
    if (REALM_UNLIKELY(socket.is_open()))
        throw std::runtime_error("Socket is already open");
    m_desc.ensure_blocking_mode(); // Throws
    m_desc.accept(socket.m_desc, m_protocol, ep, ec);
    return ec;
}

inline Acceptor::Want Acceptor::do_accept_async(Socket& socket, Endpoint* ep,
                                                std::error_code& ec) noexcept
{
    std::error_code ec_2;
    m_desc.accept(socket.m_desc, m_protocol, ep, ec_2);
    if (ec_2 == error::resource_unavailable_try_again)
        return Want::read;
    ec = ec_2;
    return Want::nothing;
}

template<class H> inline void Acceptor::async_accept(Socket& sock, Endpoint* ep, H handler)
{
    if (REALM_UNLIKELY(sock.is_open()))
        throw std::runtime_error("Socket is already open");
    LendersAcceptOperPtr op = Service::alloc<AcceptOper<H>>(m_read_oper, *this, sock, ep,
                                                            std::move(handler)); // Throws
    m_desc.initiate_oper(std::move(op)); // Throws
}

// ---------------- DeadlineTimer ----------------

template<class H>
class DeadlineTimer::WaitOper: public Service::WaitOperBase {
public:
    WaitOper(std::size_t size, DeadlineTimer& timer, clock::time_point expiration_time, H handler):
        Service::WaitOperBase{size, timer, expiration_time},
        m_handler{std::move(handler)}
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

inline DeadlineTimer::DeadlineTimer(Service& service):
    m_service_impl{*service.m_impl}
{
}

inline DeadlineTimer::~DeadlineTimer() noexcept
{
    cancel();
}

template<class R, class P, class H>
inline void DeadlineTimer::async_wait(std::chrono::duration<R,P> delay, H handler)
{
    clock::time_point now = clock::now();
    // FIXME: This method of detecting overflow does not work. Comparison
    // between distinct duration types is not overflow safe. Overflow easily
    // happens in the implied conversion of arguments to the common duration
    // type (std::common_type<>).
    auto max_add = clock::time_point::max() - now;
    if (delay > max_add)
        throw std::runtime_error("Expiration time overflow");
    clock::time_point expiration_time = now + delay;
    Service::LendersWaitOperPtr op =
        Service::alloc<WaitOper<H>>(m_wait_oper, *this, expiration_time,
                                    std::move(handler)); // Throws
    add_oper(std::move(op)); // Throws
}

// ---------------- ReadAheadBuffer ----------------

inline ReadAheadBuffer::ReadAheadBuffer():
    m_buffer{new char[s_size]} // Throws
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
