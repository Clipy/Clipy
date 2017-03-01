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

#ifndef REALM_IMPL_TRANSACT_LOG_HPP
#define REALM_IMPL_TRANSACT_LOG_HPP

#include <stdexcept>

#include <realm/string_data.hpp>
#include <realm/data_type.hpp>
#include <realm/binary_data.hpp>
#include <realm/olddatetime.hpp>
#include <realm/mixed.hpp>
#include <realm/util/safe_int_ops.hpp>
#include <realm/util/buffer.hpp>
#include <realm/util/string_buffer.hpp>
#include <realm/util/tuple.hpp>
#include <realm/impl/input_stream.hpp>

#include <realm/group.hpp>
#include <realm/descriptor.hpp>
#include <realm/link_view.hpp>

namespace realm {
namespace _impl {

/// Transaction log instruction encoding
/// NOTE: Any change to this enum is a file-format breaking change.
enum Instruction {
    instr_InsertGroupLevelTable = 1,
    instr_EraseGroupLevelTable = 2, // Remove columnless table from group
    instr_RenameGroupLevelTable = 3,
    instr_MoveGroupLevelTable = 4,
    instr_SelectTable = 5,
    instr_Set = 6,
    instr_SetUnique = 7,
    instr_SetDefault = 8,
    instr_AddInteger = 9,   // Add value to integer field
    instr_NullifyLink = 10, // Set link to null due to target being erased
    instr_InsertSubstring = 11,
    instr_EraseFromString = 12,
    instr_InsertEmptyRows = 13,
    instr_EraseRows = 14, // Remove (multiple) rows
    instr_SwapRows = 15,
    instr_MergeRows = 16,  // Replace links pointing to row A with links to row B
    instr_ClearTable = 17, // Remove all rows in selected table
    instr_OptimizeTable = 18,
    instr_SelectDescriptor = 19, // Select descriptor from currently selected root table
    instr_InsertColumn =
        20, // Insert new non-nullable column into to selected descriptor (nullable is instr_InsertNullableColumn)
    instr_InsertLinkColumn = 21,     // do, but for a link-type column
    instr_InsertNullableColumn = 22, // Insert nullable column
    instr_EraseColumn = 23,          // Remove column from selected descriptor
    instr_EraseLinkColumn = 24,      // Remove link-type column from selected descriptor
    instr_RenameColumn = 25,         // Rename column in selected descriptor
    instr_MoveColumn = 26,           // Move column in selected descriptor
    instr_AddSearchIndex = 27,       // Add a search index to a column
    instr_RemoveSearchIndex = 28,    // Remove a search index from a column
    instr_SetLinkType = 29,          // Strong/weak
    instr_SelectLinkList = 30,
    instr_LinkListSet = 31,     // Assign to link list entry
    instr_LinkListInsert = 32,  // Insert entry into link list
    instr_LinkListMove = 33,    // Move an entry within a link list
    instr_LinkListSwap = 34,    // Swap two entries within a link list
    instr_LinkListErase = 35,   // Remove an entry from a link list
    instr_LinkListNullify = 36, // Remove an entry from a link list due to linked row being erased
    instr_LinkListClear = 37,   // Ramove all entries from a link list
    instr_LinkListSetAll = 38,  // Assign to link list entry
};


class TransactLogStream {
public:
    /// Ensure contiguous free space in the transaction log
    /// buffer. This method must update `out_free_begin`
    /// and `out_free_end` such that they refer to a chunk
    /// of free space whose size is at least \a n.
    ///
    /// \param n The required amount of contiguous free space. Must be
    /// small (probably not greater than 1024)
    /// \param n Must be small (probably not greater than 1024)
    virtual void transact_log_reserve(size_t size, char** out_free_begin, char** out_free_end) = 0;

    /// Copy the specified data into the transaction log buffer. This
    /// function should be called only when the specified data does
    /// not fit inside the chunk of free space currently referred to
    /// by `out_free_begin` and `out_free_end`.
    ///
    /// This method must update `out_begin` and
    /// `out_end` such that, upon return, they still
    /// refer to a (possibly empty) chunk of free space.
    virtual void transact_log_append(const char* data, size_t size, char** out_free_begin, char** out_free_end) = 0;
};

class TransactLogBufferStream : public TransactLogStream {
public:
    void transact_log_reserve(size_t size, char** out_free_begin, char** out_free_end) override;
    void transact_log_append(const char* data, size_t size, char** out_free_begin, char** out_free_end) override;

    const char* transact_log_data() const;

    util::Buffer<char> m_buffer;
};


// LCOV_EXCL_START (because the NullInstructionObserver is trivial)
class NullInstructionObserver {
public:
    /// The following methods are also those that TransactLogParser expects
    /// to find on the `InstructionHandler`.

    // No selection needed:
    bool select_table(size_t, size_t, const size_t*)
    {
        return true;
    }
    bool select_descriptor(size_t, const size_t*)
    {
        return true;
    }
    bool select_link_list(size_t, size_t, size_t)
    {
        return true;
    }
    bool insert_group_level_table(size_t, size_t, StringData)
    {
        return true;
    }
    bool erase_group_level_table(size_t, size_t)
    {
        return true;
    }
    bool rename_group_level_table(size_t, StringData)
    {
        return true;
    }
    bool move_group_level_table(size_t, size_t)
    {
        return true;
    }

    // Must have table selected:
    bool insert_empty_rows(size_t, size_t, size_t, bool)
    {
        return true;
    }
    bool erase_rows(size_t, size_t, size_t, bool)
    {
        return true;
    }
    bool swap_rows(size_t, size_t)
    {
        return true;
    }
    bool merge_rows(size_t, size_t)
    {
        return true;
    }
    bool clear_table()
    {
        return true;
    }
    bool set_int(size_t, size_t, int_fast64_t, Instruction, size_t)
    {
        return true;
    }
    bool add_int(size_t, size_t, int_fast64_t)
    {
        return true;
    }
    bool set_bool(size_t, size_t, bool, Instruction)
    {
        return true;
    }
    bool set_float(size_t, size_t, float, Instruction)
    {
        return true;
    }
    bool set_double(size_t, size_t, double, Instruction)
    {
        return true;
    }
    bool set_string(size_t, size_t, StringData, Instruction, size_t)
    {
        return true;
    }
    bool set_binary(size_t, size_t, BinaryData, Instruction)
    {
        return true;
    }
    bool set_olddatetime(size_t, size_t, OldDateTime, Instruction)
    {
        return true;
    }
    bool set_timestamp(size_t, size_t, Timestamp, Instruction)
    {
        return true;
    }
    bool set_table(size_t, size_t, Instruction)
    {
        return true;
    }
    bool set_mixed(size_t, size_t, const Mixed&, Instruction)
    {
        return true;
    }
    bool set_link(size_t, size_t, size_t, size_t, Instruction)
    {
        return true;
    }
    bool set_null(size_t, size_t, Instruction, size_t)
    {
        return true;
    }
    bool nullify_link(size_t, size_t, size_t)
    {
        return true;
    }
    bool insert_substring(size_t, size_t, size_t, StringData)
    {
        return true;
    }
    bool erase_substring(size_t, size_t, size_t, size_t)
    {
        return true;
    }
    bool optimize_table()
    {
        return true;
    }

    // Must have descriptor selected:
    bool insert_link_column(size_t, DataType, StringData, size_t, size_t)
    {
        return true;
    }
    bool insert_column(size_t, DataType, StringData, bool)
    {
        return true;
    }
    bool erase_link_column(size_t, size_t, size_t)
    {
        return true;
    }
    bool erase_column(size_t)
    {
        return true;
    }
    bool rename_column(size_t, StringData)
    {
        return true;
    }
    bool move_column(size_t, size_t)
    {
        return true;
    }
    bool add_search_index(size_t)
    {
        return true;
    }
    bool remove_search_index(size_t)
    {
        return true;
    }
    bool set_link_type(size_t, LinkType)
    {
        return true;
    }

    // Must have linklist selected:
    bool link_list_set(size_t, size_t, size_t)
    {
        return true;
    }
    bool link_list_insert(size_t, size_t, size_t)
    {
        return true;
    }
    bool link_list_move(size_t, size_t)
    {
        return true;
    }
    bool link_list_swap(size_t, size_t)
    {
        return true;
    }
    bool link_list_erase(size_t, size_t)
    {
        return true;
    }
    bool link_list_nullify(size_t, size_t)
    {
        return true;
    }
    bool link_list_clear(size_t)
    {
        return true;
    }

    void parse_complete()
    {
    }
};
// LCOV_EXCL_STOP (NullInstructionObserver)


/// See TransactLogConvenientEncoder for information about the meaning of the
/// arguments of each of the functions in this class.
class TransactLogEncoder {
public:
    /// The following methods are also those that TransactLogParser expects
    /// to find on the `InstructionHandler`.

    // No selection needed:
    bool select_table(size_t group_level_ndx, size_t levels, const size_t* path);
    bool select_descriptor(size_t levels, const size_t* path);
    bool select_link_list(size_t col_ndx, size_t row_ndx, size_t link_target_group_level_ndx);
    bool insert_group_level_table(size_t table_ndx, size_t num_tables, StringData name);
    bool erase_group_level_table(size_t table_ndx, size_t num_tables);
    bool rename_group_level_table(size_t table_ndx, StringData new_name);
    bool move_group_level_table(size_t from_table_ndx, size_t to_table_ndx);

    /// Must have table selected.
    bool insert_empty_rows(size_t row_ndx, size_t num_rows_to_insert, size_t prior_num_rows, bool unordered);
    bool erase_rows(size_t row_ndx, size_t num_rows_to_erase, size_t prior_num_rows, bool unordered);
    bool swap_rows(size_t row_ndx_1, size_t row_ndx_2);
    bool merge_rows(size_t row_ndx, size_t new_row_ndx);
    bool clear_table();

    bool set_int(size_t col_ndx, size_t row_ndx, int_fast64_t, Instruction = instr_Set, size_t = 0);
    bool add_int(size_t col_ndx, size_t row_ndx, int_fast64_t);
    bool set_bool(size_t col_ndx, size_t row_ndx, bool, Instruction = instr_Set);
    bool set_float(size_t col_ndx, size_t row_ndx, float, Instruction = instr_Set);
    bool set_double(size_t col_ndx, size_t row_ndx, double, Instruction = instr_Set);
    bool set_string(size_t col_ndx, size_t row_ndx, StringData, Instruction = instr_Set, size_t = 0);
    bool set_binary(size_t col_ndx, size_t row_ndx, BinaryData, Instruction = instr_Set);
    bool set_olddatetime(size_t col_ndx, size_t row_ndx, OldDateTime, Instruction = instr_Set);
    bool set_timestamp(size_t col_ndx, size_t row_ndx, Timestamp, Instruction = instr_Set);
    bool set_table(size_t col_ndx, size_t row_ndx, Instruction = instr_Set);
    bool set_mixed(size_t col_ndx, size_t row_ndx, const Mixed&, Instruction = instr_Set);
    bool set_link(size_t col_ndx, size_t row_ndx, size_t, size_t target_group_level_ndx, Instruction = instr_Set);
    bool set_null(size_t col_ndx, size_t row_ndx, Instruction = instr_Set, size_t = 0);
    bool nullify_link(size_t col_ndx, size_t row_ndx, size_t target_group_level_ndx);
    bool insert_substring(size_t col_ndx, size_t row_ndx, size_t pos, StringData);
    bool erase_substring(size_t col_ndx, size_t row_ndx, size_t pos, size_t size);
    bool optimize_table();

