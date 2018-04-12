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

#include "sync/sync_permission.hpp"

#include "impl/notification_wrapper.hpp"
#include "impl/object_accessor_impl.hpp"
#include "object_schema.hpp"
#include "property.hpp"

#include "sync/sync_config.hpp"
#include "sync/sync_manager.hpp"
#include "sync/sync_session.hpp"
#include "sync/sync_user.hpp"
#include "util/event_loop_signal.hpp"
#include "util/uuid.hpp"

#include <realm/query_expression.hpp>

using namespace realm;
using namespace std::chrono;

// MARK: - Utility

namespace {

// Make a handler that extracts either an exception pointer, or the string value
// of the property with the specified name.
Permissions::AsyncOperationHandler make_handler_extracting_property(std::string property,
                                                                    Permissions::PermissionOfferCallback callback)
{
    return [property=std::move(property),
            callback=std::move(callback)](Object* object, std::exception_ptr exception) {
        if (exception) {
            callback(none, exception);
        } else {
            CppContext context;
            auto token = any_cast<std::string>(object->get_property_value<util::Any>(context, property));
            callback(util::make_optional<std::string>(std::move(token)), nullptr);
        }
    };
}

AccessLevel extract_access_level(Object& permission, CppContext& context)
{
    auto may_manage = permission.get_property_value<util::Any>(context, "mayManage");
    if (may_manage.has_value() && any_cast<bool>(may_manage))
        return AccessLevel::Admin;

    auto may_write = permission.get_property_value<util::Any>(context, "mayWrite");
    if (may_write.has_value() && any_cast<bool>(may_write))
        return AccessLevel::Write;

    auto may_read = permission.get_property_value<util::Any>(context, "mayRead");
    if (may_read.has_value() && any_cast<bool>(may_read))
        return AccessLevel::Read;

    return AccessLevel::None;
}

/// Turn a system time point value into the 64-bit integer representing ns since the Unix epoch.
int64_t ns_since_unix_epoch(const system_clock::time_point& point)
{
    tm unix_epoch{};
    unix_epoch.tm_year = 70;
    time_t epoch_time = mktime(&unix_epoch);
    auto epoch_point = system_clock::from_time_t(epoch_time);
    return duration_cast<nanoseconds>(point - epoch_point).count();
}

} // anonymous namespace

// MARK: - Permission

Permission::Permission(Object& permission)
{
    CppContext context;
    path = any_cast<std::string>(permission.get_property_value<util::Any>(context, "path"));
    access = extract_access_level(permission, context);
    condition = Condition(any_cast<std::string>(permission.get_property_value<util::Any>(context, "userId")));
    updated_at = any_cast<Timestamp>(permission.get_property_value<util::Any>(context, "updatedAt"));
}

Permission::Permission(std::string path, AccessLevel access, Condition condition, Timestamp updated_at)
: path(std::move(path))
, access(access)
, condition(std::move(condition))
, updated_at(std::move(updated_at))
{ }

std::string Permission::description_for_access_level(AccessLevel level)
{
    switch (level) {
        case AccessLevel::None: return "none";
        case AccessLevel::Read: return "read";
        case AccessLevel::Write: return "write";
        case AccessLevel::Admin: return "admin";
    }
    REALM_UNREACHABLE();
}

bool Permission::paths_are_equivalent(std::string path_1, std::string path_2,
                                      const std::string& user_id_1, const std::string& user_id_2)
{
    REALM_ASSERT_DEBUG(path_1.length() > 0);
    REALM_ASSERT_DEBUG(path_2.length() > 0);
    if (path_1 == path_2) {
        // If both paths are identical and contain `/~/`, the user IDs must match.
        return (path_1.find("/~/") == std::string::npos) || (user_id_1 == user_id_2);
    }
    // Make substitutions for the first `/~/` in the string.
    size_t index = path_1.find("/~/");
    if (index != std::string::npos)
        path_1.replace(index + 1, 1, user_id_1);

    index = path_2.find("/~/");
    if (index != std::string::npos)
        path_2.replace(index + 1, 1, user_id_2);

    return path_1 == path_2;
}

// MARK: - Permissions

