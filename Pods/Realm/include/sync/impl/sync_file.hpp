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

#include "sync/sync_user.hpp"

#include <realm/util/optional.hpp>

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

/// Create a timestamped `mktemp`-compatible template string using the current local time.
std::string create_timestamped_template(const std::string& prefix, int wildcard_count=8);

/// Reserve a unique file name based on a base directory path and a `mktemp`-compatible template string.
/// Returns the path of the file.
std::string reserve_unique_file_name(const std::string& path, const std::string& template_string);

/// Remove a directory, including non-empty directories.
void remove_nonempty_dir(const std::string& path);

} // util

class SyncFileManager {
public:
    SyncFileManager(std::string base_path) : m_base_path(std::move(base_path)) { }

    /// Return the user directory for a given user, creating it if it does not already exist.
    std::string user_directory(const std::string& local_identity,
                               util::Optional<SyncUserIdentifier> user_info=none) const;

    /// Remove the user directory for a given user.
    void remove_user_directory(const std::string& local_identity) const;       // throws

    /// Rename a user directory. Returns true if a directory at `old_name` existed
    /// and was successfully renamed to `new_name`. Returns false if no directory
    /// exists at `old_name`.
    bool try_rename_user_directory(const std::string& old_name, const std::string& new_name) const;

    /// Return the path for a given Realm, creating the user directory if it does not already exist.
    std::string path(const std::string&, const std::string&,
                     util::Optional<SyncUserIdentifier> user_info=none) const;

    /// Remove the Realm at a given path for a given user. Returns `true` if the remove operation fully succeeds.
    bool remove_realm(const std::string& local_identity, const std::string& raw_realm_path) const;

    /// Remove the Realm whose primary Realm file is located at `absolute_path`. Returns `true` if the remove
    /// operation fully succeeds.
    bool remove_realm(const std::string& absolute_path) const;

    /// Copy the Realm file at the location `old_path` to the location of `new_path`.
    bool copy_realm_file(const std::string& old_path, const std::string& new_path) const;

    /// Return the path for the metadata Realm files.
    std::string metadata_path() const;

    /// Remove the metadata Realm.
    bool remove_metadata_realm() const;

    const std::string& base_path() const
    {
        return m_base_path;
    }

    std::string recovery_directory_path() const
    {
        return get_special_directory(c_recovery_directory);
    }

private:
    std::string m_base_path;

    static constexpr const char c_sync_directory[] = "realm-object-server";
    static constexpr const char c_utility_directory[] = "io.realm.object-server-utility";
    static constexpr const char c_recovery_directory[] = "io.realm.object-server-recovered-realms";
    static constexpr const char c_metadata_directory[] = "metadata";
    static constexpr const char c_metadata_realm[] = "sync_metadata.realm";
    static constexpr const char c_user_info_file[] = "__user_info";

    std::string get_special_directory(std::string directory_name) const;

    std::string get_utility_directory() const
    {
        return get_special_directory(c_utility_directory);
    }

    std::string get_base_sync_directory() const;
};

} // realm

#endif // REALM_OS_SYNC_FILE_HPP
