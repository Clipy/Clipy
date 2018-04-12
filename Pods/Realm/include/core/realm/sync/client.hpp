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
#ifndef REALM_SYNC_CLIENT_HPP
#define REALM_SYNC_CLIENT_HPP

#include <stdint.h>
#include <memory>
#include <utility>
#include <functional>
#include <exception>
#include <string>

#include <realm/util/logger.hpp>
#include <realm/util/network.hpp>
#include <realm/impl/cont_transact_hist.hpp>
#include <realm/sync/protocol.hpp>
#include <realm/sync/history.hpp>

namespace realm {
namespace sync {

class Client {
public:
    enum class Error;

    enum class ReconnectMode {
        /// This is the mode that should always be used in production. In this
        /// mode the client uses a scheme for determining a reconnect delay that
        /// prevents it from creating too many connection requests in a short
        /// amount of time (i.e., a server hammering protection mechanism).
        normal,

        /// For testing purposes only.
        ///
        /// Never reconnect automatically after the connection is closed due to
        /// an error. Allow immediate reconnect if the connection was closed
        /// voluntarily (e.g., due to sessions being abandoned).
        ///
        /// In this mode, Client::cancel_reconnect_delay() and
        /// Session::cancel_reconnect_delay() can still be used to trigger
        /// another reconnection attempt (with no delay) after an error has
        /// caused the connection to be closed.
        testing
    };

    static constexpr std::uint_fast64_t default_connection_linger_time_ms =  30000; // 30 seconds
    static constexpr std::uint_fast64_t default_ping_keepalive_period_ms  = 600000; // 10 minutes
    static constexpr std::uint_fast64_t default_pong_keepalive_timeout_ms = 300000; //  5 minutes
    static constexpr std::uint_fast64_t default_pong_urgent_timeout_ms    =   5000; //  5 seconds

    struct Config {
        Config() {}

        /// The maximum number of Realm files that will be kept open
        /// concurrently by this client. The client keeps a cache of open Realm
        /// files for efficiency reasons.
        long max_open_files = 256;

        /// An optional logger to be used by the client. If no logger is
        /// specified, the client will use an instance of util::StderrLogger
        /// with the log level threshold set to util::Logger::Level::info. The
        /// client does not require a thread-safe logger, and it guarantees that
        /// all logging happens either on behalf of the constructor or on behalf
        /// of the invocation of run().
        util::Logger* logger = nullptr;

        /// Use ports 80 and 443 by default instead of 7800 and 7801
        /// respectively. Ideally, these default ports should have been made
        /// available via a different URI scheme instead (http/https or ws/wss).
        bool enable_default_port_hack = true;

        /// For testing purposes only.
        ReconnectMode reconnect_mode = ReconnectMode::normal;

        /// Create a separate connection for each session. For testing purposes
        /// only.
        ///
        /// FIXME: This setting needs to be true for now, due to limitations in
        /// the load balancer.
        bool one_connection_per_session = true;

        /// Do not access the local file system. Sessions will act as if
        /// initiated on behalf of an empty (or nonexisting) local Realm
        /// file. Received DOWNLOAD messages will be accepted, but otherwise
        /// ignored. No UPLOAD messages will be generated. For testing purposes
        /// only.
        bool dry_run = false;

        /// The default changeset cooker to be used by new sessions. Can be
        /// overridden by Session::Config::changeset_cooker.
        ///
        /// \sa make_client_history(), TrivialChangesetCooker.
        std::shared_ptr<ClientHistory::ChangesetCooker> changeset_cooker;

        /// The number of milliseconds to keep a connection open after all
        /// sessions have been abandoned (or suspended by errors).
        ///
        /// The purpose of this linger time is to avoid close/reopen cycles
        /// during short periods of time where there are no sessions interested
        /// in using the connection.
        ///
        /// If the connection gets closed due to an error before the linger time
        /// expires, the connection will be kept closed until there are sessions
        /// willing to use it again.
        std::uint_fast64_t connection_linger_time_ms = default_connection_linger_time_ms;

        /// The number of ms between periodic keep-alive pings.
        std::uint_fast64_t ping_keepalive_period_ms = default_ping_keepalive_period_ms;

        /// The number of ms to wait for keep-alive pongs.
        std::uint_fast64_t pong_keepalive_timeout_ms = default_pong_keepalive_timeout_ms;

        /// The number of ms to wait for urgent pongs.
        std::uint_fast64_t pong_urgent_timeout_ms = default_pong_urgent_timeout_ms;

        /// If enable_upload_log_compaction is true, every changeset will be
        /// compacted before it is uploaded to the server. Compaction will
        /// reduce the size of a changeset if the same field is set multiple
        /// times or if newly created objects are deleted within the same
        /// transaction. Log compaction increeses CPU usage and memory
        /// consumption.
        bool enable_upload_log_compaction = true;

        /// Set the `TCP_NODELAY` option on all TCP/IP sockets. This disables
        /// the Nagle algorithm. Disabling it, can in some cases be used to
        /// decrease latencies, but possibly at the expense of scalability. Be
        /// sure to research the subject before you enable this option.
        bool tcp_no_delay = false;
    };

