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
#ifndef REALM_UTIL_NETWORK_SSL_HPP
#define REALM_UTIL_NETWORK_SSL_HPP

#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <exception>
#include <system_error>

#include <realm/util/features.h>
#include <realm/util/assert.hpp>
#include <realm/util/misc_errors.hpp>
#include <realm/util/network.hpp>
#include <realm/util/optional.hpp>

#if REALM_HAVE_OPENSSL
#  include <openssl/ssl.h>
#  include <openssl/err.h>
#elif REALM_HAVE_SECURE_TRANSPORT
#  include <realm/util/cf_ptr.hpp>
#  include <Security/Security.h>
#  include <Security/SecureTransport.h>

#define REALM_HAVE_KEYCHAIN_APIS (TARGET_OS_MAC && !TARGET_OS_IPHONE)

#endif

// FIXME: Add necessary support for customizing the SSL server and client
// configurations.

// FIXME: Currently, the synchronous SSL operations (handshake, read, write,
// shutdown) do not automatically retry if the underlying SSL function returns
// with SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE. This normally never
// happens, but it can happen according to the man pages, but in the case of
// SSL_write(), only when a renegotiation has to take place. It is likely that
// the solution is to to wrap the SSL calls inside a loop, such that they keep
// retrying until they succeed, however, such a simple scheme will fail if the
// synchronous operations were to be used with an underlying TCP socket in
// nonblocking mode. Currently, the underlying TCP socket is always in blocking
// mode when performing synchronous operations, but that may continue to be the
// case in teh future.


namespace realm {
namespace util {
namespace network {
namespace ssl {

class ProtocolNotSupported;


/// `VerifyMode::none` corresponds to OpenSSL's `SSL_VERIFY_NONE`, and
/// `VerifyMode::peer` to `SSL_VERIFY_PEER`.
enum class VerifyMode { none, peer };


class Context {
public:
    Context();
    ~Context() noexcept;

    /// File must be in PEM format. Corresponds to OpenSSL's
    /// `SSL_CTX_use_certificate_chain_file()`.
    void use_certificate_chain_file(const std::string& path);

    /// File must be in PEM format. Corresponds to OpenSSL's
    /// `SSL_CTX_use_PrivateKey_file()`.
    void use_private_key_file(const std::string& path);

    /// Calling use_default_verify() will make a client use the
    /// device default certificates for server verification.
    /// For OpenSSL, use_default_verify() corresponds to
    /// SSL_CTX_set_default_verify_paths(SSL_CTX*);
    void use_default_verify();

    /// The verify file is a PEM file containing trust
    /// certificates that the client will use to
    /// verify the server crtificate. If use_verify_file()
    /// is not called, the default device trust store will
    /// be used.
    /// Corresponds roughly to OpenSSL's
    /// SSL_CTX_load_verify_locations().
    void use_verify_file(const std::string& path);

private:
    void ssl_init();
    void ssl_destroy() noexcept;
    void ssl_use_certificate_chain_file(const std::string& path, std::error_code&);
    void ssl_use_private_key_file(const std::string& path, std::error_code&);
    void ssl_use_default_verify(std::error_code&);
    void ssl_use_verify_file(const std::string& path, std::error_code&);

#if REALM_HAVE_OPENSSL
    class OpensslErrorCategory: public std::error_category {
    public:
        const char* name() const noexcept override final;
        std::string message(int value) const override final;
    };
    static OpensslErrorCategory s_openssl_error_category;

    SSL_CTX* m_ssl_ctx = nullptr;
#elif REALM_HAVE_SECURE_TRANSPORT
    class SecureTransportErrorCategory: public std::error_category {
    public:
        const char* name() const noexcept override final;
        std::string message(int value) const override final;
    };
    static SecureTransportErrorCategory s_secure_transport_error_category;

#if REALM_HAVE_KEYCHAIN_APIS
    static util::CFPtr<CFArrayRef> load_pem_file(const std::string& path, SecKeychainRef, std::error_code&);

    std::error_code open_temporary_keychain_if_needed();
    std::error_code update_identity_if_needed();

    util::CFPtr<SecKeychainRef> m_keychain;
    std::string m_keychain_path;

    util::CFPtr<SecCertificateRef> m_certificate;
    util::CFPtr<SecKeyRef> m_private_key;
    util::CFPtr<SecIdentityRef> m_identity;

    util::CFPtr<CFArrayRef> m_certificate_chain;

    util::CFPtr<CFArrayRef> m_trust_anchors;
#endif // REALM_HAVE_KEYCHAIN_APIS
#endif

    friend class Stream;
};


/// Switching between synchronous and asynchronous operations is allowed, but
/// only in a nonoverlapping fashion. That is, a synchronous operation is not
/// allowed to run concurrently with an asynchronous one on the same
/// stream. Note that an asynchronous operation is considered to be running
/// until its completion handler starts executing.
class Stream {
public:
    enum HandshakeType { client, server };

    Stream(Socket&, Context&, HandshakeType);
    ~Stream() noexcept;