void Permissions::get_permissions(std::shared_ptr<SyncUser> user,
                                  PermissionResultsCallback callback,
                                  const ConfigMaker& make_config)
{
    auto realm = Permissions::permission_realm(user, make_config);
    auto table = ObjectStore::table_for_object_type(realm->read_group(), "Permission");
    auto results = std::make_shared<_impl::NotificationWrapper<Results>>(std::move(realm), *table);

    // `get_permissions` works by temporarily adding an async notifier to the permission Realm.
    // This notifier will run the `async` callback until the Realm contains permissions or
    // an error happens. When either of these two things happen, the notifier will be
    // unregistered by nulling out the `results_wrapper` container.
    auto async = [results, callback=std::move(callback)](CollectionChangeSet, std::exception_ptr ex) mutable {
        if (ex) {
            callback(Results(), ex);
            results.reset();
            return;
        }
        if (results->size() > 0) {
            // We monitor the raw results. The presence of a `__management` Realm indicates
            // that the permissions have been downloaded (hence, we wait until size > 0).
            TableRef table = ObjectStore::table_for_object_type(results->get_realm()->read_group(), "Permission");
            size_t col_idx = table->get_descriptor()->get_column_index("path");
            auto query = !(table->column<StringData>(col_idx).ends_with("/__permission")
                           || table->column<StringData>(col_idx).ends_with("/__perm")
                           || table->column<StringData>(col_idx).ends_with("/__management"));
            // Call the callback with our new permissions object. This object will exclude the
            // private Realms.
            callback(results->filter(std::move(query)), nullptr);
            results.reset();
        }
    };
    results->add_notification_callback(std::move(async));
}

void Permissions::set_permission(std::shared_ptr<SyncUser> user,
                                 Permission permission,
                                 PermissionChangeCallback callback,
                                 const ConfigMaker& make_config)
{
    auto props = AnyDict{
        {"userId", permission.condition.user_id},
        {"realmUrl", user->server_url() + permission.path},
        {"mayRead", permission.access != AccessLevel::None},
        {"mayWrite", permission.access == AccessLevel::Write || permission.access == AccessLevel::Admin},
        {"mayManage", permission.access == AccessLevel::Admin},
    };
    if (permission.condition.type == Permission::Condition::Type::KeyValue) {
        props.insert({"metadataKey", permission.condition.key_value.first});
        props.insert({"metadataValue", permission.condition.key_value.second});
    }
    auto cb = [callback=std::move(callback)](Object*, std::exception_ptr exception) {
        callback(exception);
    };
    perform_async_operation("PermissionChange", std::move(user), std::move(cb), std::move(props), make_config);
}

void Permissions::delete_permission(std::shared_ptr<SyncUser> user,
                                    Permission permission,
                                    PermissionChangeCallback callback,
                                    const ConfigMaker& make_config)
{
    permission.access = AccessLevel::None;
    set_permission(std::move(user), std::move(permission), std::move(callback), make_config);
}

void Permissions::make_offer(std::shared_ptr<SyncUser> user,
                             PermissionOffer offer,
                             PermissionOfferCallback callback,
                             const ConfigMaker& make_config)
{
    auto props = AnyDict{
        {"expiresAt", std::move(offer.expiration)},
        {"userId", user->identity()},
        {"realmUrl", user->server_url() + offer.path},
        {"mayRead", offer.access != AccessLevel::None},
        {"mayWrite", offer.access == AccessLevel::Write || offer.access == AccessLevel::Admin},
        {"mayManage", offer.access == AccessLevel::Admin},
    };
    perform_async_operation("PermissionOffer",
                            std::move(user),
                            make_handler_extracting_property("token", std::move(callback)),
                            std::move(props),
                            make_config);
}

void Permissions::accept_offer(std::shared_ptr<SyncUser> user,
                               const std::string& token,
                               PermissionOfferCallback callback,
                               const ConfigMaker& make_config)
{
    perform_async_operation("PermissionOfferResponse",
                            std::move(user),
                            make_handler_extracting_property("realmUrl", std::move(callback)),
                            AnyDict{ {"token", token} },
                            make_config);
}

void Permissions::perform_async_operation(const std::string& object_type,
                                          std::shared_ptr<SyncUser> user,
                                          AsyncOperationHandler handler,
                                          AnyDict additional_props,
                                          const ConfigMaker& make_config)
{;
    auto realm = Permissions::management_realm(std::move(user), make_config);
    CppContext context;

    // Get the current time.
    int64_t ns_since_epoch = ns_since_unix_epoch(system_clock::now());
    int64_t s_arg = ns_since_epoch / (int64_t)Timestamp::nanoseconds_per_second;
    int32_t ns_arg = ns_since_epoch % Timestamp::nanoseconds_per_second;

    auto props = AnyDict{
        {"id", util::uuid_string()},
        {"createdAt", Timestamp(s_arg, ns_arg)},
        {"updatedAt", Timestamp(s_arg, ns_arg)},
    };
    props.insert(additional_props.begin(), additional_props.end());

    // Write the permission object.
    realm->begin_transaction();
    auto raw = Object::create<util::Any>(context, realm, *realm->schema().find(object_type), std::move(props), false);
    auto object = std::make_shared<_impl::NotificationWrapper<Object>>(std::move(raw));
    realm->commit_transaction();

    // Observe the permission object until the permission change has been processed or failed.
    // The notifier is automatically unregistered upon the completion of the permission
    // change, one way or another.
    auto block = [object, handler=std::move(handler)](CollectionChangeSet, std::exception_ptr ex) mutable {
        if (ex) {
            handler(nullptr, ex);
            object.reset();
            return;
        }

        CppContext context;
        auto status_code = object->get_property_value<util::Any>(context, "statusCode");
        if (!status_code.has_value()) {
            // Continue waiting for the sync server to complete the operation.
            return;
        }

        // Determine whether an error happened or not.
        if (auto code = any_cast<long long>(status_code)) {
            // The permission change failed because an error was returned from the server.
            auto status = object->get_property_value<util::Any>(context, "statusMessage");
            std::string error_str = (status.has_value()
                                     ? any_cast<std::string>(status)
                                     : util::format("Error code: %1", code));
            handler(nullptr, std::make_exception_ptr(PermissionActionException(error_str, code)));
        }
        else {
            handler(object.get(), nullptr);
        }
        object.reset();
    };
    object->add_notification_callback(std::move(block));
}

