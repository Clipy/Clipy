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

#include "sync/partial_sync.hpp"

#include "impl/collection_notifier.hpp"
#include "impl/notification_wrapper.hpp"
#include "impl/object_accessor_impl.hpp"
#include "impl/realm_coordinator.hpp"
#include "object_schema.hpp"
#include "results.hpp"
#include "shared_realm.hpp"
#include "sync/impl/work_queue.hpp"
#include "sync/subscription_state.hpp"
#include "sync/sync_config.hpp"
#include "sync/sync_session.hpp"

#include <realm/lang_bind_helper.hpp>
#include <realm/util/scope_exit.hpp>

namespace {
constexpr const char* result_sets_type_name = "__ResultSets";
}

namespace realm {

namespace _impl {

void initialize_schema(Group& group)
{
    std::string result_sets_table_name = ObjectStore::table_name_for_object_type(result_sets_type_name);
    TableRef table = group.get_table(result_sets_table_name);
    if (!table) {
        table = sync::create_table(group, result_sets_table_name);
        table->add_column(type_String, "query");
        table->add_column(type_String, "matches_property");
        table->add_column(type_Int, "status");
        table->add_column(type_String, "error_message");
        table->add_column(type_Int, "query_parse_counter");
    }
    else {
        // The table already existed, so it should have all of the columns that are in the shared schema.
        REALM_ASSERT(table->get_column_index("query") != npos);
        REALM_ASSERT(table->get_column_index("matches_property") != npos);
        REALM_ASSERT(table->get_column_index("status") != npos);
        REALM_ASSERT(table->get_column_index("error_message") != npos);
        REALM_ASSERT(table->get_column_index("query_parse_counter") != npos);
    }

    // We may need to add the "name" column even if the __ResultSets table already existed
    // as it's not added by the server when it creates the table.
    if (table->get_column_index("name") == npos) {
        size_t idx = table->add_column(type_String, "name");
        table->add_search_index(idx);
    }
}

// A stripped-down version of WriteTransaction that can promote an existing read transaction
// and that notifies the sync session after committing a change.
class WriteTransactionNotifyingSync {
public:
    WriteTransactionNotifyingSync(Realm::Config const& config, SharedGroup& sg)
    : m_config(config)
    , m_shared_group(&sg)
    {
        if (m_shared_group->get_transact_stage() == SharedGroup::transact_Reading)
            LangBindHelper::promote_to_write(*m_shared_group);
        else
            m_shared_group->begin_write();
    }

    ~WriteTransactionNotifyingSync()
    {
        if (m_shared_group)
            m_shared_group->rollback();
    }

    SharedGroup::version_type commit()
    {
        REALM_ASSERT(m_shared_group);
        auto version = m_shared_group->commit();
        m_shared_group = nullptr;

        auto session = SyncManager::shared().get_session(m_config.path, *m_config.sync_config);
        SyncSession::Internal::nonsync_transact_notify(*session, version);
        return version;
    }

    void rollback()
    {
        REALM_ASSERT(m_shared_group);
        m_shared_group->rollback();
        m_shared_group = nullptr;
    }

    Group& get_group() const noexcept
    {
        REALM_ASSERT(m_shared_group);
        return _impl::SharedGroupFriend::get_group(*m_shared_group);
    }

private:
    Realm::Config const& m_config;
    SharedGroup* m_shared_group;
};

// Provides a convenient way for code in this file to access private details of `Realm`
// without having to add friend declarations for each individual use.
class PartialSyncHelper {
public:
    static decltype(auto) get_shared_group(Realm& realm)
    {
        return Realm::Internal::get_shared_group(realm);
    }

    static decltype(auto) get_coordinator(Realm& realm)
    {
        return Realm::Internal::get_coordinator(realm);
    }
};

struct RowHandover {
    RowHandover(Realm& realm, Row row)
    : source_shared_group(*PartialSyncHelper::get_shared_group(realm))
    , row(source_shared_group.export_for_handover(std::move(row)))
    , version(source_shared_group.pin_version())
    {
    }

