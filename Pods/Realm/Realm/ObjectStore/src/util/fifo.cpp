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

#include "util/fifo.hpp"

#include <system_error>
#include <sys/stat.h>


// This file contains various helper methods for working with FIFOs.

namespace realm {
namespace util {

namespace {
void check_is_fifo(const std::string& path) {
    struct stat stat_buf;
    if (stat(path.c_str(), &stat_buf) == 0) {
        if ((stat_buf.st_mode & S_IFMT) != S_IFIFO) {
            throw std::runtime_error(path + " exists and it is not a fifo.");
        }
    }
}
} // Anonymous namespace

void create_fifo(const std::string& path) {
    // Create and open the named pipe
    int ret = mkfifo(path.c_str(), 0600);
    if (ret == -1) {
        int err = errno;
        // the fifo already existing isn't an error
        if (err != EEXIST) {
#ifdef __ANDROID__
            // Workaround for a mkfifo bug on Blackberry devices:
            // When the fifo already exists, mkfifo fails with error ENOSYS which is not correct.
            // In this case, we use stat to check if the path exists and it is a fifo.
            if (err == ENOSYS) {
                check_is_fifo(path);
            }
            else {
                throw std::system_error(err, std::system_category());
            }
#else
            throw std::system_error(err, std::system_category());
#endif
        }
        else {
            // If the file already exists, verify it is a FIFO
            return check_is_fifo(path);
        }
    }
}

bool try_create_fifo(const std::string& path) {
    try {
        create_fifo(path);
        return true;
    } catch (...) {
        return false;
    }
}

}
}
