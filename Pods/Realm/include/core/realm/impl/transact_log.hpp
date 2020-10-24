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
#include <realm/util/buffer.hpp>
#include <realm/util/string_buffer.hpp>
#include <realm/impl/input_stream.hpp>

#include <realm/group.hpp>
#include <realm/list.hpp>

namespace realm {

struct GlobalKey;

namespace _impl {

/// Transaction log instruction encoding
/// NOTE: Any change to this enum is a file-format breaking change.
enum Instruction {
    instr_InsertGroupLevelTable = 1,
    instr_EraseGroupLevelTable = 2, // Remove columnless table from group
    instr_RenameGroupLevelTable = 3,

    instr_SelectTable = 10,
    instr_CreateObject = 11,
    instr_RemoveObject = 12,
    instr_Set = 13,
    instr_SetDefault = 14,
    instr_ClearTable = 15, // Remove all rows in selected table

    instr_InsertColumn = 20, // Insert new column into to selected descriptor
    instr_EraseColumn = 21,  // Remove column from selected descriptor
    instr_RenameColumn = 22, // Rename column in selected descriptor
    instr_SetLinkType = 23,  // Strong/weak

    instr_SelectList = 30,
    instr_ListInsert = 31, // Insert list entry
    instr_ListSet = 32,    // Assign to list entry
    instr_ListMove = 33,   // Move an entry within a link list
    instr_ListSwap = 34,   // Swap two entries within a list
    instr_ListErase = 35,  // Remove an entry from a list
    instr_ListClear = 36,  // Remove all entries from a list
};

class TransactLogStream {
public:
    virtual ~TransactLogStream()
    {
    }

    /// Ensure contiguous free space in the transaction log
    /// buffer. This method must update `out_free_begin`
    /// and `out_free_end` such that they refer to a chunk
    /// of free space whose size is at least \a n.
    ///
    /// \param size The required amount of contiguous free space. Must be
    /// small (probably not greater than 1024)
    /// \param out_free_begin must point to current write position which must be inside earlier
    /// allocated area. Will be updated to point to new writing position.
    /// \param out_free_end Will be updated to point to end of allocated area.
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

    const char* get_data() const;
    char* get_data();
    size_t get_size();

private:
    util::Buffer<char> m_buffer;
};


// LCOV_EXCL_START (because the NullInstructionObserver is trivial)
class NullInstructionObserver {
public:
    /// The following methods are also those that TransactLogParser expects
    /// to find on the `InstructionHandler`.

    // No selection needed:
    bool select_table(TableKey)
    {
        return true;
    }
    bool select_list(ColKey, ObjKey)
    {
        return true;
    }
    bool select_link_list(ColKey, ObjKey)
    {
        return true;
    }
    bool insert_group_level_table(TableKey)
    {
        return true;
    }
    bool erase_group_level_table(TableKey)
    {
        return true;
    }
    bool rename_group_level_table(TableKey)
    {
        return true;
    }

    // Must have table selected:
    bool create_object(ObjKey)
    {
        return true;
    }
    bool remove_object(ObjKey)
    {
        return true;
    }
    bool clear_table(size_t)
    {
        return true;
    }
    bool modify_object(ColKey, ObjKey)
    {
        return true;
    }
    bool list_set(size_t)
    {
        return true;
    }
    bool list_insert(size_t)
    {
        return true;
    }

    // Must have descriptor selected:
    bool insert_column(ColKey)
    {
        return true;
    }
    bool erase_column(ColKey)
    {
        return true;
    }
    bool rename_column(ColKey)
    {
        return true;
    }
    bool set_link_type(ColKey)
    {
        return true;
    }

    // Must have linklist selected:
    bool list_move(size_t, size_t)
    {
        return true;
    }
    bool list_swap(size_t, size_t)
    {
        return true;
    }
    bool list_erase(size_t)
    {
        return true;
    }
    bool list_clear(size_t)
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
    bool select_table(TableKey key);
    bool insert_group_level_table(TableKey table_key);
    bool erase_group_level_table(TableKey table_key);
    bool rename_group_level_table(TableKey table_key);

    /// Must have table selected.
    bool create_object(ObjKey);
    bool remove_object(ObjKey);
    bool modify_object(ColKey col_key, ObjKey key);

    bool clear_table(size_t old_table_size);

    // Must have descriptor selected:
    bool insert_column(ColKey col_key);
    bool erase_column(ColKey col_key);
    bool rename_column(ColKey col_key);
    bool set_link_type(ColKey col_key);

