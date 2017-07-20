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

// Permission object used to represent a user permission.
// Permission objects can be passed into or returned by various permissions
// APIs. They are immutable objects.
struct Permission {
    // The path of the Realm to which this permission pertains.
    std::string path;

    // A permission encapsulates a single access level.
    // Each level includes all the capabilities of the level
    // above it (for example, 'write' implies 'read').
    enum class AccessLevel {
        None,
        Read,
        Write,
        Admin,
    };
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
};

class PermissionResults {
public:
    // The number of permissions represented by this PermissionResults.
    size_t size()
    {
        return m_results.size();
    }

    // Get the permission value at the given index.
    // Throws an `OutOfBoundsIndexException` if the index is invalid.
    Permission get(size_t index);

    // Create an async query from this Results.
    // The query will be run on a background thread and delivered to the callback,
    // and then rerun after each commit (if needed) and redelivered if it changed
    NotificationToken async(std::function<void(std::exception_ptr)> target)
    {
        return m_results.async(std::move(target));
    }

    // Create a new instance by further filtering this instance.
    PermissionResults filter(Query&& q) const
    {
        return PermissionResults(m_results.filter(std::move(q)));
    }

    // Create a new instance by sorting this instance.
    PermissionResults sort(SortDescriptor&& s) const
    {
        return PermissionResults(m_results.sort(std::move(s)));
    }

    // Get the results.
    Results& results()
    {
        return m_results;
    }

    // Don't use this constructor directly. Publicly exposed so `make_unique` can see it.
    PermissionResults(Results&& results)
    : m_results(results)
    { }

protected:
    Results m_results;
};

class Permissions {
public:
    // Consumers of these APIs need to pass in a method which creates a Config with the proper
    // SyncConfig and associated callbacks, as well as the path and other parameters.
    using ConfigMaker = std::function<Realm::Config(std::shared_ptr<SyncUser>, std::string url)>;

    // Callback used to asynchronously vend a `PermissionResults` object.
    using PermissionResultsCallback = std::function<void(std::unique_ptr<PermissionResults>, std::exception_ptr)>;

    // Asynchronously retrieve the permissions for the provided user.
    static void get_permissions(std::shared_ptr<SyncUser>, PermissionResultsCallback, const ConfigMaker&);

    // Callback used to monitor success or errors when changing permissions
    // `exception_ptr` is null_ptr on success
    using PermissionChangeCallback = std::function<void(std::exception_ptr)>;

    // Set a permission as the provided user.
    static void set_permission(std::shared_ptr<SyncUser>, Permission, PermissionChangeCallback, const ConfigMaker&);

    // Delete a permission as the provided user.
    static void delete_permission(std::shared_ptr<SyncUser>, Permission, PermissionChangeCallback, const ConfigMaker&);

private:
    static SharedRealm management_realm(std::shared_ptr<SyncUser>, const ConfigMaker&);
    static SharedRealm permission_realm(std::shared_ptr<SyncUser>, const ConfigMaker&);
};

struct PermissionChangeException : std::runtime_error {
    long long code;

    PermissionChangeException(std::string message, long long code)
    : std::runtime_error(std::move(message))
    , code(code)
    { }
};

}

#endif /* REALM_OS_SYNC_PERMISSION_HPP */
