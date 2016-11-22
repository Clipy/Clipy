////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#ifndef REALM_OS_SYNC_FILE_HPP
#define REALM_OS_SYNC_FILE_HPP

#include <string>

namespace realm {

namespace util {

enum class FilePathType {
    File, Directory
};

// FIXME: Does it make sense to use realm::StringData arguments for these functions instead of std::string?

/// Given a string, turn it into a percent-encoded string.
std::string make_percent_encoded_string(const std::string& raw_string);

/// Given a percent-encoded string, turn it into the original (non-encoded) string.
std::string make_raw_string(const std::string& percent_encoded_string);

/// Given a file path and a path component, return a new path created by appending the component to the path.
std::string file_path_by_appending_component(const std::string& path,
                                             const std::string& component,
                                             FilePathType path_type=FilePathType::File);

/// Given a file path and an extension, append the extension to the path.
std::string file_path_by_appending_extension(const std::string& path, const std::string& extension);

/// Remove a directory, including non-empty directories.
void remove_nonempty_dir(const std::string& path);

} // util

class SyncFileManager {
public:
    SyncFileManager(std::string base_path) : m_base_path(std::move(base_path)) { }

    /// Return the user directory for a given user, creating it if it does not already exist.
    std::string user_directory(const std::string& user_identity) const;

    /// Remove the user directory for a given user.
    void remove_user_directory(const std::string& user_identity) const;       // throws

    /// Return the path for a given Realm, creating the user directory if it does not already exist.
    std::string path(const std::string& user_identity, const std::string& raw_realm_path) const;

    /// Remove the Realm at a given path for a given user. Returns `true` if the remove operation fully succeeds.
    bool remove_realm(const std::string& user_identity, const std::string& raw_realm_path) const;

    /// Return the path for the metadata Realm files.
    std::string metadata_path() const;

    /// Remove the metadata Realm.
    bool remove_metadata_realm() const;

    const std::string& base_path() const {
        return m_base_path;
    }

private:
    std::string m_base_path;

    static constexpr const char c_sync_directory[] = "realm-object-server";
    static constexpr const char c_utility_directory[] = "io.realm.object-server-utility";
    static constexpr const char c_metadata_directory[] = "metadata";
    static constexpr const char c_metadata_realm[] = "sync_metadata.realm";

    std::string get_utility_directory() const;
    std::string get_base_sync_directory() const;
};

} // realm

#endif // REALM_OS_SYNC_FILE_HPP