    /// \throw util::EventLoop::Implementation::NotAvailable if no event loop
    /// implementation was specified, and
    /// util::EventLoop::Implementation::get_default() throws it.
    Client(Config = {});
    Client(Client&&) noexcept;
    ~Client() noexcept;

    /// Run the internal event-loop of the client. At most one thread may
    /// execute run() at any given time. The call will not return until somebody
    /// calls stop().
    void run();

    /// See run().
    ///
    /// Thread-safe.
    void stop() noexcept;

    /// \brief Cancel current or next reconnect delay for all servers.
    ///
    /// This corresponds to calling Session::cancel_reconnect_delay() on all
    /// bound sessions, but will also cancel reconnect delays applying to
    /// servers for which there are currently no bound sessions.
    ///
    /// Thread-safe.
    void cancel_reconnect_delay();

    /// \brief Wait for session termination to complete.
    ///
    /// Wait for termination of all sessions whose termination was initiated
    /// prior this call (the completion condition), or until the client's event
    /// loop thread exits from Client::run(), whichever happens
    /// first. Termination of a session can be initiated implicitly (e.g., via
    /// destruction of the session object), or explicitly by Session::detach().
    ///
    /// Note: After session termination (when this function returns true) no
    /// session specific callback function can be called or continue to execute,
    /// and the client is guaranteed to no longer have a Realm file open on
    /// behalf of the terminated session.
    ///
    /// CAUTION: If run() returns while a wait operation is in progress, this
    /// waiting function will return immediately, even if the completion
    /// condition is not yet satisfied. The completion condition is guaranteed
    /// to be satisfied only when these functions return true. If it returns
    /// false, session specific callback functions may still be executing or get
    /// called, and the associated Realm files may still not have been closed.
    ///
    /// If a new wait operation is initiated while another wait operation is in
    /// progress by another thread, the waiting period of fist operation may, or
    /// may not get extended. The application must not assume either.
    ///
    /// Note: Session termination does not imply that the client has received an
    /// UNBOUND message from the server (see the protocol specification). This
    /// may happen later.
    ///
    /// \return True only if the completion condition was satisfied. False if
    /// the client's event loop thread exited from Client::run() in which case
    /// the completion condition may, or may not have been satisfied.
    ///
    /// Note: These functions are fully thread-safe. That is, they may be called
    /// by any thread, and by multiple threads concurrently.
    bool wait_for_session_terminations_or_client_stopped();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    friend class Session;
};


/// Supported protocols:
///
///      Protocol    URL scheme     Default port
///     -----------------------------------------------------------------------------------
///      realm       "realm:"       7800 (80 if Client::Config::enable_default_port_hack)
///      realm_ssl   "realms:"      7801 (443 if Client::Config::enable_default_port_hack)
///
enum class Protocol {
    realm,
    realm_ssl
};


class BadServerUrl; // Exception


/// \brief Client-side representation of a Realm file synchronization session.
///
/// A synchronization session deals with precisely one local Realm file. To
/// synchronize multiple local Realm files, you need multiple sessions.
///
/// A session object is always associated with a particular client object (\ref
/// Client). The application must ensure that the destruction of the associated
/// client object never happens before the destruction of the session
/// object. The consequences of a violation are unspecified.
///
/// A session object is always associated with a particular local Realm file,
/// however, a session object does not represent a session until it is bound to
/// a server side Realm, i.e., until bind() is called. From the point of view of
/// the thread that calls bind(), the session starts precisely when the
/// execution of bind() starts, i.e., before bind() returns.
///
/// At most one session is allowed to exist for a particular local Realm file
/// (file system inode) at any point in time. Multiple session objects may
/// coexists for a single file, as long as bind() has been called on at most one
/// of them. Additionally, two bound session objects for the same file are
/// allowed to exist at different times, if they have no overlap in time (in
/// their bound state), as long as they are associated with the same client
/// object, or with two different client objects that do not overlap in
/// time. This means, in particular, that it is an error to create two bound
/// session objects for the same local Realm file, it they are associated with
/// two different client objects that overlap in time, even if the session
/// objects do not overlap in time (in their bound state). It is the
/// responsibility of the application to ensure that these rules are adhered
/// to. The consequences of a violation are unspecified.
///
/// Thread-safety: It is safe for multiple threads to construct, use (with some
/// exceptions), and destroy session objects concurrently, regardless of whether
/// those session objects are associated with the same, or with different Client
/// objects. Please note that some of the public member functions are fully
/// thread-safe, while others are not.
///
/// Callback semantics: All session specific callback functions will be executed
/// by the event loop thread, i.e., the thread that calls Client::run(). No
/// callback function will be called before Session::bind() is called. Callback
/// functions that are specified prior to calling bind() (e.g., any passed to
/// set_progress_handler()) may start to execute before bind() returns, as long
/// as some thread is executing Client::run(). Likewise, completion handlers,
/// such as those passed to async_wait_for_sync_completion() may start to
/// execute before the submitting function returns. All session specific
/// callback functions (including completion handlers) are guaranteed to no
/// longer be executing when session termination completes, and they are
/// guaranteed to not be called after session termination completes. Termination
/// is an event that completes asynchronously with respect to the application,
/// but is initiated by calling detach(), or implicitely by destroying a session
/// object. After having initiated one or more session terminations, the
/// application can wait for those terminations to complete by calling
/// Client::wait_for_session_terminations_or_client_stopped(). Since callback
/// functinos are always executed by the event loop thread, they are also
/// guaranteed to not be executing after Client::run() has returned.
class Session {
public:
    using port_type = util::network::Endpoint::port_type;
    using SyncTransactCallback = void(VersionID old_version, VersionID new_version);
    using ProgressHandler = void(std::uint_fast64_t downloaded_bytes,
                                 std::uint_fast64_t downloadable_bytes,
                                 std::uint_fast64_t uploaded_bytes,
                                 std::uint_fast64_t uploadable_bytes,
                                 std::uint_fast64_t progress_version,
                                 std::uint_fast64_t snapshot_version);
    using WaitOperCompletionHandler = std::function<void(std::error_code)>;
    using SSLVerifyCallback = bool(const std::string& server_address,
                                   port_type server_port,
                                   const char* pem_data,
                                   size_t pem_size,
                                   int preverify_ok,
                                   int depth);

