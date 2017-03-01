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

// Realm headers
#include <realm/util/logger.hpp>
#include <realm/util/optional.hpp>

namespace realm {
namespace config {

struct Configuration {
    std::string listen_address = "127.0.0.1";
    std::string listen_port = ""; // Empty means choose default based on `ssl`.
    realm::util::Optional<std::string> root_dir;
    std::string user_data_dir;
    std::string internal_data_dir;
    realm::util::Optional<std::string> public_key_path;
    realm::util::Optional<std::string> config_file_path;
    bool reuse_address = true;
    bool disable_sync = false;
    realm::util::Logger::Level log_level = realm::util::Logger::Level::info;
    realm::util::Optional<std::string> log_path;
    std::string stats_db_path;
    long max_open_files = 256;
    bool ssl = false;
    std::string ssl_certificate_path;
    std::string ssl_certificate_key_path;
    std::string dashboard_stats_endpoint = "localhost:28125";
};

void show_help(const std::string& program_name);
Configuration build_configuration(int argc, char* argv[]);
Configuration load_configuration(std::string configuration_file_path);

} // namespace config
} // namespace realm

#endif // REALM_SYNC_SERVER_CONFIGURATION_HPP