    ~RowHandover()
    {
        // If the row isn't already null we've not been imported and the version pin will leak.
        REALM_ASSERT(!row);
    }

    SharedGroup& source_shared_group;
    std::unique_ptr<SharedGroup::Handover<Row>> row;
    VersionID version;
};

} // namespace _impl

namespace partial_sync {

namespace {

template<typename F>
void with_open_shared_group(Realm::Config const& config, F&& function)
{
    std::unique_ptr<Replication> history;
    std::unique_ptr<SharedGroup> sg;
    std::unique_ptr<Group> read_only_group;
    Realm::open_with_config(config, history, sg, read_only_group, nullptr);

    function(*sg);
}
void update_schema(Group& group, Property matches_property)
{
    Schema current_schema;
    std::string table_name = ObjectStore::table_name_for_object_type(result_sets_type_name);
    if (group.has_table(table_name))
        current_schema = {ObjectSchema{group, result_sets_type_name}};

    Schema desired_schema({
        ObjectSchema(result_sets_type_name, {
            {"name", PropertyType::String, Property::IsPrimary{false}, Property::IsIndexed{true}},
            {"matches_property", PropertyType::String},
            {"query", PropertyType::String},
            {"status", PropertyType::Int},
            {"error_message", PropertyType::String},
            {"query_parse_counter", PropertyType::Int},
            std::move(matches_property)
        })
    });
    auto required_changes = current_schema.compare(desired_schema);
    if (!required_changes.empty())
        ObjectStore::apply_additive_changes(group, required_changes, true);
}

struct ResultSetsColumns {
    ResultSetsColumns(Table& table, std::string const& matches_property_name)
    {
        name = table.get_column_index("name");
        REALM_ASSERT(name != npos);

        query = table.get_column_index("query");
        REALM_ASSERT(query != npos);

        this->matches_property_name = table.get_column_index("matches_property");
        REALM_ASSERT(this->matches_property_name != npos);

        // This may be `npos` if the column does not yet exist.
        matches_property = table.get_column_index(matches_property_name);
    }

