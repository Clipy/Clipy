/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2015] Realm Inc
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of Realm Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Realm Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Realm Incorporated.
 *
 **************************************************************************/

#ifndef REALM_SYNC_PERMISSIONS_HPP
#define REALM_SYNC_PERMISSIONS_HPP

#include <iosfwd>

#include <realm/sync/instruction_applier.hpp>
#include <realm/sync/object_id.hpp>
#include <realm/sync/object.hpp>
#include <realm/util/metered/map.hpp>
#include <realm/util/metered/set.hpp>
#include <realm/util/metered/string.hpp>

#include <realm/table_view.hpp>
#include <realm/obj.hpp>

namespace realm {
namespace sync {

/// Permissions Schema:
///
/// class___Role:
///     string name PRIMARY_KEY;
///     User[] members;
///
/// class___Permission:
///     __Role role;
///     bool canRead;
///     bool canUpdate;
///     bool canDelete;
///     bool canSetPermissions;
///     bool canQuery;
///     bool canCreate;
///     bool canModifySchema;
///
/// class___Realm:
///     int id PRIMARY_KEY = 0; // singleton object
///     __Permission[] permissions;
///
/// class___User:
///     string id PRIMARY_KEY;
///     __Role role;
///
/// class___Class:
///     string name PRIMARY_KEY;
///     __Permission[] permissions;
///
/// class_<ANYTHING>:
///     __Permission[] <user-chosen name>;
///     __Role <resource-role>;
///

static constexpr char g_roles_table_name[] = "class___Role";
static constexpr char g_permissions_table_name[] = "class___Permission";
static constexpr char g_users_table_name[] = "class___User";
static constexpr char g_classes_table_name[] = "class___Class";
static constexpr char g_realms_table_name[] = "class___Realm";


/// Create the permissions schema if it doesn't already exist.
void create_permissions_schema(Transaction&);

/// Set up the basic "everyone" role and default permissions. The default is to
/// set up some very permissive defaults, where "everyone" can do everything.
void set_up_basic_permissions(Transaction&, TableInfoCache& table_info_cache, bool permissive = true);
// Convenience function that creates a new TableInfoCache.
void set_up_basic_permissions(Transaction&, bool permissive = true);

/// Set up some basic permissions for the class. The default is to set up some
/// very permissive default, where "everyone" can do everything in the class.
void set_up_basic_permissions_for_class(Transaction&, StringData class_name, bool permissive = true);
// void set_up_basic_default_permissions_for_class(Group&, TableRef klass, bool permissive = true);

/// Return the index of the ACL in the class, if one exists. If no ACL column is
/// defined in the class, returns `npos`.
ColKey find_permissions_column(const Transaction&, ConstTableRef);

//@{
/// Convenience functions to check permisions data
/// The functions must be called inside a read (or write) transaction.
bool permissions_schema_exist(const Transaction&);

bool user_exist(const Transaction&, StringData user_id);
//@}


/// Perform a query as user \a user_id, returning only the results that the
/// user has access to read. If the user is an admin, there is no need to call
/// this function, since admins can always read everything.
///
/// If the target table of the query does not have object-level permissions,
/// the query results will be returned without any additional filtering.
///
/// If the target table of the query has object-level permissions, but the
/// permissions schema of this Realm is invalid, an exception of type
/// `InvalidPermissionsSchema` is thrown.
///
/// LIMIT and DISTINCT will be applied *after* permission filters.
///
/// The resulting TableView can be used like any other query result.
///
/// Note: Class-level and Realm-level permissions are not taken into account in
/// the resulting TableView, since there is no way to represent this in the
/// query engine.
ConstTableView query_with_permissions(Query query, StringData user_id,
                                      const DescriptorOrdering* ordering = nullptr);

struct InvalidPermissionsSchema : util::runtime_error {
    using util::runtime_error::runtime_error;
};

//@{
/// Convenience function to modify permission data.
///
/// When a role or user has not already been defined in the Realm, these
/// functions create them on-demand.
void set_realm_permissions_for_role(Transaction&, StringData role_name,
                                    uint_least32_t privileges);
void set_realm_permissions_for_role(Transaction&, ObjKey role,
                                    uint_least32_t privileges);
void set_class_permissions_for_role(Transaction&, StringData class_name,
                                    StringData role_name, uint_least32_t privileges);
void set_class_permissions_for_role(Transaction&, StringData class_name, ObjKey role_key,
                                    uint_least32_t privileges);
// void set_default_object_permissions_for_role(Group&, StringData class_name,
//                                              StringData role_name,
//                                              uint_least32_t privileges);
void set_object_permissions_for_role(Transaction&, TableRef table, Obj& object,
                                     StringData role_name, uint_least32_t privileges);
void set_object_permissions_for_role(Transaction&, TableRef table, Obj& object, ObjKey role,
                                     uint_least32_t privileges);

void add_user_to_role(Transaction&, StringData user_id, StringData role_name);
void add_user_to_role(Transaction&, Obj user, ObjKey role);
//@}

/// The Privilege enum is intended to be used in a bitfield.
enum class Privilege : uint_least32_t {
    None = 0,

