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

#ifndef REALM_OS_OBJECT_NOTIFIER_HPP
#define REALM_OS_OBJECT_NOTIFIER_HPP

#include "impl/collection_notifier.hpp"

#include <realm/keys.hpp>

namespace realm {

namespace _impl {
class ObjectNotifier : public CollectionNotifier {
public:
    ObjectNotifier(std::shared_ptr<Realm> realm, TableKey table, ObjKey obj);

private:
    TableKey m_table;
    ObjKey m_obj;
    TransactionChangeInfo* m_info;

    void run() override;

    bool do_add_required_change_info(TransactionChangeInfo& info) override;
};
}
}

#endif // REALM_OS_OBJECT_NOTIFIER_HPP
