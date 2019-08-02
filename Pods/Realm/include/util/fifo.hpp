////////////////////////////////////////////////////////////////////////////
//
// Copyright 2018 Realm Inc.
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

#include <string>

// This file contains various helper methods for working with FIFOs.

namespace realm {
namespace util {

// Creates a fifo at the provided path. If the FIFO could not be created an exception is thrown.
// This method will also be successful if an existing FIFO already existed at the given location.
void create_fifo(const std::string& path);

// Same as create_fifo() except that this one returns `false`, rather than throwing
// an exception, if the fifo could not be created or didn't already exist.
bool try_create_fifo(const std::string& path);

// Ensure that a path representing a directory ends with `/`
inline std::string normalize_dir(const std::string& path) {
    return (!path.empty() && path.back() != '/') ? path + '/' : path;
}

}
}
