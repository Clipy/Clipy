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

#ifndef REALM_SHA_CRYPTO_HPP
#define REALM_SHA_CRYPTO_HPP

#include <cstddef>

namespace realm {
namespace util {

/// The digest functions calculate the message digest of the input in \param
/// in_buffer of size \param in_buffer_size . The digest is placed in \param
/// out_buffer . The caller must guarantee that the output buffer is large
/// enough to contain the digest.
///
/// The functions throw if the underlying platform dependent implementations
/// throw. Typically, exceptions are "out of memory" errors.
///
/// sha1() calculates the SHA-1 hash value of output size 20.
/// sha256() calculates the SHA-256 hash value of output size 32.
void sha1(const char* in_buffer, size_t in_buffer_size, unsigned char* out_buffer);
void sha256(const char* in_buffer, size_t in_buffer_size, unsigned char* out_buffer);

} // namespace util
} // namespace realm

#endif // REALM_SHA_CRYPTO_HPP