    /// The user can read the object (i.e. it can participate in the user's
    /// subscription.
    ///
    /// NOTE: On objects, it is a prerequisite that the object's class is also
    /// readable by the user.
    ///
    /// FIXME: Until we get asynchronous links, any object that is reachable
    /// through links from another readable/queryable object is also readable,
    /// regardless of whether the user specifically does not have read access.
    Read = 1,

    /// The user can modify the fields of the object.
    ///
    /// NOTE: On objects, it is a prerequisite that the object's class is also
    /// updatable by the user. When applied to a Class object, it does not
    /// imply that the user can modify the schema of the class, only the
    /// objects of that class.
    ///
    /// NOTE: This does not imply the SetPermissions privilege.
    Update = 2,

    /// The user can delete the object.
    ///
    /// NOTE: When applied to a Class object, it has no effect on whether
    /// objects of that class can be deleted by the user.
    ///
    /// NOTE: This implies the ability to implicitly nullify links pointing
    /// to the object from other objects, even if the user does not have
    /// permission to modify those objects in the normal way.
    Delete = 4,

    //@{
    /// The user can modify the object's permissions.
    ///
    /// NOTE: The user will only be allowed to assign permissions at or below
    /// their own privilege level.
    SetPermissions = 8,
    Share = SetPermissions,
    //@}

    /// When applied to a Class object, the user can query objects in that
    /// class.
    ///
    /// Has no effect when applied to objects other than Class.
    Query = 16,

    /// When applied to a Class object, the user may create objects in that
    /// class.
    ///
    /// NOTE: The user implicitly has Update and SetPermissions
    /// (but not necessarily Delete permission) within the same
    /// transaction as the object was created.
    ///
    /// NOTE: Even when a user has CreateObject rights, a CreateObject
    /// operation may still be rejected by the server, if the object has a
    /// primary key and the object already exists, but is not accessible by the
    /// user.
    Create = 32,

    /// When applied as a "Realm" privilege, the user can add classes and add
    /// columns to classes.
    ///
    /// NOTE: When applied to a class or object, this has no effect.
    ModifySchema = 64,

    ///
    /// Aggregate permissions for compatibility:
    ///
    Download = Read | Query,
    Upload = Update | Delete | Create,
    DeleteRealm = Upload, // FIXME: This seems overly permissive
};

inline constexpr uint_least32_t operator|(Privilege a, Privilege b)
{
    return static_cast<uint_least32_t>(a) | static_cast<uint_least32_t>(b);
}

inline constexpr uint_least32_t operator|(uint_least32_t a, Privilege b)
{
    return a | static_cast<uint_least32_t>(b);
}

inline constexpr uint_least32_t operator&(Privilege a, Privilege b)
{
    return static_cast<uint_least32_t>(a) & static_cast<uint_least32_t>(b);
}

inline constexpr uint_least32_t operator&(uint_least32_t a, Privilege b)
{
    return a & static_cast<uint_least32_t>(b);
}

inline uint_least32_t& operator|=(uint_least32_t& a, Privilege b)
{
    return a |= static_cast<uint_least32_t>(b);
}

inline constexpr uint_least32_t operator~(Privilege p)
{
    return ~static_cast<uint_least32_t>(p);
}

struct PermissionsCache {
    /// Each element is the index of a row in the `class___Roles` table.
    using RoleList = std::vector<ObjKey>;

    PermissionsCache(const Transaction& g, TableInfoCache& table_info_cache, StringData user_identity, bool is_admin = false);

    bool is_admin() const noexcept;

    /// Leaves out any role that has no permission objects linking to it.
    RoleList get_users_list_of_roles();

    /// Get Realm-level privileges for the current user.
    ///
    /// The user must have Read access at the Realm level to be able to see
    /// anything in the file.
    ///
    /// The user must have Update access at the Realm level to be able to make
    /// any changes at all in the Realm file.
    ///
    /// If no Realm-level permissions are defined, no access is granted for any
    /// user.
    uint_least32_t get_realm_privileges();

    /// Get class-level privileges for the current user and the given class.
    ///
    /// If the class does not have any class-level privileges defined, no access
    /// is granted to the class.
    ///
    /// Calling this function is equivalent to calling `get_object_privileges()`
    /// with an object of the type `__Class`.
    ///
    /// NOTE: This function only considers class-level permissions. It does not
    /// mask the returned value by the Realm-level permissions. See `can()`.
    uint_least32_t get_class_privileges(StringData class_name);

