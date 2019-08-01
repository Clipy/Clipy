/*************************************************************************
 *
 * Copyright 2016 Realm Inc.
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

#ifndef REALM_EXCEPTIONS_HPP
#define REALM_EXCEPTIONS_HPP

#include <stdexcept>

#include <realm/util/features.h>
#include <realm/util/backtrace.hpp>

namespace realm {

using util::ExceptionWithBacktrace;

/// Thrown by various functions to indicate that a specified table does not
/// exist.
class NoSuchTable : public ExceptionWithBacktrace<std::exception> {
public:
    const char* message() const noexcept override;
};


/// Thrown by various functions to indicate that a specified table name is
/// already in use.
class TableNameInUse : public ExceptionWithBacktrace<std::exception> {
public:
    const char* message() const noexcept override;
};


// Thrown by functions that require a table to **not** be the target of link
// columns, unless those link columns are part of the table itself.
class CrossTableLinkTarget : public ExceptionWithBacktrace<std::exception> {
public:
    const char* message() const noexcept override;
};


/// Thrown by various functions to indicate that the dynamic type of a table
/// does not match a particular other table type (dynamic or static).
class DescriptorMismatch : public ExceptionWithBacktrace<std::exception> {
public:
    const char* message() const noexcept override;
};


/// The FileFormatUpgradeRequired exception can be thrown by the SharedGroup
/// constructor when opening a database that uses a deprecated file format
/// and/or a deprecated history schema, and the user has indicated he does not
/// want automatic upgrades to be performed. This exception indicates that until
/// an upgrade of the file format is performed, the database will be unavailable
/// for read or write operations.
class FileFormatUpgradeRequired : public ExceptionWithBacktrace<std::exception> {
public:
    const char* message() const noexcept override;
};


/// Thrown when a sync agent attempts to join a session in which there is
/// already a sync agent. A session may only contain one sync agent at any given
/// time.
class MultipleSyncAgents : public ExceptionWithBacktrace<std::exception> {
public:
    const char* message() const noexcept override;
};


/// Thrown when memory can no longer be mapped to. When mmap/remap fails.
class AddressSpaceExhausted : public std::runtime_error {
public:
    AddressSpaceExhausted(const std::string& msg);
    /// runtime_error::what() returns the msg provided in the constructor.
};

/// Thrown when creating references that are too large to be contained in our ref_type (size_t)
class MaximumFileSizeExceeded : public std::runtime_error {
public:
    MaximumFileSizeExceeded(const std::string& msg);
    /// runtime_error::what() returns the msg provided in the constructor.
};

/// Thrown when writing fails because the disk is full.
class OutOfDiskSpace : public std::runtime_error {
public:
    OutOfDiskSpace(const std::string& msg);
    /// runtime_error::what() returns the msg provided in the constructor.
};


class SerialisationError : public ExceptionWithBacktrace<std::runtime_error> {
public:
    SerialisationError(const std::string& msg);
    /// runtime_error::what() returns the msg provided in the constructor.
};

/// The \c LogicError exception class is intended to be thrown only when
/// applications (or bindings) violate rules that are stated (or ought to have
/// been stated) in the documentation of the public API, and only in cases
/// where the violation could have been easily and efficiently predicted by the
/// application. In other words, this exception class is for the cases where
/// the error is due to incorrect use of the public API.
///
/// This class is not supposed to be caught by applications. It is not even
/// supposed to be considered part of the public API, and therefore the
/// documentation of the public API should **not** mention the \c LogicError
/// exception class by name. Note how this contrasts with other exception
/// classes, such as \c NoSuchTable, which are part of the public API, and are
/// supposed to be mentioned in the documentation by name. The \c LogicError
/// exception is part of Realm's private API.
///
/// In other words, the \c LogicError class should exclusively be used in
/// replacement (or in addition to) asserts (debug or not) in order to
/// guarantee program interruption, while still allowing for complete
/// test-cases to be written and run.
///
/// To this effect, the special `CHECK_LOGIC_ERROR()` macro is provided as a
/// test framework plugin to allow unit tests to check that the functions in
/// the public API do throw \c LogicError when rules are violated.
///
/// The reason behind hiding this class from the public API is to prevent users
/// from getting used to the idea that "Undefined Behaviour" equates a specific
/// exception being thrown. The whole point of properly documenting "Undefined
/// Behaviour" cases is to help the user know what the limits are, without
/// constraining the database to handle every and any use-case thrown at it.
///
/// FIXME: This exception class should probably be moved to the `_impl`
/// namespace, in order to avoid some confusion.
class LogicError : public ExceptionWithBacktrace<std::exception> {
public:
    enum ErrorKind {
        string_too_big,
        binary_too_big,
        table_name_too_long,
        column_name_too_long,
        table_index_out_of_range,
        row_index_out_of_range,
        column_index_out_of_range,
        string_position_out_of_range,
        link_index_out_of_range,
        bad_version,
        illegal_type,

        /// Indicates that an argument has a value that is illegal in combination
        /// with another argument, or with the state of an involved object.
        illegal_combination,

        /// Indicates a data type mismatch, such as when `Table::find_pkey_int()` is
        /// called and the type of the primary key is not `type_Int`.
        type_mismatch,

        /// Indicates that two involved tables are not in the same group.
        group_mismatch,

        /// Indicates that an involved descriptor is of the wrong kind, i.e., if
        /// it is a subtable descriptor, and the function requires a root table
        /// descriptor.
        wrong_kind_of_descriptor,

        /// Indicates that an involved table is of the wrong kind, i.e., if it
        /// is a subtable, and the function requires a root table, or if it is a
        /// free-standing table, and the function requires a group-level table.
        wrong_kind_of_table,

        /// Indicates that an involved accessor is was detached, i.e., was not
        /// attached to an underlying object.
        detached_accessor,

        /// Indicates that a specified row index of a target table (a link) is
        /// out of range. This is used for disambiguation in cases such as
        /// Table::set_link() where one specifies both a row index of the origin
        /// table, and a row index of the target table.
        target_row_index_out_of_range,

        // Indicates that an involved column lacks a search index.
        no_search_index,

        /// Indicates that a modification was attempted that would have produced a
        /// duplicate primary value.
        unique_constraint_violation,

        /// User attempted to insert null in non-nullable column
        column_not_nullable,

        /// Group::open() is called on a group accessor that is already in the
        /// attached state. Or Group::open() or Group::commit() is called on a
        /// group accessor that is managed by a SharedGroup object.
        wrong_group_state,

        /// No active transaction on a particular SharedGroup object (e.g.,
        /// SharedGroup::commit()), or the active transaction on the SharedGroup
        /// object is of the wrong type (read/write), or an attampt was made to
        /// initiate a new transaction while one is already in progress on the
        /// same SharedGroup object.
        wrong_transact_state,

        /// Attempted use of a continuous transaction through a SharedGroup
        /// object with no history. See Replication::get_history().
        no_history,

        /// Durability setting (as passed to the SharedGroup constructor) was
        /// not consistent across the session.
        mixed_durability,

        /// History type (as specified by the Replication implementation passed
        /// to the SharedGroup constructor) was not consistent across the
        /// session.
        mixed_history_type,

        /// History schema version (as specified by the Replication
        /// implementation passed to the SharedGroup constructor) was not
        /// consistent across the session.
        mixed_history_schema_version,

        /// Adding rows to a table with no columns is not supported.
        table_has_no_columns,

        /// Referring to a column that has been deleted.
        column_does_not_exist,

        /// You can not add index on a subtable of a subtable
        subtable_of_subtable_index
    };

    LogicError(ErrorKind message);

    const char* message() const noexcept override;
    ErrorKind kind() const noexcept;

private:
    ErrorKind m_kind;
};


// Implementation:

// LCOV_EXCL_START (Wording of what() strings are not to be tested)

inline const char* NoSuchTable::message() const noexcept
{
    return "No such table exists";
}

inline const char* TableNameInUse::message() const noexcept
{
    return "The specified table name is already in use";
}

inline const char* CrossTableLinkTarget::message() const noexcept
{
    return "Table is target of cross-table link columns";
}

inline const char* DescriptorMismatch::message() const noexcept
{
    return "Table descriptor mismatch";
}

inline const char* FileFormatUpgradeRequired::message() const noexcept
{
    return "Database upgrade required but prohibited";
}

inline const char* MultipleSyncAgents::message() const noexcept
{
    return "Multiple sync agents attempted to join the same session";
}

// LCOV_EXCL_STOP

inline AddressSpaceExhausted::AddressSpaceExhausted(const std::string& msg)
    : std::runtime_error(msg)
{
}

inline MaximumFileSizeExceeded::MaximumFileSizeExceeded(const std::string& msg)
    : std::runtime_error(msg)
{
}

inline OutOfDiskSpace::OutOfDiskSpace(const std::string& msg)
: std::runtime_error(msg)
{
}

inline SerialisationError::SerialisationError(const std::string& msg)
    : ExceptionWithBacktrace<std::runtime_error>(msg)
{
}

inline LogicError::LogicError(LogicError::ErrorKind k)
    : m_kind(k)
{
}

inline LogicError::ErrorKind LogicError::kind() const noexcept
{
    return m_kind;
}


} // namespace realm


#endif // REALM_EXCEPTIONS_HPP