    /// \brief Set the certificate verification mode for this SSL stream.
    ///
    /// Corresponds to OpenSSL's `SSL_set_verify()` with null passed as
    /// `verify_callback`.
    ///
    /// Clients should always set it to `VerifyMode::peer`, such that the client
    /// verifies the servers certificate. Servers should only set it to
    /// `VerifyMode::peer` if they want to request a certificate from the
    /// client. When testing with self-signed certificates, it is necessary to
    /// set it to `VerifyMode::none` for clients too.
    ///
    /// It is an error if this function is called after the handshake operation
    /// is initiated.
    ///
    /// The default verify mode is `VerifyMode::none`.
    void set_verify_mode(VerifyMode);

    /// \brief Check the certificate against a host_name.
    ///
    /// set_check_host() includes a host name check in the
    /// certificate verification. It is typically used by clients
    /// to secure that the received certificate has a common name
    /// or subject alternative name that matches \param host_name.
    ///
    /// set_check_host() is only useful if verify_mode is
    /// set to VerifyMode::peer.
    void set_check_host(std::string host_name);

    /// @{
    ///
    /// Read and write operations behave the same way as they do on \ref
    /// network::Socket, except that after cancellation of asynchronous
    /// operations (`lowest_layer().cancel()`), the stream may be left in a bad
    /// state (see below).
    ///
    /// The handshake operation must complete sucessfully before any read,
    /// write, or shutdown operations are performed.
    ///
    /// The shutdown operation sends the shutdown alert to the peer, and
    /// returns/completes as soon as the alert message has been written to the
    /// underlying socket. It is an error if the shutdown operation is initiated
    /// while there are read or write operations in progress. No read or write
    /// operations are allowed to be initiated after the shutdown operation has
    /// been initated. When the shutdown operation has completed, it is safe to
    /// close the underlying socket (`lowest_layer().close()`).
    ///
    /// If a write operation is executing while, or is initiated after a close
    /// notify alert is received from the remote peer, the write operation will
    /// fail with error::broken_pipe.
    ///
    /// Callback functions for async read and write operations must take two
    /// arguments, an std::error_code(), and an integer of a type std::size_t
    /// indicating the number of transferred bytes (other types are allowed as
    /// long as implicit conversion can take place).
    ///
    /// Callback functions for async handshake and shutdown operations must take
    /// a single argument of type std::error_code() (other types are allowed as
    /// long as implicit conversion can take place).
    ///
    /// Resumption of stream operation after cancellation of asynchronous
    /// operations is not supported (does not work). Since the shutdown
    /// operation involves network communication, that operation is also not
    /// allowed after cancellation. The only thing that is allowed, is to
    /// destroy the stream object. Other stream objects are not affected.

    void handshake();
    std::error_code handshake(std::error_code&) noexcept;

    std::size_t read(char* buffer, std::size_t size);
    std::size_t read(char* buffer, std::size_t size, std::error_code& ec) noexcept;
    std::size_t read(char* buffer, std::size_t size, ReadAheadBuffer&);
    std::size_t read(char* buffer, std::size_t size, ReadAheadBuffer&,
                     std::error_code& ec) noexcept;
    std::size_t read_until(char* buffer, std::size_t size, char delim, ReadAheadBuffer&);
    std::size_t read_until(char* buffer, std::size_t size, char delim, ReadAheadBuffer&,
                           std::error_code& ec) noexcept;

    std::size_t write(const char* data, std::size_t size);
    std::size_t write(const char* data, std::size_t size, std::error_code& ec) noexcept;

    std::size_t read_some(char* buffer, std::size_t size);
    std::size_t read_some(char* buffer, std::size_t size, std::error_code&) noexcept;

    std::size_t write_some(const char* data, std::size_t size);
    std::size_t write_some(const char* data, std::size_t size, std::error_code&) noexcept;

    void shutdown();
    std::error_code shutdown(std::error_code&) noexcept;

    template<class H> void async_handshake(H handler);

    template<class H> void async_read(char* buffer, std::size_t size, H handler);
    template<class H> void async_read(char* buffer, std::size_t size, ReadAheadBuffer&, H handler);
    template<class H> void async_read_until(char* buffer, std::size_t size, char delim,
                                            ReadAheadBuffer&, H handler);

    template<class H> void async_write(const char* data, std::size_t size, H handler);

    template<class H> void async_read_some(char* buffer, std::size_t size, H handler);

    template<class H> void async_write_some(const char* data, std::size_t size, H handler);

    template<class H> void async_shutdown(H handler);

    /// @}

    /// Returns a reference to the underlying socket.
    Socket& lowest_layer() noexcept;

private:
    using Want = Service::Want;
    using StreamOps = Service::BasicStreamOps<Stream>;

    class HandshakeOperBase;
    template<class H> class HandshakeOper;
    class ShutdownOperBase;
    template<class H> class ShutdownOper;

    using LendersHandshakeOperPtr = std::unique_ptr<HandshakeOperBase, Service::LendersOperDeleter>;
    using LendersShutdownOperPtr  = std::unique_ptr<ShutdownOperBase,  Service::LendersOperDeleter>;

