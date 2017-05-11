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
#ifndef REALM_SYNC_SERVER_HPP
#define REALM_SYNC_SERVER_HPP

#include <stdint.h>
#include <memory>
#include <string>

#include <realm/util/network.hpp>
#include <realm/util/logger.hpp>
#include <realm/util/optional.hpp>
#include <realm/sync/crypto_server.hpp>
#include <realm/sync/metrics.hpp>

namespace realm {
namespace sync {

class Server {
public:
    class Clock;

    struct Config {
        Config() {}

        /// The maximum number of Realm files that will be kept open
        /// concurrently by this server. The server keeps a cache of open Realm
        /// files for efficiency reasons.
        long max_open_files = 256;

        /// An optional time provider to be used by the server.
        /// If no time provider is specified, the server will use the
        /// system clock.
        Clock* clock = nullptr;

        /// An optional logger to be used by the server. If no logger is
        /// specified, the server will use an instance of util::StderrLogger
        /// with the log level threshold set to util::Logger::Level::info. The
        /// server does not require a thread-safe logger, and it guarantees that
        /// all logging happens on behalf of start() and run() (which are not
        /// allowed to execute concurrently).
        util::Logger* logger = nullptr;

        /// An optional sink for recording metrics about the internal operation
        /// of the server. For the list of counters and gauges see
        /// "doc/monitoring.md".
        Metrics* metrics = nullptr;

        /// The address at which the listening socket is bound.
        /// The address can be a name or on numerical form.
        /// Use "localhost" to listen on the loopback interface.
        std::string listen_address;

        /// The port at which the listening socket is bound.
        /// The port can be a name or on numerical form.
        /// Use the empty string to have the system assign a dynamic
        /// listening port.
        std::string listen_port;

        bool reuse_address = true;

        /// The listening socket accepts TLS/SSL connections if `ssl` is
        /// true, and non-secure tcp connections otherwise.
        bool ssl = false;

        /// The path of the certificate that will be sent to clients during
        /// the SSL/TLS handshake.
        ///
        /// From the point of view of OpenSSL, this file will be passed to
        /// `SSL_CTX_use_certificate_chain_file()`.
        ///
        /// This option is ignore if `ssl` is false.
        std::string ssl_certificate_path;

        /// The path of the private key corresponding to the certificate.
        ///
        /// From the point of view of OpenSSL, this file will be passed to
        /// `SSL_CTX_use_PrivateKey_file()`.
        ///
        /// This option is ignore if `ssl` is false.
        std::string ssl_certificate_key_path;

        // A connection which has not been sending any messages or pings for
        // `idle_timeout_ms` is considered idle and will be dropped by the server.
        uint_fast64_t idle_timeout_ms = 1800000;

        // How often the server scans through the connection list to drop idle ones.
        uint_fast64_t drop_period_ms = 60000;
    };

    Server(const std::string& root_dir, util::Optional<PKey> public_key, Config = {});
    Server(Server&&) noexcept;
    ~Server() noexcept;

    /// start() binds a listening socket to the address and port specified in
    /// Config and starts accepting connections.
    /// The resolved endpoint (including the dynamically assigned port, if requested)
    /// can be obtained by calling listen_endpoint().
    /// This can be done immediately after start() returns.
    void start();

    /// A helper function, for backwards compatibility, that starts a listening
    /// socket without SSL at the specified address and port.
    void start(const std::string& listen_address,
               const std::string& listen_port,
               bool reuse_address = true);

    /// Return the resolved and bound endpoint of the listening socket.
    util::network::Endpoint listen_endpoint() const;

    /// Run the internal event-loop of the server. At most one thread may
    /// execute run() at any given time. It is an error if run() is called
    /// before start() has been successfully executed. The call to run() will
    /// not return until somebody calls stop().
    void run();

    /// Stop any thread that is currently executing run(). This function may be
    /// called by any thread.
    void stop() noexcept;

    /// Must not be called while run() is executing.
    uint_fast64_t errors_seen() const noexcept;

    /// A connection which has not been sending any messages or pings for
    /// `idle_timeout_ms` is considered idle and will be dropped by the server.
    void set_idle_timeout_ms(uint_fast64_t idle_timeout_ms);

    /// Close all connections with error code ProtocolError::connection_closed.
    ///
    /// This function exists mainly for debugging purposes.
    void close_connections();

private:
    class Implementation;
    std::unique_ptr<Implementation> m_impl;
};


class Server::Clock {
public:
    virtual int_fast64_t now() = 0;

    virtual ~Clock() {}
};

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_SERVER_HPP
