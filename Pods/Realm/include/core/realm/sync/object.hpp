/*************************************************************************
 *
 * Copyright 2017 Realm Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 **************************************************************************/

#ifndef REALM_SYNC_OBJECT_HPP
#define REALM_SYNC_OBJECT_HPP

#include <realm/util/logger.hpp>
#include <realm/table_ref.hpp>
#include <realm/string_data.hpp>

#include <realm/sync/object_id.hpp>

#include <vector>

/// This file presents a convenience API for making changes to a Realm file that
/// adhere to the conventions of assigning stable IDs to every object.

namespace realm {

class Group;

namespace sync {

class SyncHistory;

static const char object_id_column_name[] = "!OID";
static const char array_value_column_name[] = "!ARRAY_VALUE"; // FIXME call Jorgen

struct TableInfoCache;

/// Determine whether the Group has a sync-type history, and therefore whether
/// it supports globally stable object IDs.
///
/// The Group does not need to be in a transaction.
bool has_object_ids(const Group&);

/// Determine whether object IDs for objects without primary keys are globally
/// stable. This is true if and only if the Group has been in touch with the
/// server (or is the server), and will remain true forever thereafter.
///
/// It is an error to call this function for groups that do not have object IDs
/// (i.e. where `has_object_ids()` returns false).
///
/// The Group is assumed to be in a read transaction.
bool is_object_id_stability_achieved(const Group&);

/// Create a table with an object ID column.
///
/// It is an error to add tables to Groups with a sync history type directly.
/// This function or related functions must be used instead.
///
/// The resulting table will be born with 1 column, which is a column used
/// in the maintenance of object IDs.
///
/// NOTE: The table name must begin with the prefix "class_" in accordance with
/// Object Store conventions.
///
/// The Group must be in a write transaction.
TableRef create_table(Group&, StringData name);

/// Create a table with an object ID column and a primary key column.
///
/// It is an error to add tables to Groups with a sync history type directly.
/// This function or related functions must be used instead.
///
/// The resulting table will be born with 2 columns, which is a column used
/// in the maintenance of object IDs and the requested primary key column.
/// The primary key column must have either integer or string type, and it
/// will be given the name provided in the argument \a pk_column_name.
///
/// The 'pk' metadata table is updated with information about the primary key
/// column. If the 'pk' table does not yet exist, it is created.
///
/// Please note: The 'pk' metadata table will not be synchronized directly,
/// so subsequent updates to it will be lost (as they constitute schema-breaking
/// changes).
///
/// NOTE: The table name must begin with the prefix "class_" in accordance with
/// Object Store conventions.
///
/// The Group must be in a write transaction.
TableRef create_table_with_primary_key(Group&, StringData name, DataType pk_type,
                                       StringData pk_column_name, bool nullable = false);

/// Create an array column with the specified element type.
///
/// The result will be a column of type type_Table with one subcolumn named
/// "!ARRAY_VALUE" of the specified element type.
///
/// Return the column index of the inserted array column.
size_t add_array_column(Table&, DataType element_type, StringData column_name);


//@{
/// Calculate the object ID from the argument, where the argument is a primary
/// key value.
ObjectID object_id_for_primary_key(StringData);
ObjectID object_id_for_primary_key(util::Optional<int64_t>);
//@}

/// Determine whether it is safe to call `object_id_for_row()` on tables without
/// primary keys. If the table has a primary key, always returns true.
bool has_globally_stable_object_ids(const Table&);

bool table_has_primary_key(const TableInfoCache&, const Table&);

/// Get the globally unique object ID for the row.
///
/// If the table has a primary key, this is guaranteed to succeed. Otherwise, if
/// the server has not been contacted yet (`has_globally_stable_object_ids()`
/// returns false), an exception is thrown.
ObjectID object_id_for_row(const TableInfoCache&, const Table&, size_t);

/// Get the index of the row with the object ID.
///
/// \returns realm::npos if the object does not exist in the table.
size_t row_for_object_id(const TableInfoCache&, const Table&, ObjectID);

//@{
/// Add a row to the table and populate the object ID with an appropriate value.
///
/// In the variant which takes an ObjectID parameter, a check is performed to see
/// if the object already exists. If it does, the row index of the existing object
/// is returned.
///
/// If the table has a primary key column, an exception is thrown.
///
/// \returns the row index of the object.
size_t create_object(const TableInfoCache&, Table&);
size_t create_object(const TableInfoCache&, Table&, ObjectID);
//@}

//@{
/// Create an object with a primary key value and populate the object ID with an
/// appropriate value.
///
/// If the table does not have a primary key column (as indicated by the Object
/// Store's metadata in the special "pk" table), or the type of the primary key
/// column does not match the argument provided, an exception is thrown.
///
/// The primary key column's value is populated with the appropriate
/// `set_int_unique()`, `set_string_unique()`, or `set_null_unique()` method
/// called on \a table.
///
/// If an object with the given primary key value already exists, its row number
/// is returned without creating any new objects.
///
/// These are convenience functions, equivalent to the following:
///   - Add an empty row to the table.
///   - Obtain an `ObjectID` with `object_id_for_primary_key()`.
///   - Obtain a local object ID with `global_to_local_object_id()`.
///   - Store the local object ID in the object ID column.
///   - Call `set_int_unique()`,`set_string_unique()`, or `set_null_unique()`
///     to set the primary key value.
///
/// \returns the row index of the created object.
size_t create_object_with_primary_key(const TableInfoCache&, Table&, util::Optional<int64_t> primary_key);
size_t create_object_with_primary_key(const TableInfoCache&, Table&, StringData primary_key);
//@}

struct TableInfoCache {
    const Group& m_group;

    // Implicit conversion deliberately allowed for the purpose of calling the above
    // functions without constructing a cache manually.
    TableInfoCache(const Group&);
    TableInfoCache(TableInfoCache&&) noexcept = default;

    struct TableInfo {
        struct VTable;

        StringData name;
        const VTable* vtable;
        size_t object_id_index;
        size_t primary_key_index;
        DataType primary_key_type = DataType(-1);
        bool primary_key_nullable = false;
    };

    mutable std::vector<util::Optional<TableInfo>> m_table_info;

    const TableInfo& get_table_info(const Table&) const;
    const TableInfo& get_table_info(size_t table_index) const;
    void clear();
};


/// Migrate a server-side Realm file whose history type is
/// `Replication::hist_SyncServer` and whose history schema version is 0 (i.e.,
/// Realm files without stable identifiers).
void import_from_legacy_format(const Group& old_group, Group& new_group, util::Logger&);

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_OBJECT_HPP

