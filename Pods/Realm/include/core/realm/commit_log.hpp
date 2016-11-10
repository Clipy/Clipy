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
#ifndef REALM_COMMIT_LOG_HPP
#define REALM_COMMIT_LOG_HPP

#include <stdexcept>
#include <string>

#include <realm/binary_data.hpp>
#include <realm/replication.hpp>


namespace realm {

// FIXME: Why is this exception class exposed?
class LogFileError: public std::runtime_error {
public:
    LogFileError(const std::string& file_name):
        std::runtime_error(file_name)
    {
    }
};

/// Create a writelog collector and associate it with a filepath. You'll need
/// one writelog collector for each shared group. Commits from writelog
/// collectors for a specific filepath may later be obtained through other
/// writelog collectors associated with said filepath.  The caller assumes
/// ownership of the writelog collector and must destroy it, but only AFTER
/// destruction of the shared group using it.
std::unique_ptr<Replication>
make_client_history(const std::string& path, const char* encryption_key = nullptr);

} // namespace realm


#endif // REALM_COMMIT_LOG_HPP
