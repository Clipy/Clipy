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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <functional>
#include <exception>
#include <string>

#include <realm/util/buffer.hpp>
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

    using port_type = util::network::Endpoint::port_type;
    using RoundtripTimeHandler = void(milliseconds_type roundtrip_time);

    static constexpr milliseconds_type default_connect_timeout        = 120000; // 2 minutes
    static constexpr milliseconds_type default_connection_linger_time =  30000; // 30 seconds
    static constexpr milliseconds_type default_ping_keepalive_period  =  60000; // 1 minute
    static constexpr milliseconds_type default_pong_keepalive_timeout = 120000; // 2 minutes
    static constexpr milliseconds_type default_fast_reconnect_limit   =  60000; // 1 minute

    struct Config {
        Config() {}

        /// An optional custom platform description to be sent to server as part
        /// of a user agent description (HTTP `User-Agent` header).
        ///
        /// If left empty, the platform description will be whatever is returned
        /// by util::get_platform_info().
        std::string user_agent_platform_info;

        /// Optional information about the application to be added to the user
        /// agent description as sent to the server. The intention is that the
        /// application describes itself using the following (rough) syntax:
        ///
        ///     <application info>  ::=  (<space> <layer>)*
        ///     <layer>             ::=  <name> "/" <version> [<space> <details>]
        ///     <name>              ::=  (<alnum>)+
        ///     <version>           ::=  <digit> (<alnum> | "." | "-" | "_")*
        ///     <details>           ::=  <parentherized>
        ///     <parentherized>     ::=  "(" (<nonpar> | <parentherized>)* ")"
        ///
        /// Where `<space>` is a single space character, `<digit>` is a decimal
        /// digit, `<alnum>` is any alphanumeric character, and `<nonpar>` is
        /// any character other than `(` and `)`.
        ///
        /// When multiple levels are present, the innermost layer (the one that
        /// is closest to this API) should appear first.
        ///
        /// Example:
        ///
        ///     RealmJS/2.13.0 RealmStudio/2.9.0
        ///
        /// Note: The user agent description is not intended for machine
        /// interpretation, but should still follow the specified syntax such
        /// that it remains easily interpretable by human beings.
        std::string user_agent_application_info;

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
        ///
        /// Many operations, such as serialized transactions, are not suppored
        /// in this mode.
        bool dry_run = false;

        /// The default changeset cooker to be used by new sessions. Can be
        /// overridden by Session::Config::changeset_cooker.
        ///
        /// \sa make_client_replication(), TrivialChangesetCooker.
        std::shared_ptr<ClientReplication::ChangesetCooker> changeset_cooker;

        /// The maximum number of milliseconds to allow for a connection to
        /// become fully established. This includes the time to resolve the
        /// network address, the TCP connect operation, the SSL handshake, and
        /// the WebSocket handshake.
        milliseconds_type connect_timeout = default_connect_timeout;

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
        milliseconds_type connection_linger_time = default_connection_linger_time;

        /// The client will send PING messages periodically to allow the server
        /// to detect dead connections (heartbeat). This parameter specifies the
        /// time, in milliseconds, between these PING messages. When scheduling
        /// the next PING message, the client will deduct a small random amount
        /// from the specified value to help spread the load on the server from
        /// many clients.
        milliseconds_type ping_keepalive_period = default_ping_keepalive_period;

        /// Whenever the server receives a PING message, it is supposed to
        /// respond with a PONG messsage to allow the client to detect dead
        /// connections (heartbeat). This parameter specifies the time, in
        /// milliseconds, that the client will wait for the PONG response
        /// message before it assumes that the connection is dead, and
        /// terminates it.
        milliseconds_type pong_keepalive_timeout = default_pong_keepalive_timeout;

        /// The maximum amount of time, in milliseconds, since the loss of a
        /// prior connection, for a new connection to be considered a *fast
        /// reconnect*.
        ///
        /// In general, when a client establishes a connection to the server,
        /// the uploading process remains suspended until the initial
        /// downloading process completes (as if by invocation of
        /// Session::async_wait_for_download_completion()). However, to avoid
        /// unnecessary latency in change propagation during ongoing
        /// application-level activity, if the new connection is established
        /// less than a certain amount of time (`fast_reconnect_limit`) since
        /// the client was previously connected to the server, then the
        /// uploading process will be activated immediately.
        ///
        /// For now, the purpose of the general delaying of the activation of
        /// the uploading process, is to increase the chance of multiple initial
        /// transactions on the client-side, to be uploaded to, and processed by
        /// the server as a single unit. In the longer run, the intention is
        /// that the client should upload transformed (from reciprocal history),
        /// rather than original changesets when applicable to reduce the need
        /// for changeset to be transformed on both sides. The delaying of the
        /// upload process will increase the number of cases where this is
        /// possible.
        ///
        /// FIXME: Currently, the time between connections is not tracked across
        /// sessions, so if the application closes its session, and opens a new
        /// one immediately afterwards, the activation of the upload process
        /// will be delayed unconditionally.
        milliseconds_type fast_reconnect_limit = default_fast_reconnect_limit;

        /// Set to true to completely disable delaying of the upload process. In
        /// this mode, the upload process will be activated immediately, and the
        /// value of `fast_reconnect_limit` is ignored.
        ///
        /// For testing purposes only.
        bool disable_upload_activation_delay = false;

        /// If `disable_upload_compaction` is true, every changeset will be
        /// compacted before it is uploaded to the server. Compaction will
        /// reduce the size of a changeset if the same field is set multiple
        /// times or if newly created objects are deleted within the same
        /// transaction. Log compaction increeses CPU usage and memory
        /// consumption.
        bool disable_upload_compaction = false;

        /// Set the `TCP_NODELAY` option on all TCP/IP sockets. This disables
        /// the Nagle algorithm. Disabling it, can in some cases be used to
        /// decrease latencies, but possibly at the expense of scalability. Be
        /// sure to research the subject before you enable this option.
        bool tcp_no_delay = false;

        /// The specified function will be called whenever a PONG message is
        /// received on any connection. The round-trip time in milliseconds will
        /// be pased to the function. The specified function will always be
        /// called by the client's event loop thread, i.e., the thread that
        /// calls `Client::run()`. This feature is mainly for testing purposes.
        std::function<RoundtripTimeHandler> roundtrip_time_handler;

        /// Disable sync to disk (fsync(), msync()) for all realm files managed
        /// by this client.
        ///
        /// Testing/debugging feature. Should never be enabled in production.
        bool disable_sync_to_disk = false;
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

    /// Returns false if the specified URL is invalid.
    bool decompose_server_url(const std::string& url, ProtocolEnvelope& protocol,
                              std::string& address, port_type& port, std::string& path) const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    friend class Session;
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
    using SerialTransactChangeset = util::Buffer<char>;
    using SerialTransactInitiationHandler = std::function<void(std::error_code)>;
    using SerialTransactCompletionHandler = std::function<void(std::error_code, bool accepted)>;
    using SSLVerifyCallback = bool(const std::string& server_address,
                                   port_type server_port,
                                   const char* pem_data,
                                   size_t pem_size,
                                   int preverify_ok,
                                   int depth);

    struct Config {
        Config() {}

        /// server_address is the fully qualified host name, or IP address of
        /// the server.
        std::string server_address = "localhost";

        /// server_port is the port at which the server listens. If server_port
        /// is zero, the default port for the specified protocol is used. See
        /// ProtocolEnvelope for information on default ports.
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

        /// The protocol used for communicating with the server. See
        /// ProtocolEnvelope.
        ProtocolEnvelope protocol_envelope = ProtocolEnvelope::realm;

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

        /// Controls whether the server certificate is verified for SSL
        /// connections. It should generally be true in production.
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
        util::Optional<std::string> ssl_trust_certificate_path;

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
        /// \sa make_client_replication(), TrivialChangesetCooker.
        std::shared_ptr<ClientReplication::ChangesetCooker> changeset_cooker;

        /// The encryption key the DB will be opened with.
        util::Optional<std::array<char, 64>> encryption_key;

        /// ClientReset is used for both async open and client reset. If
        /// client_reset is not util::none, the sync client will perform
        /// async open for this session if the local Realm does not exist, and
        /// client reset if the local Realm exists. If client_reset is
        /// util::none, an ordinary sync session will take place.
        ///
        /// A session will perform async open by downloading a state Realm, and
        /// some metadata, from the server, patching up the metadata part of
        /// the Realm and finally move the downloaded Realm into the path of
        /// the local Realm. After completion of async open, the application
        /// can open and use the Realm.
        ///
        /// A session will perform client reset by downloading a state Realm, and
        /// some metadata, from the server. After download, the state Realm will
        /// be integrated into the local Realm in a write transaction. The
        /// application is free to use the local realm during the entire client
        /// reset. Like a DOWNLOAD message, the application will not be able
        /// to perform a write transaction at the same time as the sync client
        /// performs its own write transaction. Client reset is not more
        /// disturbing for the application than any DOWNLOAD message. The
        /// application can listen to change notifications from the client
        /// reset exactly as in a DOWNLOAD message.
        ///
        /// The client reset will recover non-uploaded changes in the local
        /// Realm if and only if 'recover_local_changes' is true. In case,
        /// 'recover_local_changes' is false, the local Realm state will hence
        /// be set to the server's state (server wins).
        ///
        /// Async open and client reset require a private directory for
        /// metadata. This directory must be specified in the option
        /// 'metadata_dir'. The metadata_dir must not be touched during async
        /// open or client reset. The metadata_dir can safely be removed at
        /// times where async open or client reset do not take place. The sync
        /// client attempts to clean up metadata_dir. The metadata_dir can be
        /// reused across app restarts to resume an interrupted download. It is
        /// recommended to leave the metadata_dir unchanged except when it is
        /// known that async open or client reset is done.
        ///
        /// The recommended usage of async open is to use it for the initial
        /// bootstrap if Realm usage is not needed until after the server state
        /// has been downloaded.
        ///
        /// The recommended usage of client reset is after a previous session
        /// encountered an error that implies the need for a client reset. It
        /// is not recommended to persist the need for a client reset. The
        /// application should just attempt to synchronize in the usual fashion
        /// and only after hitting an error, start a new session with a client
        /// reset. In other words, if the application crashes during a client reset,
        /// the application should attempt to perform ordinary synchronization
        /// after restart and switch to client reset if needed.
        ///
        /// Error codes that imply the need for a client reset are the session
        /// level error codes:
        ///
        /// bad_client_file_ident        = 208, // Bad client file identifier (IDENT)
        /// bad_server_version           = 209, // Bad server version (IDENT, UPLOAD)
        /// bad_client_version           = 210, // Bad client version (IDENT, UPLOAD)
        /// diverging_histories          = 211, // Diverging histories (IDENT)
        ///
        /// However, other errors such as bad changeset (UPLOAD) could also be resolved
        /// with a client reset. Client reset can even be used without any prior error
        /// if so desired.
        ///
        /// After completion of async open and client reset, the sync client
        /// will continue synchronizing with the server in the usual fashion.
        ///
        /// The progress of async open and client reset can be tracked with the
        /// standard progress handler.
        ///
        /// Async open and client reset are done when the progress handler
        /// arguments satisfy "progress_version > 0". However, if the
        /// application wants to ensure that it has all data present on the
        /// server, it should wait for download completion using either
        /// void async_wait_for_download_completion(WaitOperCompletionHandler)
        /// or
        /// bool wait_for_download_complete_or_client_stopped().
        ///
        /// The option 'require_recent_state_realm' is used for async open to
        /// request a recent state Realm. A recent state Realm is never empty
        /// (unless there is no data), and is recent in the sense that it was
        /// produced by the current incarnation of the server. Recent does not
        /// mean the absolutely newest possible state Realm, since that might
        /// lead to too excessive work on the server. Setting
        /// 'require_recent_state_realm' to true might lead to more work
        /// performed by the server but it ensures that more data is downloaded
        /// using async open instead of ordinary synchronization. It is
        /// recommended to set 'require_recent_state_realm' to true. Client
        /// reset always downloads a recent state Realm.
        struct ClientReset {
            std::string metadata_dir;
            bool recover_local_changes = true;
            bool require_recent_state_realm = true;
        };
        util::Optional<ClientReset> client_reset_config;

        struct ProxyConfig {
            enum class Type { HTTP, HTTPS } type;
            std::string address;
            port_type port;
        };
        util::Optional<ProxyConfig> proxy_config;

        /// Set to true to disable the upload process for this session. This
        /// includes the sending of empty UPLOAD messages.
        ///
        /// This feature exists exclusively for testing purposes at this time.
        bool disable_upload = false;

        /// Set to true to disable sending of empty UPLOAD messages for this
        /// session.
        ///
        /// This feature exists exclusively for testing purposes at this time.
        bool disable_empty_upload = false;

        /// Set to true to cause the integration of the first received changeset
        /// (in a DOWNLOAD message) to fail.
        ///
        /// This feature exists exclusively for testing purposes at this time.
        bool simulate_integration_error = false;
    };

    /// \brief Start a new session for the specified client-side Realm.
    ///
    /// Note that the session is not fully activated until you call bind().
    /// Also note that if you call set_sync_transact_callback(), it must be
    /// done before calling bind().
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
    /// downloadable_bytes is equal to downloaded_bytes plus an estimate of
    /// the size of the remaining server history.
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
    /// can receive new changesets from other clients. downloadable_bytes can
    /// decrease for two reasons: server side compaction and changesets of
    /// local origin. Code using downloadable_bytes must not assume that it
    /// is increasing.
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
    ///           ProtocolEnvelope protocol = ProtocolEnvelope::realm);
    /// replaces the corresponding parameters from the Session::Config object
    /// before the session is bound.
    /// void bind(std::string server_url, std::string signed_user_token) parses
    /// the \p server_url and replaces the parameters in the Session::Config object
    /// before the session is bound.
    ///
    /// \throw BadServerUrl if the specified server URL is malformed.
    void bind();
    /// \param server_url For example "realm://sync.realm.io/test". See
    /// server_address, server_path, and server_port in Session::Config for
    /// information about the individual components of the URL. See
    /// ProtocolEnvelope for the list of available URL schemes and the
    /// associated default ports.
    void bind(std::string server_url, std::string signed_user_token);
    void bind(std::string server_address, std::string server_path,
              std::string signed_user_token, port_type server_port = 0,
              ProtocolEnvelope protocol = ProtocolEnvelope::realm);
    /// @}

    /// \brief Refresh the access token associated with this session.
    ///
    /// This causes the REFRESH protocol message to be sent to the server. See
    /// ProtocolEnvelope. It is an error to pass a token with a different user
    /// identity than the token used to initiate the session.
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
    /// identity and access rights of the current user. See ProtocolEnvelope.
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
    /// These functions are synchronous equivalents of
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

    /// \brief Initiate a serialized transaction.
    ///
    /// Asynchronously waits for completion of any serialized transactions, that
    /// are already in progress via the same session object, then waits for
    /// the download process to complete (async_wait_for_download_completion()),
    /// then pauses the upload process. The upload process will be resumed when
    /// async_try_complete_serial_transact() or abort_serial_transact() is
    /// called.
    ///
    /// Changesets produced by local transactions, that are committed after the
    /// completion of the initiation of a serialized transaction, are guaranteed
    /// to not be uploaded until after (or during) the completion of that
    /// serialized transaction (async_try_complete_serial_transact()).
    ///
    /// If the initiation of a serialized transaction is successfully completed,
    /// that is, if the specified handler gets called with an std::error_code
    /// argument that evaluates to false in a boolean context, then the
    /// application is required to eventually call
    /// async_try_complete_serial_transact() to complete the transaction, or
    /// abort_serial_transact() to abort it. If
    /// async_try_complete_serial_transact() fails (throws), the application is
    /// required to follow up with a call to abort_serial_transact().
    ///
    /// If the session object is destroyed before initiation process completes,
    /// the specified handler will be called with error
    /// `util::error::operation_aborted`. Currently, this is the only possible
    /// error that can be reported through this handler.
    ///
    /// This feature is only available when the server supports version 28, or
    /// later, of the synchronization protocol. See
    /// get_current_protocol_version().
    ///
    /// This feature is not currently supported with Partial Synchronization,
    /// and in a server cluster, it is currently only supported on the root
    /// node.
    void async_initiate_serial_transact(SerialTransactInitiationHandler);

    /// \brief Complete a serialized transaction.
    ///
    /// Initiate the completion of the serialized transaction. This involves
    /// sending the specified changeset to the server, and waiting for the
    /// servers response.
    ///
    /// If the session object is destroyed before completion process completes,
    /// the specified handler will be called with error
    /// `util::error::operation_aborted`.
    ///
    /// Otherwise, if the server does not support serialized transactions, the
    /// specified handler will be called with error
    /// `util::MiscExtErrors::operation_not_supported`. This happens if the
    /// negotiated protocol version is too old, if serialized transactions are
    /// disallowed by the server, or if it is not allowed for the Realm file in
    /// question (partial synchronization).
    ///
    /// Otherwise, the specified handler will be called with an error code
    /// argument that evaluates to false in a boolean context, and the
    /// `accepted` argument will be true if, and only if the transaction was
    /// accepted by the server.
    ///
    /// \param upload_anchor The upload cursor associated with the snapshot on
    /// which the specified changeset is based. Use
    /// sync::ClientHistory::get_upload_anchor_of_current_transact() to obtain
    /// it. Note that
    /// sync::ClientHistory::get_upload_anchor_of_current_transact() needs to be
    /// called during the transaction that is used to produce the changeset of
    /// the serialized transaction.
    ///
    /// \param changeset A changeset obtained from an aborted transaction on the
    /// Realm file associated with this session. Use
    /// sync::ClientHistory::get_sync_changeset() to obtain it. The transaction,
    /// which is used to produce teh changeset, needs to be rolled back rather
    /// than committed, because the decision of whether to accept the changes
    /// need to be delegated to the server. Note that
    /// sync::ClientHistory::get_sync_Changeset_of_current_transact() needs to
    /// be called at the end of the transaction, that is used to produce the
    /// changeset, but before the rollback operation.
    void async_try_complete_serial_transact(UploadCursor upload_anchor,
                                            SerialTransactChangeset changeset,
                                            SerialTransactCompletionHandler);

    /// \brief Abort a serialized transaction.
    ///
    /// Must be called if async_try_complete_serial_transact() fails, i.e., if
    /// it throws, or if async_try_complete_serial_transact() is not called at
    /// all. Must not be called if async_try_complete_serial_transact()
    /// succeeds, i.e., if it does not throw.
    ///
    /// Will resume upload process.
    void abort_serial_transact() noexcept;

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
    connect_timeout             = 121, ///< Sync connection was not fully established in time
    bad_timestamp               = 122, ///< Bad timestamp (PONG)
    bad_protocol_from_server    = 123, ///< Bad or missing protocol version information from server
    client_too_old_for_server   = 124, ///< Protocol version negotiation failed: Client is too old for server
    client_too_new_for_server   = 125, ///< Protocol version negotiation failed: Client is too new for server
    protocol_mismatch           = 126, ///< Protocol version negotiation failed: No version supported by both client and server
    bad_state_message           = 127, ///< Bad values in state message (STATE)
    missing_protocol_feature    = 128, ///< Requested feature missing in negotiated protocol version
    bad_serial_transact_status  = 129, ///< Bad status of serialized transaction (TRANSACT)
    bad_object_id_substitutions = 130, ///< Bad encoded object identifier substitutions (TRANSACT)
    http_tunnel_failed          = 131, ///< Failed to establish HTTP tunnel with configured proxy
};

const std::error_category& client_error_category() noexcept;

std::error_code make_error_code(Client::Error) noexcept;

std::ostream& operator<<(std::ostream& os, Session::Config::ProxyConfig::Type);

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