    Socket& m_tcp_socket;
    Context& m_ssl_context;
    const HandshakeType m_handshake_type;

    // The host name that the certificate should be checked against.
    std::string m_host_name;

    // For async_handshake(), async_read_some()
    Service::OwnersOperPtr m_read_oper;

    // For async_write_some(), async_shutdown()
    Service::OwnersOperPtr m_write_oper;

    // See Service::BasicStreamOps for details on these these 8 functions.
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

    // The meaning of the arguments and return values of ssl_read() and
    // ssl_write() are identical to do_read_some_async() and
    // do_write_some_async() respectively, except that when the return value is
    // nonzero, `want` is always `Want::nothing`, meaning that after bytes have
    // been transferred, ssl_read() and ssl_write() must be called again to
    // figure out whether it is necessary to wait for read or write readiness.
    //
    // The first invocation of ssl_shutdown() must send the shutdown alert to
    // the peer. In blocking mode it must wait until the alert has been sent. In
    // nonblocking mode, it must keep setting `want` to something other than
    // `Want::nothing` until the alert has been sent. When the shutdown alert
    // has been sent, it is safe to shut down the sending side of the underlying
    // socket. On failure, ssl_shutdown() must set `ec` to something differet
    // than `std::error_code()` and return false. On success, it must set `ec`
    // to `std::error_code()`, and return true if a shutdown alert from the peer
    // has already been received, otherwise it must return false. When it sets
    // `want` to something other than `Want::nothing`, it must set `ec` to
    // `std::error_code()` and return false.
    //
    // The second invocation of ssl_shutdown() (after the first invocation
    // completed) must wait for reception on the peers shutdown alert.
    //
    // Note: The semantics around the second invocation of shutdown is currently
    // unused by the higer level API, because of a requirement of compatibility
    // with Apple's Secure Transport API.
    void ssl_init();
    void ssl_destroy() noexcept;
    void ssl_set_verify_mode(VerifyMode, std::error_code&);
    void ssl_set_check_host(std::string, std::error_code&);
    void ssl_handshake(std::error_code&, Want& want) noexcept;
    bool ssl_shutdown(std::error_code& ec, Want& want) noexcept;
    std::size_t ssl_read(char* buffer, std::size_t size,
                         std::error_code&, Want& want) noexcept;
    std::size_t ssl_write(const char* data, std::size_t size,
                          std::error_code&, Want& want) noexcept;

#if REALM_HAVE_OPENSSL
    class BioMethod;
    static BioMethod s_bio_method;
    SSL* m_ssl = nullptr;
    std::error_code m_bio_error_code;

    template<class Oper>
    std::size_t ssl_perform(Oper oper, std::error_code& ec, Want& want) noexcept;

    int do_ssl_accept() noexcept;
    int do_ssl_connect() noexcept;
    int do_ssl_shutdown() noexcept;
    int do_ssl_read(char* buffer, std::size_t size) noexcept;
    int do_ssl_write(const char* data, std::size_t size) noexcept;

    static int bio_write(BIO*, const char*, int) noexcept;
    static int bio_read(BIO*, char*, int) noexcept;
    static int bio_puts(BIO*, const char*) noexcept;
    static long bio_ctrl(BIO*, int, long, void*) noexcept;
    static int bio_create(BIO*) noexcept;
    static int bio_destroy(BIO*) noexcept;
    static int verify_callback(int preverify_ok, X509_STORE_CTX *ctx) noexcept;

#elif REALM_HAVE_SECURE_TRANSPORT
    util::CFPtr<SSLContextRef> m_ssl;
    VerifyMode m_verify_mode = VerifyMode::none;

    enum class BlockingOperation {
        read,
        write,
    };
    util::Optional<BlockingOperation> m_last_operation;

    // Details of the underlying I/O error that lead to errSecIO being returned from a SecureTransport function.
    std::error_code m_last_error;

    template<class Oper>
    std::size_t ssl_perform(Oper oper, std::error_code& ec, Want& want) noexcept;

    std::pair<OSStatus, std::size_t> do_ssl_handshake() noexcept;
    std::pair<OSStatus, std::size_t> do_ssl_shutdown() noexcept;
    std::pair<OSStatus, std::size_t> do_ssl_read(char* buffer, std::size_t size) noexcept;
    std::pair<OSStatus, std::size_t> do_ssl_write(const char* data, std::size_t size) noexcept;

    static OSStatus tcp_read(SSLConnectionRef, void*, std::size_t* length) noexcept;
    static OSStatus tcp_write(SSLConnectionRef, const void*, std::size_t* length) noexcept;

    OSStatus tcp_read(void*, std::size_t* length) noexcept;
    OSStatus tcp_write(const void*, std::size_t* length) noexcept;

    OSStatus verify_peer() noexcept;
#endif

