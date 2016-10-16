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

/// sha1() calculates the sha1 hash value of \param in_buffer of size \param
/// in_buffer_size. The value is placed in \param out_buffer. The value has
/// size 20, and the caller must ensure that \param out_buffer has size at
/// least 20.
/// sha1() throws an exception if the underlying openssl implementation
/// fails, which should just happen in case of memory allocation failure.
void sha1(const char* in_buffer, size_t in_buffer_size, char* out_buffer);

} // namespace crypto
} // namespace sync
} // namespace realm

#endif // REALM_SYNC_CRYPTO_HPP