SharedRealm Permissions::management_realm(std::shared_ptr<SyncUser> user, const ConfigMaker& make_config)
{
    // FIXME: maybe we should cache the management Realm on the user, so we don't need to open it every time.
    const auto realm_url = util::format("realm%1/~/__management", user->server_url().substr(4));
    Realm::Config config = make_config(user, std::move(realm_url));
    config.sync_config->stop_policy = SyncSessionStopPolicy::Immediately;
    config.schema = Schema{
        {"PermissionChange", {
            Property{"id",                PropertyType::String, Property::IsPrimary{true}},
            Property{"createdAt",         PropertyType::Date},
            Property{"updatedAt",         PropertyType::Date},
            Property{"statusCode",        PropertyType::Int|PropertyType::Nullable},
            Property{"statusMessage",     PropertyType::String|PropertyType::Nullable},
            Property{"userId",            PropertyType::String},
            Property{"metadataKey",       PropertyType::String|PropertyType::Nullable},
            Property{"metadataValue",     PropertyType::String|PropertyType::Nullable},
            Property{"metadataNameSpace", PropertyType::String|PropertyType::Nullable},
            Property{"realmUrl",          PropertyType::String},
            Property{"mayRead",           PropertyType::Bool|PropertyType::Nullable},
            Property{"mayWrite",          PropertyType::Bool|PropertyType::Nullable},
            Property{"mayManage",         PropertyType::Bool|PropertyType::Nullable},
        }},
        {"PermissionOffer", {
            Property{"id",                PropertyType::String, Property::IsPrimary{true}},
            Property{"createdAt",         PropertyType::Date},
            Property{"updatedAt",         PropertyType::Date},
            Property{"expiresAt",         PropertyType::Date|PropertyType::Nullable},
            Property{"statusCode",        PropertyType::Int|PropertyType::Nullable},
            Property{"statusMessage",     PropertyType::String|PropertyType::Nullable},
            Property{"token",             PropertyType::String|PropertyType::Nullable},
            Property{"realmUrl",          PropertyType::String},
            Property{"mayRead",           PropertyType::Bool},
            Property{"mayWrite",          PropertyType::Bool},
            Property{"mayManage",         PropertyType::Bool},
        }},
        {"PermissionOfferResponse", {
            Property{"id",                PropertyType::String, Property::IsPrimary{true}},
            Property{"createdAt",         PropertyType::Date},
            Property{"updatedAt",         PropertyType::Date},
            Property{"statusCode",        PropertyType::Int|PropertyType::Nullable},
            Property{"statusMessage",     PropertyType::String|PropertyType::Nullable},
            Property{"token",             PropertyType::String},
            Property{"realmUrl",          PropertyType::String|PropertyType::Nullable},
        }},
    };
    config.schema_version = 0;
    auto shared_realm = Realm::get_shared_realm(std::move(config));
    user->register_management_session(shared_realm->config().path);
    return shared_realm;
}

SharedRealm Permissions::permission_realm(std::shared_ptr<SyncUser> user, const ConfigMaker& make_config)
{
    // FIXME: maybe we should cache the permission Realm on the user, so we don't need to open it every time.
    const auto realm_url = util::format("realm%1/~/__permission", user->server_url().substr(4));
    Realm::Config config = make_config(user, std::move(realm_url));
    config.sync_config->stop_policy = SyncSessionStopPolicy::Immediately;
    config.schema = Schema{
        {"Permission", {
            {"updatedAt", PropertyType::Date},
            {"userId", PropertyType::String},
            {"path", PropertyType::String},
            {"mayRead", PropertyType::Bool},
            {"mayWrite", PropertyType::Bool},
            {"mayManage", PropertyType::Bool},
        }}
    };
    config.schema_version = 0;
    auto shared_realm = Realm::get_shared_realm(std::move(config));
    user->register_permission_session(shared_realm->config().path);
    return shared_realm;
}
