////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
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

#include "impl/apple/keychain_helper.hpp"

#include "util/format.hpp"

#include <realm/util/cf_ptr.hpp>
#include <realm/util/optional.hpp>

#include <Security/Security.h>

#include <string>

using realm::util::CFPtr;
using realm::util::adoptCF;

namespace realm {
namespace keychain {

KeychainAccessException::KeychainAccessException(int32_t error_code)
: std::runtime_error(util::format("Keychain returned unexpected status code: %1", error_code)) { }

CFPtr<CFStringRef> convert_string(const std::string& string);
CFPtr<CFMutableDictionaryRef> build_search_dictionary(const std::string& account, const std::string& service,
                                                      util::Optional<std::string> group);

CFPtr<CFStringRef> convert_string(const std::string& string)
{
    auto result = adoptCF(CFStringCreateWithBytes(nullptr, reinterpret_cast<const UInt8*>(string.data()),
                                                  string.size(), kCFStringEncodingASCII, false));
    if (!result) {
        throw std::bad_alloc();
    }
    return result;
}

CFPtr<CFMutableDictionaryRef> build_search_dictionary(const std::string& account, const std::string& service,
                                                      __unused util::Optional<std::string> group)
{
    auto d = adoptCF(CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                                               &kCFTypeDictionaryValueCallBacks));
    if (!d) {
        throw std::bad_alloc();
    }
    CFDictionaryAddValue(d.get(), kSecClass, kSecClassGenericPassword);
    CFDictionaryAddValue(d.get(), kSecReturnData, kCFBooleanTrue);
    CFDictionaryAddValue(d.get(), kSecAttrAccessible, kSecAttrAccessibleAlways);
    CFDictionaryAddValue(d.get(), kSecAttrAccount, convert_string(account).get());
    CFDictionaryAddValue(d.get(), kSecAttrService, convert_string(service).get());
#if TARGET_IPHONE_SIMULATOR
#else
    if (group) {
        CFDictionaryAddValue(d.get(), kSecAttrAccessGroup, convert_string(*group).get());
    }
#endif
    return d;
}

std::vector<char> metadata_realm_encryption_key()
{
    const size_t key_size = 64;
    const std::string service = "io.realm.sync.keychain";
    const std::string account = "metadata";

    auto search_dictionary = build_search_dictionary(account, service, none);
    CFDataRef retained_key_data;
    if (OSStatus status = SecItemCopyMatching(search_dictionary.get(), (CFTypeRef *)&retained_key_data)) {
        if (status != errSecItemNotFound) {
            throw KeychainAccessException(status);
        }

        // Key was not found. Generate a new key, store it, and return it.
        std::vector<char> key(key_size);
        arc4random_buf(key.data(), key_size);
        auto key_data = adoptCF(CFDataCreate(nullptr, reinterpret_cast<const UInt8 *>(key.data()), key_size));
        if (!key_data) {
            throw std::bad_alloc();
        }

        CFDictionaryAddValue(search_dictionary.get(), kSecValueData, key_data.get());
        if (OSStatus status = SecItemAdd(search_dictionary.get(), nullptr)) {
            throw KeychainAccessException(status);
        }

        return key;
    }
    CFPtr<CFDataRef> key_data = adoptCF(retained_key_data);

    // Key was previously stored. Extract it.
    if (key_size != CFDataGetLength(key_data.get())) {
        throw std::runtime_error("Password stored in keychain was not expected size.");
    }

    auto key_bytes = reinterpret_cast<const char *>(CFDataGetBytePtr(key_data.get()));
    return std::vector<char>(key_bytes, key_bytes + key_size);
}
    
}
}