    // Must have descriptor selected:
    bool insert_link_column(size_t col_ndx, DataType, StringData name, size_t link_target_table_ndx,
                            size_t backlink_col_ndx);
    bool insert_column(size_t col_ndx, DataType, StringData name, bool nullable = false);
    bool erase_link_column(size_t col_ndx, size_t link_target_table_ndx, size_t backlink_col_ndx);
    bool erase_column(size_t col_ndx);
    bool rename_column(size_t col_ndx, StringData new_name);
    bool move_column(size_t col_ndx_1, size_t col_ndx_2);
    bool add_search_index(size_t col_ndx);
    bool remove_search_index(size_t col_ndx);
    bool set_link_type(size_t col_ndx, LinkType);

    // Must have linklist selected:
    bool link_list_set(size_t link_ndx, size_t value, size_t prior_size);
    bool link_list_set_all(const IntegerColumn& values);
    bool link_list_insert(size_t link_ndx, size_t value, size_t prior_size);
    bool link_list_move(size_t from_link_ndx, size_t to_link_ndx);
    bool link_list_swap(size_t link1_ndx, size_t link2_ndx);
    bool link_list_erase(size_t link_ndx, size_t prior_size);
    bool link_list_nullify(size_t link_ndx, size_t prior_size);
    bool link_list_clear(size_t old_list_size);

    /// End of methods expected by parser.


    TransactLogEncoder(TransactLogStream& out_stream);
    void set_buffer(char* new_free_begin, char* new_free_end);
    char* write_position() const
    {
        return m_transact_log_free_begin;
    }

private:
    // Make sure this is in agreement with the actual integer encoding
    // scheme (see encode_int()).
    static const int max_enc_bytes_per_int = 10;
    static const int max_enc_bytes_per_double = sizeof(double);
    static const int max_enc_bytes_per_num =
        max_enc_bytes_per_int < max_enc_bytes_per_double ? max_enc_bytes_per_double : max_enc_bytes_per_int;

    // This value is used in Set* instructions in place of the 'type' field in
    // the stream to indicate that the value of the Set* instruction is NULL,
    // which doesn't have a type.
    static constexpr int set_null_sentinel()
    {
        return -1;
    }

    TransactLogStream& m_stream;

    // These two delimit a contiguous region of free space in a
    // transaction log buffer following the last written data. It may
    // be empty.
    char* m_transact_log_free_begin = nullptr;
    char* m_transact_log_free_end = nullptr;

    char* reserve(size_t size);
    /// \param ptr Must be in the range [m_transact_log_free_begin, m_transact_log_free_end]
    void advance(char* ptr) noexcept;

    template <class L>
    void append_simple_instr(Instruction, const util::Tuple<L>& numbers);

    template <class L>
    void append_string_instr(Instruction, const util::Tuple<L>& numbers, StringData);

    template <class L>
    void append_mixed_instr(Instruction, const util::Tuple<L>& numbers, const Mixed&);

    template <class L, class I>
    bool append_variable_size_instr(Instruction instr, const util::Tuple<L>& numbers, I var_begin, I var_end);

    template <class T>
    static char* encode_int(char*, T value);
    static char* encode_bool(char*, bool value);
    static char* encode_float(char*, float value);
    static char* encode_double(char*, double value);
    template <class>
    struct EncodeNumber;
    friend class TransactLogParser;
};

class TransactLogConvenientEncoder {
public:
    void insert_group_level_table(size_t table_ndx, size_t num_tables, StringData name);
    void erase_group_level_table(size_t table_ndx, size_t num_tables);
    void rename_group_level_table(size_t table_ndx, StringData new_name);
    void move_group_level_table(size_t from_table_ndx, size_t to_table_ndx);
    void insert_column(const Descriptor&, size_t col_ndx, DataType type, StringData name, LinkTargetInfo& link,
                       bool nullable = false);
    void erase_column(const Descriptor&, size_t col_ndx);
    void rename_column(const Descriptor&, size_t col_ndx, StringData name);
    void move_column(const Descriptor&, size_t from, size_t to);

    void set_int(const Table*, size_t col_ndx, size_t ndx, int_fast64_t value, Instruction variant = instr_Set);
    void add_int(const Table*, size_t col_ndx, size_t ndx, int_fast64_t value);
    void set_bool(const Table*, size_t col_ndx, size_t ndx, bool value, Instruction variant = instr_Set);
    void set_float(const Table*, size_t col_ndx, size_t ndx, float value, Instruction variant = instr_Set);
    void set_double(const Table*, size_t col_ndx, size_t ndx, double value, Instruction variant = instr_Set);
    void set_string(const Table*, size_t col_ndx, size_t ndx, StringData value, Instruction variant = instr_Set);
    void set_binary(const Table*, size_t col_ndx, size_t ndx, BinaryData value, Instruction variant = instr_Set);
    void set_olddatetime(const Table*, size_t col_ndx, size_t ndx, OldDateTime value,
                         Instruction variant = instr_Set);
    void set_timestamp(const Table*, size_t col_ndx, size_t ndx, Timestamp value, Instruction variant = instr_Set);
    void set_table(const Table*, size_t col_ndx, size_t ndx, Instruction variant = instr_Set);
    void set_mixed(const Table*, size_t col_ndx, size_t ndx, const Mixed& value, Instruction variant = instr_Set);
    void set_link(const Table*, size_t col_ndx, size_t ndx, size_t value, Instruction variant = instr_Set);
    void set_null(const Table*, size_t col_ndx, size_t ndx, Instruction variant = instr_Set);
    void set_link_list(const LinkView&, const IntegerColumn& values);
    void insert_substring(const Table*, size_t col_ndx, size_t row_ndx, size_t pos, StringData);
    void erase_substring(const Table*, size_t col_ndx, size_t row_ndx, size_t pos, size_t size);

    /// \param prior_num_rows The number of rows in the table prior to the
    /// modification.
    void insert_empty_rows(const Table*, size_t row_ndx, size_t num_rows_to_insert, size_t prior_num_rows);

    /// \param prior_num_rows The number of rows in the table prior to the
    /// modification.
    void erase_rows(const Table*, size_t row_ndx, size_t num_rows_to_erase, size_t prior_num_rows,
                    bool is_move_last_over);

    void swap_rows(const Table*, size_t row_ndx_1, size_t row_ndx_2);
    void merge_rows(const Table*, size_t row_ndx, size_t new_row_ndx);
    void add_search_index(const Table*, size_t col_ndx);
    void remove_search_index(const Table*, size_t col_ndx);
    void set_link_type(const Table*, size_t col_ndx, LinkType);
    void clear_table(const Table*);
    void optimize_table(const Table*);

    void link_list_set(const LinkView&, size_t link_ndx, size_t value);
    void link_list_insert(const LinkView&, size_t link_ndx, size_t value);
    void link_list_move(const LinkView&, size_t from_link_ndx, size_t to_link_ndx);
    void link_list_swap(const LinkView&, size_t link_ndx_1, size_t link_ndx_2);
    void link_list_erase(const LinkView&, size_t link_ndx);
    void link_list_clear(const LinkView&);

    //@{

    /// Implicit nullifications due to removal of target row. This is redundant
    /// information from the point of view of replication, as the removal of the
    /// target row will reproduce the implicit nullifications in the target
    /// Realm anyway. The purpose of this instruction is to allow observers
    /// (reactor pattern) to be explicitly notified about the implicit
    /// nullifications.

    void nullify_link(const Table*, size_t col_ndx, size_t ndx);
    void link_list_nullify(const LinkView&, size_t link_ndx);

    //@}

    void on_table_destroyed(const Table*) noexcept;
    void on_spec_destroyed(const Spec*) noexcept;
    void on_link_list_destroyed(const LinkView&) noexcept;

protected:
    TransactLogConvenientEncoder(TransactLogStream& encoder);

    void reset_selection_caches() noexcept;
    void set_buffer(char* new_free_begin, char* new_free_end)
    {
        m_encoder.set_buffer(new_free_begin, new_free_end);
    }
    char* write_position() const
    {
        return m_encoder.write_position();
    }

private:
    TransactLogEncoder m_encoder;
    // These are mutable because they are caches.
    mutable util::Buffer<size_t> m_subtab_path_buf;
    mutable const Table* m_selected_table;
    mutable const Spec* m_selected_spec;
    // Has to be atomic to support concurrent reset when a linklist
    // is unselected. This can happen on a different thread. In case
    // of races, setting of a new value must win.
    mutable std::atomic<const LinkView*> m_selected_link_list;

    void unselect_all() noexcept;
    void select_table(const Table*);        // unselects descriptor and link list
    void select_desc(const Descriptor&);    // unselects link list
    void select_link_list(const LinkView&); // unselects descriptor

    void record_subtable_path(const Table&, size_t*& out_begin, size_t*& out_end);
    void do_select_table(const Table*);
    void do_select_desc(const Descriptor&);
    void do_select_link_list(const LinkView&);

    friend class TransactReverser;
};


class TransactLogParser {
public:
    class BadTransactLog; // Exception

    TransactLogParser();
    ~TransactLogParser() noexcept;

    /// See `TransactLogEncoder` for a list of methods that the `InstructionHandler` must define.
    /// parse() promises that the path passed by reference to
    /// InstructionHandler::select_descriptor() will remain valid
    /// during subsequent calls to all descriptor modifying functions.
    template <class InstructionHandler>
    void parse(InputStream&, InstructionHandler&);

    template <class InstructionHandler>
    void parse(NoCopyInputStream&, InstructionHandler&);

private:
    util::Buffer<char> m_input_buffer;

    // The input stream is assumed to consist of chunks of memory organised such that
    // every instruction resides in a single chunk only.
    NoCopyInputStream* m_input;
    // pointer into transaction log, each instruction is parsed from m_input_begin and onwards.
    // Each instruction are assumed to be contiguous in memory.
    const char* m_input_begin;
    // pointer to one past current instruction log chunk. If m_input_begin reaches m_input_end,
    // a call to next_input_buffer will move m_input_begin and m_input_end to a new chunk of
    // memory. Setting m_input_end to 0 disables this check, and is used if it is already known
    // that all of the instructions are in memory.
    const char* m_input_end;
    util::StringBuffer m_string_buffer;
    static const int m_max_levels = 1024;
    util::Buffer<size_t> m_path;

    REALM_NORETURN void parser_error() const;

    template <class InstructionHandler>
    void parse_one(InstructionHandler&);
    bool has_next() noexcept;

    template <class T>
    T read_int();

    void read_bytes(char* data, size_t size);
    BinaryData read_buffer(util::StringBuffer&, size_t size);

    bool read_bool();
    float read_float();
    double read_double();

    StringData read_string(util::StringBuffer&);
    BinaryData read_binary(util::StringBuffer&);
    Timestamp read_timestamp();
    void read_mixed(Mixed*);

    // Advance m_input_begin and m_input_end to reflect the next block of instructions
    // Returns false if no more input was available
    bool next_input_buffer();

    // return true if input was available
    bool read_char(char&); // throws

