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

#ifndef REALM_OS_PARTIAL_SYNC_HPP
#define REALM_OS_PARTIAL_SYNC_HPP

#include <functional>
#include <memory>
#include <string>

namespace realm {

class Realm;
class Results;

namespace partial_sync {

void register_query(std::shared_ptr<Realm>, const std::string &object_class,
                    const std::string &query,
                    std::function<void (Results, std::exception_ptr)>);

} // namespace partial_sync
} // namespace realm

#endif // REALM_OS_PARTIAL_SYNC_HPP