    // Must have linklist selected:
    bool select_list(ColKey col_key, ObjKey key);
    bool list_set(size_t list_ndx);
    bool list_insert(size_t ndx);
    bool list_move(size_t from_link_ndx, size_t to_link_ndx);
    bool list_swap(size_t link1_ndx, size_t link2_ndx);
    bool list_erase(size_t list_ndx);
    bool list_clear(size_t old_list_size);

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
    static constexpr int max_enc_bytes_per_int = 10;
// Space is reserved in chunks to avoid excessive over allocation.
#ifdef REALM_DEBUG
    static constexpr int max_numbers_per_chunk = 2; // Increase the chance of chunking in debug mode
#else
    static constexpr int max_numbers_per_chunk = 8;
#endif

    TransactLogStream& m_stream;

    // These two delimit a contiguous region of free space in a
    // transaction log buffer following the last written data. It may
    // be empty.
    char* m_transact_log_free_begin = nullptr;
    char* m_transact_log_free_end = nullptr;

    char* reserve(size_t size);
    /// \param ptr Must be in the range [m_transact_log_free_begin, m_transact_log_free_end]
    void advance(char* ptr) noexcept;

    template <class T>
    size_t max_size(T);

    size_t max_size_list()
    {
        return 0;
    }

    template <class T, class... Args>
    size_t max_size_list(T val, Args... args)
    {
        return max_size(val) + max_size_list(args...);
    }

    template <class T>
    char* encode(char* ptr, T value);

    char* encode_list(char* ptr)
    {
        advance(ptr);
        return ptr;
    }

    template <class T, class... Args>
    char* encode_list(char* ptr, T value, Args... args)
    {
        return encode_list(encode(ptr, value), args...);
    }

    template <class... L>
    void append_simple_instr(L... numbers);

    template <class T>
    static char* encode_int(char*, T value);
    friend class TransactLogParser;
};

class TransactLogConvenientEncoder {
public:
    virtual ~TransactLogConvenientEncoder();
    virtual void add_class(StringData table_name);
    virtual void add_class_with_primary_key(StringData table_name, DataType pk_type, StringData pk_field,
                                            bool nullable);
    virtual void insert_group_level_table(TableKey table_key, size_t num_tables, StringData name);
    virtual void erase_group_level_table(TableKey table_key, size_t num_tables);
    virtual void rename_group_level_table(TableKey table_key, StringData new_name);
    virtual void insert_column(const Table*, ColKey col_key, DataType type, StringData name, LinkTargetInfo& link,
                               bool nullable = false, bool listtype = false, LinkType link_type = link_Weak);
    virtual void erase_column(const Table*, ColKey col_key);
    virtual void rename_column(const Table*, ColKey col_key, StringData name);

    virtual void set_int(const Table*, ColKey col_key, ObjKey key, int_fast64_t value,
                         Instruction variant = instr_Set);
    virtual void add_int(const Table*, ColKey col_key, ObjKey key, int_fast64_t value);
    virtual void set_bool(const Table*, ColKey col_key, ObjKey key, bool value, Instruction variant = instr_Set);
    virtual void set_float(const Table*, ColKey col_key, ObjKey key, float value, Instruction variant = instr_Set);
    virtual void set_double(const Table*, ColKey col_key, ObjKey key, double value, Instruction variant = instr_Set);
    virtual void set_string(const Table*, ColKey col_key, ObjKey key, StringData value,
                            Instruction variant = instr_Set);
    virtual void set_binary(const Table*, ColKey col_key, ObjKey key, BinaryData value,
                            Instruction variant = instr_Set);
    virtual void set_timestamp(const Table*, ColKey col_key, ObjKey key, Timestamp value,
                               Instruction variant = instr_Set);
    virtual void set_link(const Table*, ColKey col_key, ObjKey key, ObjKey value, Instruction variant = instr_Set);
    virtual void set_null(const Table*, ColKey col_key, ObjKey key, Instruction variant = instr_Set);
    virtual void insert_substring(const Table*, ColKey col_key, ObjKey key, size_t pos, StringData);
    virtual void erase_substring(const Table*, ColKey col_key, ObjKey key, size_t pos, size_t size);

    virtual void list_set_int(const ConstLstBase& list, size_t list_ndx, int64_t value);
    virtual void list_set_bool(const ConstLstBase& list, size_t list_ndx, bool value);
    virtual void list_set_float(const ConstLstBase& list, size_t list_ndx, float value);
    virtual void list_set_double(const ConstLstBase& list, size_t list_ndx, double value);
    virtual void list_set_string(const Lst<String>& list, size_t list_ndx, StringData value);
    virtual void list_set_binary(const Lst<Binary>& list, size_t list_ndx, BinaryData value);
    virtual void list_set_timestamp(const Lst<Timestamp>& list, size_t list_ndx, Timestamp value);