    /// Get object-level privileges for the current user and the given object.
    ///
    /// If the object's class has an ACL property (a linklist to the
    /// `__Permission` class), and it isn't empty, the user's privileges is the
    /// OR'ed privileges for the intersection of roles that have a defined
    /// permission on the object and the roles of which the user is a member.
    ///
    /// If the object's ACL property is empty (but the column exists), no access
    /// is granted to anyone.
    ///
    /// If the object does not exist in the table, the returned value is
    /// equivalent to that of an object with an empty ACL property, i.e. no
    /// privileges are granted. Note that the existence of the column is checked
    /// first, so an absent ACL property (granting all privileges) takes
    /// precedence over an absent object (granting no privileges) in terms of
    /// calculating permissions.
    ///
    /// NOTE: This function only considers object-level permissions (per-object
    /// ACLs or default object permissions). It does not mask the returned value
    /// by the object's class-level permissions, or by the Realm-level
    /// permissions. See `can()`.
    uint_least32_t get_object_privileges(GlobalID);

    /// Get object-level privileges without adding it to the cache.
    uint_least32_t get_object_privileges_nocache(GlobalID);

    //@{
    /// Check permissions for the object, taking all levels of permission into
    /// account.
    ///
    /// This method only returns `true` if the user has Realm-level access to
    /// the object, class-level access to the object, and object-level access to
    /// the object.
    ///
    /// In the version where the first argument is a mask of privileges, the
    /// method only returns `true` when all privileges are satisfied.
    bool can(Privilege privilege, GlobalID object_id);
    bool can(uint_least32_t privileges, GlobalID object_id);
    //@}

    /// Invalidate all cache entries pertaining to the object.
    ///
    /// The object may be an instance of `__Class`.
    void object_permissions_modified(GlobalID);

    /// Register the object as created in this transaction, meaning that the
    /// user gets full privileges until the end of the transaction.
    void object_created(GlobalID);

    /// Invalidate all cache entries pertaining to the class.
    // void default_object_permissions_modified(StringData class_name);

    /// Invalidate all cached permissions.
    void clear();

    /// Check that all cache permissions correspond to the current permission
    /// state in the database.
    void verify();

private:
    const Transaction& group;
    TableInfoCache& m_table_info_cache;
    std::string user_id;
    bool m_is_admin;
    util::Optional<uint_least32_t> realm_privileges;
    util::metered::map<GlobalID, uint_least32_t> object_privileges;
    ObjectIDSet created_objects;

    // uint_least32_t get_default_object_privileges(ConstTableRef);
    uint_least32_t get_privileges_for_permissions(const ConstLnkLst&);
    friend struct InstructionApplierWithPermissionCheck;
};

inline bool PermissionsCache::is_admin() const noexcept
{
    return m_is_admin;
}

/// PermissionCorrections is a struct that describes some changes that must be
/// sent to the client because the client tried to perform changes to a database
/// that it wasn't allowed to make.
struct PermissionCorrections {
    using TableColumnSet = util::metered::map<std::string, util::metered::set<std::string>>;
    using TableSet = util::metered::set<std::string>;

    // Objects that a client tried to delete without being allowed.
    ObjectIDSet recreate_objects;

    // Objects that a client tried to create without being allowed.
    ObjectIDSet erase_objects;

    // Fields that were illegally modified by the client and must be reset.
    //
    // Objects mentioned in `recreate_objects` and `erase_objects` are not
    // mentioned here.
    FieldSet reset_fields;

    // Columns that were illegally added by the client.
    TableColumnSet erase_columns;

    // Columns that were illegally removed by the client.
    TableColumnSet recreate_columns;

    // Tables that were illegally added by the client.
    // std::set<StringData> erase_tables;
    TableSet erase_tables;

    // Tables that were illegally removed by the client.
    TableSet recreate_tables;

    bool empty() const noexcept;
};

// Function for printing out a permission correction object. Useful for debugging purposes.
std::ostream& operator<<(std::ostream&, const PermissionCorrections&);



/// InstructionApplierWithPermissionCheck conditionally applies each
/// instruction, and builds a `PermissionCorrections` struct based on the
/// illicit changes. The member `m_corrections` can be used to synthesize a
/// changeset that can be sent to the client to revert the illicit changes that
/// were detected by the applier.
struct InstructionApplierWithPermissionCheck {
    explicit InstructionApplierWithPermissionCheck(Transaction& reference_realm,
                                                   bool is_admin,
                                                   StringData user_identity);
    ~InstructionApplierWithPermissionCheck();

    /// Apply \a incoming_changeset, checking permissions in the process.
    /// Populates `m_corrections`.
    void apply(const Changeset& incoming_changeset, util::Logger*);

    PermissionCorrections m_corrections;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};


// Implementation:

inline bool PermissionCorrections::empty() const noexcept
{
    return recreate_objects.empty() && erase_objects.empty()
        && reset_fields.empty() && erase_columns.empty()
        && recreate_columns.empty() && erase_tables.empty()
        && recreate_tables.empty();
}

} // namespace sync
} // namespace realm


#endif // REALM_SYNC_PERMISSIONS_HPP
