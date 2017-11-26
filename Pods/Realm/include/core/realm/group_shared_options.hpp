/*************************************************************************
 *
 * Copyright 2016 Realm Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 **************************************************************************/

#ifndef REALM_GROUP_SHARED_OPTIONS_HPP
#define REALM_GROUP_SHARED_OPTIONS_HPP

#include <functional>
#include <string>

namespace realm {

struct SharedGroupOptions {

    /// The persistence level of the SharedGroup.
    /// uint16_t is the type of SharedGroup::SharedInfo::durability
    enum class Durability : uint16_t {
        Full,
        MemOnly,
        Async ///< Not yet supported on windows.
    };

    explicit SharedGroupOptions(Durability level = Durability::Full, const char* key = nullptr,
                                bool allow_upgrade = true,
                                std::function<void(int, int)> file_upgrade_callback = std::function<void(int, int)>(),
                                std::string temp_directory = sys_tmp_dir,
                                bool track_metrics = false)
        : durability(level)
        , encryption_key(key)
        , allow_file_format_upgrade(allow_upgrade)
        , upgrade_callback(file_upgrade_callback)
        , temp_dir(temp_directory)
        , enable_metrics(track_metrics)

    {
    }

    explicit SharedGroupOptions(const char* key)
        : durability(Durability::Full)
        , encryption_key(key)
        , allow_file_format_upgrade(true)
        , upgrade_callback(std::function<void(int, int)>())
        , temp_dir(sys_tmp_dir)
        , enable_metrics(false)
    {
    }

    /// The persistence level of the Realm file. See Durability.
    Durability durability;

    /// The key to encrypt and decrypt the Realm file with, or nullptr to
    /// indicate that encryption should not be used.
    const char* encryption_key;

    /// If \a allow_file_format_upgrade is set to `true`, this function will
    /// automatically upgrade the file format used in the specified Realm file
    /// if necessary (and if it is possible). In order to prevent this, set \a
    /// allow_upgrade to `false`.
    ///
    /// If \a allow_upgrade is set to `false`, only two outcomes are possible:
    ///
    /// - the specified Realm file is already using the latest file format, and
    ///   can be used, or
    ///
    /// - the specified Realm file uses a deprecated file format, resulting a
    ///   the throwing of FileFormatUpgradeRequired.
    bool allow_file_format_upgrade;

    /// Optionally allows a custom function to be called immediately after the
    /// Realm file is upgraded. The two parameters in the function are the
    /// previous version and the version just upgraded to, respectively.
    /// If the callback function throws, the Realm file will safely abort the
    /// upgrade (rollback the transaction) but the SharedGroup will not be opened.
    std::function<void(int, int)> upgrade_callback;

    /// A path to a directory where Realm can write temporary files or pipes to.
    /// This string should include a trailing slash '/'.
    std::string temp_dir;

    /// Controls the feature of collecting various metrics to the SharedGroup.
    /// A prerequisite is compiling with REALM_METRICS=ON.
    bool enable_metrics;

    /// sys_tmp_dir will be used if the temp_dir is empty when creating SharedGroupOptions.
    /// It must be writable and allowed to create pipe/fifo file on it.
    /// set_sys_tmp_dir is not a thread-safe call and it is only supposed to be called once
    //  when process starts.
    static void set_sys_tmp_dir(const std::string& dir) noexcept { sys_tmp_dir = dir; }
    static std::string get_sys_tmp_dir() noexcept { return sys_tmp_dir; }

private:
    static std::string sys_tmp_dir;
};

} // end namespace realm

#endif // REALM_GROUP_SHARED_OPTIONS_HPP
