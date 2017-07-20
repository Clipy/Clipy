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
#include <realm/impl/continuous_transactions_history.hpp>
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

        /// Delay reconnect attempts indefinitely. For testing purposes only.
        ///
        /// A reconnect attempt can be manually scheduled by calling
        /// cancel_reconnect_delay(). In particular, when a connection breaks,
        /// or when an attempt at establishing the connection fails, the error
        /// handler is called. If one calls cancel_reconnect_delay() from that
        /// invocation of the error handler, the effect is to allow another
        /// reconnect attempt to occur.
        never,

        /// Never delay reconnect attempts. Perform them immediately. For
        /// testing purposes only.
        immediate
    };

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
        /// all logging happens on behalf of the thread that executes run().
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
        // FIXME: This setting is ignored for now, due to limitations in the
        // load balancer.
        bool one_connection_per_session = false;

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

        /// The number of ms between periodic keep-alive pings.
        uint_fast64_t ping_keepalive_period_ms = 600000;

        /// The number of ms to wait for keep-alive pongs.
        uint_fast64_t pong_keepalive_timeout_ms = 300000;

        /// The number of ms to wait for urgent pongs.
        uint_fast64_t pong_urgent_timeout_ms = 5000;
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
     // servers for which there are currently no bound sessions.
    void cancel_reconnect_delay();

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
/// (file system inode) at any point in time. Multiple objects may coexists, as
/// long as bind() has been called on at most one of them. Additionally, two
/// sessions are allowed to exist at different times, and with no overlap in
/// time, as long as they are associated with the same client object, or with
/// two different client objects that do not overlap in time. This means, in
/// particular, that it is an error to create two nonoverlapping sessions for
/// the same local Realm file, it they are associated with two different client
/// objects that overlap in time. It is the responsibility of the application to
/// ensure that these rules are adhered to. The consequences of a violation are
/// unspecified.
///
/// Thread-safety: It is safe for multiple threads to construct, use (with some
/// exceptions), and destroy session objects concurrently, regardless of whether
/// those session objects are associated with the same, or with different Client
/// objects. Please note that some of the public member functions are fully
/// thread-safe, while others are not.
class Session {
public:
    using port_type = util::network::Endpoint::port_type;
    using version_type = _impl::History::version_type;
    using SyncTransactCallback = void(VersionID old_version, VersionID new_version);
    using ProgressHandler = void(std::uint_fast64_t downloaded_bytes,
                                 std::uint_fast64_t downloadable_bytes,
                                 std::uint_fast64_t uploaded_bytes,
                                 std::uint_fast64_t uploadable_bytes,
                                 std::uint_fast64_t progress_version,
                                 std::uint_fast64_t snapshot_version);
    using WaitOperCompletionHandler = std::function<void(std::error_code)>;

    class Config {
    public:
        Config() {}

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
        /// destroyed. The application must specify an object for which that
        /// function can safely be called, and continue to execute from the
        /// point in time where bind() starts executing, and up until the point
        /// in time where the last invocation of `client.run()` returns. Here,
        /// `client` refers to the associated Client object.
        ///
        /// \sa make_client_history(), TrivialChangesetCooker.
        std::shared_ptr<ClientHistory::ChangesetCooker> changeset_cooker;

        /// The encryption key the SharedGroup will be opened with.
        Optional<std::array<char, 64>> encryption_key;
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

    Session(Session&&) noexcept;

