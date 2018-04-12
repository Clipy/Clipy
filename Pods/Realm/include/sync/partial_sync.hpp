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

#include "object_schema.hpp"
#include "results.hpp"

#include <realm/util/optional.hpp>

#include <functional>
#include <memory>
#include <string>

namespace realm {

class Group;
class Object;
class Realm;

namespace partial_sync {
enum class SubscriptionState : int8_t;

struct SubscriptionNotificationToken {
    NotificationToken registration_token;
    NotificationToken result_sets_token;
};

class Subscription {
public:
    ~Subscription();
    Subscription(Subscription&&);
    Subscription& operator=(Subscription&&);

    SubscriptionState state() const;
    std::exception_ptr error() const;

    Results results() const;

    SubscriptionNotificationToken add_notification_callback(std::function<void()> callback);

private:
    Subscription(std::string name, std::string object_type, std::shared_ptr<Realm>);

    util::Optional<Object> result_set_object() const;

    void error_occurred(std::exception_ptr);

    ObjectSchema m_object_schema;

    mutable Results m_result_sets;

    struct Notifier;
    _impl::CollectionNotifier::Handle<Notifier> m_notifier;

    friend Subscription subscribe(Results const&, util::Optional<std::string>);
    friend void unsubscribe(Subscription&);
};

/// Create a partial sync subscription from the query associated with the `Results`.
///
/// The subscription is created asynchronously.
///
/// State changes, including runtime errors, are communicated via notifications
/// registered on the resulting `Subscription` object.
///
/// Programming errors, such as attempting to create a subscription in that is not
/// partially synced, or subscribing to an unsupported query, will throw an exception.
Subscription subscribe(Results const&, util::Optional<std::string> name);

/// Remove a partial sync subscription.
///
/// The operation is performed asynchronously. Completion will be indicated by the
/// `Subscription` transitioning to the `Invalidated` state.
void unsubscribe(Subscription&);

// Deprecated
void register_query(std::shared_ptr<Realm>, const std::string &object_class, const std::string &query,
					std::function<void (Results, std::exception_ptr)>);

} // namespace partial_sync

namespace _impl {

void initialize_schema(Group&);

} // namespace _impl
} // namespace realm

#endif // REALM_OS_PARTIAL_SYNC_HPP
