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

#ifndef REALM_UTIL_MISC_ERRORS_HPP
#define REALM_UTIL_MISC_ERRORS_HPP

#include <system_error>


namespace realm {
namespace util {
namespace error {

enum misc_errors {
    unknown = 1,
};

std::error_code make_error_code(misc_errors);

} // namespace error
} // namespace util
} // namespace realm

namespace std {

template <>
class is_error_code_enum<realm::util::error::misc_errors> {
public:
    static const bool value = true;
};

} // namespace std

#endif // REALM_UTIL_MISC_ERRORS_HPP
