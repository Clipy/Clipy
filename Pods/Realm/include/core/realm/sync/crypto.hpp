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

#ifndef REALM_SYNC_CRYPTO_HPP
#define REALM_SYNC_CRYPTO_HPP

#include <memory>
#include <stdexcept>

#include <realm/binary_data.hpp>
#include <realm/util/buffer.hpp>

namespace realm {
namespace sync {
namespace crypto {

/// The digest functions calculate the message digest of the input in \param
/// in_buffer of size \param in_buffer_size. The digest is placed in \param
/// out_buffer. The caller must guarantee that the output buffer is large
/// enough to contain the digest.
///
/// The functions throw if the underlying platform dependent implementations
/// throw. Typically, exceptions are "out of memory" errors.
///
/// sha1() calculates the SHA-1 hash value of output size 20.
/// sha256() calculates the SHA-256 hash value of output size 32.
void sha1(const char* in_buffer, size_t in_buffer_size, unsigned char* out_buffer);
void sha256(const char* in_buffer, size_t in_buffer_size, unsigned char* out_buffer);

} // namespace crypto
} // namespace sync
} // namespace realm

#endif // REALM_SYNC_CRYPTO_HPP
