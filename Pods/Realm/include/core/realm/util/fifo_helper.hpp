/*************************************************************************
 *
 * Copyright 2019 Realm Inc.
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

#ifndef REALM_UTIL_FIFO_HELPER_HPP
#define REALM_UTIL_FIFO_HELPER_HPP

#include <string>

namespace realm {
namespace util {

// Attempts to create a FIFO file at the location determined by `path`.
// If creating the FIFO at this location fails, an exception is thrown.
// If a FIFO already exists at the given location, this method does nothing.
void create_fifo(std::string path); // throws

// Same as above, but returns `false` if the FIFO could not be created instead of throwing.
bool try_create_fifo(const std::string& path);

// Ensure that a path representing a directory ends with `/`
inline std::string normalize_dir(const std::string& path) {
    return (!path.empty() && path.back() != '/') ? path + '/' : path;
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_FIFO_HELPER_HPP
