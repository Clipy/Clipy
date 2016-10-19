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

#ifndef REALM_UTIL_BASIC_SYSTEM_ERRORS_HPP
#define REALM_UTIL_BASIC_SYSTEM_ERRORS_HPP

#include <cerrno>
#include <system_error>


namespace realm {
namespace util {
namespace error {

enum basic_system_errors {
    /// Address family not supported by protocol.
    address_family_not_supported = EAFNOSUPPORT,

    /// Invalid argument.
    invalid_argument = EINVAL,

    /// Cannot allocate memory.
    no_memory = ENOMEM,

    /// Operation cancelled.
    operation_aborted = ECANCELED,

    /// Connection aborted.
    connection_aborted = ECONNABORTED,

    /// Connection reset by peer
    connection_reset = ECONNRESET,

    /// Broken pipe
    broken_pipe = EPIPE,

    /// Resource temporarily unavailable
    resource_unavailable_try_again = EAGAIN,
};

std::error_code make_error_code(basic_system_errors) noexcept;

} // namespace error
} // namespace util
} // namespace realm

namespace std {

template <>
class is_error_code_enum<realm::util::error::basic_system_errors> {
public:
    static const bool value = true;
};

} // namespace std

namespace realm {
namespace util {

std::error_code make_basic_system_error_code(int) noexcept;


// implementation

inline std::error_code make_basic_system_error_code(int err) noexcept
{
    using namespace error;
    return make_error_code(basic_system_errors(err));
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_BASIC_SYSTEM_ERRORS_HPP