    size_t name;
    size_t query;
    size_t matches_property_name;
    size_t matches_property;
};

bool validate_existing_subscription(Table& table, ResultSetsColumns const& columns, std::string const& name,
                                    std::string const& query, std::string const& matches_property)
{
    auto existing_row_ndx = table.find_first_string(columns.name, name);
    if (existing_row_ndx == npos)
        return false;

    StringData existing_query = table.get_string(columns.query, existing_row_ndx);
    if (existing_query != query)
        throw std::runtime_error(util::format("An existing subscription exists with the same name, "
                                              "but a different query ('%1' vs '%2').",
                                              existing_query, query));

    StringData existing_matches_property = table.get_string(columns.matches_property_name, existing_row_ndx);
    if (existing_matches_property != matches_property)
        throw std::runtime_error(util::format("An existing subscription exists with the same name, "
                                              "but a different result type ('%1' vs '%2').",
                                              existing_matches_property, matches_property));

    return true;
}

void enqueue_registration(Realm& realm, std::string object_type, std::string query, std::string name,
                          std::function<void(std::exception_ptr)> callback)
{
    auto config = realm.config();

    auto& work_queue = _impl::PartialSyncHelper::get_coordinator(realm).partial_sync_work_queue();
    work_queue.enqueue([object_type=std::move(object_type), query=std::move(query), name=std::move(name),
                        callback=std::move(callback), config=std::move(config)] {
        try {
            with_open_shared_group(config, [&](SharedGroup& sg) {
                _impl::WriteTransactionNotifyingSync write(config, sg);

                auto matches_property = std::string(object_type) + "_matches";

                auto table = ObjectStore::table_for_object_type(write.get_group(), result_sets_type_name);
                ResultSetsColumns columns(*table, matches_property);

                // Update schema if needed.
                if (columns.matches_property == npos) {
                    auto target_table = ObjectStore::table_for_object_type(write.get_group(), object_type);
                    columns.matches_property = table->add_column_link(type_LinkList, matches_property, *target_table);
                } else {
                    // FIXME: Validate that the column type and link target are correct.
                }

                if (!validate_existing_subscription(*table, columns, name, query, matches_property)) {
                    auto row_ndx = sync::create_object(write.get_group(), *table);
                    table->set_string(columns.name, row_ndx, name);
                    table->set_string(columns.query, row_ndx, query);
                    table->set_string(columns.matches_property_name, row_ndx, matches_property);
                }

                write.commit();
            });
        } catch (...) {
            callback(std::current_exception());
            return;
        }

        callback(nullptr);
    });
}

void enqueue_unregistration(Object result_set, std::function<void()> callback)
{
    auto realm = result_set.realm();
    auto config = realm->config();
    auto& work_queue = _impl::PartialSyncHelper::get_coordinator(*realm).partial_sync_work_queue();

    // Export a reference to the __ResultSets row so we can hand it to the worker thread.
    // We store it in a shared_ptr as it would otherwise prevent the lambda from being copyable,
    // which `std::function` requires.
    auto handover = std::make_shared<_impl::RowHandover>(*realm, result_set.row());

    work_queue.enqueue([handover=std::move(handover), callback=std::move(callback),
                        config=std::move(config)] () {
        with_open_shared_group(config, [&](SharedGroup& sg) {
            // Import handed-over object.
            sg.begin_read(handover->version);
            Row row = *sg.import_from_handover(std::move(handover->row));
            sg.unpin_version(handover->version);

            _impl::WriteTransactionNotifyingSync write(config, sg);
            if (row.is_attached()) {
                row.move_last_over();
                write.commit();
            }
            else {
                write.rollback();
            }
        });
        callback();
    });
}

std::string default_name_for_query(const std::string& query, const std::string& object_type)
{
    return util::format("[%1] %2", object_type, query);
}

} // unnamed namespace

void register_query(std::shared_ptr<Realm> realm, const std::string &object_class, const std::string &query,
                    std::function<void (Results, std::exception_ptr)> callback)
{
    auto sync_config = realm->config().sync_config;
    if (!sync_config || !sync_config->is_partial)
        throw std::logic_error("A partial sync query can only be registered in a partially synced Realm");

    if (realm->schema().find(object_class) == realm->schema().end())
        throw std::logic_error("A partial sync query can only be registered for a type that exists in the Realm's schema");

    auto matches_property = object_class + "_matches";

    // The object schema must outlive `object` below.
    std::unique_ptr<ObjectSchema> result_sets_schema;
    Object raw_object;
    {
        realm->begin_transaction();
        auto cleanup = util::make_scope_exit([&]() noexcept {
            if (realm->is_in_transaction())
                realm->cancel_transaction();
        });

        update_schema(realm->read_group(),
                      Property(matches_property, PropertyType::Object|PropertyType::Array, object_class));

        result_sets_schema = std::make_unique<ObjectSchema>(realm->read_group(), result_sets_type_name);

        CppContext context;
        raw_object = Object::create<util::Any>(context, realm, *result_sets_schema,
                                               AnyDict{
                                                   {"name", query},
                                                   {"matches_property", matches_property},
                                                   {"query", query},
                                                   {"status", int64_t(0)},
                                                   {"error_message", std::string()},
                                                   {"query_parse_counter", int64_t(0)},
                                               }, false);

        realm->commit_transaction();
    }

    auto object = std::make_shared<_impl::NotificationWrapper<Object>>(std::move(raw_object));

    // Observe the new object and notify listener when the results are complete (status != 0).
    auto notification_callback = [object, matches_property,
                                  result_sets_schema=std::move(result_sets_schema),
                                  callback=std::move(callback)](CollectionChangeSet, std::exception_ptr error) mutable {
        if (error) {
            callback(Results(), error);
            object.reset();
            return;
        }

        CppContext context;
        auto status = any_cast<int64_t>(object->get_property_value<util::Any>(context, "status"));
        if (status == 0) {
            // Still computing...
            return;
        } else if (status == 1) {
            // Finished successfully.
            auto list = any_cast<List>(object->get_property_value<util::Any>(context, matches_property));
            callback(list.as_results(), nullptr);
        } else {
            // Finished with error.
            auto message = any_cast<std::string>(object->get_property_value<util::Any>(context, "error_message"));
            callback(Results(), std::make_exception_ptr(std::runtime_error(std::move(message))));
        }
        object.reset();
    };
    object->add_notification_callback(std::move(notification_callback));
}

struct Subscription::Notifier : public _impl::CollectionNotifier {
    enum State {
        Creating,
        Complete,
        Removed,
    };