    class Config {
    public:
        Config() {}

        /// server_address is the fully qualified host name, or IP address of
        /// the server.
        std::string server_address = "localhost";

        /// server_port is the port at which the server listens. If server_port
        /// is zero, the default port for the specified protocol is used. See \ref
        /// Protocol for information on default ports.
        port_type server_port = 0;

        /// server_path is  the virtual path by which the server identifies the
        /// Realm. This path must always be an absolute path, and must therefore
        /// always contain a leading slash (`/`). Further more, each segment of the
        /// virtual path must consist of one or more characters that are either
        /// alpha-numeric or in (`_`, `-`, `.`), and each segment is not allowed to
        /// equal `.` or `..`, and must not end with `.realm`, `.realm.lock`, or
        /// `.realm.management`. These rules are necessary because the server
        /// currently reserves the right to use the specified path as part of the
        /// file system path of a Realm file. It is expected that these rules will
        /// be significantly relaxed in the future by completely decoupling the
        /// virtual paths from actual file system paths.
        std::string server_path = "/";

        /// The protocol used for communicating with the server. See \ref Protocol.
        Protocol protocol = Protocol::realm;

        /// url_prefix is a prefix that is prepended to the server_path
        /// in the HTTP GET request that initiates a sync connection. The value
        /// specified here must match with the server's expectation. Changing
        /// the value of url_prefix should be matched with a corresponding
        /// change of the server side proxy.
        std::string url_prefix = "/realm-sync";

        /// authorization_header_name is the name of the HTTP header containing
        /// the Realm access token. The value of the HTTP header is
        /// "Realm-Access-Token version=1 token=....".
        /// authorization_header_name does not participate in session
        /// multiplexing partitioning.
        std::string authorization_header_name = "Authorization";

        /// custom_http_headers is a map of custom HTTP headers. The keys of the map
        /// are HTTP header names, and the values are the corresponding HTTP
        /// header values.
        /// If "Authorization" is used as a custom header name,
        /// authorization_header_name must be set to anther value.
        std::map<std::string, std::string> custom_http_headers;

        /// Sessions can be multiplexed over the same TCP/SSL connection.
        /// Sessions might share connection if they have identical server_address,
        /// server_port, and protocol. multiplex_ident is a parameter that allows
        /// finer control over session multiplexing. If two sessions have distinct
        /// multiplex_ident, they will never share connection. The typical use of
        /// multilex_ident is to give sessions with incompatible SSL requirements
        /// distinct multiplex_idents.
        /// multiplex_ident can be any string and the value has no meaning except
        /// for partitioning the sessions.
        std::string multiplex_ident;

        /// verify_servers_ssl_certificate controls whether the server
        /// certificate is verified for SSL connections. It should generally be
        /// true in production.
        bool verify_servers_ssl_certificate = true;

        /// ssl_trust_certificate_path is the path of a trust/anchor
        /// certificate used by the client to verify the server certificate.
        /// ssl_trust_certificate_path is only used if the protocol is ssl and
        /// verify_servers_ssl_certificate is true.
        ///
        /// A server certificate is verified by first checking that the
        /// certificate has a valid signature chain back to a trust/anchor
        /// certificate, and secondly checking that the server_address matches
        /// a host name contained in the certificate. The host name of the
        /// certificate is stored in either Common Name or the Alternative
        /// Subject Name (DNS section).
        ///
        /// If ssl_trust_certificate_path is None (default), ssl_verify_callback
        /// (see below) is used if set, and the default device trust/anchor
        /// store is used otherwise.
        Optional<std::string> ssl_trust_certificate_path;

        /// If Client::Config::ssl_verify_callback is set, that function is called
        /// to verify the certificate, unless verify_servers_ssl_certificate is
        /// false.

