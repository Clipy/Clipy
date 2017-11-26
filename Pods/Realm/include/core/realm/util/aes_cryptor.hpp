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

#include <cstddef>
#include <memory>
#include <realm/util/features.h>
#include <cstdint>
#include <vector>
#include <realm/util/file.hpp>

#if REALM_ENABLE_ENCRYPTION

#if REALM_PLATFORM_APPLE
#include <CommonCrypto/CommonCrypto.h>
#elif defined(_WIN32)
#include <windows.h>
#include <stdio.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <openssl/aes.h>
#include <openssl/sha.h>
#endif

namespace realm {
namespace util {

struct iv_table;
class EncryptedFileMapping;

class AESCryptor {
public:
    AESCryptor(const uint8_t* key);
    ~AESCryptor() noexcept;

    void set_file_size(off_t new_size);

    bool read(FileDesc fd, off_t pos, char* dst, size_t size);
    void write(FileDesc fd, off_t pos, const char* src, size_t size) noexcept;

private:
    enum EncryptionMode {
#if REALM_PLATFORM_APPLE
        mode_Encrypt = kCCEncrypt,
        mode_Decrypt = kCCDecrypt
#elif defined(_WIN32)
        mode_Encrypt = 0,
        mode_Decrypt = 1
#else
        mode_Encrypt = AES_ENCRYPT,
        mode_Decrypt = AES_DECRYPT
#endif
    };

#if REALM_PLATFORM_APPLE
    CCCryptorRef m_encr;
    CCCryptorRef m_decr;
#elif defined(_WIN32)
    BCRYPT_KEY_HANDLE m_aes_key_handle;
#else
    AES_KEY m_ectx;
    AES_KEY m_dctx;
#endif

    uint8_t m_hmacKey[32];
    std::vector<iv_table> m_iv_buffer;
    std::unique_ptr<char[]> m_rw_buffer;
    std::unique_ptr<char[]> m_dst_buffer;

    void calc_hmac(const void* src, size_t len, uint8_t* dst, const uint8_t* key) const;
    bool check_hmac(const void* data, size_t len, const uint8_t* hmac) const;
    void crypt(EncryptionMode mode, off_t pos, char* dst, const char* src, const char* stored_iv) noexcept;
    iv_table& get_iv_table(FileDesc fd, off_t data_pos) noexcept;
};

struct SharedFileInfo {
    FileDesc fd;
    AESCryptor cryptor;
    std::vector<EncryptedFileMapping*> mappings;

    SharedFileInfo(const uint8_t* key, FileDesc file_descriptor);
};
}
}

#endif // REALM_ENABLE_ENCRYPTION