    Notifier(std::shared_ptr<Realm> realm)
    : _impl::CollectionNotifier(std::move(realm))
    , m_coordinator(&_impl::PartialSyncHelper::get_coordinator(*get_realm()))
    {
    }

    void release_data() noexcept override { }
    void run() override
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_has_results_to_deliver) {
            // Mark the object as being modified so that CollectionNotifier is aware
            // that there are changes to deliver.
            m_changes.modify(0);
        }
    }

    void deliver(SharedGroup&) override
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_error = m_pending_error;
        m_pending_error = nullptr;

        m_state = m_pending_state;
        m_has_results_to_deliver = false;
    }

    void finished_subscribing(std::exception_ptr error)
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_pending_error = error;
            m_pending_state = Complete;
            m_has_results_to_deliver = true;
        }

        // Trigger processing of change notifications.
        m_coordinator->wake_up_notifier_worker();
    }

    void finished_unsubscribing()
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_pending_state = Removed;
            m_has_results_to_deliver = true;
        }

        // Trigger processing of change notifications.
        m_coordinator->wake_up_notifier_worker();
    }

    std::exception_ptr error() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_error;
    }

    State state() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_state;
    }

private:
    void do_attach_to(SharedGroup&) override { }
    void do_detach_from(SharedGroup&) override { }

    void do_prepare_handover(SharedGroup&) override
    {
        add_changes(std::move(m_changes));
    }

    bool do_add_required_change_info(_impl::TransactionChangeInfo&) override { return false; }
    bool prepare_to_deliver() override { return m_has_results_to_deliver; }

    _impl::RealmCoordinator *m_coordinator;

    mutable std::mutex m_mutex;
    _impl::CollectionChangeBuilder m_changes;
    std::exception_ptr m_pending_error = nullptr;
    std::exception_ptr m_error = nullptr;
    bool m_has_results_to_deliver = false;

    State m_state = Creating;
    State m_pending_state = Creating;
};

Subscription subscribe(Results const& results, util::Optional<std::string> user_provided_name)
{
    auto realm = results.get_realm();

    auto sync_config = realm->config().sync_config;
    if (!sync_config || !sync_config->is_partial)
        throw std::logic_error("A partial sync query can only be registered in a partially synced Realm");

    auto query = results.get_query().get_description(); // Throws if the query cannot be serialized.
    query += " " + results.get_descriptor_ordering().get_description(results.get_query().get_table());

    std::string name = user_provided_name ? std::move(*user_provided_name)
                                          : default_name_for_query(query, results.get_object_type());

    Subscription subscription(name, results.get_object_type(), realm);
    std::weak_ptr<Subscription::Notifier> weak_notifier = subscription.m_notifier;
    enqueue_registration(*realm, results.get_object_type(), std::move(query), std::move(name),
                         [weak_notifier=std::move(weak_notifier)](std::exception_ptr error) {
        if (auto notifier = weak_notifier.lock())
            notifier->finished_subscribing(error);
    });
    return subscription;
}

