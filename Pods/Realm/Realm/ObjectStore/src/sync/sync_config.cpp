////////////////////////////////////////////////////////////////////////////
//
// Copyright 2017 Realm Inc.
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

#include "sync/sync_config.hpp"

#include "sync/sync_manager.hpp"

#include <realm/util/sha_crypto.hpp>

namespace realm {

std::string SyncConfig::partial_sync_identifier(const SyncUser& user)
{
    std::string raw_identifier = SyncManager::shared().client_uuid() + "/" + user.local_identity();

    // The type of the argument to sha1() changed in sync 3.11.1. Implicitly
    // convert to either char or unsigned char so that both signatures work.
    struct cast {
        uint8_t* value;
        operator uint8_t*() { return value; }
        operator char*() { return reinterpret_cast<char*>(value); }
    };
    uint8_t identifier[20];
    realm::util::sha1(raw_identifier.data(), raw_identifier.size(), cast{&identifier[0]});

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t c : identifier)
        ss << std::setw(2) << (unsigned)c;
    return ss.str();
}

std::string SyncConfig::realm_url() const
{
    REALM_ASSERT(reference_realm_url.length() > 0);
    REALM_ASSERT(user);

    if (!is_partial)
        return reference_realm_url;

    std::string base_url = reference_realm_url;
    if (base_url.back() == '/')
        base_url.pop_back();

    if (custom_partial_sync_identifier)
        return util::format("%1/__partial/%2", base_url, *custom_partial_sync_identifier);
    return util::format("%1/__partial/%2/%3", base_url, user->identity(), partial_sync_identifier(*user));
}

} // namespace realm
