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
static constexpr const char* result_sets_type_name = "__ResultSets";
static constexpr const char* property_name = "name";
static constexpr const char* property_query = "query";
static constexpr const char* property_matches_property_name = "matches_property";
static constexpr const char* property_status = "status";
static constexpr const char* property_error_message = "error_message";
static constexpr const char* property_query_parse_counter = "query_parse_counter";
static constexpr const char* property_created_at = "created_at";
static constexpr const char* property_updated_at = "updated_at";
static constexpr const char* property_expires_at = "expires_at";
static constexpr const char* property_time_to_live = "time_to_live";
static constexpr const size_t result_sets_property_count = 10;

struct InvalidRealmStateException : public std::logic_error {
    InvalidRealmStateException(const std::string& msg);
};

struct ExistingSubscriptionException : public std::runtime_error {
    ExistingSubscriptionException(const std::string& msg);
};

struct QueryTypeMismatchException: public std::logic_error {
    QueryTypeMismatchException(const std::string& msg);
};

enum class SubscriptionState : int8_t;

struct SubscriptionNotificationToken {
    NotificationToken registration_token;
    NotificationToken result_sets_token;
};

struct SubscriptionCallbackWrapper {
    std::function<void()> callback;
    util::Optional<SubscriptionState> last_state;
};

struct SubscriptionOptions {
    // A user defined name for referencing this subscription later. If no name is provided,
    // a default name will be generated based off the contents of the query.
    util::Optional<std::string> user_provided_name;
    // `time_to_live` is expressed in milliseconds and indicates for how long a subscription should
    // be persisted when not used. If no value is provided, the subscription will live until manually
    // deleted.
    util::Optional<int64_t> time_to_live_ms = none;
    // If a subscription with the given name already exists the behaviour depends on `update`. If
    // `update = true` the existing subscription will replace its query and time_to_live with the
    // provided values. If `update = false` an exception is thrown if the new query doesn't match
    // the old one. If no name is provided, the `update` flag is ignored.
    bool update = false;
    // A container which denotes a set of backlinks (linkingObjects) which should be included
    // in the subscription.
    IncludeDescriptor inclusions;
};

class Subscription {
public:
    ~Subscription();
    Subscription(Subscription&&);
    Subscription& operator=(Subscription&&);

    SubscriptionState state() const;
    std::exception_ptr error() const;

    SubscriptionNotificationToken add_notification_callback(std::function<void()> callback);

    util::Optional<Object> result_set_object() const;

private:
    Subscription(std::string name, std::string object_type, std::shared_ptr<Realm>);

    void error_occurred(std::exception_ptr);
    void run_callback(SubscriptionCallbackWrapper& callback_wrapper);

    ObjectSchema m_object_schema;

    mutable Results m_result_sets;

    // Timestamp indicating when the subscription wrapper is created. This is used when checking the Results notifications
    // By comparing this timestamp against the real subscriptions `created_at` and `updated_at` fields we can determine
    // whether the subscription is in progress of being updated or created.
    Timestamp m_wrapper_created_at;

    // Track the actual underlying subscription object once it is available. This is used to better track
    // unsubscriptions.
    util::Optional<Row> m_result_sets_object = none;

    struct Notifier;
    _impl::CollectionNotifier::Handle<Notifier> m_notifier;

    friend Subscription subscribe(Results const&, SubscriptionOptions);
    friend void unsubscribe(Subscription&);

};

/// Create a Query-based subscription from the query associated with the `Results`.
///
/// The subscription is created asynchronously.
///
/// State changes, including runtime errors, are communicated via notifications
/// registered on the resulting `Subscription` object.
///
/// Programming errors, such as attempting to create a subscription in a Realm that is not
/// Query-based, or subscribing to an unsupported query, will throw an exception.
Subscription subscribe(Results const&, SubscriptionOptions options);

// Create a subscription from the query associated with the `Results`
//
// The subscription is created synchronously, so this method should only be called inside
// a write transaction.
//
// Programming errors, such as attempting to create a subscription outside a write transaction or in
// a Realm that is not Query-based, or subscribing to an unsupported query, will throw an exception.
//
// The Row that represents the Subscription in the  __ResultsSets table is returned.
//
// If a subscription with the given name already exists the behaviour depends on `update`. If
// `update = true` the existing subscription will replace its query and time_to_live with the
// provided values. If `update = false` an exception is thrown if the new query doesn't match
// the old one. If no name is provided, the `update` flag is ignored.
//
// `time_to_live` is expressed in milliseconds and indicates for how long a subscription should
// be persisted when not used. If no value is provided, the subscription will live until manually
// deleted.
Row subscribe_blocking(Results const&, util::Optional<std::string> name, util::Optional<int64_t> time_to_live_ms = none, bool update = false);

/// Remove a partial sync subscription.
///
/// The operation is performed asynchronously. Completion will be indicated by the
/// `Subscription` transitioning to the `Invalidated` state.
void unsubscribe(Subscription&);

/// Remove a partial sync subscription.
///
/// This is effectively just obj.row().move_last_over(), but the deletion is
/// performed asynchronously on the partial sync worker thread rather than
/// the current thread. The object must be an object in the ResultSets table.
void unsubscribe(Object&&);

} // namespace partial_sync

namespace _impl {

void initialize_schema(Group&);
void ensure_partial_sync_schema_initialized(Realm&);

} // namespace _impl
} // namespace realm

#endif // REALM_OS_PARTIAL_SYNC_HPP