        /// ssl_verify_callback is used to implement custom SSL certificate
        /// verification. it is only used if the protocol is SSL,
        /// verify_servers_ssl_certificate is true and ssl_trust_certificate_path
        /// is None.
        ///
        /// The signature of ssl_verify_callback is
        ///
        /// bool(const std::string& server_address,
        ///      port_type server_port,
        ///      const char* pem_data,
        ///      size_t pem_size,
        ///      int preverify_ok,
        ///      int depth);
        ///
        /// server address and server_port is the address and port of the server
        /// that a SSL connection is being established to. They are identical to
        /// the server_address and server_port set in this config file and are
        /// passed for convenience.
        /// pem_data is the certificate of length pem_size in
        /// the PEM format. preverify_ok is OpenSSL's preverification of the
        /// certificate. preverify_ok is either 0, or 1. If preverify_ok is 1,
        /// OpenSSL has accepted the certificate and it will generally be safe
        /// to trust that certificate. depth represents the position of the
        /// certificate in the certificate chain sent by the server. depth = 0
        /// represents the actual server certificate that should contain the
        /// host name(server address) of the server. The highest depth is the
        /// root certificate.
        /// The callback function will receive the certificates starting from
        /// the root certificate and moving down the chain until it reaches the
        /// server's own certificate with a host name. The depth of the last
        /// certificate is 0. The depth of the first certificate is chain
        /// length - 1.
        ///
        /// The return value of the callback function decides whether the
        /// client accepts the certificate. If the return value is false, the
        /// processing of the certificate chain is interrupted and the SSL
        /// connection is rejected. If the return value is true, the verification
        /// process continues. If the callback function returns true for all
        /// presented certificates including the depth == 0 certificate, the
        /// SSL connection is accepted.
        ///
        /// A recommended way of using the callback function is to return true
        /// if preverify_ok = 1 and depth > 0,
        /// always check the host name if depth = 0,
        /// and use an independent verification step if preverify_ok = 0.
        ///
        /// Another possible way of using the callback is to collect all the
        /// certificates until depth = 0, and present the entire chain for
        /// independent verification.
        std::function<SSLVerifyCallback> ssl_verify_callback;

        /// signed_user_token is a cryptographically signed token describing the
        /// identity and access rights of the current user.
        std::string signed_user_token;

        /// If not null, overrides whatever is specified by
        /// Client::Config::changeset_cooker.
        ///
        /// The shared ownership over the cooker will be relinquished shortly
        /// after the destruction of the session object as long as the event
        /// loop of the client is being executed (Client::run()).
        ///
        /// CAUTION: ChangesetCooker::cook_changeset() of the specified cooker
        /// may get called before the call to bind() returns, and it may get
        /// called (or continue to execute) after the session object is
        /// destroyed. Please see "Callback semantics" section under Client for
        /// more on this.
        ///
        /// \sa make_client_history(), TrivialChangesetCooker.
        std::shared_ptr<ClientHistory::ChangesetCooker> changeset_cooker;

        /// The encryption key the SharedGroup will be opened with.
        Optional<std::array<char, 64>> encryption_key;

        /// FIXME: This value must currently be true in a cluster setup.
        /// This restriction will be lifted in the future.
        bool one_connection_per_session = true;
    };

    /// \brief Start a new session for the specified client-side Realm.
    ///
    /// Note that the session is not fully activated until you call bind(). Also
    /// note that if you call set_sync_transact_callback(), it must be done
    /// before calling bind().
    ///
    /// \param realm_path The file-system path of a local client-side Realm
    /// file.
    Session(Client&, std::string realm_path, Config = {});

    /// This leaves the right-hand side session object detached. See "Thread
    /// safety" section under detach().
    Session(Session&&) noexcept;

    /// Create a detached session object (see detach()).
    Session() noexcept;

    /// Implies detachment. See "Thread safety" section under detach().
    ~Session() noexcept;

    /// Detach the object on the left-hand side, then "steal" the session from
    /// the object on the right-hand side, if there is one. This leaves the
    /// object on the right-hand side detached. See "Thread safety" section
    /// under detach().
    Session& operator=(Session&&) noexcept;

    /// Detach this sesion object from the client object (Client). If the
    /// session object is already detached, this function has no effect
    /// (idempotency).
    ///
    /// Detachment initiates session termination, which is an event that takes
    /// place shortly therafter in the context of the client's event loop
    /// thread.
    ///
    /// A detached session object may be destroyed, move-assigned to, and moved
    /// from. Apart from that, it is an error to call any function other than
    /// detach() on a detached session object.
    ///
    /// Thread safety: Detachment is not a thread-safe operation. This means
    /// that detach() may not be executed by two threads concurrently, and may
    /// not execute concurrently with object destruction. Additionally,
    /// detachment must not execute concurrently with a moving operation
    /// involving the session object on the left or right-hand side. See move
    /// constructor and assigment operator.
    void detach() noexcept;