    ~Session() noexcept;

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
    /// CAUTION: The specified callback function may be called before the call
    /// to bind() returns, and it may be called (or continue to execute) after
    /// the session object is destroyed. The application must pass a handler
    /// that can be safely called, and can safely continue to execute from the
    /// point in time where bind() starts executing, and up until the point in
    /// time where the last invocation of `client.run()` returns. Here, `client`
    /// refers to the associated Client object.
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
    /// The handler is called on the event loop thread.
    /// The handler is called after or during set_progress_handler(),
    /// after bind(), after each DOWNLOAD message, and after each local
    /// transaction (nonsync_transact_notify).
    ///
    /// set_progress_handler() is not thread safe and it must be called before
    /// bind() is called. Subsequent calls to set_progress_handler() overwrite
    /// the previous calls. Typically, this function is called once per session.
    ///
    /// The progress handler is also posted to the event loop during the
    /// execution of set_progress_handler().
    ///
    /// CAUTION: The specified callback function may be called before the call
    /// to set_progress_handler() returns, and it may be called
    /// (or continue to execute) after the session object is destroyed.
    /// The application must pass a handler that can be safely called, and can
    /// execute from the point in time where set_progress_handler() is called,
    /// and up until the point in time where the last invocation of
    /// `client.run()` returns. Here, `client` refers to the associated
    /// Client object.
    void set_progress_handler(std::function<ProgressHandler>);

    /// \brief Signature of an error handler.
    ///
    /// \param ec The error code. For the list of errors reported by the server,
    /// see \ref ProtocolError (or `protocol.md`). For the list of errors
    /// corresponding with protocol violation that are detected by the client,
    /// see Client::Error.
    ///
    /// \param is_fatal The error is of a kind that is likely to persist, and
    /// cause all future reconnect attempts to fail. The client may choose to
    /// try to reconnect again later, but if so, the waiting period will be
    /// substantial.
    ///
    /// \param detailed_message The message associated with the error. It is
    /// usually equal to `ec.message()`, but may also be a more specific message
    /// (one that provides extra context). The purpose of this message is mostly
    /// to aid debugging. For non-debugging purposes, `ec.message()` should
    /// generally be considered sufficient.
    ///
    /// \sa set_error_handler().
    using ErrorHandler = void(std::error_code ec, bool is_fatal,
                              const std::string& detailed_message);

    /// \brief Set the error handler for this session.
    ///
    /// Sets a function to be called when an error causes a connection
    /// initiation attempt to fail, or an established connection to be broken.
    ///
    /// When a connection is established on behalf of multiple sessions, a
    /// connection-level error will be reported to all those sessions. A
    /// session-level error, on the other hand, will only be reported to the
    /// affected session.
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
    /// CAUTION: The specified callback function may be called before the call
    /// to bind() returns, and it may be called (or continue to execute) after
    /// the session object is destroyed. The application must pass a handler
    /// that can be safely called, and can safely continue to execute from the
    /// point in time where bind() starts executing, and up until the point in
    /// time where the last invocation of `client.run()` returns. Here, `client`
    /// refers to the associated Client object.
    void set_error_handler(std::function<ErrorHandler>);

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
    /// \param server_url For example "realm://sync.realm.io/test". See \a
    /// server_address, \a server_path, and \a server_port for information about
    /// the individual components of the URL. See \ref Protocol for the list of
    /// available URL schemes and the associated default ports. The 2-argument
    /// version has exactly the same affect as the 5-argument version.
    ///
    /// \param server_address The fully qualified host name, or IP address of
    /// the server.
    ///
    /// \param server_path The virtual path by which the server identifies the
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
    ///
    /// \param signed_user_token A cryptographically signed token describing the
    /// identity and access rights of the current user. See \ref Protocol.
    ///
    /// \param server_port If zero, use the default port for the specified
    /// protocol. See \ref Protocol for information on default ports.
    ///
    /// \param verify_servers_ssl_certificate controls whether the server
    /// certificate is verified for SSL connections. It should generally be true
    /// in production. The default value of verify_servers_ssl_certificate is
    /// true.
    ///
    /// A server certificate is verified by first checking that the
    /// certificate has a valid signature chain back to a trust/anchor certificate,
    /// and secondly checking that the host name of the Realm URL matches
    /// a host name contained in the certificate.
    /// The host name of the certificate is stored in either Common Name or
    /// the Alternative Subject Name (DNS section).
    ///
    /// From the point of view of OpenSSL, setting verify_servers_ssl_certificate
    /// to false means calling `SSL_set_verify()` with `SSL_VERIFY_NONE`.
    /// Setting verify_servers_ssl_certificate to true means calling `SSL_set_verify()`
    /// with `SSL_VERIFY_PEER`, and setting the host name using the function
    /// X509_VERIFY_PARAM_set1_host() (OpenSSL version 1.0.2 or newer).
    /// For other platforms, an equivalent procedure is followed.
    ///
    /// \param ssl_trust_certificate_path is the path of a trust/anchor
    /// certificate used by the client to verify the server certificate.
    /// If ssl_trust_certificate_path is None (default), the default device
    /// trust/anchor store is used.
    ///
    /// \param protocol See \ref Protocol.
    ///
    /// \throw BadServerUrl if the specified server URL is malformed.
    void bind(std::string server_url, std::string signed_user_token,
            bool verify_servers_ssl_certificate = true,
            util::Optional<std::string> ssl_trust_certificate_path = util::none);
    void bind(std::string server_address, std::string server_path,
            std::string signed_user_token, port_type server_port = 0,
            Protocol protocol = Protocol::realm,
            bool verify_servers_ssl_certificate = true,
            util::Optional<std::string> ssl_trust_certificate_path = util::none);
    /// @}

