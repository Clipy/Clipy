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

namespace realm {
namespace sync {

class Client {
public:
    enum class Reconnect {
        /// This is the mode that should always be used in production. In this
        /// mode the client uses a scheme for determining a reconnect delay that
        /// prevents it from creating too many connection requests in a short
        /// amount of time.
        normal,

        /// Never delay reconnect attempts. For testing purposes only.
        immediately
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

        /// verify_servers_ssl_certificate controls whether the server certificate
        /// is verified for SSL connections. It should always be true in production.
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
        bool verify_servers_ssl_certificate = true;

        /// ssl_trust_certificate_path is the path of a trust/anchor certificate
        /// used by the client to verify the server certificate.
        /// If ssl_trust_certificate_path is None (default), the default device
        /// trust/anchor store is used.
        util::Optional<std::string> ssl_trust_certificate_path; // default None

        /// Use ports 80 and 443 by default instead of 7800 and 7801
        /// respectively. Ideally, these default ports should have been made
        /// available via a different URI scheme instead (http/https or ws/wss).
        bool enable_default_port_hack = true;

        /// For testing purposes only.
        Reconnect reconnect = Reconnect::normal;

        /// Create a separate connection for each session. For testing purposes
        /// only.
        bool one_connection_per_session = false;

        /// Do not access the local file system. Sessions will act as if
        /// initiated on behalf of an empty (or nonexisting) local Realm
        /// file. Received DOWNLOAD messages will be accepted, but otherwise
        /// ignored. No UPLOAD messages will be generated. For testing purposes
        /// only.
        bool dry_run = false;
    };

    /// \throw util::EventLoop::Implementation::NotAvailable if no event loop
    /// implementation was specified, and
    /// util::EventLoop::Implementation::get_default() throws it.
    Client(Config = Config());
    Client(Client&&) noexcept;
    ~Client() noexcept;

    using ErrorHandler = void(int error_code, std::string message);

    /// \brief Set a function to be called when the server reports a
    /// connection-level error.
    ///
    /// Only connection-level errors are reported through this callback. See
    /// also Session::set_error_handler(). See \ref Error (or `protocol.md`) for
    /// a list of errors and their categorization.
    ///
    /// The callback function will always be called by the thread that executes
    /// run(). If the callback function throws an exception, that exception will
    /// "travel" out through run().
    ///
    /// Note: Any call to this function must have returned before run() is
    /// called. If this function is called multiple times, each call overrides
    /// the previous setting.
    ///
    /// Note: This function is **not thread-safe**.
    void set_error_handler(std::function<ErrorHandler>);

    /// Run the internal event-loop of the client. At most one thread may
    /// execute run() at any given time. The call will not return until somebody
    /// calls stop().
    void run();

    // Thread-safe
    void stop() noexcept;

    /// Technically thread-safe, but the returned value is not accurate until
    /// after run() has returned.
    ///
    /// For testing purposes.
    uint_fast64_t errors_seen() const noexcept;

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


/// Session objects must be destroyed before the Client object with which they
/// are assocoated is destroyed.
///
/// It is an error to create two Session objects for a particular Realm file if
/// those Session objects overlap in time, or if they are associated with two
/// different Client objects that overlap in time.
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
    using ErrorHandler = Client::ErrorHandler;

    /// \brief Start a new session for the specified client-side Realm.
    ///
    /// Note that the session is not fully activated until you call bind(). Also
    /// note that if you call set_sync_transact_callback(), it must be done
    /// before calling bind().
    ///
    /// \param realm_path The file-system path of a local client-side Realm
    /// file.
    Session(Client&, std::string realm_path);

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
    /// time where the last invocation of `clint.run()` returns. Here, `client`
    /// refers to the associated Client object.
    void set_sync_transact_callback(std::function<SyncTransactCallback>);

    /// \brief Set a function to be called when an error is reported by the
    /// server to have occurred in this session.
    ///
    /// Only session-level errors are reported through the callback. See also
    /// Client::set_error_handler(). See \ref Error (or `protocol.md`) for a
    /// list of errors and their categorization.
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
    /// time where the last invocation of `clint.run()` returns. Here, `client`
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
    /// and the callback function may therfore be called before bind() returns.
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
    /// \param protocol See \ref Protocol.
    ///
    /// \throw BadServerUrl if the specified server URL is malformed.
    void bind(std::string server_url, std::string signed_user_token);
    void bind(std::string server_address, std::string server_path,
              std::string signed_user_token, port_type server_port = 0,
              Protocol protocol = Protocol::realm);
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
    /// It is an error if the user token used with this message represents a
    /// different user identity than a previously used user token. The server
    /// will detect this scenario and report an error.
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

    /// \brief Wait for upload to complete
    ///
    /// This function waits for all currently outstanding client-side changesets
    /// to be uploaded to, and acknowledged by the server. More specifically, it
    /// waits until all changesets that must be uploaded, and that precede a
    /// certain client version threshold have been uploaded. The threshold is
    /// the highest client version passed to nonsync_transact_notify() prior to
    /// the invocation of wait_for_upload_complete_or_client_stopped().
    ///
    /// If nonsync_transact_notify() is called while
    /// wait_for_upload_complete_or_client_stopped() executes, and with a
    /// version that greater than the currently applying threashold, then that
    /// may, or may not cause the threshold to be pushed forward and the wait to
    /// be extended.
    ///
    /// It is an error to call this function before bind() has been called, and
    /// has returned.
    ///
    /// Note: This function is fully thread-safe. That is, it may be called by
    /// any thread, and by multiple threads concurrently.
    void wait_for_upload_complete_or_client_stopped();

    /// \brief Wait for download to complete
    ///
    /// This function waits for all currently outstanding server-side changesets
    /// to be downloaded. More specifically, it waits until all changesets that
    /// must be downloaded, and that precede a certain server version threshold
    /// have been downloaded (FIXME: Shouldn't it have been downloaded and
    /// integrated?). The threshold is the currently latest server version at
    /// the time where wait_for_download_complete_or_client_stopped() is called,
    /// or shortly thereafter.
    ///
    /// If wait_for_download_complete_or_client_stopped() is called by one
    /// thread while it is being executed by another thread, then the later call
    /// may, or may not push forward the threshold that applies the the earlier
    /// call, and cause its wait to be correspondingly extended.
    ///
    /// It is an error to call this function before bind() has been called, and
    /// has returned.
    ///
    /// Note: This function is fully thread-safe. That is, it may be called by
    /// any thread, and by multiple threads concurrently.
    void wait_for_download_complete_or_client_stopped();

private:
    class Impl;
    Impl* m_impl;
};



// Implementation

class BadServerUrl: public std::exception {
public:
    const char* what() const noexcept override
    {
        return "Bad server URL";
    }
};

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_CLIENT_HPP