    /// \brief Set a function to be called when the local Realm has changed due
    /// to integration of a downloaded changeset.
    ///
    /// Specify the callback function that will be called when one or more
    /// transactions are performed to integrate downloaded changesets into the
    /// client-side Realm, that is associated with this session.
    ///
    /// The callback function will always be called by the thread that executes
    /// the event loop (Client::run()), but not until bind() is called. If the
    /// callback function throws an exception, that exception will "travel" out
    /// through Client::run().
    ///
    /// Note: Any call to this function must have returned before bind() is
    /// called. If this function is called multiple times, each call overrides
    /// the previous setting.
    ///
    /// Note: This function is **not thread-safe**. That is, it is an error if
    /// it is called while another thread is executing any member function on
    /// the same Session object.
    ///
    /// CAUTION: The specified callback function may get called before the call
    /// to bind() returns, and it may get called (or continue to execute) after
    /// the session object is destroyed. Please see "Callback semantics" section
    /// under Session for more on this.
    void set_sync_transact_callback(std::function<SyncTransactCallback>);

    /// \brief Set a handler to monitor the state of download and upload
    /// progress.
    ///
    /// The handler must have signature
    ///
    ///     void(uint_fast64_t downloaded_bytes, uint_fast64_t downloadable_bytes,
    ///          uint_fast64_t uploaded_bytes, uint_fast64_t uploadable_bytes,
    ///          uint_fast64_t progress_version);
    ///
    /// downloaded_bytes is the size in bytes of all downloaded changesets.
    /// downloadable_bytes is the size in bytes of the part of the server
    /// history that do not originate from this client.
    ///
    /// uploaded_bytes is the size in bytes of all locally produced changesets
    /// that have been received and acknowledged by the server.
    /// uploadable_bytes is the size in bytes of all locally produced changesets.
    ///
    /// Due to the nature of the merge rules, it is possible that the size of an
    /// uploaded changeset uploaded from one client is not equal to the size of
    /// the changesets that other clients will download.
    ///
    /// Typical uses of this function:
    ///
    /// Upload completion can be checked by
    ///
    ///    bool upload_complete = (uploaded_bytes == uploadable_bytes);
    ///
    /// Download completion could be checked by
    ///
    ///     bool download_complete = (downloaded_bytes == downloadable_bytes);
    ///
    /// However, download completion might never be reached because the server
    /// can receive new changesets from other clients.
    /// An alternative strategy is to cache downloadable_bytes from the callback,
    /// and use the cached value as the threshold.
    ///
    ///     bool download_complete = (downloaded_bytes == cached_downloadable_bytes);
    ///
    /// Upload progress can be calculated by caching an initial value of
    /// uploaded_bytes from the last, or next, callback. Then
    ///
    ///     double upload_progress =
    ///        (uploaded_bytes - initial_uploaded_bytes)
    ///       -------------------------------------------
    ///       (uploadable_bytes - initial_uploaded_bytes)
    ///
    /// Download progress can be calculates similarly:
    ///
    ///     double download_progress =
    ///        (downloaded_bytes - initial_downloaded_bytes)
    ///       -----------------------------------------------
    ///       (downloadable_bytes - initial_downloaded_bytes)
    ///
    /// progress_version is 0 at the start of a session. When at least one
    /// DOWNLOAD message has been received from the server, progress_version is
    /// positive. progress_version can be used to ensure that the reported
    /// progress contains information obtained from the server in the current
    /// session. The server will send a message as soon as possible, and the
    /// progress handler will eventually be called with a positive progress_version
    /// unless the session is interrupted before a message from the server has
    /// been received.
    ///
    /// The handler is called on the event loop thread.The handler after bind(),
    /// after each DOWNLOAD message, and after each local transaction
    /// (nonsync_transact_notify).
    ///
    /// set_progress_handler() is not thread safe and it must be called before
    /// bind() is called. Subsequent calls to set_progress_handler() overwrite
    /// the previous calls. Typically, this function is called once per session.
    ///
    /// CAUTION: The specified callback function may get called before the call
    /// to bind() returns, and it may get called (or continue to execute) after
    /// the session object is destroyed. Please see "Callback semantics" section
    /// under Session for more on this.
    void set_progress_handler(std::function<ProgressHandler>);

    enum class ConnectionState { disconnected, connecting, connected };

    /// \brief Information about an error causing a session to be temporarily
    /// disconnected from the server.
    ///
    /// In general, the connection will be automatically reestablished
    /// later. Whether this happens quickly, generally depends on \ref
    /// is_fatal. If \ref is_fatal is true, it means that the error is deemed to
    /// be of a kind that is likely to persist, and cause all future reconnect
    /// attempts to fail. In that case, if another attempt is made at
    /// reconnecting, the delay will be substantial (at least an hour).
    ///
    /// \ref error_code specifies the error that caused the connection to be
    /// closed. For the list of errors reported by the server, see \ref
    /// ProtocolError (or `protocol.md`). For the list of errors corresponding
    /// to protocol violations that are detected by the client, see
    /// Client::Error. The error may also be a system level error, or an error
    /// from one of the potential intermediate protocol layers (SSL or
    /// WebSocket).
    ///
    /// \ref detailed_message is the most detailed message available to describe
    /// the error. It is generally equal to `error_code.message()`, but may also
    /// be a more specific message (one that provides extra context). The
    /// purpose of this message is mostly to aid in debugging. For non-debugging
    /// purposes, `error_code.message()` should generally be considered
    /// sufficient.
    ///
    /// \sa set_connection_state_change_listener().
    struct ErrorInfo {
        std::error_code error_code;
        bool is_fatal;
        const std::string& detailed_message;
    };

