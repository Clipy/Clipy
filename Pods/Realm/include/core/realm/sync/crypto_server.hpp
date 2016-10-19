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

#ifndef REALM_SYNC_CRYPTO_SERVER_HPP
#define REALM_SYNC_CRYPTO_SERVER_HPP

#include <memory>
#include <stdexcept>

#include <realm/binary_data.hpp>
#include <realm/util/buffer.hpp>

namespace realm {
namespace sync {

struct CryptoError: std::runtime_error {
    CryptoError(std::string message) : std::runtime_error(std::move(message)) {}
};

/// This class represents a public/private keypair, or more commonly a single public
/// key used for verifying signatures.
///
/// Only RSA keys are supported for now.
///
/// Its methods correspond roughly to the EVP_PKEY_* set of functionality found in
/// the OpenSSL library.
class PKey {
public:
    PKey(PKey&&);
    PKey& operator=(PKey&&);
    ~PKey();

    /// Load RSA public key from \a pemfile.
    static PKey load_public(const std::string& pemfile);
    /// Load RSA public/private keypair from \a pemfile.
    static PKey load_private(const std::string& pemfile);

    /// Whether or not the key can be used for signing.
    ///
    /// True if the private part is loaded.
    bool can_sign() const noexcept;

    /// Whether or not the key can be used for verifying.
    ///
    /// Always true for RSA keys.
    bool can_verify() const noexcept;

    /// Sign \a message with the loaded key, if the private part is
    /// loaded. Store the signed message as binary data in \a signature.
    ///
    /// If a private key is not loaded, throws an exception of type CryptoError.
    void sign(BinaryData message, util::Buffer<unsigned char>& signature) const;

    /// Verify that \a signature is a valid digest of \a message.
    ///
    /// Returns true if the signature is valid, otherwise false. If an error occurs while
    /// attempting verification, an exception of type CryptoError is thrown.
    bool verify(BinaryData message, BinaryData signature) const;

private:
    PKey();
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_CRYPTO_SERVER_HPP