    /// \brief Refresh the user token associated with this session.
    ///
    /// This causes the REFRESH protocol message to be sent to the server. See
    /// \ref Protocol.
    ///
    /// In an on-going session a client may expect its access token to expire at
    /// a certain time and schedule acquisition of a fresh access token (using a
    /// refresh token or by other means) in due time to provide a better user
    /// experience. Without refreshing the token, the client will be notified
    /// that the session is terminated due to insufficient privileges and must
    /// reacquire a fresh token, which is a potentially disruptive process.
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
    /// performed on its behlaf, that is, after a transaction that is not
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
    /// changeset is announced, then all previous changesets are implicitely
    /// announced. Also all preexisting changesets are implicitely announced
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
    /// If a new wait operation is initiated while other wait operations are in
    /// progress, the waiting period of operations in progress may, or may not
    /// get extended. The client must not assume either. The client may assume,
    /// however, that async_wait_for_upload_completion() will not affect the
    /// waiting period of async_wait_for_download_completion(), and vice versa.
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
    /// CAUTION: The specified completion handlers may be called before the
    /// initiation function returns (e.g. before
    /// async_wait_for_upload_completion() returns), and it may be called (or
    /// continue to execute) after the session object is destroyed. The
    /// application must pass a handler that can be safely called, and can
    /// safely continue to execute from the point in time where the initiating
    /// function starts executing, and up until the point in time where the last
    /// invocation of `clint.run()` returns. Here, `client` refers to the
    /// associated Client object.
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
    /// client event loop is stopped (Client::stop()).
    ///
    /// It is an error to call these functions before bind() has been called,
    /// and has returned.
    ///
    /// Note: These functions are fully thread-safe. That is, they may be called
    /// by any thread, and by multiple threads concurrently.
    void wait_for_upload_complete_or_client_stopped();
    void wait_for_download_complete_or_client_stopped();
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

private:
    class Impl;
    Impl* m_impl;

    void async_wait_for(bool upload_completion, bool download_completion,
                        WaitOperCompletionHandler);
};


/// \brief Protocol errors discovered by the client.
///
/// These errors will terminate the network connection (disconnect all sessions
/// associated with the affected connection), and the error will be reported to
/// the application via the error handlers of the affected sessions.
enum class Client::Error {
    connection_closed           = 100, ///< Connection closed (no error)
    unknown_message             = 101, ///< Unknown type of input message
    bad_syntax                  = 102, ///< Bad syntax in input message head
    limits_exceeded             = 103, ///< Limits exceeded in input message
    bad_session_ident           = 104, ///< Bad session identifier in input message
    bad_message_order           = 105, ///< Bad input message order
    bad_file_ident_pair         = 106, ///< Bad file identifier pair (ALLOC)
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