    using ConnectionStateChangeListener = void(ConnectionState, const ErrorInfo*);

    /// \brief Install a connection state change listener.
    ///
    /// Sets a function to be called whenever the state of the underlying
    /// network connection changes between "disconnected", "connecting", and
    /// "connected". The initial state is always "disconnected". The next state
    /// after "disconnected" is always "connecting". The next state after
    /// "connecting" is either "connected" or "disconnected". The next state
    /// after "connected" is always "disconnected". A switch to the
    /// "disconnected" state only happens when an error occurs.
    ///
    /// Whenever the installed function is called, an ErrorInfo object is passed
    /// when, and only when the passed state is ConnectionState::disconnected.
    ///
    /// When multiple sessions share a single connection, the state changes will
    /// be reported for each session in turn.
    ///
    /// The callback function will always be called by the thread that executes
    /// the event loop (Client::run()), but not until bind() is called. If the
    /// callback function throws an exception, that exception will "travel" out
    /// through Client::run().
    ///
    /// Note: Any call to this function must have returned before bind() is
    /// called. If this function is called multiple times, each call overrides
    /// the previous setting.
    ///
    /// Note: This function is **not thread-safe**. That is, it is an error if
    /// it is called while another thread is executing any member function on
    /// the same Session object.
    ///
    /// CAUTION: The specified callback function may get called before the call
    /// to bind() returns, and it may get called (or continue to execute) after
    /// the session object is destroyed. Please see "Callback semantics" section
    /// under Session for more on this.
    void set_connection_state_change_listener(std::function<ConnectionStateChangeListener>);

    //@{
    /// Deprecated! Use set_connection_state_change_listener() instead.
    using ErrorHandler = void(std::error_code, bool is_fatal, const std::string& detailed_message);
    void set_error_handler(std::function<ErrorHandler>);
    //@}

    /// @{ \brief Bind this session to the specified server side Realm.
    ///
    /// No communication takes place on behalf of this session before the
    /// session is bound, but as soon as the session becomes bound, the server
    /// will start to push changes to the client, and vice versa.
    ///
    /// If a callback function was set using set_sync_transact_callback(), then
    /// that callback function will start to be called as changesets are
    /// downloaded and integrated locally. It is important to understand that
    /// callback functions are executed by the event loop thread (Client::run())
    /// and the callback function may therefore be called before bind() returns.
    ///
    /// Note: It is an error if this function is called more than once per
    /// Session object.
    ///
    /// Note: This function is **not thread-safe**. That is, it is an error if
    /// it is called while another thread is executing any member function on
    /// the same Session object.
    ///
    /// bind() binds this session to the specified server side Realm using the
    /// parameters specified in the Session::Config object.
    ///
    /// The two other forms of bind() are convenience functions.
    /// void bind(std::string server_address, std::string server_path,
    ///           std::string signed_user_token, port_type server_port = 0,
    ///           Protocol protocol = Protocol::realm);
    /// replaces the corresponding parameters from the Session::Config object
    /// before the session is bound.
    /// void bind(std::string server_url, std::string signed_user_token) parses
    /// the \param server_url and replaces the parameters in the Session::Config object
    /// before the session is bound.
    ///
    /// \param server_url For example "realm://sync.realm.io/test". See
    /// server_address, server_path, and server_port in Session::Config for information
    /// about the individual components of the URL. See \ref Protocol for the list of
    /// available URL schemes and the associated default ports.
    ///
    /// \throw BadServerUrl if the specified server URL is malformed.
    void bind();
    void bind(std::string server_url, std::string signed_user_token);
    void bind(std::string server_address, std::string server_path,
              std::string signed_user_token, port_type server_port = 0,
              Protocol protocol = Protocol::realm);
    /// @}

    /// \brief Refresh the access token associated with this session.
    ///
    /// This causes the REFRESH protocol message to be sent to the server. See
    /// \ref Protocol.
    ///
    /// In an on-going session the application may expect the access token to
    /// expire at a certain time and schedule acquisition of a fresh access
    /// token (using a refresh token or by other means) in due time to provide a
    /// better user experience, and seamless connectivity to the server.
    ///
    /// If the application does not proactively refresh an expiring token, the
    /// session will eventually be disconnected. The application can detect this
    /// by monitoring the connection state
    /// (set_connection_state_change_listener()), and check whether the error
    /// code is `ProtocolError::token_expired`. Such a session can then be
    /// revived by calling refresh() with a newly acquired access token.
    ///
    /// Due to protocol techicalities, a race condition exists that can cause a
    /// session to become, and remain disconnected after a new access token has
    /// been passed to refresh(). The application can work around this race
    /// condition by detecting the `ProtocolError::token_expired` error, and
    /// always initiate a token renewal in this case.
    ///
    /// It is an error to call this function before calling `Client::bind()`.
    ///
    /// Note: This function is thread-safe.
    ///
    /// \param signed_user_token A cryptographically signed token describing the
    /// identity and access rights of the current user. See \ref Protocol.
    void refresh(std::string signed_user_token);