    friend class Service::BasicStreamOps<Stream>;
    friend class network::ReadAheadBuffer;
};




// Implementation

class ProtocolNotSupported: public std::exception {
public:
    const char* what() const noexcept override final;
};

inline Context::Context()
{
    ssl_init(); // Throws
}

inline Context::~Context() noexcept
{
    ssl_destroy();
}

inline void Context::use_certificate_chain_file(const std::string& path)
{
    std::error_code ec;
    ssl_use_certificate_chain_file(path, ec); // Throws
    if (ec)
        throw std::system_error(ec);
}

inline void Context::use_private_key_file(const std::string& path)
{
    std::error_code ec;
    ssl_use_private_key_file(path, ec); // Throws
    if (ec)
        throw std::system_error(ec);
}

inline void Context::use_default_verify()
{
    std::error_code ec;
    ssl_use_default_verify(ec);
    if (ec)
        throw std::system_error(ec);
}

inline void Context::use_verify_file(const std::string& path)
{
    std::error_code ec;
    ssl_use_verify_file(path, ec);
    if (ec)
        throw std::system_error(ec);
}


class Stream::HandshakeOperBase: public Service::IoOper {
public:
    HandshakeOperBase(std::size_t size, Stream& stream):
        IoOper{size},
        m_stream{&stream}
    {
    }
    Want initiate() noexcept
    {
        REALM_ASSERT(this == m_stream->m_read_oper.get());
        REALM_ASSERT(!is_complete());
        if (m_stream->lowest_layer().ensure_nonblocking_mode(m_error_code)) {
            set_is_complete(true); // Failure
            return Want::nothing;
        }
        return proceed();
    }
    Want proceed() noexcept override final
    {
        REALM_ASSERT(!is_complete());
        REALM_ASSERT(!is_canceled());
        REALM_ASSERT(!m_error_code);
        Want want = Want::nothing;
        m_stream->ssl_handshake(m_error_code, want);
        set_is_complete(want == Want::nothing);
        return want;
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
    SocketBase& get_socket() noexcept
    {
        return m_stream->lowest_layer();
    }
protected:
    Stream* m_stream;
    std::error_code m_error_code;
};

template<class H> class Stream::HandshakeOper: public HandshakeOperBase {
public:
    HandshakeOper(std::size_t size, Stream& stream, H handler):
        HandshakeOperBase{size, stream},
        m_handler{std::move(handler)}
    {
    }
    void recycle_and_execute() override final
    {
        REALM_ASSERT(is_complete() || is_canceled());
        bool orphaned = !m_stream;
        std::error_code ec = m_error_code;
        if (is_canceled())
            ec = error::operation_aborted;
        // Note: do_recycle_and_execute() commits suicide.
        do_recycle_and_execute<H>(orphaned, m_handler, ec); // Throws
    }
private:
    H m_handler;
};

class Stream::ShutdownOperBase: public Service::IoOper {
public:
    ShutdownOperBase(std::size_t size, Stream& stream):
        IoOper{size},
        m_stream{&stream}
    {
    }
    Want initiate() noexcept
    {
        REALM_ASSERT(this == m_stream->m_write_oper.get());
        REALM_ASSERT(!is_complete());
        if (m_stream->lowest_layer().ensure_nonblocking_mode(m_error_code)) {
            set_is_complete(true); // Failure
            return Want::nothing;
        }
        return proceed();
    }
    Want proceed() noexcept override final
    {
        REALM_ASSERT(!is_complete());
        REALM_ASSERT(!is_canceled());
        REALM_ASSERT(!m_error_code);
        Want want = Want::nothing;
        m_stream->ssl_shutdown(m_error_code, want);
        if (want == Want::nothing)
            set_is_complete(true);
        return want;
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
    SocketBase& get_socket() noexcept
    {
        return m_stream->lowest_layer();
    }
protected:
    Stream* m_stream;
    std::error_code m_error_code;
};

template<class H> class Stream::ShutdownOper: public ShutdownOperBase {
public:
    ShutdownOper(std::size_t size, Stream& stream, H handler):
        ShutdownOperBase{size, stream},
        m_handler{std::move(handler)}
    {
    }
    void recycle_and_execute() override final
    {
        REALM_ASSERT(is_complete() || is_canceled());
        bool orphaned = !m_stream;
        std::error_code ec = m_error_code;
        if (is_canceled())
            ec = error::operation_aborted;
        // Note: do_recycle_and_execute() commits suicide.
        do_recycle_and_execute<H>(orphaned, m_handler, ec); // Throws
    }
private:
    H m_handler;
};

inline Stream::Stream(Socket& socket, Context& context, HandshakeType type):
    m_tcp_socket{socket},
    m_ssl_context{context},
    m_handshake_type{type}
{
    ssl_init(); // Throws
}

inline Stream::~Stream() noexcept
{
    bool any_incomplete = false;
    if (m_read_oper && m_read_oper->is_uncanceled()) {
        m_read_oper->cancel();
        if (!m_read_oper->is_complete())
            any_incomplete = true;
    }
    if (m_write_oper && m_write_oper->is_uncanceled()) {
        m_write_oper->cancel();
        if (!m_write_oper->is_complete())
            any_incomplete = true;
    }
    if (any_incomplete) {
        Service& service = m_tcp_socket.get_service();
        service.cancel_incomplete_io_ops(m_tcp_socket.get_sock_fd());
    }
    ssl_destroy();
}

inline void Stream::set_verify_mode(VerifyMode mode)
{
    std::error_code ec;
    ssl_set_verify_mode(mode, ec); // Throws
    if (ec)
        throw std::system_error(ec);
}

inline void Stream::set_check_host(std::string host_name)
{
    std::error_code ec;
    ssl_set_check_host(host_name, ec);
    if (ec)
        throw std::system_error(ec);
}

inline void Stream::handshake()
{
    std::error_code ec;
    if (handshake(ec))
        throw std::system_error(ec);
}

inline std::size_t Stream::read(char* buffer, std::size_t size)
{
    std::error_code ec;
    read(buffer, size, ec);
    if (ec)
        throw std::system_error(ec);
    return size;
}

inline std::size_t Stream::read(char* buffer, std::size_t size, std::error_code& ec) noexcept
{
    return StreamOps::read(*this, buffer, size, ec);
}

inline std::size_t Stream::read(char* buffer, std::size_t size, ReadAheadBuffer& rab)
{
    std::error_code ec;
    read(buffer, size, rab, ec);
    if (ec)
        throw std::system_error(ec);
    return size;
}

inline std::size_t Stream::read(char* buffer, std::size_t size, ReadAheadBuffer& rab,
                                std::error_code& ec) noexcept
{
    int delim = std::char_traits<char>::eof();
    return StreamOps::buffered_read(*this, buffer, size, delim, rab, ec);
}

inline std::size_t Stream::read_until(char* buffer, std::size_t size, char delim,
                                      ReadAheadBuffer& rab)
{
    std::error_code ec;
    std::size_t n = read_until(buffer, size, delim, rab, ec);
    if (ec)
        throw std::system_error(ec);
    return n;
}

inline std::size_t Stream::read_until(char* buffer, std::size_t size, char delim,
                                      ReadAheadBuffer& rab, std::error_code& ec) noexcept
{
    int delim_2 = std::char_traits<char>::to_int_type(delim);
    return StreamOps::buffered_read(*this, buffer, size, delim_2, rab, ec);
}

inline std::size_t Stream::write(const char* data, std::size_t size)
{
    std::error_code ec;
    write(data, size, ec);
    if (ec)
        throw std::system_error(ec);
    return size;
}

inline std::size_t Stream::write(const char* data, std::size_t size, std::error_code& ec) noexcept
{
    return StreamOps::write(*this, data, size, ec);
}

inline std::size_t Stream::read_some(char* buffer, std::size_t size)
{
    std::error_code ec;
    std::size_t n = read_some(buffer, size, ec);
    if (ec)
        throw std::system_error(ec);
    return n;
}

inline std::size_t Stream::read_some(char* buffer, std::size_t size, std::error_code& ec) noexcept
{
    return StreamOps::read_some(*this, buffer, size, ec);
}

inline std::size_t Stream::write_some(const char* data, std::size_t size)
{
    std::error_code ec;
    std::size_t n = write_some(data, size, ec);
    if (ec)
        throw std::system_error(ec);
    return n;
}

inline std::size_t Stream::write_some(const char* data, std::size_t size,
                                      std::error_code& ec) noexcept
{
    return StreamOps::write_some(*this, data, size, ec);
}

inline void Stream::shutdown()
{
    std::error_code ec;
    if (shutdown(ec))
        throw std::system_error(ec);
}

template<class H> inline void Stream::async_handshake(H handler)
{
    LendersHandshakeOperPtr op =
        Service::alloc<HandshakeOper<H>>(m_read_oper, *this, std::move(handler)); // Throws
    Service::initiate_io_oper(std::move(op)); // Throws
}

template<class H> inline void Stream::async_read(char* buffer, std::size_t size, H handler)
{
    bool is_read_some = false;
    StreamOps::async_read(*this, buffer, size, is_read_some, std::move(handler)); // Throws
}

template<class H>
inline void Stream::async_read(char* buffer, std::size_t size, ReadAheadBuffer& rab, H handler)
{
    int delim = std::char_traits<char>::eof();
    StreamOps::async_buffered_read(*this, buffer, size, delim, rab, std::move(handler)); // Throws
}

template<class H>
inline void Stream::async_read_until(char* buffer, std::size_t size, char delim,
                                     ReadAheadBuffer& rab, H handler)
{
    int delim_2 = std::char_traits<char>::to_int_type(delim);
    StreamOps::async_buffered_read(*this, buffer, size, delim_2, rab, std::move(handler)); // Throws
}

template<class H> inline void Stream::async_write(const char* data, std::size_t size, H handler)
{
    bool is_write_some = false;
    StreamOps::async_write(*this, data, size, is_write_some, std::move(handler)); // Throws
}

template<class H> inline void Stream::async_read_some(char* buffer, std::size_t size, H handler)
{
    bool is_read_some = true;
    StreamOps::async_read(*this, buffer, size, is_read_some, std::move(handler)); // Throws
}

template<class H> inline void Stream::async_write_some(const char* data, std::size_t size, H handler)
{
    bool is_write_some = true;
    StreamOps::async_write(*this, data, size, is_write_some, std::move(handler)); // Throws
}

template<class H> inline void Stream::async_shutdown(H handler)
{
    LendersShutdownOperPtr op =
        Service::alloc<ShutdownOper<H>>(m_write_oper, *this, std::move(handler)); // Throws
    Service::initiate_io_oper(std::move(op)); // Throws
}

inline void Stream::do_init_read_sync(std::error_code& ec) noexcept
{
    lowest_layer().ensure_blocking_mode(ec);
}

inline void Stream::do_init_write_sync(std::error_code& ec) noexcept
{
    lowest_layer().ensure_blocking_mode(ec);
}

inline void Stream::do_init_read_async(std::error_code& ec, Want& want) noexcept
{
    lowest_layer().ensure_nonblocking_mode(ec);
    want = Want::nothing; // Proceed immediately unless there is an error
}

inline void Stream::do_init_write_async(std::error_code& ec, Want& want) noexcept
{
    lowest_layer().ensure_nonblocking_mode(ec);
    want = Want::nothing; // Proceed immediately unless there is an error
}

inline std::size_t Stream::do_read_some_sync(char* buffer, std::size_t size,
                                             std::error_code& ec) noexcept
{
    Want want = Want::nothing;
    std::size_t n = do_read_some_async(buffer, size, ec, want);
    if (n == 0 && want != Want::nothing)
        ec = error::resource_unavailable_try_again;
    return n;
}

inline std::size_t Stream::do_write_some_sync(const char* data, std::size_t size,
                                              std::error_code& ec) noexcept
{
    Want want = Want::nothing;
    std::size_t n = do_write_some_async(data, size, ec, want);
    if (n == 0 && want != Want::nothing)
        ec = error::resource_unavailable_try_again;
    return n;
}

inline std::size_t Stream::do_read_some_async(char* buffer, std::size_t size,
                                              std::error_code& ec, Want& want) noexcept
{
    return ssl_read(buffer, size, ec, want);
}

inline std::size_t Stream::do_write_some_async(const char* data, std::size_t size,
                                               std::error_code& ec, Want& want) noexcept
{
    return ssl_write(data, size, ec, want);
}

inline Socket& Stream::lowest_layer() noexcept
{
    return m_tcp_socket;
}


#if REALM_HAVE_OPENSSL

inline void Stream::ssl_handshake(std::error_code& ec, Want& want) noexcept
{
    auto perform = [this]() noexcept {
        switch (m_handshake_type) {
            case client:
                return do_ssl_connect();
            case server:
                return do_ssl_accept();
        }
        REALM_ASSERT(false);
        return 0;
    };
    std::size_t n = ssl_perform(std::move(perform), ec, want);
    REALM_ASSERT(n == 0 || n == 1);
    if (want == Want::nothing && n == 0 && !ec) {
        // End of input on TCP socket
        ec = network::premature_end_of_input;
    }
}

inline std::size_t Stream::ssl_read(char* buffer, std::size_t size,
                                    std::error_code& ec, Want& want) noexcept
{
    auto perform = [this, buffer, size]() noexcept {
        return do_ssl_read(buffer, size);
    };
    std::size_t n = ssl_perform(std::move(perform), ec, want);
    if (want == Want::nothing && n == 0 && !ec) {
        // End of input on TCP socket
        if (SSL_get_shutdown(m_ssl) & SSL_RECEIVED_SHUTDOWN) {
            ec = network::end_of_input;
        }
        else {
            ec = network::premature_end_of_input;
        }
    }
    return n;
}

inline std::size_t Stream::ssl_write(const char* data, std::size_t size,
                                     std::error_code& ec, Want& want) noexcept
{
    // While OpenSSL is able to continue writing after we have received the
    // close notify alert fro the remote peer, Apple's Secure Transport API is
    // not, so to achieve common behaviour, we make sure that any such attempt
    // will result in an `error::broken_pipe` error.
    if ((SSL_get_shutdown(m_ssl) & SSL_RECEIVED_SHUTDOWN) != 0) {
        ec = error::broken_pipe;
        want = Want::nothing;
        return 0;
    }
    auto perform = [this, data, size]() noexcept {
        return do_ssl_write(data, size);
    };
    std::size_t n = ssl_perform(std::move(perform), ec, want);
    if (want == Want::nothing && n == 0 && !ec) {
        // End of input on TCP socket
        ec = network::premature_end_of_input;
    }
    return n;
}

inline bool Stream::ssl_shutdown(std::error_code& ec, Want& want) noexcept
{
    auto perform = [this]() noexcept {
        return do_ssl_shutdown();
    };
    std::size_t n = ssl_perform(std::move(perform), ec, want);
    REALM_ASSERT(n == 0 || n == 1);
    if (want == Want::nothing && n == 0 && !ec) {
        // The first invocation of SSL_shutdown() does not signal completion
        // until the shutdown alert has been sent to the peer, or an error
        // occurred (does not wait for acknowledgment).
        //
        // The second invocation (after a completed first invocation) does not
        // signal completion until the peers shutdown alert has been received,
        // or an error occurred.
        //
        // It is believed that:
        //
        // If this is the first time SSL_shutdown() is called, and
        // `SSL_get_shutdown() & SSL_SENT_SHUTDOWN` evaluates to nonzero, then a
        // zero return value means "partial success" (shutdown alert was sent,
        // but the peers shutdown alert was not yet received), and 1 means "full
        // success" (peers shutdown alert has already been received).
        //
        // If this is the first time SSL_shutdown() is called, and
        // `SSL_get_shutdown() & SSL_SENT_SHUTDOWN` valuates to zero, then a
        // zero return value means "premature end of input", and 1 is supposedly
        // not a possibility.
        //
        // If this is the second time SSL_shutdown() is called (after the first
        // call has returned zero), then a zero return value means "premature
        // end of input", and 1 means "full success" (peers shutdown alert has
        // now been received).
        if ((SSL_get_shutdown(m_ssl) & SSL_SENT_SHUTDOWN) == 0)
            ec = network::premature_end_of_input;
    }
    return (n > 0);
}

// Provides a homogeneous, and mostly quirks-free interface across the OpenSSL
// operations (handshake, read, write, shutdown).
//
// First of all, if the operation remains incomplete (neither successfully
// completed, nor failed), ssl_perform() will set `ec` to `std::system_error()`,
// `want` to something other than `Want::nothing`, and return zero.
//
// Such a situation will normally only happen when the underlying TCP socket is
// in nonblocking mode, and the read/write requirements of the operation could
// not be immediately accommodated. However, as is noted in the SSL_write() man
// page, it can also happen in blocking mode (at least while writing).
//
// If an error occurred, ssl_perform() will set `ec` to something other than
// `std::system_error()`, `want` to `Want::nothing`, and return 0.
//
// If no error occurred, and the operation completed (`!ec && want ==
// Want::nothing`), then the return value indicates the outcome of the
// operation.
//
// In general, a nonzero value means "full" success, and a zero value means
// "partial" success, however, a zero result can also generally mean "premature
// end of input" / "unclean protocol termination".
//
// Assuming there is no premature end of input, then for reads and writes, the
// returned value is the number of transferred bytes. Zero for read on end of
// input. Never zero for write. For handshake it is always 1. For shutdown it is
// 1 if the peer shutdown alert was already received, otherwise it is zero.
//
// ssl_read() should use `SSL_get_shutdown() & SSL_RECEIVED_SHUTDOWN` to
// distinguish between the two possible meanings of zero.
//
// ssl_shutdown() should use `SSL_get_shutdown() & SSL_SENT_SHUTDOWN` to
// distinguish between the two possible meanings of zero.
template<class Oper>
std::size_t Stream::ssl_perform(Oper oper, std::error_code& ec, Want& want) noexcept
{
    ERR_clear_error();
    m_bio_error_code = std::error_code(); // Success
    int ret = oper();
    int ssl_error = SSL_get_error(m_ssl, ret);
    int sys_error = int(ERR_get_error());

    // Guaranteed by the documentstion of SSL_get_error()
    REALM_ASSERT((ret > 0) == (ssl_error == SSL_ERROR_NONE));

    REALM_ASSERT(!m_bio_error_code || ssl_error == SSL_ERROR_SYSCALL);

    // Judging from various comments in the man pages, and from experience with
    // the API, it seems that,
    //
    //   ret=0, ssl_error=SSL_ERROR_SYSCALL, sys_error=0
    //
    // is supposed to be an indicator of "premature end of input" / "unclean
    // protocol termination", while
    //
    //   ret=0, ssl_error=SSL_ERROR_ZERO_RETURN
    //
    // is supposed to be an indicator of the following success conditions:
    //
    //   - Mature end of input / clean protocol termination.
    //
    //   - Successful transmission of the shutdown alert, but no prior reception
    //     of shutdown alert from peer.
    //
    // Unfortunately, as is also remarked in various places in the man pages,
    // those two success conditions may actually result in `ret=0,
    // ssl_error=SSL_ERROR_SYSCALL, sys_error=0` too, and it seems that they
    // almost always do.
    //
    // This means that we cannot properly discriminate between these conditions
    // in ssl_perform(), and will have to defer to the caller to interpret the
    // situation. Since thay cannot be properly told apart, we report all
    // `ret=0, ssl_error=SSL_ERROR_SYSCALL, sys_error=0` and `ret=0,
    // ssl_error=SSL_ERROR_ZERO_RETURN` cases as the latter.
    switch (ssl_error) {
        case SSL_ERROR_NONE:
            ec = std::error_code(); // Success
            want = Want::nothing;
            return std::size_t(ret); // ret > 0
        case SSL_ERROR_ZERO_RETURN:
            ec = std::error_code(); // Success
            want = Want::nothing;
            return 0;
        case SSL_ERROR_WANT_READ:
            ec = std::error_code(); // Success
            want = Want::read;
            return 0;
        case SSL_ERROR_WANT_WRITE:
            ec = std::error_code(); // Success
            want = Want::write;
            return 0;
        case SSL_ERROR_SYSCALL:
            if (REALM_UNLIKELY(sys_error != 0)) {
                ec = make_basic_system_error_code(sys_error);
            }
            else if (REALM_UNLIKELY(m_bio_error_code)) {
                ec = m_bio_error_code;
            }
            else if (ret == 0) {
                // ret = 0, ssl_eror = SSL_ERROR_SYSCALL, sys_error = 0
                //
                // See remarks above!
                ec = std::error_code(); // Success
            }
            else {
                // ret = -1, ssl_eror = SSL_ERROR_SYSCALL, sys_error = 0
                //
                // This situation arises in OpenSSL version >= 1.1.
                // It has been observed in the SSL_connect call if the
                // other endpoint terminates the connection during
                // SSL_connect. The OpenSSL documentation states
                // that ret = -1 implies an underlying BIO error and
                // that errno should be consulted. However,
                // errno = 0(Undefined error) in the observed case.
                // At the moment. we will report
                // premature_end_of_input.
                // If we see this error case occuring in other situations in
                // the future, we will have to update this case.
                ec = network::premature_end_of_input;
            }
            want = Want::nothing;
            return 0;
        case SSL_ERROR_SSL:
            ec = std::error_code(sys_error, Context::s_openssl_error_category);
            want = Want::nothing;
            return 0;
        default:
            break;
    }
    // We are not supposed to ever get here
    REALM_ASSERT(false);
    return 0;
}

inline int Stream::do_ssl_accept() noexcept
{
    int ret = SSL_accept(m_ssl);
    return ret;
}

inline int Stream::do_ssl_connect() noexcept
{
    int ret = SSL_connect(m_ssl);
    return ret;
}

inline int Stream::do_ssl_read(char* buffer, std::size_t size) noexcept
{
    int size_2 = int(size);
    if (size > unsigned(std::numeric_limits<int>::max()))
        size_2 = std::size_t(std::numeric_limits<int>::max());
    int ret = SSL_read(m_ssl, buffer, size_2);
    return ret;
}

inline int Stream::do_ssl_write(const char* data, std::size_t size) noexcept
{
    int size_2 = int(size);
    if (size > unsigned(std::numeric_limits<int>::max()))
        size_2 = std::size_t(std::numeric_limits<int>::max());
    int ret = SSL_write(m_ssl, data, size_2);
    return ret;
}

inline int Stream::do_ssl_shutdown() noexcept
{
    int ret = SSL_shutdown(m_ssl);
    return ret;
}

#elif REALM_HAVE_SECURE_TRANSPORT

// Provides a homogeneous, and mostly quirks-free interface across the SecureTransport
// operations (handshake, read, write, shutdown).
//
// First of all, if the operation remains incomplete (neither successfully
// completed, nor failed), ssl_perform() will set `ec` to `std::system_error()`,
// `want` to something other than `Want::nothing`, and return zero.
//
// If an error occurred, ssl_perform() will set `ec` to something other than
// `std::system_error()`, `want` to `Want::nothing`, and return 0.
//
// If no error occured, and the operation completed (`!ec && want ==
// Want::nothing`), then the return value indicates the outcome of the
// operation.
//
// In general, a nonzero value means "full" success, and a zero value means
// "partial" success, however, a zero result can also generally mean "premature
// end of input" / "unclean protocol termination".
//
// Assuming there is no premature end of input, then for reads and writes, the
// returned value is the number of transferred bytes. Zero for read on end of
// input. Never zero for write. For handshake it is always 1. For shutdown it is
// 1 if the peer shutdown alert was already received, otherwise it is zero.
template<class Oper>
std::size_t Stream::ssl_perform(Oper oper, std::error_code& ec, Want& want) noexcept
{
    OSStatus result;
    std::size_t n;
    std::tie(result, n) = oper();

    if (result == noErr) {
        ec = std::error_code();
        want = Want::nothing;
        return n;
    }

    if (result == errSSLWouldBlock) {
        REALM_ASSERT(m_last_operation);
        ec = std::error_code();
        want = m_last_operation == BlockingOperation::read ? Want::read : Want::write;
        m_last_operation = {};
        return n;
    }

    if (result == errSSLClosedGraceful) {
        ec = network::end_of_input;
        want = Want::nothing;
        return n;
    }

    if (result == errSSLClosedAbort || result == errSSLClosedNoNotify) {
        ec = network::premature_end_of_input;
        want = Want::nothing;
        return n;
    }

    if (result == errSecIO) {
        // A generic I/O error means something went wrong at a lower level. Use the error
        // code we smuggled out of our lower-level functions to provide a more specific error.
        REALM_ASSERT(m_last_error);
        ec = m_last_error;
        want = Want::nothing;
        return n;
    }

    ec = std::error_code(result, Context::s_secure_transport_error_category);
    want = Want::nothing;
    return 0;
}
#endif // REALM_HAVE_OPENSSL / REALM_HAVE_SECURE_TRANSPORT

} // namespace ssl
} // namespace network
} // namespace util
} // namespace realm

#endif // REALM_UTIL_NETWORK_SSL_HPP
