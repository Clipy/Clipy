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

#ifndef REALM_OS_SYNC_PERMISSION_HPP
#define REALM_OS_SYNC_PERMISSION_HPP

#include "results.hpp"
#include "shared_realm.hpp"

namespace realm {

class Permissions;
class SyncUser;
class Object;

namespace util {
    class Any;
}

// A permission encapsulates a single access level.
// Each level includes all the capabilities of the level
// above it (for example, 'write' implies 'read').
enum class AccessLevel {
    None,
    Read,
    Write,
    Admin,
};

// Permission object used to represent a user permission.
// Permission objects can be passed into or returned by various permissions
// APIs. They are immutable objects.
struct Permission {
    // The path of the Realm to which this permission pertains.
    std::string path;

    AccessLevel access;

    // Return the string description of an `AccessLevel`.
    static std::string description_for_access_level(AccessLevel level);

    // Return whether two paths are equivalent: either because they are exactly
    // equal, or because user ID subtitution of one tilde-delimited path results
    // in a path identical to the other path.
    // Warning: this method does NOT strip or add leading or trailing slashes or whitespace.
    // For example: "/~/foo" is equivalent to "/~/foo"; "/1/foo" is equivalent to "/1/foo".
    // "/~/foo" is equivalent to "/1/foo" for a user ID of 1.
    static bool paths_are_equivalent(std::string path_1, std::string path_2,
                                     const std::string& user_id_1, const std::string& user_id_2);

    // Condition is a userId or a KeyValue pair
    // Other conditions may be supported in the future
    struct Condition {
        enum class Type {
            // The permission is applied to a single user based on their user ID
            UserId,
            // The permission is based on any user that meets a criterion specified by key/value.
            KeyValue,
        };
        Type type;

        // FIXME: turn this back into a union type
        std::string user_id;
        std::pair<std::string, std::string> key_value;

        Condition() {}

        Condition(std::string id)
        : type(Type::UserId)
        , user_id(std::move(id))
        { }

        Condition(std::string key, std::string value)
        : type(Type::KeyValue)
        , key_value(std::make_pair(std::move(key), std::move(value)))
        { }
    };
    Condition condition;

    Timestamp updated_at;

    /// Create a Permission value from an `Object`.
    Permission(Object&);

    /// Create a Permission value from raw values.
    Permission(std::string path, AccessLevel, Condition, Timestamp updated_at=Timestamp());
};

struct PermissionOffer {
    std::string path;
    AccessLevel access;
    Timestamp expiration;
};

class Permissions {
public:
    // Consumers of these APIs need to pass in a method which creates a Config with the proper
    // SyncConfig and associated callbacks, as well as the path and other parameters.
    using ConfigMaker = std::function<Realm::Config(std::shared_ptr<SyncUser>, std::string url)>;

    // Callback used to asynchronously vend permissions results.
    using PermissionResultsCallback = std::function<void(Results, std::exception_ptr)>;

    // Callback used to asynchronously vend permission offer or response URL.
    using PermissionOfferCallback = std::function<void(util::Optional<std::string>, std::exception_ptr)>;

    // Asynchronously retrieve a `Results` containing the permissions for the provided user.
    static void get_permissions(std::shared_ptr<SyncUser>, PermissionResultsCallback, const ConfigMaker&);

    // Callback used to monitor success or errors when changing permissions
    // or accepting a permission offer.
    // `exception_ptr` is null_ptr on success
    using PermissionChangeCallback = std::function<void(std::exception_ptr)>;

    // Set a permission as the provided user.
    static void set_permission(std::shared_ptr<SyncUser>, Permission, PermissionChangeCallback, const ConfigMaker&);

    // Delete a permission as the provided user.
    static void delete_permission(std::shared_ptr<SyncUser>, Permission, PermissionChangeCallback, const ConfigMaker&);

    // Create a permission offer. The callback will be passed the token, if successful.
    static void make_offer(std::shared_ptr<SyncUser>, PermissionOffer, PermissionOfferCallback, const ConfigMaker&);

    // Accept a permission offer based on the token value within the offer.
    static void accept_offer(std::shared_ptr<SyncUser>, const std::string&, PermissionOfferCallback, const ConfigMaker&);

    using AsyncOperationHandler = std::function<void(Object*, std::exception_ptr)>;

private:
    static SharedRealm management_realm(std::shared_ptr<SyncUser>, const ConfigMaker&);
    static SharedRealm permission_realm(std::shared_ptr<SyncUser>, const ConfigMaker&);

    /**
     Perform an asynchronous operation that involves writing an object to the
     user's management Realm, and then waiting for the operation to succeed or
     fail.

     The object in question must have at least `id`, `createdAt`, and `updatedAt`,
     properties to be set as part of the request, and it must report its success
     or failure by setting its `statusCode` and `statusMessage` properties.

     The callback is invoked upon success or failure, and will be called with
     exactly one of its two arguments not set to null. The object can be used to
     extract additional data to be returned to the caller.
     */
    static void perform_async_operation(const std::string& object_type,
                                        std::shared_ptr<SyncUser>,
                                        AsyncOperationHandler,
                                        std::map<std::string, util::Any>,
                                        const ConfigMaker&);
};

struct PermissionActionException : std::runtime_error {
    long long code;

    PermissionActionException(std::string message, long long code)
    : std::runtime_error(std::move(message))
    , code(code)
    { }
};

}

#endif /* REALM_OS_SYNC_PERMISSION_HPP */