    virtual void list_insert_int(const ConstLstBase& list, size_t list_ndx, int64_t value);
    virtual void list_insert_bool(const ConstLstBase& list, size_t list_ndx, bool value);
    virtual void list_insert_float(const ConstLstBase& list, size_t list_ndx, float value);
    virtual void list_insert_double(const ConstLstBase& list, size_t list_ndx, double value);
    virtual void list_insert_string(const Lst<String>& list, size_t list_ndx, StringData value);
    virtual void list_insert_binary(const Lst<Binary>& list, size_t list_ndx, BinaryData value);
    virtual void list_insert_timestamp(const Lst<Timestamp>& list, size_t list_ndx, Timestamp value);

    virtual void create_object(const Table*, GlobalKey);
    virtual void create_object(const Table*, ObjKey);
    virtual void create_object_with_primary_key(const Table*, GlobalKey, Mixed);
    virtual void remove_object(const Table*, ObjKey);
    virtual void set_link_type(const Table*, ColKey col_key, LinkType);
    virtual void clear_table(const Table*, size_t prior_num_rows);

    virtual void list_set_null(const ConstLstBase&, size_t ndx);
    virtual void list_insert_null(const ConstLstBase&, size_t ndx);
    virtual void list_set_link(const Lst<ObjKey>&, size_t link_ndx, ObjKey value);
    virtual void list_insert_link(const Lst<ObjKey>&, size_t link_ndx, ObjKey value);
    virtual void list_move(const ConstLstBase&, size_t from_link_ndx, size_t to_link_ndx);
    virtual void list_swap(const ConstLstBase&, size_t link_ndx_1, size_t link_ndx_2);
    virtual void list_erase(const ConstLstBase&, size_t link_ndx);
    virtual void list_clear(const ConstLstBase&);

    //@{

    /// Implicit nullifications due to removal of target row. This is redundant
    /// information from the point of view of replication, as the removal of the
    /// target row will reproduce the implicit nullifications in the target
    /// Realm anyway. The purpose of this instruction is to allow observers
    /// (reactor pattern) to be explicitly notified about the implicit
    /// nullifications.

    virtual void nullify_link(const Table*, ColKey col_key, ObjKey key);
    virtual void link_list_nullify(const Lst<ObjKey>&, size_t link_ndx);

    //@}

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
    struct LinkListId {
        TableKey table_key;
        ObjKey object_key;
        ColKey col_id;

        LinkListId() = default;
        LinkListId(const ConstLstBase& list)
            : table_key(list.get_table()->get_key())
            , object_key(list.get_key())
            , col_id(list.get_col_key())
        {
        }
        LinkListId(TableKey t, ObjKey k, ColKey c)
            : table_key(t)
            , object_key(k)
            , col_id(c)
        {
        }
        bool operator!=(const LinkListId& other)
        {
            return object_key != other.object_key || table_key != other.table_key || col_id != other.col_id;
        }
    };
    TransactLogEncoder m_encoder;
    mutable const Table* m_selected_table = nullptr;
    mutable LinkListId m_selected_list;

    void unselect_all() noexcept;
    void select_table(const Table*); // unselects link list
    void select_list(const ConstLstBase&);

    void do_select_table(const Table*);
    void do_select_list(const ConstLstBase&);

    void do_set(const Table*, ColKey col_key, ObjKey key, Instruction variant = instr_Set);

    friend class TransactReverser;
};


class TransactLogParser {
public:
    class BadTransactLog; // Exception

    TransactLogParser();
    ~TransactLogParser() noexcept;

    /// See `TransactLogEncoder` for a list of methods that the `InstructionHandler` must define.
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

    REALM_NORETURN void parser_error() const;

    template <class InstructionHandler>
    void parse_one(InstructionHandler&);
    bool has_next() noexcept;

    template <class T>
    T read_int();

    // Advance m_input_begin and m_input_end to reflect the next block of instructions
    // Returns false if no more input was available
    bool next_input_buffer();

