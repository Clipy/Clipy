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
    class TokenExpirationClock;

    struct Config {
        Config() {}

        /// The maximum number of Realm files that will be kept open
        /// concurrently by this server. The server keeps a cache of open Realm
        /// files for efficiency reasons.
        long max_open_files = 256;

        /// An optional custom clock to be used for token expiration checks. If
        /// no clock is specified, the server will use the system clock.
        TokenExpirationClock* token_expiration_clock = nullptr;

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

        /// A unique id of this server. Used in the backup protocol to tell
        /// slaves apart.
        std::string id = "unknown";

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

        /// @{ \brief The operating mode of the Sync worker.
        ///
        /// MasterWithNoSlave is a standard Sync worker without backup.
        /// If a backup slave attempts to contact a MasterNoBackup server,
        /// the slave will be rejected.
        ///
        /// MasterWithAsynchronousSlave represents a Sync worker that operates
        /// independently of a backup slave. If a slave connects to the
        /// MasterAsynchronousSlave server, the server will accept the connection
        /// and send backup information to the slave. This type of master server
        /// will never wait for the slave, however.
        ///
        /// MasterWithSynchronousSlave represents a Sync worker that works in
        /// coordination with a slave. The master will send all updates to the
        /// slave and wait for acknowledgment before the master sends its own
        /// acknowledgment to the clients. This mode of operation is the safest
        /// type of backup, but it generally will have higher latency than the previous
        /// two types of server.
        ///
        /// Slave represents a backup server. A slave is used to backup a master.
        /// The slave connects to the master and reconnects in case a network fallout.
        /// The slave receives updates from the master and acknowledges them.
        /// A slave rejects all connections from Sync clients.
        enum class OperatingMode {
            MasterWithNoSlave,
            MasterWithAsynchronousSlave,
            MasterWithSynchronousSlave,
            Slave
        };
        OperatingMode operating_mode = OperatingMode::MasterWithNoSlave;
        /// @}

        /// @{ \brief Adress of master sync work.
        ///
        /// master_address and master_port are only meaningful in Slave mode.
        /// The parameters represent the address of the master from which this
        /// slave obtains Realm updates.
        std::string master_address;
        std::string master_port;
        /// @}

        /// @{ \brief SSL for master slave communication.
        ///
        /// The master and slave communicate over a SSL connection if
        /// master_slave_ssl is set to true(default = false). The certificate of the
        /// master is verified if master_verify_ssl_certificate is set to true.
        /// The certificate verification attempts to use the default trust store of the
        /// instance if master_ssl_trust_certificate_path is none(default), otherwise
        /// the certificate at the master_ssl_trust_certificate_path is used for
        /// verification.
        bool master_slave_ssl = false;
        bool master_verify_ssl_certificate = true;
        util::Optional<std::string> master_ssl_trust_certificate_path = util::none;
        /// @}

        /// A master Sync server will only accept a backup connection from a slave
        /// that can present the correct master_slave_shared_secret.
        /// The configuration of the master and the slave must contain the same
        /// secret string.
        /// The secret is sent in a HTTP header and must be a valid HTTP header value.
        std::string master_slave_shared_secret = "replace-this-string-with-a-secret";

        /// A callback which gets called by the backup master every time the slave
        /// changes its status to up-to-date or back. The arguments carry the
        /// slave's id (string) and its up-to-dateness state (bool).
        std::function<void(std::string, bool)> slave_status_callback;

        /// The feature token is used by the server to gate access to various
        /// features.
        util::Optional<std::string> feature_token;

        /// The server can try to eliminate redundant instructions from
        /// changesets before sending them to clients, minimizing download sizes
        /// at the expense of server CPU usage.
        bool enable_download_log_compaction = true;

        /// The accumulated size of changesets that are included in download
        /// messages. The size of the changesets is calculated before log
        /// compaction (if enabled). A larger value leads to more efficient
        /// log compaction and download, at the expense of higher memory pressure,
        /// higher latency for sending the first changeset, and a higher probability
        /// for the need to resend the same changes after network disconnects.
        size_t max_download_size = 0x20000; // 128 KB

        /// Set the `TCP_NODELAY` option on all TCP/IP sockets. This disables
        /// the Nagle algorithm. Disabling it, can in some cases be used to
        /// decrease latencies, but possibly at the expense of scalability. Be
        /// sure to research the subject before you enable this option.
        bool tcp_no_delay = false;
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


class Server::TokenExpirationClock {
public:
    /// Number of seconds since the Epoch. The Epoch is the epoch of
    /// std::chrono::system_clock.
    virtual std::int_fast64_t now() noexcept = 0;

    virtual ~TokenExpirationClock() {}
};

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_SERVER_HPP