    /// \brief Inform the synchronization agent about changes of local origin.
    ///
    /// This function must be called by the application after a transaction
    /// performed on its behalf, that is, after a transaction that is not
    /// performed to integrate a changeset that was downloaded from the server.
    ///
    /// It is an error to call this function before bind() has been called, and
    /// has returned.
    ///
    /// Note: This function is fully thread-safe. That is, it may be called by
    /// any thread, and by multiple threads concurrently.
    void nonsync_transact_notify(version_type new_version);

    /// @{ \brief Wait for upload, download, or upload+download completion.
    ///
    /// async_wait_for_upload_completion() initiates an asynchronous wait for
    /// upload to complete, async_wait_for_download_completion() initiates an
    /// asynchronous wait for download to complete, and
    /// async_wait_for_sync_completion() initiates an asynchronous wait for
    /// upload and download to complete.
    ///
    /// Upload is considered complete when all non-empty changesets of local
    /// origin have been uploaded to the server, and the server has acknowledged
    /// reception of them. Changesets of local origin introduced after the
    /// initiation of the session (after bind() is called) will generally not be
    /// considered for upload unless they are announced to this client through
    /// nonsync_transact_notify() prior to the initiation of the wait operation,
    /// i.e., prior to the invocation of async_wait_for_upload_completion() or
    /// async_wait_for_sync_completion(). Unannounced changesets may get picked
    /// up, but there is no guarantee that they will be, however, if a certain
    /// changeset is announced, then all previous changesets are implicitly
    /// announced. Also all preexisting changesets are implicitly announced
    /// when the session is initiated.
    ///
    /// Download is considered complete when all non-empty changesets of remote
    /// origin have been downloaded from the server, and integrated into the
    /// local Realm state. To know what is currently outstanding on the server,
    /// the client always sends a special "marker" message to the server, and
    /// waits until it has downloaded all outstanding changesets that were
    /// present on the server at the time when the server received that marker
    /// message. Each call to async_wait_for_download_completion() and
    /// async_wait_for_sync_completion() therefore requires a full client <->
    /// server round-trip.
    ///
    /// If a new wait operation is initiated while another wait operation is in
    /// progress by another thread, the waiting period of first operation may,
    /// or may not get extended. The application must not assume either. The
    /// application may assume, however, that async_wait_for_upload_completion()
    /// will not affect the waiting period of
    /// async_wait_for_download_completion(), and vice versa.
    ///
    /// It is an error to call these functions before bind() has been called,
    /// and has returned.
    ///
    /// The specified completion handlers will always be executed by the thread
    /// that executes the event loop (the thread that calls Client::run()). If
    /// the handler throws an exception, that exception will "travel" out
    /// through Client::run().
    ///
    /// If incomplete wait operations exist when the session is terminated,
    /// those wait operations will be canceled. Session termination is an event
    /// that happens in the context of the client's event loop thread shortly
    /// after the destruction of the session object. The std::error_code
    /// argument passed to the completion handler of a canceled wait operation
    /// will be `util::error::operation_aborted`. For uncanceled wait operations
    /// it will be `std::error_code{}`. Note that as long as the client's event
    /// loop thread is running, all completion handlers will be called
    /// regardless of whether the operations get canceled or not.
    ///
    /// CAUTION: The specified completion handlers may get called before the
    /// call to the waiting function returns, and it may get called (or continue
    /// to execute) after the session object is destroyed. Please see "Callback
    /// semantics" section under Session for more on this.
    ///
    /// Note: These functions are fully thread-safe. That is, they may be called
    /// by any thread, and by multiple threads concurrently.
    void async_wait_for_sync_completion(WaitOperCompletionHandler);
    void async_wait_for_upload_completion(WaitOperCompletionHandler);
    void async_wait_for_download_completion(WaitOperCompletionHandler);
    /// @}

    /// @{ \brief Synchronous wait for upload or download completion.
    ///
    /// These functions are synchronous equivalents to
    /// async_wait_for_upload_completion() and
    /// async_wait_for_download_completion() respectively. This means that they
    /// block the caller until the completion condition is satisfied, or the
    /// client's event loop thread exits from Client::run(), whichever happens
    /// first.
    ///
    /// It is an error to call these functions before bind() has been called,
    /// and has returned.
    ///
    /// CAUTION: If Client::run() returns while a wait operation is in progress,
    /// these waiting functions return immediately, even if the completion
    /// condition is not yet satisfied. The completion condition is guaranteed
    /// to be satisfied only when these functions return true.
    ///
    /// \return True only if the completion condition was satisfied. False if
    /// the client's event loop thread exited from Client::run() in which case
    /// the completion condition may, or may not have been satisfied.
    ///
    /// Note: These functions are fully thread-safe. That is, they may be called
    /// by any thread, and by multiple threads concurrently.
    bool wait_for_upload_complete_or_client_stopped();
    bool wait_for_download_complete_or_client_stopped();
    /// @}

