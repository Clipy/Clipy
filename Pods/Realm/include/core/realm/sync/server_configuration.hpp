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
#ifndef REALM_SYNC_SERVER_CONFIGURATION_HPP
#define REALM_SYNC_SERVER_CONFIGURATION_HPP

#include <vector>

// Realm headers
#include <realm/util/logger.hpp>
#include <realm/util/optional.hpp>
#include <realm/sync/server.hpp>

namespace realm {
namespace config {

struct Configuration {
    std::string id = "";
    std::string listen_address = "127.0.0.1";
    std::string listen_port = ""; // Empty means choose default based on `ssl`.
    realm::util::Optional<std::string> root_dir;
    std::string user_data_dir;
    realm::util::Optional<std::string> public_key_path;
    realm::util::Optional<std::string> config_file_path;
    bool reuse_address = true;
    bool disable_sync = false;
    realm::util::Logger::Level log_level = realm::util::Logger::Level::info;
    realm::util::Optional<std::string> log_path;
    long max_open_files = 256;
    bool ssl = false;
    std::string ssl_certificate_path;
    std::string ssl_certificate_key_path;
    std::string dashboard_stats_endpoint = "localhost:28125";
    uint_fast64_t drop_period_s = 60;
    uint_fast64_t idle_timeout_s = 1800;
    realm::sync::Server::Config::OperatingMode operating_mode =
       realm::sync::Server::Config::OperatingMode::MasterWithNoSlave;
    std::string master_address;
    std::string master_port;
    bool master_slave_ssl = false;
    bool master_verify_ssl_certificate = true;
    util::Optional<std::string> master_ssl_trust_certificate_path;
    std::string master_slave_shared_secret;
    util::Optional<std::string> feature_token;
    util::Optional<std::string> feature_token_path;
    bool enable_download_log_compaction = true;
    size_t max_download_size = 0x20000; // 128 KB
};

#if !REALM_MOBILE
void show_help(const std::string& program_name);
Configuration build_configuration(int argc, char* argv[]);
#endif
Configuration load_configuration(std::string configuration_file_path);

} // namespace config


namespace sync {

/// Initialise the directory structure as required (create missing directory
/// structure) for correct operation of the server. This function is supposed to
/// be executed prior to instantiating the \c Server object.
///
/// Note: This function also handles migration of server-side Realm files from
/// the legacy format (see _impl::ensure_legacy_migration_1()).
///
/// The type of migration performed by this function is nonatomic, and it
/// therefore requires that no other thread or process has any of the servers
/// Realm files open concurrently. The application is advised to make sure that
/// all agents (including the sync server), that might open server-side Realm
/// files are not started until after this function has completed sucessfully.
void prepare_server_directory(const config::Configuration&, util::Logger&);

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_SERVER_CONFIGURATION_HPP