void unsubscribe(Subscription& subscription)
{
    if (auto result_set_object = subscription.result_set_object()) {
        // The subscription has its result set object, so we can queue up the unsubscription immediately.
        std::weak_ptr<Subscription::Notifier> weak_notifier = subscription.m_notifier;
        enqueue_unregistration(*result_set_object, [weak_notifier=std::move(weak_notifier)]() {
            if (auto notifier = weak_notifier.lock())
                notifier->finished_unsubscribing();
        });
        return;
    }

    switch (subscription.state()) {
        case SubscriptionState::Creating: {
            // The result set object is in the process of being created. Try unsubscribing again once it exists.
            auto token = std::make_shared<SubscriptionNotificationToken>();
            *token = subscription.add_notification_callback([token, &subscription] () {
                if (subscription.state() == SubscriptionState::Creating)
                    return;

                unsubscribe(subscription);

                // Invalidate the notification token so we do not receive further callbacks.
                *token = SubscriptionNotificationToken();
            });
            return;
        }

        case SubscriptionState::Error:
            // We encountered an error when creating the subscription. There's nothing to remove, so just
            // mark the subscription as removed.
            subscription.m_notifier->finished_unsubscribing();
            break;

        case SubscriptionState::Invalidated:
            // Nothing to do. We have already removed the subscription.
            break;

        case SubscriptionState::Pending:
        case SubscriptionState::Complete:
            // This should not be reachable as these states require the result set object to exist.
            REALM_ASSERT(false);
            break;
    }
}

Subscription::Subscription(std::string name, std::string object_type, std::shared_ptr<Realm> realm)
: m_object_schema(realm->read_group(), result_sets_type_name)
{
    // FIXME: Why can't I do this in the initializer list?
    m_notifier = std::make_shared<Notifier>(realm);
    _impl::RealmCoordinator::register_notifier(m_notifier);

    auto matches_property = std::string(object_type) + "_matches";

    TableRef table = ObjectStore::table_for_object_type(realm->read_group(), result_sets_type_name);
    Query query = table->where();
    query.equal(m_object_schema.property_for_name("name")->table_column, name);
    query.equal(m_object_schema.property_for_name("matches_property")->table_column, matches_property);
    m_result_sets = Results(std::move(realm), std::move(query));
}

Subscription::~Subscription() = default;
Subscription::Subscription(Subscription&&) = default;
Subscription& Subscription::operator=(Subscription&&) = default;

SubscriptionNotificationToken Subscription::add_notification_callback(std::function<void ()> callback)
{
    auto result_sets_token = m_result_sets.add_notification_callback([callback] (CollectionChangeSet, std::exception_ptr) {
        callback();
    });
    NotificationToken registration_token(m_notifier, m_notifier->add_callback([callback] (CollectionChangeSet, std::exception_ptr) {
        callback();
    }));
    return SubscriptionNotificationToken{std::move(registration_token), std::move(result_sets_token)};
}

util::Optional<Object> Subscription::result_set_object() const
{
    if (m_notifier->state() == Notifier::Complete) {
        if (auto row = m_result_sets.first())
            return Object(m_result_sets.get_realm(), m_object_schema, *row);
    }

    return util::none;
}

SubscriptionState Subscription::state() const
{
    switch (m_notifier->state()) {
        case Notifier::Creating:
            return SubscriptionState::Creating;
        case Notifier::Removed:
            return SubscriptionState::Invalidated;
        case Notifier::Complete:
            break;
    }

    if (m_notifier->error())
        return SubscriptionState::Error;

    if (auto object = result_set_object()) {
        CppContext context;
        auto value = any_cast<int64_t>(object->get_property_value<util::Any>(context, "status"));
        return (SubscriptionState)value;
    }

    // We may not have an object even if the subscription has completed if the completion callback fired
    // but the result sets callback is yet to fire.
    return SubscriptionState::Creating;
}

std::exception_ptr Subscription::error() const
{
    if (auto error = m_notifier->error())
        return error;

    if (auto object = result_set_object()) {
        CppContext context;
        auto message = any_cast<std::string>(object->get_property_value<util::Any>(context, "error_message"));
        if (message.size())
            return make_exception_ptr(std::runtime_error(message));
    }

    return nullptr;
}

Results Subscription::results() const
{
    auto object = result_set_object();
    REALM_ASSERT_RELEASE(object);

    CppContext context;
    auto matches_property = any_cast<std::string>(object->get_property_value<util::Any>(context, "matches_property"));
    auto list = any_cast<List>(object->get_property_value<util::Any>(context, matches_property));
    return list.as_results();
}

} // namespace partial_sync
} // namespace realm