    /// \brief Cancel the current or next reconnect delay for the server
    /// associated with this session.
    ///
    /// When the network connection is severed, or an attempt to establish
    /// connection fails, a certain delay will take effect before the client
    /// will attempt to reestablish the connection. This delay will generally
    /// grow with the number of unsuccessful reconnect attempts, and can grow to
    /// over a minute. In some cases however, the application will know when it
    /// is a good time to stop waiting and retry immediately. One example is
    /// when a device has been offline for a while, and the operating system
    /// then tells the application that network connectivity has been restored.
    ///
    /// Clearly, this function should not be called too often and over extended
    /// periods of time, as that would effectively disable the built-in "server
    /// hammering" protection.
    ///
    /// It is an error to call this function before bind() has been called, and
    /// has returned.
    ///
    /// This function is fully thread-safe. That is, it may be called by any
    /// thread, and by multiple threads concurrently.
    void cancel_reconnect_delay();

    /// \brief Change address of server for this session.
    void override_server(std::string address, port_type);

private:
    class Impl;
    Impl* m_impl = nullptr;

    void abandon() noexcept;
    void async_wait_for(bool upload_completion, bool download_completion,
                        WaitOperCompletionHandler);
};


/// \brief Protocol errors discovered by the client.
///
/// These errors will terminate the network connection (disconnect all sessions
/// associated with the affected connection), and the error will be reported to
/// the application via the connection state change listeners of the affected
/// sessions.
enum class Client::Error {
    connection_closed           = 100, ///< Connection closed (no error)
    unknown_message             = 101, ///< Unknown type of input message
    bad_syntax                  = 102, ///< Bad syntax in input message head
    limits_exceeded             = 103, ///< Limits exceeded in input message
    bad_session_ident           = 104, ///< Bad session identifier in input message
    bad_message_order           = 105, ///< Bad input message order
    bad_client_file_ident       = 106, ///< Bad client file identifier (IDENT)
    bad_progress                = 107, ///< Bad progress information (DOWNLOAD)
    bad_changeset_header_syntax = 108, ///< Bad syntax in changeset header (DOWNLOAD)
    bad_changeset_size          = 109, ///< Bad changeset size in changeset header (DOWNLOAD)
    bad_origin_file_ident       = 110, ///< Bad origin file identifier in changeset header (DOWNLOAD)
    bad_server_version          = 111, ///< Bad server version in changeset header (DOWNLOAD)
    bad_changeset               = 112, ///< Bad changeset (DOWNLOAD)
    bad_request_ident           = 113, ///< Bad request identifier (MARK)
    bad_error_code              = 114, ///< Bad error code (ERROR),
    bad_compression             = 115, ///< Bad compression (DOWNLOAD)
    bad_client_version          = 116, ///< Bad last integrated client version in changeset header (DOWNLOAD)
    ssl_server_cert_rejected    = 117, ///< SSL server certificate rejected
    pong_timeout                = 118, ///< Timeout on reception of PONG respone message
    bad_client_file_ident_salt  = 119, ///< Bad client file identifier salt (IDENT)
    bad_file_ident              = 120, ///< Bad file identifier (ALLOC)
};

const std::error_category& client_error_category() noexcept;

std::error_code make_error_code(Client::Error) noexcept;

} // namespace sync
} // namespace realm

namespace std {

template<> struct is_error_code_enum<realm::sync::Client::Error> {
    static const bool value = true;
};

} // namespace std

namespace realm {
namespace sync {



// Implementation

class BadServerUrl: public std::exception {
public:
    const char* what() const noexcept override
    {
        return "Bad server URL";
    }
};

inline Session::Session(Session&& sess) noexcept:
    m_impl{sess.m_impl}
{
    sess.m_impl = nullptr;
}

inline Session::Session() noexcept
{
}

inline Session::~Session() noexcept
{
    if (m_impl)
        abandon();
}

inline Session& Session::operator=(Session&& sess) noexcept
{
    if (m_impl)
        abandon();
    m_impl = sess.m_impl;
    sess.m_impl = nullptr;
    return *this;
}

inline void Session::detach() noexcept
{
    if (m_impl)
        abandon();
    m_impl = nullptr;
}

inline void Session::set_error_handler(std::function<ErrorHandler> handler)
{
    auto handler_2 = [handler=std::move(handler)](ConnectionState state,
                                                  const ErrorInfo* error_info) {
        if (state != ConnectionState::disconnected)
            return;
        REALM_ASSERT(error_info);
        std::error_code ec = error_info->error_code;
        bool is_fatal = error_info->is_fatal;
        const std::string& detailed_message = error_info->detailed_message;
        handler(ec, is_fatal, detailed_message); // Throws
    };
    set_connection_state_change_listener(std::move(handler_2)); // Throws
}

inline void Session::async_wait_for_sync_completion(WaitOperCompletionHandler handler)
{
    bool upload_completion = true, download_completion = true;
    async_wait_for(upload_completion, download_completion, std::move(handler)); // Throws
}

inline void Session::async_wait_for_upload_completion(WaitOperCompletionHandler handler)
{
    bool upload_completion = true, download_completion = false;
    async_wait_for(upload_completion, download_completion, std::move(handler)); // Throws
}

inline void Session::async_wait_for_download_completion(WaitOperCompletionHandler handler)
{
    bool upload_completion = false, download_completion = true;
    async_wait_for(upload_completion, download_completion, std::move(handler)); // Throws
}

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_CLIENT_HPP