    bool is_valid_data_type(int type);
    bool is_valid_link_type(int type);
};


class TransactLogParser::BadTransactLog : public std::exception {
public:
    const char* what() const noexcept override
    {
        return "Bad transaction log";
    }
};


/// Implementation:

inline void TransactLogBufferStream::transact_log_reserve(size_t n, char** inout_new_begin, char** out_new_end)
{
    char* data = m_buffer.data();
    REALM_ASSERT(*inout_new_begin >= data);
    REALM_ASSERT(*inout_new_begin <= (data + m_buffer.size()));
    size_t size = *inout_new_begin - data;
    m_buffer.reserve_extra(size, n);
    data = m_buffer.data(); // May have changed
    *inout_new_begin = data + size;
    *out_new_end = data + m_buffer.size();
}

inline void TransactLogBufferStream::transact_log_append(const char* data, size_t size, char** out_new_begin,
                                                         char** out_new_end)
{
    transact_log_reserve(size, out_new_begin, out_new_end);
    *out_new_begin = std::copy_n(data, size, *out_new_begin);
}

inline const char* TransactLogBufferStream::transact_log_data() const
{
    return m_buffer.data();
}

inline TransactLogEncoder::TransactLogEncoder(TransactLogStream& stream)
    : m_stream(stream)
{
}

inline void TransactLogEncoder::set_buffer(char* free_begin, char* free_end)
{
    REALM_ASSERT(free_begin <= free_end);
    m_transact_log_free_begin = free_begin;
    m_transact_log_free_end = free_end;
}

inline void TransactLogConvenientEncoder::reset_selection_caches() noexcept
{
    unselect_all();
}

inline char* TransactLogEncoder::reserve(size_t n)
{
    if (size_t(m_transact_log_free_end - m_transact_log_free_begin) < n) {
        m_stream.transact_log_reserve(n, &m_transact_log_free_begin, &m_transact_log_free_end);
    }
    return m_transact_log_free_begin;
}

inline void TransactLogEncoder::advance(char* ptr) noexcept
{
    REALM_ASSERT_DEBUG(m_transact_log_free_begin <= ptr);
    REALM_ASSERT_DEBUG(ptr <= m_transact_log_free_end);
    m_transact_log_free_begin = ptr;
}


// The integer encoding is platform independent. Also, it does not
// depend on the type of the specified integer. Integers of any type
// can be encoded as long as the specified buffer is large enough (see
// below). The decoding does not have to use the same type. Decoding
// will fail if, and only if the encoded value falls outside the range
// of the requested destination type.
//
// The encoding uses one or more bytes. It never uses more than 8 bits
// per byte. The last byte in the sequence is the first one that has
// its 8th bit set to zero.
//
// Consider a particular non-negative value V. Let W be the number of
// bits needed to encode V using the trivial binary encoding of
// integers. The total number of bytes produced is then
// ceil((W+1)/7). The first byte holds the 7 least significant bits of
// V. The last byte holds at most 6 bits of V including the most
// significant one. The value of the first bit of the last byte is
// always 2**((N-1)*7) where N is the total number of bytes.
//
// A negative value W is encoded by setting the sign bit to one and
// then encoding the positive result of -(W+1) as described above. The
// advantage of this representation is that it converts small negative
// values to small positive values which require a small number of
// bytes. This would not have been true for 2's complements
// representation, for example. The sign bit is always stored as the
// 7th bit of the last byte.
//
//               value bits    value + sign    max bytes
//     --------------------------------------------------
//     int8_t         7              8              2
//     uint8_t        8              9              2
//     int16_t       15             16              3
//     uint16_t      16             17              3
//     int32_t       31             32              5
//     uint32_t      32             33              5
//     int64_t       63             64             10
//     uint64_t      64             65             10
//
template <class T>
char* TransactLogEncoder::encode_int(char* ptr, T value)
{
    static_assert(std::numeric_limits<T>::is_integer, "Integer required");
    bool negative = util::is_negative(value);
    if (negative) {
        // The following conversion is guaranteed by C++11 to never
        // overflow (contrast this with "-value" which indeed could
        // overflow). See C99+TC3 section 6.2.6.2 paragraph 2.
        REALM_DIAG_PUSH();
        REALM_DIAG_IGNORE_UNSIGNED_MINUS();
        value = -(value + 1);
        REALM_DIAG_POP();
    }
    // At this point 'value' is always a positive number. Also, small
    // negative numbers have been converted to small positive numbers.
    REALM_ASSERT(!util::is_negative(value));
    // One sign bit plus number of value bits
    const int num_bits = 1 + std::numeric_limits<T>::digits;
    // Only the first 7 bits are available per byte. Had it not been
    // for the fact that maximum guaranteed bit width of a char is 8,
    // this value could have been increased to 15 (one less than the
    // number of value bits in 'unsigned').
    const int bits_per_byte = 7;
    const int max_bytes = (num_bits + (bits_per_byte - 1)) / bits_per_byte;
    static_assert(max_bytes <= max_enc_bytes_per_int, "Bad max_enc_bytes_per_int");
    // An explicit constant maximum number of iterations is specified
    // in the hope that it will help the optimizer (to do loop
    // unrolling, for example).
    typedef unsigned char uchar;
    for (int i = 0; i < max_bytes; ++i) {
        if (value >> (bits_per_byte - 1) == 0)
            break;
        *reinterpret_cast<uchar*>(ptr) = uchar((1U << bits_per_byte) | unsigned(value & ((1U << bits_per_byte) - 1)));
        ++ptr;
        value >>= bits_per_byte;
    }
    *reinterpret_cast<uchar*>(ptr) = uchar(negative ? (1U << (bits_per_byte - 1)) | unsigned(value) : value);
    return ++ptr;
}

inline char* TransactLogEncoder::encode_bool(char* ptr, bool value)
{
    // A `char` is the smallest element that the encoder/decoder can process. So we encode the bool
    // in a char. If we called encode_int<bool> it would end up as a char too, but we would get
    // Various warnings about arithmetic on non-arithmetic type.
    return encode_int<char>(ptr, value);
}

inline char* TransactLogEncoder::encode_float(char* ptr, float value)
{
    static_assert(std::numeric_limits<float>::is_iec559 &&
                      sizeof(float) * std::numeric_limits<unsigned char>::digits == 32,
                  "Unsupported 'float' representation");
    const char* val_ptr = reinterpret_cast<char*>(&value);
    return std::copy_n(val_ptr, sizeof value, ptr);
}

inline char* TransactLogEncoder::encode_double(char* ptr, double value)
{
    static_assert(std::numeric_limits<double>::is_iec559 &&
                      sizeof(double) * std::numeric_limits<unsigned char>::digits == 64,
                  "Unsupported 'double' representation");
    const char* val_ptr = reinterpret_cast<char*>(&value);
    return std::copy_n(val_ptr, sizeof value, ptr);
}

template <class T>
struct TransactLogEncoder::EncodeNumber {
    void operator()(T value, char** ptr)
    {
        auto value_2 = value + 0; // Perform integral promotion
        *ptr = encode_int(*ptr, value_2);
    }
};
template <>
struct TransactLogEncoder::EncodeNumber<char> {
    void operator()(char value, char** ptr)
    {
        // Write the char as-is without encoding.
        **ptr = value;
        ++(*ptr);
    }
};
template <>
struct TransactLogEncoder::EncodeNumber<bool> {
    void operator()(bool value, char** ptr)
    {
        *ptr = encode_bool(*ptr, value);
    }
};
template <>
struct TransactLogEncoder::EncodeNumber<float> {
    void operator()(float value, char** ptr)
    {
        *ptr = encode_float(*ptr, value);
    }
};
template <>
struct TransactLogEncoder::EncodeNumber<double> {
    void operator()(double value, char** ptr)
    {
        *ptr = encode_double(*ptr, value);
    }
};
template <>
struct TransactLogEncoder::EncodeNumber<DataType> {
    void operator()(DataType type, char** ptr)
    {
        auto value_2 = type + 0; // Perform integral promotion
        *ptr = encode_int(*ptr, value_2);
    }
};

template <class L>
void TransactLogEncoder::append_simple_instr(Instruction instr, const util::Tuple<L>& numbers)
{
    size_t num_numbers = util::TypeCount<L>::value;
    size_t max_required_bytes = 1 + max_enc_bytes_per_num * num_numbers;
    char* ptr = reserve(max_required_bytes); // Throws
    *ptr++ = char(instr);
    util::for_each<EncodeNumber>(numbers, &ptr);
    advance(ptr);
}

template <class L>
void TransactLogEncoder::append_string_instr(Instruction instr, const util::Tuple<L>& numbers, StringData string)
{
    size_t num_numbers = util::TypeCount<L>::value + 1;
    size_t max_required_bytes = 1 + max_enc_bytes_per_num * num_numbers + string.size();
    char* ptr = reserve(max_required_bytes); // Throws
    *ptr++ = char(instr);
    util::for_each<EncodeNumber>(append(numbers, string.size()), &ptr);
    ptr = std::copy_n(string.data(), string.size(), ptr);
    advance(ptr);
}

template <class L>
void TransactLogEncoder::append_mixed_instr(Instruction instr, const util::Tuple<L>& numbers, const Mixed& value)
{
    DataType type = value.get_type();
    auto numbers_2 = append(numbers, type);
    switch (type) {
        case type_Int:
            append_simple_instr(instr, append(numbers_2, value.get_int())); // Throws
            return;
        case type_Bool:
            append_simple_instr(instr, append(numbers_2, value.get_bool())); // Throws
            return;
        case type_Float:
            append_simple_instr(instr, append(numbers_2, value.get_float())); // Throws
            return;
        case type_Double:
            append_simple_instr(instr, append(numbers_2, value.get_double())); // Throws
            return;
        case type_OldDateTime: {
            auto value_2 = value.get_olddatetime().get_olddatetime();
            append_simple_instr(instr, append(numbers_2, value_2)); // Throws
            return;
        }
        case type_String: {
            append_string_instr(instr, numbers_2, value.get_string()); // Throws
            return;
        }
        case type_Binary: {
            BinaryData value_2 = value.get_binary();
            StringData value_3(value_2.data(), value_2.size());
            append_string_instr(instr, numbers_2, value_3); // Throws
            return;
        }
        case type_Timestamp: {
            Timestamp ts = value.get_timestamp();
            int64_t seconds = ts.get_seconds();
            int32_t nano_seconds = ts.get_nanoseconds();
            auto numbers_3 = append(numbers_2, seconds);
            append_simple_instr(instr, append(numbers_3, nano_seconds)); // Throws
            return;
        }
        case type_Table:
            append_simple_instr(instr, numbers_2); // Throws
            return;
        case type_Mixed:
            // Mixed in mixed is not possible
            REALM_ASSERT_RELEASE(false);
        case type_Link:
        case type_LinkList:
            // FIXME: Need to handle new link types here.
            REALM_ASSERT_RELEASE(false);
    }
    REALM_ASSERT_RELEASE(false);
}

template <class L, class I>
bool TransactLogEncoder::append_variable_size_instr(Instruction instr, const util::Tuple<L>& numbers, I var_begin,
                                                    I var_end)
{
// Space is reserved in chunks to avoid excessive over allocation.
#ifdef REALM_DEBUG
    const int max_numbers_per_chunk = 2; // Increase the chance of chunking in debug mode
#else
    const int max_numbers_per_chunk = 8;
#endif
    size_t num_numbers = util::TypeCount<L>::value + max_numbers_per_chunk;
    size_t max_required_bytes = 1 + max_enc_bytes_per_num * num_numbers;
    char* ptr = reserve(max_required_bytes); // Throws
    *ptr++ = char(instr);
    util::for_each<EncodeNumber>(numbers, &ptr);
    I i = var_begin;
    while (var_end - i > max_numbers_per_chunk) {
        for (int j = 0; j < max_numbers_per_chunk; ++j)
            ptr = encode_int(ptr, *i++);
        advance(ptr);
        size_t max_required_bytes_2 = max_enc_bytes_per_num * max_numbers_per_chunk;
        ptr = reserve(max_required_bytes_2); // Throws
    }
    while (i != var_end)
        ptr = encode_int(ptr, *i++);
    advance(ptr);
    return true;
}

inline void TransactLogConvenientEncoder::unselect_all() noexcept
{
    m_selected_table = nullptr;
    m_selected_spec = nullptr;
    // no race with on_link_list_destroyed since both are setting to nullptr
    m_selected_link_list = nullptr;
}

inline void TransactLogConvenientEncoder::select_table(const Table* table)
{
    if (table != m_selected_table)
        do_select_table(table); // Throws
    m_selected_spec = nullptr;
    // no race with on_link_list_destroyed since both are setting to nullptr
    m_selected_link_list = nullptr;
}

inline void TransactLogConvenientEncoder::select_desc(const Descriptor& desc)
{
    typedef _impl::DescriptorFriend df;
    if (&df::get_spec(desc) != m_selected_spec)
        do_select_desc(desc); // Throws
    // no race with on_link_list_destroyed since both are setting to nullptr
    m_selected_link_list = nullptr;
}

inline void TransactLogConvenientEncoder::select_link_list(const LinkView& list)
{
    // A race between this and a call to on_link_list_destroyed() must
    // end up with m_selected_link_list pointing to the list argument given
    // here. We assume that the list given to on_link_list_destroyed() can
    // *never* be the same as the list argument given here. We resolve the
    // race by a) always updating m_selected_link_list in do_select_link_list()
    // and b) only atomically and conditionally updating it in
    // on_link_list_destroyed().
    if (&list != m_selected_link_list) {
        do_select_link_list(list); // Throws
    }
    m_selected_spec = nullptr;
}


inline bool TransactLogEncoder::insert_group_level_table(size_t table_ndx, size_t prior_num_tables, StringData name)
{
    append_string_instr(instr_InsertGroupLevelTable, util::tuple(table_ndx, prior_num_tables), name); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::insert_group_level_table(size_t table_ndx, size_t prior_num_tables,
                                                                   StringData name)
{
    unselect_all();
    m_encoder.insert_group_level_table(table_ndx, prior_num_tables, name); // Throws
}

inline bool TransactLogEncoder::erase_group_level_table(size_t table_ndx, size_t prior_num_tables)
{
    append_simple_instr(instr_EraseGroupLevelTable, util::tuple(table_ndx, prior_num_tables)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::erase_group_level_table(size_t table_ndx, size_t prior_num_tables)
{
    unselect_all();
    m_encoder.erase_group_level_table(table_ndx, prior_num_tables); // Throws
}

inline bool TransactLogEncoder::rename_group_level_table(size_t table_ndx, StringData new_name)
{
    append_string_instr(instr_RenameGroupLevelTable, util::tuple(table_ndx), new_name); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::rename_group_level_table(size_t table_ndx, StringData new_name)
{
    unselect_all();
    m_encoder.rename_group_level_table(table_ndx, new_name); // Throws
}

inline bool TransactLogEncoder::move_group_level_table(size_t from_table_ndx, size_t to_table_ndx)
{
    REALM_ASSERT(from_table_ndx != to_table_ndx);
    append_simple_instr(instr_MoveGroupLevelTable, util::tuple(from_table_ndx, to_table_ndx));
    return true;
}

inline void TransactLogConvenientEncoder::move_group_level_table(size_t from_table_ndx, size_t to_table_ndx)
{
    unselect_all();
    m_encoder.move_group_level_table(from_table_ndx, to_table_ndx);
}

inline bool TransactLogEncoder::insert_column(size_t col_ndx, DataType type, StringData name, bool nullable)
{
    Instruction instr = (nullable ? instr_InsertNullableColumn : instr_InsertColumn);
    append_string_instr(instr, util::tuple(col_ndx, type), name); // Throws
    return true;
}

inline bool TransactLogEncoder::insert_link_column(size_t col_ndx, DataType type, StringData name,
                                                   size_t link_target_table_ndx, size_t backlink_col_ndx)
{
    REALM_ASSERT(_impl::TableFriend::is_link_type(ColumnType(type)));
    append_string_instr(instr_InsertLinkColumn, util::tuple(col_ndx, type, link_target_table_ndx, backlink_col_ndx),
                        name); // Throws
    return true;
}


inline void TransactLogConvenientEncoder::insert_column(const Descriptor& desc, size_t col_ndx, DataType type,
                                                        StringData name, LinkTargetInfo& link, bool nullable)
{
    select_desc(desc); // Throws
    if (link.is_valid()) {
        typedef _impl::TableFriend tf;
        typedef _impl::DescriptorFriend df;
        size_t target_table_ndx = link.m_target_table->get_index_in_group();
        const Table& origin_table = df::get_root_table(desc);
        REALM_ASSERT(origin_table.is_group_level());
        const Spec& target_spec = tf::get_spec(*(link.m_target_table));
        size_t origin_table_ndx = origin_table.get_index_in_group();
        size_t backlink_col_ndx = target_spec.find_backlink_column(origin_table_ndx, col_ndx);
        REALM_ASSERT_3(backlink_col_ndx, ==, link.m_backlink_col_ndx);
        m_encoder.insert_link_column(col_ndx, type, name, target_table_ndx, backlink_col_ndx); // Throws
    }
    else {
        m_encoder.insert_column(col_ndx, type, name, nullable); // Throws
    }
}

inline bool TransactLogEncoder::erase_column(size_t col_ndx)
{
    append_simple_instr(instr_EraseColumn, util::tuple(col_ndx)); // Throws
    return true;
}

inline bool TransactLogEncoder::erase_link_column(size_t col_ndx, size_t link_target_table_ndx,
                                                  size_t backlink_col_ndx)
{
    append_simple_instr(instr_EraseLinkColumn, util::tuple(col_ndx, link_target_table_ndx,
                                                           backlink_col_ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::erase_column(const Descriptor& desc, size_t col_ndx)
{
    select_desc(desc); // Throws

    DataType type = desc.get_column_type(col_ndx);
    typedef _impl::TableFriend tf;
    if (!tf::is_link_type(ColumnType(type))) {
        m_encoder.erase_column(col_ndx); // Throws
    }
    else { // it's a link column:
        REALM_ASSERT(desc.is_root());
        typedef _impl::DescriptorFriend df;
        const Table& origin_table = df::get_root_table(desc);
        REALM_ASSERT(origin_table.is_group_level());
        const Table& target_table = *tf::get_link_target_table_accessor(origin_table, col_ndx);
        size_t target_table_ndx = target_table.get_index_in_group();
        const Spec& target_spec = tf::get_spec(target_table);
        size_t origin_table_ndx = origin_table.get_index_in_group();
        size_t backlink_col_ndx = target_spec.find_backlink_column(origin_table_ndx, col_ndx);
        m_encoder.erase_link_column(col_ndx, target_table_ndx, backlink_col_ndx); // Throws
    }
}

inline bool TransactLogEncoder::rename_column(size_t col_ndx, StringData new_name)
{
    append_string_instr(instr_RenameColumn, util::tuple(col_ndx), new_name); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::rename_column(const Descriptor& desc, size_t col_ndx, StringData name)
{
    select_desc(desc);                      // Throws
    m_encoder.rename_column(col_ndx, name); // Throws
}


inline bool TransactLogEncoder::move_column(size_t from, size_t to)
{
    append_simple_instr(instr_MoveColumn, util::tuple(from, to)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::move_column(const Descriptor& desc, size_t from, size_t to)
{
    select_desc(desc); // Throws
    m_encoder.move_column(from, to);
}


inline bool TransactLogEncoder::set_int(size_t col_ndx, size_t ndx, int_fast64_t value, Instruction variant,
                                        size_t prior_num_rows)
{
    REALM_ASSERT_EX(variant == instr_Set || variant == instr_SetDefault || variant == instr_SetUnique, variant);
    if (REALM_UNLIKELY(variant == instr_SetUnique))
        append_simple_instr(variant, util::tuple(type_Int, col_ndx, ndx, prior_num_rows, value)); // Throws
    else
        append_simple_instr(variant, util::tuple(type_Int, col_ndx, ndx, value)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_int(const Table* t, size_t col_ndx, size_t ndx, int_fast64_t value,
                                                  Instruction variant)
{
    select_table(t); // Throws
    size_t prior_num_rows = (variant == instr_SetUnique ? t->size() : 0);
    m_encoder.set_int(col_ndx, ndx, value, variant, prior_num_rows); // Throws
}


inline bool TransactLogEncoder::add_int(size_t col_ndx, size_t ndx, int_fast64_t value)
{
    append_simple_instr(instr_AddInteger, util::tuple(col_ndx, ndx, value)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::add_int(const Table* t, size_t col_ndx, size_t ndx, int_fast64_t value)
{
    select_table(t); // Throws
    m_encoder.add_int(col_ndx, ndx, value);
}

inline bool TransactLogEncoder::set_bool(size_t col_ndx, size_t ndx, bool value, Instruction variant)
{
    REALM_ASSERT_EX(variant == instr_Set || variant == instr_SetDefault, variant);
    append_simple_instr(variant, util::tuple(type_Bool, col_ndx, ndx, value)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_bool(const Table* t, size_t col_ndx, size_t ndx, bool value,
                                                   Instruction variant)
{
    select_table(t);                                  // Throws
    m_encoder.set_bool(col_ndx, ndx, value, variant); // Throws
}

inline bool TransactLogEncoder::set_float(size_t col_ndx, size_t ndx, float value, Instruction variant)
{
    REALM_ASSERT_EX(variant == instr_Set || variant == instr_SetDefault, variant);
    append_simple_instr(variant, util::tuple(type_Float, col_ndx, ndx, value)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_float(const Table* t, size_t col_ndx, size_t ndx, float value,
                                                    Instruction variant)
{
    select_table(t);                                   // Throws
    m_encoder.set_float(col_ndx, ndx, value, variant); // Throws
}

inline bool TransactLogEncoder::set_double(size_t col_ndx, size_t ndx, double value, Instruction variant)
{
    REALM_ASSERT_EX(variant == instr_Set || variant == instr_SetDefault, variant);
    append_simple_instr(instr_Set, util::tuple(type_Double, col_ndx, ndx, value)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_double(const Table* t, size_t col_ndx, size_t ndx, double value,
                                                     Instruction variant)
{
    select_table(t);                                    // Throws
    m_encoder.set_double(col_ndx, ndx, value, variant); // Throws
}

inline bool TransactLogEncoder::set_string(size_t col_ndx, size_t ndx, StringData value, Instruction variant,
                                           size_t prior_num_rows)
{
    REALM_ASSERT_EX(variant == instr_Set || variant == instr_SetDefault || variant == instr_SetUnique, variant);
    if (value.is_null()) {
        set_null(col_ndx, ndx, variant, prior_num_rows); // Throws
    }
    else {
        if (REALM_UNLIKELY(variant == instr_SetUnique))
            append_string_instr(variant, util::tuple(type_String, col_ndx, ndx, prior_num_rows), value); // Throws
        else
            append_string_instr(variant, util::tuple(type_String, col_ndx, ndx), value); // Throws
    }
    return true;
}

inline void TransactLogConvenientEncoder::set_string(const Table* t, size_t col_ndx, size_t ndx, StringData value,
                                                     Instruction variant)
{
    select_table(t); // Throws
    size_t prior_num_rows = (variant == instr_SetUnique ? t->size() : 0);
    m_encoder.set_string(col_ndx, ndx, value, variant, prior_num_rows); // Throws
}

inline bool TransactLogEncoder::set_binary(size_t col_ndx, size_t row_ndx, BinaryData value, Instruction variant)
{
    REALM_ASSERT_EX(variant == instr_Set || variant == instr_SetDefault, variant);
    if (value.is_null()) {
        set_null(col_ndx, row_ndx, variant); // Throws
    }
    else {
        StringData value_2(value.data(), value.size());
        append_string_instr(variant, util::tuple(type_Binary, col_ndx, row_ndx), value_2); // Throws
    }
    return true;
}

inline void TransactLogConvenientEncoder::set_binary(const Table* t, size_t col_ndx, size_t ndx, BinaryData value,
                                                     Instruction variant)
{
    select_table(t);                                    // Throws
    m_encoder.set_binary(col_ndx, ndx, value, variant); // Throws
}

inline bool TransactLogEncoder::set_olddatetime(size_t col_ndx, size_t ndx, OldDateTime value, Instruction variant)
{
    REALM_ASSERT_EX(variant == instr_Set || variant == instr_SetDefault, variant);
    append_simple_instr(variant, util::tuple(type_OldDateTime, col_ndx, ndx, value.get_olddatetime())); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_olddatetime(const Table* t, size_t col_ndx, size_t ndx,
                                                          OldDateTime value, Instruction variant)
{
    select_table(t);                                         // Throws
    m_encoder.set_olddatetime(col_ndx, ndx, value, variant); // Throws
}

inline bool TransactLogEncoder::set_timestamp(size_t col_ndx, size_t ndx, Timestamp value, Instruction variant)
{
    REALM_ASSERT_EX(variant == instr_Set || variant == instr_SetDefault, variant);
    append_simple_instr(
        variant, util::tuple(type_Timestamp, col_ndx, ndx, value.get_seconds(), value.get_nanoseconds())); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_timestamp(const Table* t, size_t col_ndx, size_t ndx, Timestamp value,
                                                        Instruction variant)
{
    select_table(t);                                       // Throws
    m_encoder.set_timestamp(col_ndx, ndx, value, variant); // Throws
}

inline bool TransactLogEncoder::set_table(size_t col_ndx, size_t ndx, Instruction variant)
{
    REALM_ASSERT_EX(variant == instr_Set || variant == instr_SetDefault, variant);
    append_simple_instr(variant, util::tuple(type_Table, col_ndx, ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_table(const Table* t, size_t col_ndx, size_t ndx, Instruction variant)
{
    select_table(t);                            // Throws
    m_encoder.set_table(col_ndx, ndx, variant); // Throws
}

inline bool TransactLogEncoder::set_mixed(size_t col_ndx, size_t ndx, const Mixed& value, Instruction variant)
{
    REALM_ASSERT_EX(variant == instr_Set || variant == instr_SetDefault, variant);
    append_mixed_instr(variant, util::tuple(type_Mixed, col_ndx, ndx), value); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_mixed(const Table* t, size_t col_ndx, size_t ndx, const Mixed& value,
                                                    Instruction variant)
{
    select_table(t);                                   // Throws
    m_encoder.set_mixed(col_ndx, ndx, value, variant); // Throws
}

inline bool TransactLogEncoder::set_link(size_t col_ndx, size_t ndx, size_t value, size_t target_group_level_ndx,
                                         Instruction variant)
{
    REALM_ASSERT_EX(variant == instr_Set || variant == instr_SetDefault, variant);
    // Map `realm::npos` to zero, and `n` to `n+1`, where `n` is a target row
    // index.
    size_t value_2 = size_t(1) + value;
    append_simple_instr(variant, util::tuple(type_Link, col_ndx, ndx, value_2, target_group_level_ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_link(const Table* t, size_t col_ndx, size_t ndx, size_t value,
                                                   Instruction variant)
{
    select_table(t); // Throws
    size_t target_group_level_ndx = t->get_descriptor()->get_column_link_target(col_ndx);
    m_encoder.set_link(col_ndx, ndx, value, target_group_level_ndx, variant); // Throws
}

inline bool TransactLogEncoder::set_null(size_t col_ndx, size_t ndx, Instruction variant, size_t prior_num_rows)
{
    REALM_ASSERT_EX(variant == instr_Set || variant == instr_SetDefault || variant == instr_SetUnique, variant);
    if (REALM_UNLIKELY(variant == instr_SetUnique)) {
        append_simple_instr(variant, util::tuple(set_null_sentinel(), col_ndx, ndx, prior_num_rows)); // Throws
    }
    else {
        append_simple_instr(variant, util::tuple(set_null_sentinel(), col_ndx, ndx)); // Throws
    }
    return true;
}

inline void TransactLogConvenientEncoder::set_null(const Table* t, size_t col_ndx, size_t row_ndx,
                                                   Instruction variant)
{
    select_table(t); // Throws
    size_t prior_num_rows = (variant == instr_SetUnique ? t->size() : 0);
    m_encoder.set_null(col_ndx, row_ndx, variant, prior_num_rows); // Throws
}

inline bool TransactLogEncoder::nullify_link(size_t col_ndx, size_t ndx, size_t target_group_level_ndx)
{
    append_simple_instr(instr_NullifyLink, util::tuple(col_ndx, ndx, target_group_level_ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::nullify_link(const Table* t, size_t col_ndx, size_t ndx)
{
    select_table(t); // Throws
    size_t target_group_level_ndx = t->get_descriptor()->get_column_link_target(col_ndx);
    m_encoder.nullify_link(col_ndx, ndx, target_group_level_ndx); // Throws
}

inline bool TransactLogEncoder::insert_substring(size_t col_ndx, size_t row_ndx, size_t pos, StringData value)
{
    append_string_instr(instr_InsertSubstring, util::tuple(col_ndx, row_ndx, pos), value); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::insert_substring(const Table* t, size_t col_ndx, size_t row_ndx, size_t pos,
                                                           StringData value)
{
    if (value.size() > 0) {
        select_table(t);                                          // Throws
        m_encoder.insert_substring(col_ndx, row_ndx, pos, value); // Throws
    }
}

inline bool TransactLogEncoder::erase_substring(size_t col_ndx, size_t row_ndx, size_t pos, size_t size)
{
    append_simple_instr(instr_EraseFromString, util::tuple(col_ndx, row_ndx, pos, size)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::erase_substring(const Table* t, size_t col_ndx, size_t row_ndx, size_t pos,
                                                          size_t size)
{
    if (size > 0) {
        select_table(t);                                        // Throws
        m_encoder.erase_substring(col_ndx, row_ndx, pos, size); // Throws
    }
}

inline bool TransactLogEncoder::insert_empty_rows(size_t row_ndx, size_t num_rows_to_insert, size_t prior_num_rows,
                                                  bool unordered)
{
    append_simple_instr(instr_InsertEmptyRows,
                        util::tuple(row_ndx, num_rows_to_insert, prior_num_rows, unordered)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::insert_empty_rows(const Table* t, size_t row_ndx, size_t num_rows_to_insert,
                                                            size_t prior_num_rows)
{
    select_table(t); // Throws
    bool unordered = false;
    m_encoder.insert_empty_rows(row_ndx, num_rows_to_insert, prior_num_rows, unordered); // Throws
}

inline bool TransactLogEncoder::erase_rows(size_t row_ndx, size_t num_rows_to_erase, size_t prior_num_rows,
                                           bool unordered)
{
    append_simple_instr(instr_EraseRows, util::tuple(row_ndx, num_rows_to_erase, prior_num_rows,
                                                     unordered)); // Throws
    return true;
}


inline void TransactLogConvenientEncoder::erase_rows(const Table* t, size_t row_ndx, size_t num_rows_to_erase,
                                                     size_t prior_num_rows, bool is_move_last_over)
{
    select_table(t); // Throws
    bool unordered = is_move_last_over;
    m_encoder.erase_rows(row_ndx, num_rows_to_erase, prior_num_rows, unordered); // Throws
}

inline bool TransactLogEncoder::swap_rows(size_t row_ndx_1, size_t row_ndx_2)
{
    append_simple_instr(instr_SwapRows, util::tuple(row_ndx_1, row_ndx_2)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::swap_rows(const Table* t, size_t row_ndx_1, size_t row_ndx_2)
{
    REALM_ASSERT(row_ndx_1 < row_ndx_2);
    select_table(t); // Throws
    m_encoder.swap_rows(row_ndx_1, row_ndx_2);
}

inline bool TransactLogEncoder::merge_rows(size_t row_ndx, size_t new_row_ndx)
{
    append_simple_instr(instr_MergeRows, util::tuple(row_ndx, new_row_ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::merge_rows(const Table* t, size_t row_ndx, size_t new_row_ndx)
{
    select_table(t); // Throws
    m_encoder.merge_rows(row_ndx, new_row_ndx);
}

inline bool TransactLogEncoder::add_search_index(size_t col_ndx)
{
    append_simple_instr(instr_AddSearchIndex, util::tuple(col_ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::add_search_index(const Table* t, size_t col_ndx)
{
    select_table(t);                     // Throws
    m_encoder.add_search_index(col_ndx); // Throws
}


inline bool TransactLogEncoder::remove_search_index(size_t col_ndx)
{
    append_simple_instr(instr_RemoveSearchIndex, util::tuple(col_ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::remove_search_index(const Table* t, size_t col_ndx)
{
    select_table(t);                        // Throws
    m_encoder.remove_search_index(col_ndx); // Throws
}

inline bool TransactLogEncoder::set_link_type(size_t col_ndx, LinkType link_type)
{
    append_simple_instr(instr_SetLinkType, util::tuple(col_ndx, int(link_type))); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_link_type(const Table* t, size_t col_ndx, LinkType link_type)
{
    select_table(t);                             // Throws
    m_encoder.set_link_type(col_ndx, link_type); // Throws
}


inline bool TransactLogEncoder::clear_table()
{
    append_simple_instr(instr_ClearTable, util::tuple()); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::clear_table(const Table* t)
{
    select_table(t);         // Throws
    m_encoder.clear_table(); // Throws
}

inline bool TransactLogEncoder::optimize_table()
{
    append_simple_instr(instr_OptimizeTable, util::tuple()); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::optimize_table(const Table* t)
{
    select_table(t);            // Throws
    m_encoder.optimize_table(); // Throws
}

inline bool TransactLogEncoder::link_list_set(size_t link_ndx, size_t value, size_t prior_size)
{
    append_simple_instr(instr_LinkListSet, util::tuple(link_ndx, value, prior_size)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::link_list_set(const LinkView& list, size_t link_ndx, size_t value)
{
    select_link_list(list);                                // Throws
    m_encoder.link_list_set(link_ndx, value, list.size()); // Throws
}

inline bool TransactLogEncoder::link_list_nullify(size_t link_ndx, size_t prior_size)
{
    append_simple_instr(instr_LinkListNullify, util::tuple(link_ndx, prior_size)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::link_list_nullify(const LinkView& list, size_t link_ndx)
{
    select_link_list(list);                            // Throws
    size_t prior_size = list.size();                   // Instruction is emitted before the fact.
    m_encoder.link_list_nullify(link_ndx, prior_size); // Throws
}

inline bool TransactLogEncoder::link_list_set_all(const IntegerColumn& values)
{
    struct iter {
        iter(const IntegerColumn& iter_values, size_t ndx)
            : m_values(&iter_values)
            , m_ndx(ndx)
        {
        }
        const IntegerColumn* m_values;
        size_t m_ndx;
        bool operator==(const iter& i) const
        {
            return m_ndx == i.m_ndx;
        }
        bool operator!=(const iter& i) const
        {
            return m_ndx != i.m_ndx;
        }
        size_t operator-(const iter& i) const
        {
            return m_ndx - i.m_ndx;
        }
        int_fast64_t operator*() const
        {
            return m_values->get(m_ndx);
        }
        iter& operator++()
        {
            ++m_ndx;
            return *this;
        }
        iter operator++(int)
        {
            iter i = *this;
            ++m_ndx;
            return i;
        }
    };
    size_t num_values = values.size();
    append_variable_size_instr(instr_LinkListSetAll, util::tuple(num_values), iter(values, 0),
                               iter(values, num_values)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_link_list(const LinkView& list, const IntegerColumn& values)
{
    select_link_list(list);              // Throws
    m_encoder.link_list_set_all(values); // Throws
}

inline bool TransactLogEncoder::link_list_insert(size_t link_ndx, size_t value, size_t prior_size)
{
    append_simple_instr(instr_LinkListInsert, util::tuple(link_ndx, value, prior_size)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::link_list_insert(const LinkView& list, size_t link_ndx, size_t value)
{
    select_link_list(list);                                  // Throws
    size_t prior_size = list.size() - 1;                     // The instruction is emitted after the fact.
    m_encoder.link_list_insert(link_ndx, value, prior_size); // Throws
}

inline bool TransactLogEncoder::link_list_move(size_t from_link_ndx, size_t to_link_ndx)
{
    REALM_ASSERT(from_link_ndx != to_link_ndx);
    append_simple_instr(instr_LinkListMove, util::tuple(from_link_ndx, to_link_ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::link_list_move(const LinkView& list, size_t from_link_ndx,
                                                         size_t to_link_ndx)
{
    select_link_list(list);                               // Throws
    m_encoder.link_list_move(from_link_ndx, to_link_ndx); // Throws
}

inline bool TransactLogEncoder::link_list_swap(size_t link1_ndx, size_t link2_ndx)
{
    append_simple_instr(instr_LinkListSwap, util::tuple(link1_ndx, link2_ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::link_list_swap(const LinkView& list, size_t link1_ndx, size_t link2_ndx)
{
    select_link_list(list);                         // Throws
    m_encoder.link_list_swap(link1_ndx, link2_ndx); // Throws
}

inline bool TransactLogEncoder::link_list_erase(size_t link_ndx, size_t prior_size)
{
    append_simple_instr(instr_LinkListErase, util::tuple(link_ndx, prior_size)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::link_list_erase(const LinkView& list, size_t link_ndx)
{
    select_link_list(list);                          // Throws
    size_t prior_size = list.size();                 // The instruction is emitted before the fact.
    m_encoder.link_list_erase(link_ndx, prior_size); // Throws
}

inline bool TransactLogEncoder::link_list_clear(size_t old_list_size)
{
    append_simple_instr(instr_LinkListClear, util::tuple(old_list_size)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::on_table_destroyed(const Table* t) noexcept
{
    if (m_selected_table == t)
        m_selected_table = nullptr;
}

inline void TransactLogConvenientEncoder::on_spec_destroyed(const Spec* s) noexcept
{
    if (m_selected_spec == s)
        m_selected_spec = nullptr;
}


inline void TransactLogConvenientEncoder::on_link_list_destroyed(const LinkView& list) noexcept
{
    const LinkView* lw_ptr = &list;
    // atomically clear m_selected_link_list iff it already points to 'list':
    // (lw_ptr will be modified if the swap fails, but we ignore that)
    m_selected_link_list.compare_exchange_strong(lw_ptr, nullptr, std::memory_order_relaxed,
                                                 std::memory_order_relaxed);
}


inline TransactLogParser::TransactLogParser()
    : m_input_buffer(1024) // Throws
{
}


inline TransactLogParser::~TransactLogParser() noexcept
{
}


template <class InstructionHandler>
void TransactLogParser::parse(NoCopyInputStream& in, InstructionHandler& handler)
{
    m_input = &in;
    m_input_begin = m_input_end = nullptr;

    while (has_next())
        parse_one(handler); // Throws
}

template <class InstructionHandler>
void TransactLogParser::parse(InputStream& in, InstructionHandler& handler)
{
    NoCopyInputStreamAdaptor in_2(in, m_input_buffer.data(), m_input_buffer.size());
    parse(in_2, handler); // Throws
}

inline bool TransactLogParser::has_next() noexcept
{
    return m_input_begin != m_input_end || next_input_buffer();
}

template <class InstructionHandler>
void TransactLogParser::parse_one(InstructionHandler& handler)
{
    char instr_ch;
    if (!read_char(instr_ch))
        parser_error(); // Throws
    //    std::cerr << "parsing " << util::promote(instr) << " @ " << std::hex << long(m_input_begin) << std::dec <<
    //    "\n";
    Instruction instr = Instruction(instr_ch);
    switch (instr) {
        case instr_SetDefault:
        case instr_SetUnique:
        case instr_Set: {
            int type = read_int<int>();          // Throws
            size_t col_ndx = read_int<size_t>(); // Throws
            size_t row_ndx = read_int<size_t>(); // Throws
            size_t prior_num_rows = 0;
            if (REALM_UNLIKELY(instr == instr_SetUnique))
                prior_num_rows = read_int<size_t>(); // Throws

            if (type == TransactLogEncoder::set_null_sentinel()) {
                // Special case for set_null
                if (!handler.set_null(col_ndx, row_ndx, instr, prior_num_rows)) // Throws
                    parser_error();
                return;
            }

            switch (DataType(type)) {
                case type_Int: {
                    int_fast64_t value = read_int<int64_t>();                             // Throws
                    if (!handler.set_int(col_ndx, row_ndx, value, instr, prior_num_rows)) // Throws
                        parser_error();
                    return;
                }
                case type_Bool: {
                    bool value = read_bool();                              // Throws
                    if (!handler.set_bool(col_ndx, row_ndx, value, instr)) // Throws
                        parser_error();
                    return;
                }
                case type_Float: {
                    float value = read_float();                             // Throws
                    if (!handler.set_float(col_ndx, row_ndx, value, instr)) // Throws
                        parser_error();
                    return;
                }
                case type_Double: {
                    double value = read_double();                            // Throws
                    if (!handler.set_double(col_ndx, row_ndx, value, instr)) // Throws
                        parser_error();
                    return;
                }
                case type_String: {
                    StringData value = read_string(m_string_buffer);                         // Throws
                    if (!handler.set_string(col_ndx, row_ndx, value, instr, prior_num_rows)) // Throws
                        parser_error();
                    return;
                }
                case type_Binary: {
                    BinaryData value = read_binary(m_string_buffer);         // Throws
                    if (!handler.set_binary(col_ndx, row_ndx, value, instr)) // Throws
                        parser_error();
                    return;
                }
                case type_OldDateTime: {
                    int_fast64_t value = read_int<int_fast64_t>();                // Throws
                    if (!handler.set_olddatetime(col_ndx, row_ndx, value, instr)) // Throws
                        parser_error();
                    return;
                }
                case type_Timestamp: {
                    int64_t seconds = read_int<int64_t>();     // Throws
                    int32_t nanoseconds = read_int<int32_t>(); // Throws
                    Timestamp value = Timestamp(seconds, nanoseconds);
                    if (!handler.set_timestamp(col_ndx, row_ndx, value, instr)) // Throws
                        parser_error();
                    return;
                }
                case type_Table: {
                    if (!handler.set_table(col_ndx, row_ndx, instr)) // Throws
                        parser_error();
                    return;
                }
                case type_Mixed: {
                    Mixed value;
                    read_mixed(&value);                                     // Throws
                    if (!handler.set_mixed(col_ndx, row_ndx, value, instr)) // Throws
                        parser_error();
                    return;
                }
                case type_Link: {
                    size_t value = read_int<size_t>(); // Throws
                    // Map zero to realm::npos, and `n+1` to `n`, where `n` is a target row index.
                    size_t target_row_ndx = size_t(value - 1);
                    size_t target_group_level_ndx = read_int<size_t>();                                     // Throws
                    if (!handler.set_link(col_ndx, row_ndx, target_row_ndx, target_group_level_ndx, instr)) // Throws
                        parser_error();
                    return;
                }
                case type_LinkList: {
                    // Unsupported column type for Set.
                    parser_error();
                    return;
                }
            }
            parser_error();
            return;
        }
        case instr_AddInteger: {
            size_t col_ndx = read_int<size_t>();           // Throws
            size_t row_ndx = read_int<size_t>();           // Throws
            int_fast64_t value = read_int<int64_t>();      // Throws
            if (!handler.add_int(col_ndx, row_ndx, value)) // Throws
                parser_error();
            return;
        }
        case instr_NullifyLink: {
            size_t col_ndx = read_int<size_t>();                                 // Throws
            size_t row_ndx = read_int<size_t>();                                 // Throws
            size_t target_group_level_ndx = read_int<size_t>();                  // Throws
            if (!handler.nullify_link(col_ndx, row_ndx, target_group_level_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_InsertSubstring: {
            size_t col_ndx = read_int<size_t>();                         // Throws
            size_t row_ndx = read_int<size_t>();                         // Throws
            size_t pos = read_int<size_t>();                             // Throws
            StringData value = read_string(m_string_buffer);             // Throws
            if (!handler.insert_substring(col_ndx, row_ndx, pos, value)) // Throws
                parser_error();
            return;
        }
        case instr_EraseFromString: {
            size_t col_ndx = read_int<size_t>();                       // Throws
            size_t row_ndx = read_int<size_t>();                       // Throws
            size_t pos = read_int<size_t>();                           // Throws
            size_t size = read_int<size_t>();                          // Throws
            if (!handler.erase_substring(col_ndx, row_ndx, pos, size)) // Throws
                parser_error();
            return;
        }
        case instr_InsertEmptyRows: {
            size_t row_ndx = read_int<size_t>();                                                    // Throws
            size_t num_rows_to_insert = read_int<size_t>();                                         // Throws
            size_t prior_num_rows = read_int<size_t>();                                             // Throws
            bool unordered = read_bool();                                                           // Throws
            if (!handler.insert_empty_rows(row_ndx, num_rows_to_insert, prior_num_rows, unordered)) // Throws
                parser_error();
            return;
        }
        case instr_EraseRows: {
            size_t row_ndx = read_int<size_t>();                                            // Throws
            size_t num_rows_to_erase = read_int<size_t>();                                  // Throws
            size_t prior_num_rows = read_int<size_t>();                                     // Throws
            bool unordered = read_bool();                                                   // Throws
            if (!handler.erase_rows(row_ndx, num_rows_to_erase, prior_num_rows, unordered)) // Throws
                parser_error();
            return;
        }
        case instr_SwapRows: {
            size_t row_ndx_1 = read_int<size_t>();        // Throws
            size_t row_ndx_2 = read_int<size_t>();        // Throws
            if (!handler.swap_rows(row_ndx_1, row_ndx_2)) // Throws
                parser_error();
            return;
        }
        case instr_MergeRows: {
            size_t row_ndx = read_int<size_t>();           // Throws
            size_t new_row_ndx = read_int<size_t>();       // Throws
            if (!handler.merge_rows(row_ndx, new_row_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_SelectTable: {
            int levels = read_int<int>(); // Throws
            if (levels < 0 || levels > m_max_levels)
                parser_error();
            m_path.reserve(0, 2 * levels); // Throws
            size_t* path = m_path.data();
            size_t group_level_ndx = read_int<size_t>(); // Throws
            for (int i = 0; i != levels; ++i) {
                size_t col_ndx = read_int<size_t>(); // Throws
                size_t row_ndx = read_int<size_t>(); // Throws
                path[2 * i + 0] = col_ndx;
                path[2 * i + 1] = row_ndx;
            }
            if (!handler.select_table(group_level_ndx, levels, path)) // Throws
                parser_error();
            return;
        }
        case instr_ClearTable: {
            if (!handler.clear_table()) // Throws
                parser_error();
            return;
        }
        case instr_LinkListSet: {
            size_t link_ndx = read_int<size_t>();                    // Throws
            size_t value = read_int<size_t>();                       // Throws
            size_t prior_size = read_int<size_t>();                  // Throws
            if (!handler.link_list_set(link_ndx, value, prior_size)) // Throws
                parser_error();
            return;
        }
        case instr_LinkListSetAll: {
            // todo, log that it's a SetAll we're doing
            size_t size = read_int<size_t>(); // Throws
            for (size_t i = 0; i < size; i++) {
                size_t link = read_int<size_t>();          // Throws
                if (!handler.link_list_set(i, link, size)) // Throws
                    parser_error();
            }
            return;
        }
        case instr_LinkListInsert: {
            size_t link_ndx = read_int<size_t>();                       // Throws
            size_t value = read_int<size_t>();                          // Throws
            size_t prior_size = read_int<size_t>();                     // Throws
            if (!handler.link_list_insert(link_ndx, value, prior_size)) // Throws
                parser_error();
            return;
        }
        case instr_LinkListMove: {
            size_t from_link_ndx = read_int<size_t>();               // Throws
            size_t to_link_ndx = read_int<size_t>();                 // Throws
            if (!handler.link_list_move(from_link_ndx, to_link_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_LinkListSwap: {
            size_t link1_ndx = read_int<size_t>();             // Throws
            size_t link2_ndx = read_int<size_t>();             // Throws
            if (!handler.link_list_swap(link1_ndx, link2_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_LinkListErase: {
            size_t link_ndx = read_int<size_t>();               // Throws
            size_t prior_size = read_int<size_t>();             // Throws
            if (!handler.link_list_erase(link_ndx, prior_size)) // Throws
                parser_error();
            return;
        }
        case instr_LinkListNullify: {
            size_t link_ndx = read_int<size_t>();                 // Throws
            size_t prior_size = read_int<size_t>();               // Throws
            if (!handler.link_list_nullify(link_ndx, prior_size)) // Throws
                parser_error();
            return;
        }
        case instr_LinkListClear: {
            size_t old_list_size = read_int<size_t>();   // Throws
            if (!handler.link_list_clear(old_list_size)) // Throws
                parser_error();
            return;
        }
        case instr_SelectLinkList: {
            size_t col_ndx = read_int<size_t>();                                     // Throws
            size_t row_ndx = read_int<size_t>();                                     // Throws
            size_t target_group_level_ndx = read_int<size_t>();                      // Throws
            if (!handler.select_link_list(col_ndx, row_ndx, target_group_level_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_AddSearchIndex: {
            size_t col_ndx = read_int<size_t>();    // Throws
            if (!handler.add_search_index(col_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_RemoveSearchIndex: {
            size_t col_ndx = read_int<size_t>();       // Throws
            if (!handler.remove_search_index(col_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_SetLinkType: {
            size_t col_ndx = read_int<size_t>(); // Throws
            int link_type = read_int<int>();     // Throws
            if (!is_valid_link_type(link_type))
                parser_error();
            if (!handler.set_link_type(col_ndx, LinkType(link_type))) // Throws
                parser_error();
            return;
        }
        case instr_InsertColumn:
        case instr_InsertNullableColumn: {
            size_t col_ndx = read_int<size_t>(); // Throws
            int type = read_int<int>();          // Throws
            if (!is_valid_data_type(type))
                parser_error();
            if (REALM_UNLIKELY(type == type_Link || type == type_LinkList))
                parser_error();
            StringData name = read_string(m_string_buffer); // Throws
            bool nullable = (Instruction(instr) == instr_InsertNullableColumn);
            if (REALM_UNLIKELY(nullable && (type == type_Table || type == type_Mixed))) {
                // Nullability not supported for Table and Mixed columns.
                parser_error();
            }
            if (!handler.insert_column(col_ndx, DataType(type), name, nullable)) // Throws
                parser_error();
            return;
        }
        case instr_InsertLinkColumn: {
            size_t col_ndx = read_int<size_t>(); // Throws
            int type = read_int<int>();          // Throws
            if (!is_valid_data_type(type))
                parser_error();
            if (REALM_UNLIKELY(type != type_Link && type != type_LinkList))
                parser_error();
            size_t link_target_table_ndx = read_int<size_t>(); // Throws
            size_t backlink_col_ndx = read_int<size_t>();      // Throws
            StringData name = read_string(m_string_buffer);    // Throws
            if (!handler.insert_link_column(col_ndx, DataType(type), name, link_target_table_ndx,
                                            backlink_col_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_EraseColumn: {
            size_t col_ndx = read_int<size_t>(); // Throws
            if (!handler.erase_column(col_ndx))  // Throws
                parser_error();
            return;
        }
        case instr_EraseLinkColumn: {
            size_t col_ndx = read_int<size_t>();                                              // Throws
            size_t link_target_table_ndx = read_int<size_t>();                                // Throws
            size_t backlink_col_ndx = read_int<size_t>();                                     // Throws
            if (!handler.erase_link_column(col_ndx, link_target_table_ndx, backlink_col_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_RenameColumn: {
            size_t col_ndx = read_int<size_t>();            // Throws
            StringData name = read_string(m_string_buffer); // Throws
            if (!handler.rename_column(col_ndx, name))      // Throws
                parser_error();
            return;
        }
        case instr_MoveColumn: {
            size_t col_ndx_1 = read_int<size_t>();          // Throws
            size_t col_ndx_2 = read_int<size_t>();          // Throws
            if (!handler.move_column(col_ndx_1, col_ndx_2)) // Throws
                parser_error();
            return;
        }
        case instr_SelectDescriptor: {
            int levels = read_int<int>(); // Throws
            if (levels < 0 || levels > m_max_levels)
                parser_error();
            m_path.reserve(0, levels); // Throws
            size_t* path = m_path.data();
            for (int i = 0; i != levels; ++i) {
                size_t col_ndx = read_int<size_t>(); // Throws
                path[i] = col_ndx;
            }
            if (!handler.select_descriptor(levels, path)) // Throws
                parser_error();
            return;
        }
        case instr_InsertGroupLevelTable: {
            size_t table_ndx = read_int<size_t>();                              // Throws
            size_t num_tables = read_int<size_t>();                             // Throws
            StringData name = read_string(m_string_buffer);                     // Throws
            if (!handler.insert_group_level_table(table_ndx, num_tables, name)) // Throws
                parser_error();
            return;
        }
        case instr_EraseGroupLevelTable: {
            size_t table_ndx = read_int<size_t>();                             // Throws
            size_t prior_num_tables = read_int<size_t>();                      // Throws
            if (!handler.erase_group_level_table(table_ndx, prior_num_tables)) // Throws
                parser_error();
            return;
        }
        case instr_RenameGroupLevelTable: {
            size_t table_ndx = read_int<size_t>();                      // Throws
            StringData new_name = read_string(m_string_buffer);         // Throws
            if (!handler.rename_group_level_table(table_ndx, new_name)) // Throws
                parser_error();
            return;
        }
        case instr_MoveGroupLevelTable: {
            size_t from_table_ndx = read_int<size_t>();                        // Throws
            size_t to_table_ndx = read_int<size_t>();                          // Throws
            if (!handler.move_group_level_table(from_table_ndx, to_table_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_OptimizeTable: {
            if (!handler.optimize_table()) // Throws
                parser_error();
            return;
        }
    }

    throw BadTransactLog();
}


template <class T>
T TransactLogParser::read_int()
{
    T value = 0;
    int part = 0;
    const int max_bytes = (std::numeric_limits<T>::digits + 1 + 6) / 7;
    for (int i = 0; i != max_bytes; ++i) {
        char c;
        if (!read_char(c))
            goto bad_transact_log;
        part = static_cast<unsigned char>(c);
        if (0xFF < part)
            goto bad_transact_log; // Only the first 8 bits may be used in each byte
        if ((part & 0x80) == 0) {
            T p = part & 0x3F;
            if (util::int_shift_left_with_overflow_detect(p, i * 7))
                goto bad_transact_log;
            value |= p;
            break;
        }
        if (i == max_bytes - 1)
            goto bad_transact_log; // Too many bytes
        value |= T(part & 0x7F) << (i * 7);
    }
    if (part & 0x40) {
        // The real value is negative. Because 'value' is positive at
        // this point, the following negation is guaranteed by C++11
        // to never overflow. See C99+TC3 section 6.2.6.2 paragraph 2.
        REALM_DIAG_PUSH();
        REALM_DIAG_IGNORE_UNSIGNED_MINUS();
        value = -value;
        REALM_DIAG_POP();
        if (util::int_subtract_with_overflow_detect(value, 1))
            goto bad_transact_log;
    }
    return value;

bad_transact_log:
    throw BadTransactLog();
}


inline void TransactLogParser::read_bytes(char* data, size_t size)
{
    for (;;) {
        const size_t avail = m_input_end - m_input_begin;
        if (size <= avail)
            break;
        std::copy_n(m_input_begin, avail, data);
        if (!next_input_buffer())
            throw BadTransactLog();
        data += avail;
        size -= avail;
    }
    const char* to = m_input_begin + size;
    std::copy_n(m_input_begin, size, data);
    m_input_begin = to;
}


inline BinaryData TransactLogParser::read_buffer(util::StringBuffer& buf, size_t size)
{
    const size_t avail = m_input_end - m_input_begin;
    if (avail >= size) {
        m_input_begin += size;
        return BinaryData(m_input_begin - size, size);
    }

    buf.clear();
    buf.resize(size); // Throws
    read_bytes(buf.data(), size);
    return BinaryData(buf.data(), size);
}


inline bool TransactLogParser::read_bool()
{
    return read_int<char>();
}


inline float TransactLogParser::read_float()
{
    static_assert(std::numeric_limits<float>::is_iec559 &&
                      sizeof(float) * std::numeric_limits<unsigned char>::digits == 32,
                  "Unsupported 'float' representation");
    float value;
    read_bytes(reinterpret_cast<char*>(&value), sizeof value); // Throws
    return value;
}


inline double TransactLogParser::read_double()
{
    static_assert(std::numeric_limits<double>::is_iec559 &&
                      sizeof(double) * std::numeric_limits<unsigned char>::digits == 64,
                  "Unsupported 'double' representation");
    double value;
    read_bytes(reinterpret_cast<char*>(&value), sizeof value); // Throws
    return value;
}


inline StringData TransactLogParser::read_string(util::StringBuffer& buf)
{
    size_t size = read_int<size_t>(); // Throws

    if (size > Table::max_string_size)
        parser_error();

    BinaryData buffer = read_buffer(buf, size);
    return StringData{buffer.data(), size};
}

inline Timestamp TransactLogParser::read_timestamp()
{
    REALM_ASSERT(false);
    return Timestamp{};
}


inline BinaryData TransactLogParser::read_binary(util::StringBuffer& buf)
{
    size_t size = read_int<size_t>(); // Throws

    if (size > ArrayBlob::max_binary_size)
        parser_error();

    return read_buffer(buf, size);
}


inline void TransactLogParser::read_mixed(Mixed* mixed)
{
    DataType type = DataType(read_int<int>()); // Throws
    switch (type) {
        case type_Int: {
            int_fast64_t value = read_int<int64_t>(); // Throws
            mixed->set_int(value);
            return;
        }
        case type_Bool: {
            bool value = read_bool(); // Throws
            mixed->set_bool(value);
            return;
        }
        case type_Float: {
            float value = read_float(); // Throws
            mixed->set_float(value);
            return;
        }
        case type_Double: {
            double value = read_double(); // Throws
            mixed->set_double(value);
            return;
        }
        case type_OldDateTime: {
            int_fast64_t value = read_int<int_fast64_t>(); // Throws
            mixed->set_olddatetime(value);
            return;
        }
        case type_Timestamp: {
            Timestamp value = read_timestamp(); // Throws
            mixed->set_timestamp(value);
            return;
        }
        case type_String: {
            StringData value = read_string(m_string_buffer); // Throws
            mixed->set_string(value);
            return;
        }
        case type_Binary: {
            BinaryData value = read_binary(m_string_buffer); // Throws
            mixed->set_binary(value);
            return;
        }
        case type_Table: {
            *mixed = Mixed::subtable_tag();
            return;
        }
        case type_Mixed:
            break;
        case type_Link:
        case type_LinkList:
            // FIXME: Need to handle new link types here
            break;
    }
    throw BadTransactLog();
}


inline bool TransactLogParser::next_input_buffer()
{
    return m_input->next_block(m_input_begin, m_input_end);
}


inline bool TransactLogParser::read_char(char& c)
{
    if (m_input_begin == m_input_end && !next_input_buffer())
        return false;
    c = *m_input_begin++;
    return true;
}


inline bool TransactLogParser::is_valid_data_type(int type)
{
    switch (DataType(type)) {
        case type_Int:
        case type_Bool:
        case type_Float:
        case type_Double:
        case type_String:
        case type_Binary:
        case type_OldDateTime:
        case type_Timestamp:
        case type_Table:
        case type_Mixed:
        case type_Link:
        case type_LinkList:
            return true;
    }
    return false;
}


inline bool TransactLogParser::is_valid_link_type(int type)
{
    switch (LinkType(type)) {
        case link_Strong:
        case link_Weak:
            return true;
    }
    return false;
}


class TransactReverser {
public:
    bool select_table(size_t group_level_ndx, size_t levels, const size_t* path)
    {
        sync_table();
        m_encoder.select_table(group_level_ndx, levels, path);
        m_pending_ts_instr = get_inst();
        return true;
    }

    bool select_descriptor(size_t levels, const size_t* path)
    {
        sync_descriptor();
        m_encoder.select_descriptor(levels, path);
        m_pending_ds_instr = get_inst();
        return true;
    }

    bool insert_group_level_table(size_t table_ndx, size_t num_tables, StringData)
    {
        sync_table();
        m_encoder.erase_group_level_table(table_ndx, num_tables + 1);
        append_instruction();
        return true;
    }

    bool erase_group_level_table(size_t table_ndx, size_t num_tables)
    {
        sync_table();
        m_encoder.insert_group_level_table(table_ndx, num_tables - 1, "");
        append_instruction();
        return true;
    }

    bool rename_group_level_table(size_t, StringData)
    {
        sync_table();
        return true;
    }

    bool move_group_level_table(size_t from_table_ndx, size_t to_table_ndx)
    {
        sync_table();
        m_encoder.move_group_level_table(to_table_ndx, from_table_ndx);
        append_instruction();
        return true;
    }

    bool optimize_table()
    {
        return true; // No-op
    }

    bool insert_empty_rows(size_t row_ndx, size_t num_rows_to_insert, size_t prior_num_rows, bool unordered)
    {
        size_t num_rows_to_erase = num_rows_to_insert;
        size_t prior_num_rows_2 = prior_num_rows + num_rows_to_insert;
        m_encoder.erase_rows(row_ndx, num_rows_to_erase, prior_num_rows_2, unordered); // Throws
        append_instruction();
        return true;
    }

    bool erase_rows(size_t row_ndx, size_t num_rows_to_erase, size_t prior_num_rows, bool unordered)
    {
        size_t num_rows_to_insert = num_rows_to_erase;
        // Number of rows in table after removal, but before inverse insertion
        size_t prior_num_rows_2 = prior_num_rows - num_rows_to_erase;
        m_encoder.insert_empty_rows(row_ndx, num_rows_to_insert, prior_num_rows_2, unordered); // Throws
        append_instruction();
        return true;
    }

    bool swap_rows(size_t row_ndx_1, size_t row_ndx_2)
    {
        m_encoder.swap_rows(row_ndx_1, row_ndx_2);
        append_instruction();
        return true;
    }

    bool merge_rows(size_t row_ndx, size_t new_row_ndx)
    {
        static_cast<void>(row_ndx);
        static_cast<void>(new_row_ndx);
        // There is no instruction we can generate here to change back.
        return true;
    }

    bool set_int(size_t col_ndx, size_t row_ndx, int_fast64_t value, Instruction variant, size_t prior_num_rows)
    {
        m_encoder.set_int(col_ndx, row_ndx, value, variant, prior_num_rows);
        append_instruction();
        return true;
    }

    bool add_int(size_t col_ndx, size_t row_ndx, int_fast64_t value)
    {
        m_encoder.add_int(col_ndx, row_ndx, -value);
        append_instruction();
        return true;
    }

    bool set_bool(size_t col_ndx, size_t row_ndx, bool value, Instruction variant)
    {
        m_encoder.set_bool(col_ndx, row_ndx, value, variant);
        append_instruction();
        return true;
    }

    bool set_float(size_t col_ndx, size_t row_ndx, float value, Instruction variant)
    {
        m_encoder.set_float(col_ndx, row_ndx, value, variant);
        append_instruction();
        return true;
    }

    bool set_double(size_t col_ndx, size_t row_ndx, double value, Instruction variant)
    {
        m_encoder.set_double(col_ndx, row_ndx, value, variant);
        append_instruction();
        return true;
    }

    bool set_string(size_t col_ndx, size_t row_ndx, StringData value, Instruction variant, size_t prior_num_rows)
    {
        m_encoder.set_string(col_ndx, row_ndx, value, variant, prior_num_rows);
        append_instruction();
        return true;
    }

    bool set_binary(size_t col_ndx, size_t row_ndx, BinaryData value, Instruction variant)
    {
        m_encoder.set_binary(col_ndx, row_ndx, value, variant);
        append_instruction();
        return true;
    }

    bool set_olddatetime(size_t col_ndx, size_t row_ndx, OldDateTime value, Instruction variant)
    {
        m_encoder.set_olddatetime(col_ndx, row_ndx, value, variant);
        append_instruction();
        return true;
    }

    bool set_timestamp(size_t col_ndx, size_t row_ndx, Timestamp value, Instruction variant)
    {
        m_encoder.set_timestamp(col_ndx, row_ndx, value, variant);
        append_instruction();
        return true;
    }

    bool set_table(size_t col_ndx, size_t row_ndx, Instruction variant)
    {
        m_encoder.set_table(col_ndx, row_ndx, variant);
        append_instruction();
        return true;
    }

    bool set_mixed(size_t col_ndx, size_t row_ndx, const Mixed& value, Instruction variant)
    {
        m_encoder.set_mixed(col_ndx, row_ndx, value, variant);
        append_instruction();
        return true;
    }

    bool set_null(size_t col_ndx, size_t row_ndx, Instruction variant, size_t prior_num_rows)
    {
        m_encoder.set_null(col_ndx, row_ndx, variant, prior_num_rows);
        append_instruction();
        return true;
    }

    bool set_link(size_t col_ndx, size_t row_ndx, size_t value, size_t target_group_level_ndx, Instruction variant)
    {
        m_encoder.set_link(col_ndx, row_ndx, value, target_group_level_ndx, variant);
        append_instruction();
        return true;
    }

    bool insert_substring(size_t, size_t, size_t, StringData)
    {
        return true; // No-op
    }

    bool erase_substring(size_t, size_t, size_t, size_t)
    {
        return true; // No-op
    }

    bool clear_table()
    {
        // FIXME: Add a comment on why we call insert_empty_rows() inside clear_table()
        m_encoder.insert_empty_rows(0, 0, 0, true);
        append_instruction();
        return true;
    }

    bool add_search_index(size_t)
    {
        return true; // No-op
    }

    bool remove_search_index(size_t)
    {
        return true; // No-op
    }

    bool set_link_type(size_t, LinkType)
    {
        return true; // No-op
    }

    bool insert_link_column(size_t col_idx, DataType, StringData, size_t target_table_idx, size_t backlink_col_ndx)
    {
        m_encoder.erase_link_column(col_idx, target_table_idx, backlink_col_ndx);
        append_instruction();
        return true;
    }

    bool erase_link_column(size_t col_idx, size_t target_table_idx, size_t backlink_col_idx)
    {
        DataType type = type_Link; // The real type of the column doesn't matter here,
        // but the encoder asserts that it's actually a link type.
        m_encoder.insert_link_column(col_idx, type, "", target_table_idx, backlink_col_idx);
        append_instruction();
        return true;
    }

    bool insert_column(size_t col_idx, DataType, StringData, bool)
    {
        m_encoder.erase_column(col_idx);
        append_instruction();
        return true;
    }

    bool erase_column(size_t col_idx)
    {
        m_encoder.insert_column(col_idx, DataType(), "");
        append_instruction();
        return true;
    }

    bool rename_column(size_t, StringData)
    {
        return true; // No-op
    }

    bool move_column(size_t col_ndx_1, size_t col_ndx_2)
    {
        m_encoder.move_column(col_ndx_2, col_ndx_1);
        append_instruction();
        return true;
    }

    bool select_link_list(size_t col_ndx, size_t row_ndx, size_t link_target_group_level_ndx)
    {
        sync_linkview();
        m_encoder.select_link_list(col_ndx, row_ndx, link_target_group_level_ndx);
        m_pending_lv_instr = get_inst();
        return true;
    }

    bool link_list_set(size_t row, size_t value, size_t prior_size)
    {
        m_encoder.link_list_set(row, value, prior_size);
        append_instruction();
        return true;
    }

    bool link_list_insert(size_t link_ndx, size_t, size_t prior_size)
    {
        m_encoder.link_list_erase(link_ndx, prior_size + 1);
        append_instruction();
        return true;
    }

    bool link_list_move(size_t from_link_ndx, size_t to_link_ndx)
    {
        m_encoder.link_list_move(from_link_ndx, to_link_ndx);
        append_instruction();
        return true;
    }

    bool link_list_swap(size_t link1_ndx, size_t link2_ndx)
    {
        m_encoder.link_list_swap(link1_ndx, link2_ndx);
        append_instruction();
        return true;
    }

    bool link_list_erase(size_t link_ndx, size_t prior_size)
    {
        m_encoder.link_list_insert(link_ndx, 0, prior_size - 1);
        append_instruction();
        return true;
    }

    bool link_list_clear(size_t old_list_size)
    {
        // Append in reverse order because the reversed log is itself applied
        // in reverse, and this way it generates all back-insertions rather than
        // all front-insertions
        for (size_t i = old_list_size; i > 0; --i) {
            m_encoder.link_list_insert(i - 1, 0, old_list_size - i);
            append_instruction();
        }
        return true;
    }

    bool nullify_link(size_t col_ndx, size_t row_ndx, size_t target_group_level_ndx)
    {
        size_t value = 0;
        // FIXME: Is zero this right value to pass here, or should
        // TransactReverser::nullify_link() also have taken a
        // `target_group_level_ndx` argument.
        m_encoder.set_link(col_ndx, row_ndx, value, target_group_level_ndx);
        append_instruction();
        return true;
    }

    bool link_list_nullify(size_t link_ndx, size_t prior_size)
    {
        m_encoder.link_list_insert(link_ndx, 0, prior_size - 1);
        append_instruction();
        return true;
    }

private:
    _impl::TransactLogBufferStream m_buffer;
    _impl::TransactLogEncoder m_encoder{m_buffer};
    struct Instr {
        size_t begin;
        size_t end;
    };
    std::vector<Instr> m_instructions;
    size_t current_instr_start = 0;
    Instr m_pending_ts_instr{0, 0};
    Instr m_pending_ds_instr{0, 0};
    Instr m_pending_lv_instr{0, 0};

    Instr get_inst()
    {
        Instr instr;
        instr.begin = current_instr_start;
        current_instr_start = transact_log_size();
        instr.end = current_instr_start;
        return instr;
    }

    size_t transact_log_size() const
    {
        REALM_ASSERT_3(m_encoder.write_position(), >=, m_buffer.transact_log_data());
        return m_encoder.write_position() - m_buffer.transact_log_data();
    }

    void append_instruction()
    {
        m_instructions.push_back(get_inst());
    }

    void append_instruction(Instr instr)
    {
        m_instructions.push_back(instr);
    }

    void sync_select(Instr& pending_instr)
    {
        if (pending_instr.begin != pending_instr.end) {
            append_instruction(pending_instr);
            pending_instr = {0, 0};
        }
    }

    void sync_linkview()
    {
        sync_select(m_pending_lv_instr);
    }

    void sync_descriptor()
    {
        sync_linkview();
        sync_select(m_pending_ds_instr);
    }

    void sync_table()
    {
        sync_descriptor();
        sync_select(m_pending_ts_instr);
    }

    friend class ReversedNoCopyInputStream;
};


class ReversedNoCopyInputStream : public NoCopyInputStream {
public:
    ReversedNoCopyInputStream(TransactReverser& reverser)
        : m_instr_order(reverser.m_instructions)
    {
        // push any pending select_table or select_descriptor into the buffer
        reverser.sync_table();

        m_buffer = reverser.m_buffer.transact_log_data();
        m_current = m_instr_order.size();
    }

    bool next_block(const char*& begin, const char*& end) override
    {
        if (m_current != 0) {
            m_current--;
            begin = m_buffer + m_instr_order[m_current].begin;
            end = m_buffer + m_instr_order[m_current].end;
            return (end > begin);
        }
        return false;
    }

private:
    const char* m_buffer;
    std::vector<TransactReverser::Instr>& m_instr_order;
    size_t m_current;
};

} // namespace _impl
} // namespace realm

#endif // REALM_IMPL_TRANSACT_LOG_HPP