    // return true if input was available
    bool read_char(char&); // throws
};


class TransactLogParser::BadTransactLog : public std::exception {
public:
    const char* what() const noexcept override
    {
        return "Bad transaction log";
    }
};


/// Implementation:

inline void TransactLogBufferStream::transact_log_reserve(size_t size, char** inout_new_begin, char** out_new_end)
{
    char* data = m_buffer.data();
    REALM_ASSERT(*inout_new_begin >= data);
    REALM_ASSERT(*inout_new_begin <= (data + m_buffer.size()));
    size_t used_size = *inout_new_begin - data;
    m_buffer.reserve_extra(used_size, size);
    data = m_buffer.data(); // May have changed
    *inout_new_begin = data + used_size;
    *out_new_end = data + m_buffer.size();
}

inline void TransactLogBufferStream::transact_log_append(const char* data, size_t size, char** out_new_begin,
                                                         char** out_new_end)
{
    transact_log_reserve(size, out_new_begin, out_new_end);
    *out_new_begin = realm::safe_copy_n(data, size, *out_new_begin);
}

inline const char* TransactLogBufferStream::get_data() const
{
    return m_buffer.data();
}

inline char* TransactLogBufferStream::get_data()
{
    return m_buffer.data();
}

inline size_t TransactLogBufferStream::get_size()
{
    return m_buffer.size();
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

template <class T>
inline char* TransactLogEncoder::encode(char* ptr, T inst)
{
    return encode_int<T>(ptr, inst);
}

template <>
inline char* TransactLogEncoder::encode<TableKey>(char* ptr, TableKey key)
{
    return encode_int<int64_t>(ptr, key.value);
}

template <>
inline char* TransactLogEncoder::encode<ColKey>(char* ptr, ColKey key)
{
    return encode_int<int64_t>(ptr, key.value);
}

template <>
inline char* TransactLogEncoder::encode<ObjKey>(char* ptr, ObjKey key)
{
    return encode_int<int64_t>(ptr, key.value);
}

template <>
inline char* TransactLogEncoder::encode<Instruction>(char* ptr, Instruction inst)
{
    return encode_int<int64_t>(ptr, inst);
}

template <class T>
size_t TransactLogEncoder::max_size(T)
{
    return max_enc_bytes_per_int;
}

template <>
inline size_t TransactLogEncoder::max_size(Instruction)
{
    return 1;
}

template <class... L>
void TransactLogEncoder::append_simple_instr(L... numbers)
{
    size_t max_required_bytes = max_size_list(numbers...);
    char* ptr = reserve(max_required_bytes); // Throws
    encode_list(ptr, numbers...);
}

inline void TransactLogConvenientEncoder::unselect_all() noexcept
{
    m_selected_table = nullptr;
    m_selected_list = LinkListId();
}

inline void TransactLogConvenientEncoder::select_table(const Table* table)
{
    if (table != m_selected_table)
        do_select_table(table); // Throws
    m_selected_list = LinkListId();
}

inline void TransactLogConvenientEncoder::select_list(const ConstLstBase& list)
{
    if (LinkListId(list) != m_selected_list) {
        do_select_list(list); // Throws
    }
}

inline bool TransactLogEncoder::insert_group_level_table(TableKey table_key)
{
    append_simple_instr(instr_InsertGroupLevelTable, table_key); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::insert_group_level_table(TableKey table_key, size_t, StringData)
{
    unselect_all();
    m_encoder.insert_group_level_table(table_key); // Throws
}

inline bool TransactLogEncoder::erase_group_level_table(TableKey table_key)
{
    append_simple_instr(instr_EraseGroupLevelTable, table_key); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::erase_group_level_table(TableKey table_key, size_t)
{
    unselect_all();
    m_encoder.erase_group_level_table(table_key); // Throws
}

inline bool TransactLogEncoder::rename_group_level_table(TableKey table_key)
{
    append_simple_instr(instr_RenameGroupLevelTable, table_key); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::rename_group_level_table(TableKey table_key, StringData)
{
    unselect_all();
    m_encoder.rename_group_level_table(table_key); // Throws
}

inline bool TransactLogEncoder::insert_column(ColKey col_key)
{
    append_simple_instr(instr_InsertColumn, col_key); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::insert_column(const Table* t, ColKey col_key, DataType, StringData,
                                                        LinkTargetInfo&, bool, bool, LinkType)
{
    select_table(t); // Throws
    m_encoder.insert_column(col_key); // Throws
}

inline bool TransactLogEncoder::erase_column(ColKey col_key)
{
    append_simple_instr(instr_EraseColumn, col_key); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::erase_column(const Table* t, ColKey col_key)
{
    select_table(t); // Throws
    m_encoder.erase_column(col_key); // Throws
}

inline bool TransactLogEncoder::rename_column(ColKey col_key)
{
    append_simple_instr(instr_RenameColumn, col_key); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::rename_column(const Table* t, ColKey col_key, StringData)
{
    select_table(t);                  // Throws
    m_encoder.rename_column(col_key); // Throws
}


inline bool TransactLogEncoder::modify_object(ColKey col_key, ObjKey key)
{
    append_simple_instr(instr_Set, col_key, key); // Throws
    return true;
}


inline void TransactLogConvenientEncoder::do_set(const Table* t, ColKey col_key, ObjKey key, Instruction variant)
{
    if (variant != Instruction::instr_SetDefault) {
        select_table(t);                       // Throws
        m_encoder.modify_object(col_key, key); // Throws
    }
}


inline void TransactLogConvenientEncoder::set_int(const Table* t, ColKey col_key, ObjKey key, int_fast64_t,
                                                  Instruction variant)
{
    do_set(t, col_key, key, variant); // Throws
}


inline void TransactLogConvenientEncoder::add_int(const Table* t, ColKey col_key, ObjKey key, int_fast64_t)
{
    do_set(t, col_key, key); // Throws
}

inline void TransactLogConvenientEncoder::set_bool(const Table* t, ColKey col_key, ObjKey key, bool,
                                                   Instruction variant)
{
    do_set(t, col_key, key, variant); // Throws
}

inline void TransactLogConvenientEncoder::set_float(const Table* t, ColKey col_key, ObjKey key, float,
                                                    Instruction variant)
{
    do_set(t, col_key, key, variant); // Throws
}

inline void TransactLogConvenientEncoder::set_double(const Table* t, ColKey col_key, ObjKey key, double,
                                                     Instruction variant)
{
    do_set(t, col_key, key, variant); // Throws
}

inline void TransactLogConvenientEncoder::set_string(const Table* t, ColKey col_key, ObjKey key, StringData,
                                                     Instruction variant)
{
    do_set(t, col_key, key, variant); // Throws
}

inline void TransactLogConvenientEncoder::set_binary(const Table* t, ColKey col_key, ObjKey key, BinaryData,
                                                     Instruction variant)
{
    do_set(t, col_key, key, variant); // Throws
}

inline void TransactLogConvenientEncoder::set_timestamp(const Table* t, ColKey col_key, ObjKey key, Timestamp,
                                                        Instruction variant)
{
    do_set(t, col_key, key, variant); // Throws
}

inline void TransactLogConvenientEncoder::set_link(const Table* t, ColKey col_key, ObjKey key, ObjKey,
                                                   Instruction variant)
{
    do_set(t, col_key, key, variant); // Throws
}

inline void TransactLogConvenientEncoder::set_null(const Table* t, ColKey col_key, ObjKey key, Instruction variant)
{
    do_set(t, col_key, key, variant); // Throws
}

inline void TransactLogConvenientEncoder::nullify_link(const Table* t, ColKey col_key, ObjKey key)
{
    select_table(t); // Throws
    m_encoder.modify_object(col_key, key); // Throws
}

inline void TransactLogConvenientEncoder::insert_substring(const Table* t, ColKey col_key, ObjKey key, size_t,
                                                           StringData value)
{
    if (value.size() > 0) {
        do_set(t, col_key, key); // Throws
    }
}

inline void TransactLogConvenientEncoder::erase_substring(const Table* t, ColKey col_key, ObjKey key, size_t,
                                                          size_t size)
{
    if (size > 0) {
        do_set(t, col_key, key); // Throws
    }
}

inline bool TransactLogEncoder::list_set(size_t list_ndx)
{
    append_simple_instr(instr_ListSet, list_ndx); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::list_set_int(const ConstLstBase& list, size_t list_ndx, int64_t)
{
    select_list(list);            // Throws
    m_encoder.list_set(list_ndx); // Throws
}

inline void TransactLogConvenientEncoder::list_set_bool(const ConstLstBase& list, size_t list_ndx, bool)
{
    select_list(list);            // Throws
    m_encoder.list_set(list_ndx); // Throws
}

inline void TransactLogConvenientEncoder::list_set_float(const ConstLstBase& list, size_t list_ndx, float)
{
    select_list(list);            // Throws
    m_encoder.list_set(list_ndx); // Throws
}

inline void TransactLogConvenientEncoder::list_set_double(const ConstLstBase& list, size_t list_ndx, double)
{
    select_list(list);            // Throws
    m_encoder.list_set(list_ndx); // Throws
}

inline void TransactLogConvenientEncoder::list_set_string(const Lst<String>& list, size_t list_ndx, StringData)
{
    select_list(list);            // Throws
    m_encoder.list_set(list_ndx); // Throws
}

inline void TransactLogConvenientEncoder::list_set_binary(const Lst<Binary>& list, size_t list_ndx, BinaryData)
{
    select_list(list);            // Throws
    m_encoder.list_set(list_ndx); // Throws
}

inline void TransactLogConvenientEncoder::list_set_timestamp(const Lst<Timestamp>& list, size_t list_ndx, Timestamp)
{
    select_list(list);            // Throws
    m_encoder.list_set(list_ndx); // Throws
}

inline bool TransactLogEncoder::list_insert(size_t list_ndx)
{
    append_simple_instr(instr_ListInsert, list_ndx); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::list_insert_int(const ConstLstBase& list, size_t list_ndx, int64_t)
{
    select_list(list);               // Throws
    m_encoder.list_insert(list_ndx); // Throws
}

inline void TransactLogConvenientEncoder::list_insert_bool(const ConstLstBase& list, size_t list_ndx, bool)
{
    select_list(list);               // Throws
    m_encoder.list_insert(list_ndx); // Throws
}

inline void TransactLogConvenientEncoder::list_insert_float(const ConstLstBase& list, size_t list_ndx, float)
{
    select_list(list);               // Throws
    m_encoder.list_insert(list_ndx); // Throws
}

inline void TransactLogConvenientEncoder::list_insert_double(const ConstLstBase& list, size_t list_ndx, double)
{
    select_list(list);               // Throws
    m_encoder.list_insert(list_ndx); // Throws
}

inline void TransactLogConvenientEncoder::list_insert_string(const Lst<String>& list, size_t list_ndx, StringData)
{
    select_list(list);               // Throws
    m_encoder.list_insert(list_ndx); // Throws
}

inline void TransactLogConvenientEncoder::list_insert_binary(const Lst<Binary>& list, size_t list_ndx, BinaryData)
{
    select_list(list);               // Throws
    m_encoder.list_insert(list_ndx); // Throws
}

inline void TransactLogConvenientEncoder::list_insert_timestamp(const Lst<Timestamp>& list, size_t list_ndx,
                                                                Timestamp)
{
    select_list(list);               // Throws
    m_encoder.list_insert(list_ndx); // Throws
}

inline bool TransactLogEncoder::create_object(ObjKey key)
{
    append_simple_instr(instr_CreateObject, key); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::create_object(const Table* t, ObjKey key)
{
    select_table(t);              // Throws
    m_encoder.create_object(key); // Throws
}

inline bool TransactLogEncoder::remove_object(ObjKey key)
{
    append_simple_instr(instr_RemoveObject, key); // Throws
    return true;
}


inline void TransactLogConvenientEncoder::remove_object(const Table* t, ObjKey key)
{
    select_table(t);              // Throws
    m_encoder.remove_object(key); // Throws
}

inline bool TransactLogEncoder::set_link_type(ColKey col_key)
{
    append_simple_instr(instr_SetLinkType, col_key); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_link_type(const Table* t, ColKey col_key, LinkType)
{
    select_table(t);                  // Throws
    m_encoder.set_link_type(col_key); // Throws
}


inline bool TransactLogEncoder::clear_table(size_t old_size)
{
    append_simple_instr(instr_ClearTable, old_size); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::clear_table(const Table* t, size_t prior_num_rows)
{
    select_table(t);         // Throws
    m_encoder.clear_table(prior_num_rows); // Throws
}

inline void TransactLogConvenientEncoder::list_set_null(const ConstLstBase& list, size_t list_ndx)
{
    select_list(list);            // Throws
    m_encoder.list_set(list_ndx); // Throws
}

inline void TransactLogConvenientEncoder::list_insert_null(const ConstLstBase& list, size_t list_ndx)
{
    select_list(list);                                 // Throws
    m_encoder.list_insert(list_ndx);                   // Throws
}

inline void TransactLogConvenientEncoder::list_set_link(const Lst<ObjKey>& list, size_t link_ndx, ObjKey)
{
    select_list(list);                      // Throws
    m_encoder.list_set(link_ndx);           // Throws
}

inline void TransactLogConvenientEncoder::list_insert_link(const Lst<ObjKey>& list, size_t link_ndx, ObjKey)
{
    select_list(list);                                      // Throws
    m_encoder.list_insert(link_ndx);                        // Throws
}

inline void TransactLogConvenientEncoder::link_list_nullify(const Lst<ObjKey>& list, size_t link_ndx)
{
    select_list(list);            // Throws
    m_encoder.list_erase(link_ndx); // Throws
}

inline bool TransactLogEncoder::list_move(size_t from_link_ndx, size_t to_link_ndx)
{
    REALM_ASSERT(from_link_ndx != to_link_ndx);
    append_simple_instr(instr_ListMove, from_link_ndx, to_link_ndx); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::list_move(const ConstLstBase& list, size_t from_link_ndx,
                                                    size_t to_link_ndx)
{
    select_list(list);                               // Throws
    m_encoder.list_move(from_link_ndx, to_link_ndx); // Throws
}

inline bool TransactLogEncoder::list_swap(size_t link1_ndx, size_t link2_ndx)
{
    append_simple_instr(instr_ListSwap, link1_ndx, link2_ndx); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::list_swap(const ConstLstBase& list, size_t link1_ndx, size_t link2_ndx)
{
    select_list(list);                         // Throws
    m_encoder.list_swap(link1_ndx, link2_ndx); // Throws
}

inline bool TransactLogEncoder::list_erase(size_t list_ndx)
{
    append_simple_instr(instr_ListErase, list_ndx); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::list_erase(const ConstLstBase& list, size_t link_ndx)
{
    select_list(list);              // Throws
    m_encoder.list_erase(link_ndx); // Throws
}

inline bool TransactLogEncoder::list_clear(size_t old_list_size)
{
    append_simple_instr(instr_ListClear, old_list_size); // Throws
    return true;
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
    char instr_ch = 0; // silence a warning
    if (!read_char(instr_ch))
        parser_error(); // Throws
    //    std::cerr << "parsing " << util::promote(instr) << " @ " << std::hex << long(m_input_begin) << std::dec <<
    //    "\n";
    Instruction instr = Instruction(instr_ch);
    switch (instr) {
        case instr_Set: {
            ColKey col_key = ColKey(read_int<int64_t>()); // Throws
            ObjKey key(read_int<int64_t>());             // Throws
            if (!handler.modify_object(col_key, key))    // Throws
                parser_error();
            return;
        }
        case instr_SetDefault:
            // Should not appear in the transaction log
            parser_error();
        case instr_ListSet: {
            size_t list_ndx = read_int<size_t>();
            if (!handler.list_set(list_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_CreateObject: {
            ObjKey key(read_int<int64_t>()); // Throws
            if (!handler.create_object(key)) // Throws
                parser_error();
            return;
        }
        case instr_RemoveObject: {
            ObjKey key(read_int<int64_t>()); // Throws
            if (!handler.remove_object(key)) // Throws
                parser_error();
            return;
        }
        case instr_SelectTable: {
            int levels = read_int<int>(); // Throws
            REALM_ASSERT(levels == 0);
            TableKey key = TableKey(read_int<uint32_t>());
            if (!handler.select_table(key)) // Throws
                parser_error();
            return;
        }
        case instr_ClearTable: {
            size_t old_size = read_int<size_t>();   // Throws
            if (!handler.clear_table(old_size)) // Throws
                parser_error();
            return;
        }
        case instr_ListInsert: {
            size_t list_ndx = read_int<size_t>();
            if (!handler.list_insert(list_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_ListMove: {
            size_t from_link_ndx = read_int<size_t>();               // Throws
            size_t to_link_ndx = read_int<size_t>();                 // Throws
            if (!handler.list_move(from_link_ndx, to_link_ndx))      // Throws
                parser_error();
            return;
        }
        case instr_ListSwap: {
            size_t link1_ndx = read_int<size_t>();             // Throws
            size_t link2_ndx = read_int<size_t>();             // Throws
            if (!handler.list_swap(link1_ndx, link2_ndx))      // Throws
                parser_error();
            return;
        }
        case instr_ListErase: {
            size_t link_ndx = read_int<size_t>();               // Throws
            if (!handler.list_erase(link_ndx))                  // Throws
                parser_error();
            return;
        }
        case instr_ListClear: {
            size_t old_list_size = read_int<size_t>();   // Throws
            if (!handler.list_clear(old_list_size))      // Throws
                parser_error();
            return;
        }
        case instr_SelectList: {
            ColKey col_key = ColKey(read_int<int64_t>());                            // Throws
            ObjKey key = ObjKey(read_int<int64_t>());                                // Throws
            if (!handler.select_list(col_key, key))                                  // Throws
                parser_error();
            return;
        }
        case instr_SetLinkType: {
            ColKey col_key = ColKey(read_int<int64_t>()); // Throws
            if (!handler.set_link_type(col_key))          // Throws
                parser_error();
            return;
        }
        case instr_InsertColumn: {
            ColKey col_key = ColKey(read_int<int64_t>()); // Throws
            if (!handler.insert_column(col_key))         // Throws
                parser_error();
            return;
        }
        case instr_EraseColumn: {
            ColKey col_key = ColKey(read_int<int64_t>()); // Throws
            if (!handler.erase_column(col_key))          // Throws
                parser_error();
            return;
        }
        case instr_RenameColumn: {
            ColKey col_key = ColKey(read_int<int64_t>());    // Throws
            if (!handler.rename_column(col_key))            // Throws
                parser_error();
            return;
        }
        case instr_InsertGroupLevelTable: {
            TableKey table_key = TableKey(read_int<uint32_t>()); // Throws
            if (!handler.insert_group_level_table(table_key))  // Throws
                parser_error();
            return;
        }
        case instr_EraseGroupLevelTable: {
            TableKey table_key = TableKey(read_int<uint32_t>());               // Throws
            if (!handler.erase_group_level_table(table_key))                   // Throws
                parser_error();
            return;
        }
        case instr_RenameGroupLevelTable: {
            TableKey table_key = TableKey(read_int<uint32_t>());        // Throws
            if (!handler.rename_group_level_table(table_key))           // Throws
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


class TransactReverser {
public:
    bool select_table(TableKey key)
    {
        sync_table();
        m_encoder.select_table(key);
        m_pending_ts_instr = get_inst();
        return true;
    }

    bool insert_group_level_table(TableKey table_key)
    {
        sync_table();
        m_encoder.erase_group_level_table(table_key);
        append_instruction();
        return true;
    }

    bool erase_group_level_table(TableKey table_key)
    {
        sync_table();
        m_encoder.insert_group_level_table(table_key);
        append_instruction();
        return true;
    }

    bool rename_group_level_table(TableKey)
    {
        sync_table();
        return true;
    }

    bool create_object(ObjKey key)
    {
        m_encoder.remove_object(key); // Throws
        append_instruction();
        return true;
    }

    bool remove_object(ObjKey key)
    {
        m_encoder.create_object(key); // Throws
        append_instruction();
        return true;
    }

    bool modify_object(ColKey col_key, ObjKey key)
    {
        m_encoder.modify_object(col_key, key);
        append_instruction();
        return true;
    }

    bool list_set(size_t ndx)
    {
        m_encoder.list_set(ndx);
        append_instruction();
        return true;
    }

    bool list_insert(size_t ndx)
    {
        m_encoder.list_erase(ndx);
        append_instruction();
        return true;
    }

    bool clear_table(size_t old_size)
    {
        while (old_size--) {
            m_encoder.create_object(null_key);
            append_instruction();
        }
        return true;
    }

    bool set_link_type(ColKey key)
    {
        m_encoder.set_link_type(key);
        return true;
    }

    bool insert_column(ColKey col_key)
    {
        m_encoder.erase_column(col_key);
        append_instruction();
        return true;
    }

    bool erase_column(ColKey col_key)
    {
        m_encoder.insert_column(col_key);
        append_instruction();
        return true;
    }

    bool rename_column(ColKey col_key)
    {
        m_encoder.rename_column(col_key);
        return true;
    }

    bool select_list(ColKey col_key, ObjKey key)
    {
        sync_list();
        m_encoder.select_list(col_key, key);
        m_pending_ls_instr = get_inst();
        return true;
    }

    bool list_move(size_t from_link_ndx, size_t to_link_ndx)
    {
        m_encoder.list_move(from_link_ndx, to_link_ndx);
        append_instruction();
        return true;
    }

    bool list_swap(size_t link1_ndx, size_t link2_ndx)
    {
        m_encoder.list_swap(link1_ndx, link2_ndx);
        append_instruction();
        return true;
    }

    bool list_erase(size_t list_ndx)
    {
        m_encoder.list_insert(list_ndx);
        append_instruction();
        return true;
    }

    bool list_clear(size_t old_list_size)
    {
        // Append in reverse order because the reversed log is itself applied
        // in reverse, and this way it generates all back-insertions rather than
        // all front-insertions
        for (size_t i = old_list_size; i > 0; --i) {
            m_encoder.list_insert(i - 1);
            append_instruction();
        }
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
    Instr m_pending_ls_instr{0, 0};

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
        REALM_ASSERT_3(m_encoder.write_position(), >=, m_buffer.get_data());
        return m_encoder.write_position() - m_buffer.get_data();
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

    void sync_list()
    {
        sync_select(m_pending_ls_instr);
    }

    void sync_table()
    {
        sync_list();
        sync_select(m_pending_ts_instr);
    }

    friend class ReversedNoCopyInputStream;
};


class ReversedNoCopyInputStream : public NoCopyInputStream {
public:
    ReversedNoCopyInputStream(TransactReverser& reverser)
        : m_instr_order(reverser.m_instructions)
    {
        // push any pending select_table into the buffer
        reverser.sync_table();

        m_buffer = reverser.m_buffer.get_data();
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
