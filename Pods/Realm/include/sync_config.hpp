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

#ifndef REALM_OS_SYNC_CONFIG_HPP
#define REALM_OS_SYNC_CONFIG_HPP

#include <functional>
#include <string>

namespace realm {

enum class SyncSessionStopPolicy;

enum class SyncSessionError {
    Debug,                  // An informational error, nothing to do. Only for debug purposes.
    SessionFatal,           // The session is invalid and should be killed.
    AccessDenied,           // Permissions error with the session.
    UserFatal,              // The user associated with the session is invalid.
};

using SyncSessionErrorHandler = void(int error_code, std::string message, SyncSessionError);

struct SyncConfig {
    SyncConfig(std::string user_tag, std::string realm_url, std::function<SyncSessionErrorHandler> error_handler,
               SyncSessionStopPolicy stop_policy)
    : user_tag(std::move(user_tag))
    , realm_url(std::move(realm_url))
    , error_handler(std::move(error_handler))
    , stop_policy(stop_policy)
    {
    }

    std::string user_tag;
    std::string realm_url;
    std::function<SyncSessionErrorHandler> error_handler;
    SyncSessionStopPolicy stop_policy;
};

} // realm

#endif // REALM_OS_SYNC_CONFIG_HPP
