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

#ifndef REALM_INDEX_STRING_HPP
#define REALM_INDEX_STRING_HPP

#include <cstring>
#include <memory>
#include <array>

#include <realm/array.hpp>
#include <realm/column_fwd.hpp>

/*
The StringIndex class is used for both type_String and all integral types, such as type_Bool, type_OldDateTime and
type_Int. When used for integral types, the 64-bit integer is simply casted to a string of 8 bytes through a
pretty simple "wrapper layer" in all public methods.

The StringIndex data structure is like an "inversed" B+ tree where the leafs contain row indexes and the non-leafs
contain 4-byte chunks of payload. Imagine a table with following strings:

       hello, kitty, kitten, foobar, kitty, foobar

The topmost level of the index tree contains prefixes of the payload strings of length <= 4. The next level contains
prefixes of the remaining parts of the strings. Unnecessary levels of the tree are optimized away; the prefix "foob"
is shared only by rows that are identical ("foobar"), so "ar" is not needed to be stored in the tree.

       hell   kitt      foob
        |      /\        |
        0     en  y    {3, 5}
              |    \
           {1, 4}   2

Each non-leafs consists of two integer arrays of the same length, one containing payload and the other containing
references to the sublevel nodes.

The leafs can be either a single value or a Column. If the reference in its parent node has its least significant
bit set, then the remaining upper bits specify the row index at which the string is stored. If the bit is clear,
it must be interpreted as a reference to a Column that stores the row indexes at which the string is stored.

If a Column is used, then all row indexes are guaranteed to be sorted increasingly, which means you an search in it
using our binary search functions such as upper_bound() and lower_bound(). Each duplicate value will be stored in
the same Column, but Columns may contain more than just duplicates if the depth of the tree exceeds the value
`s_max_offset` This is to avoid stack overflow problems with many of our recursive functions if we have two very
long strings that have a long common prefix but differ in the last couple bytes. If a Column stores more than just
duplicates, then the list is kept sorted in ascending order by string value and within the groups of common
strings, the rows are sorted in ascending order.
*/

namespace realm {

class Spec;
class Timestamp;

class IndexArray : public Array {
public:
    IndexArray(Allocator& allocator)
        : Array(allocator)
    {
    }

    size_t index_string_find_first(StringData value, ColumnBase* column) const;
    void index_string_find_all(IntegerColumn& result, StringData value, ColumnBase* column, bool case_insensitive = false) const;
    FindRes index_string_find_all_no_copy(StringData value, ColumnBase* column, InternalFindResult& result) const;
    size_t index_string_count(StringData value, ColumnBase* column) const;

private:
    template <IndexMethod>
    size_t from_list(StringData value, InternalFindResult& result_ref, const IntegerColumn& rows,
                     ColumnBase* column) const;

    void from_list_all(StringData value, IntegerColumn& result, const IntegerColumn& rows, ColumnBase* column) const;

    void from_list_all_ins(StringData value, std::vector<size_t>& result, const IntegerColumn& rows,
                           ColumnBase* column) const;

    template <IndexMethod method>
    size_t index_string(StringData value, InternalFindResult& result_ref, ColumnBase* column) const;

    void index_string_all(StringData value, IntegerColumn& result, ColumnBase* column) const;

    void index_string_all_ins(StringData value, IntegerColumn& result, ColumnBase* column) const;
};


class StringIndex {
public:
    StringIndex(ColumnBase* target_column, Allocator&);
    StringIndex(ref_type, ArrayParent*, size_t ndx_in_parent, ColumnBase* target_column, Allocator&);
    ~StringIndex() noexcept
    {
    }

    static ref_type create_empty(Allocator& alloc);

    void set_target(ColumnBase* target_column) noexcept;

    // Accessor concept:
    Allocator& get_alloc() const noexcept;
    void destroy() noexcept;
    void detach();
    bool is_attached() const noexcept;
    void set_parent(ArrayParent* parent, size_t ndx_in_parent) noexcept;
    size_t get_ndx_in_parent() const noexcept;
    void set_ndx_in_parent(size_t ndx_in_parent) noexcept;
    void update_from_parent(size_t old_baseline) noexcept;
    void refresh_accessor_tree(size_t, const Spec&);
    ref_type get_ref() const noexcept;

    // StringIndex interface:

    // 12 is the biggest element size of any non-string/binary Realm type
    static const size_t string_conversion_buffer_size = 12;
    using StringConversionBuffer = std::array<char, string_conversion_buffer_size>;

    bool is_empty() const;

    template <class T>
    void insert(size_t row_ndx, T value, size_t num_rows, bool is_append);
    template <class T>
    void insert(size_t row_ndx, util::Optional<T> value, size_t num_rows, bool is_append);

    template <class T>
    void set(size_t row_ndx, T new_value);
    template <class T>
    void set(size_t row_ndx, util::Optional<T> new_value);

    template <class T>
    void erase(size_t row_ndx, bool is_last);

    template <class T>
    size_t find_first(T value) const;
    template <class T>
    void find_all(IntegerColumn& result, T value, bool case_insensitive = false) const;
    template <class T>
    FindRes find_all_no_copy(T value, InternalFindResult& result) const;
    template <class T>
    size_t count(T value) const;
    template <class T>
    void update_ref(T value, size_t old_row_ndx, size_t new_row_ndx);

    void clear();

    void distinct(IntegerColumn& result) const;
    bool has_duplicate_values() const noexcept;

    void verify() const;
#ifdef REALM_DEBUG
    template <typename T>
    void verify_entries(const T& column) const;
    void do_dump_node_structure(std::ostream&, int) const;
    void to_dot() const;
    void to_dot(std::ostream&, StringData title = StringData()) const;
    void to_dot_2(std::ostream&, StringData title = StringData()) const;
#endif

    typedef int32_t key_type;

    // s_max_offset specifies the number of levels of recursive string indexes
    // allowed before storing everything in lists. This is to avoid nesting
    // to too deep of a level. Since every SubStringIndex stores 4 bytes, this
    // means that a StringIndex is helpful for strings of a common prefix up to
    // 4 times this limit (200 bytes shared). Lists are stored in sorted order,
    // so strings sharing a common prefix of more than this limit will use a
    // binary search of approximate complexity log2(n) from `std::lower_bound`.
    static const size_t s_max_offset = 200; // max depth * s_index_key_length
    static const size_t s_index_key_length = 4;
    static key_type create_key(StringData) noexcept;
    static key_type create_key(StringData, size_t) noexcept;

private:
    // m_array is a compact representation for storing the children of this StringIndex.
    // Children can be:
    // 1) a row number
    // 2) a reference to a list which stores row numbers (for duplicate strings).
    // 3) a reference to a sub-index
    // m_array[0] is always a reference to a values array which stores the 4 byte chunk
    // of payload data for quick string chunk comparisons. The array stored
    // at m_array[0] lines up with the indices of values in m_array[1] so for example
    // starting with an empty StringIndex:
    // StringColumn::insert(target_row_ndx=42, value="test_string") would result with
    // get_array_from_ref(m_array[0])[0] == create_key("test") and
    // m_array[1] == 42
    // In this way, m_array which stores one child has a size of two.
    // Children are type 1 (row number) if the LSB of the value is set.
    // To get the actual row value, shift value down by one.
    // If the LSB of the value is 0 then the value is a reference and can be either
    // type 2, or type 3 (no shifting in either case).
    // References point to a list if the context header flag is NOT set.
    // If the header flag is set, references point to a sub-StringIndex (nesting).
    std::unique_ptr<IndexArray> m_array;
    ColumnBase* m_target_column;

    struct inner_node_tag {
    };
    StringIndex(inner_node_tag, Allocator&);

    static IndexArray* create_node(Allocator&, bool is_leaf);

    void insert_with_offset(size_t row_ndx, StringData value, size_t offset);
    void insert_row_list(size_t ref, size_t offset, StringData value);
    void insert_to_existing_list(size_t row, StringData value, IntegerColumn& list);
    void insert_to_existing_list_at_lower(size_t row, StringData value, IntegerColumn& list,
                                          const IntegerColumnIterator& lower);
    key_type get_last_key() const;

    /// Add small signed \a diff to all elements that are greater than, or equal
    /// to \a min_row_ndx.
    void adjust_row_indexes(size_t min_row_ndx, int diff);

    struct NodeChange {
        size_t ref1;
        size_t ref2;
        enum ChangeType { none, insert_before, insert_after, split } type;
        NodeChange(ChangeType t, size_t r1 = 0, size_t r2 = 0)
            : ref1(r1)
            , ref2(r2)
            , type(t)
        {
        }
        NodeChange()
            : ref1(0)
            , ref2(0)
            , type(none)
        {
        }
    };

    // B-Tree functions
    void TreeInsert(size_t row_ndx, key_type, size_t offset, StringData value);
    NodeChange do_insert(size_t ndx, key_type, size_t offset, StringData value);
    /// Returns true if there is room or it can join existing entries
    bool leaf_insert(size_t row_ndx, key_type, size_t offset, StringData value, bool noextend = false);
    void node_insert_split(size_t ndx, size_t new_ref);
    void node_insert(size_t ndx, size_t ref);
    void do_delete(size_t ndx, StringData, size_t offset);
    void do_update_ref(StringData value, size_t row_ndx, size_t new_row_ndx, size_t offset);

    StringData get(size_t ndx, StringConversionBuffer& buffer) const;

    void node_add_key(ref_type ref);

#ifdef REALM_DEBUG
    static void dump_node_structure(const Array& node, std::ostream&, int level);
    static void array_to_dot(std::ostream&, const Array&);
    static void keys_to_dot(std::ostream&, const Array&, StringData title = StringData());
#endif
};


class SortedListComparator {
public:
    SortedListComparator(ColumnBase& column_values);
    bool operator()(int64_t ndx, StringData needle);
    bool operator()(StringData needle, int64_t ndx);

private:
    ColumnBase& values;
};


// Implementation:

template <class T>
struct GetIndexData;

template <>
struct GetIndexData<int64_t> {
    static StringData get_index_data(const int64_t& value, StringIndex::StringConversionBuffer& buffer)
    {
        const char* c = reinterpret_cast<const char*>(&value);
        realm::safe_copy_n(c, sizeof(int64_t), buffer.data());
        return StringData{buffer.data(), sizeof(int64_t)};
    }
};

template <>
struct GetIndexData<StringData> {
    static StringData get_index_data(StringData data, StringIndex::StringConversionBuffer&)
    {
        return data;
    }
};

template <>
struct GetIndexData<null> {
    static StringData get_index_data(null, StringIndex::StringConversionBuffer&)
    {
        return null{};
    }
};

template <>
struct GetIndexData<Timestamp> {
    static StringData get_index_data(const Timestamp&, StringIndex::StringConversionBuffer&);
};

template <class T>
struct GetIndexData<util::Optional<T>> {
    static StringData get_index_data(const util::Optional<T>& value, StringIndex::StringConversionBuffer& buffer)
    {
        if (value)
            return GetIndexData<T>::get_index_data(*value, buffer);
        return null{};
    }
};

template <>
struct GetIndexData<float> {
    static StringData get_index_data(float, StringIndex::StringConversionBuffer&)
    {
        REALM_ASSERT_RELEASE(false); // LCOV_EXCL_LINE; Index on float not supported
    }
};

template <>
struct GetIndexData<double> {
    static StringData get_index_data(double, StringIndex::StringConversionBuffer&)
    {
        REALM_ASSERT_RELEASE(false); // LCOV_EXCL_LINE; Index on float not supported
    }
};

template <>
struct GetIndexData<const char*> : GetIndexData<StringData> {
};

// to_str() is used by the integer index. The existing StringIndex is re-used for this
// by making IntegerColumn convert its integers to strings by calling to_str().

template <class T>
inline StringData to_str(T&& value, StringIndex::StringConversionBuffer& buffer)
{
    return GetIndexData<typename std::remove_reference<T>::type>::get_index_data(value, buffer);
}


inline StringIndex::StringIndex(ColumnBase* target_column, Allocator& alloc)
    : m_array(create_node(alloc, true)) // Throws
    , m_target_column(target_column)
{
}

inline StringIndex::StringIndex(ref_type ref, ArrayParent* parent, size_t ndx_in_parent, ColumnBase* target_column,
                                Allocator& alloc)
    : m_array(new IndexArray(alloc))
    , m_target_column(target_column)
{
    REALM_ASSERT_EX(Array::get_context_flag_from_header(alloc.translate(ref)), ref, size_t(alloc.translate(ref)));
    m_array->init_from_ref(ref);
    set_parent(parent, ndx_in_parent);
}

inline StringIndex::StringIndex(inner_node_tag, Allocator& alloc)
    : m_array(create_node(alloc, false)) // Throws
    , m_target_column(nullptr)
{
}

// Byte order of the key is *reversed*, so that for the integer index, the least significant
// byte comes first, so that it fits little-endian machines. That way we can perform fast
// range-lookups and iterate in order, etc, as future features. This, however, makes the same
// features slower for string indexes. Todo, we should reverse the order conditionally, depending
// on the column type.
inline StringIndex::key_type StringIndex::create_key(StringData str) noexcept
{
    key_type key = 0;

    if (str.size() >= 4)
        goto four;
    if (str.size() < 2) {
        if (str.size() == 0)
            goto none;
        goto one;
    }
    if (str.size() == 2)
        goto two;
    goto three;

// Create 4 byte index key
// (encoded like this to allow literal comparisons
// independently of endianness)
four:
    key |= (key_type(static_cast<unsigned char>(str[3])) << 0);
three:
    key |= (key_type(static_cast<unsigned char>(str[2])) << 8);
two:
    key |= (key_type(static_cast<unsigned char>(str[1])) << 16);
one:
    key |= (key_type(static_cast<unsigned char>(str[0])) << 24);
none:
    return key;
}

// Index works as follows: All non-NULL values are stored as if they had appended an 'X' character at the end. So
// "foo" is stored as if it was "fooX", and "" (empty string) is stored as "X". And NULLs are stored as empty strings.
inline StringIndex::key_type StringIndex::create_key(StringData str, size_t offset) noexcept
{
    if (str.is_null())
        return 0;

    if (offset > str.size())
        return 0;

    // for very short strings
    size_t tail = str.size() - offset;
    if (tail <= sizeof(key_type) - 1) {
        char buf[sizeof(key_type)];
        memset(buf, 0, sizeof(key_type));
        buf[tail] = 'X';
        memcpy(buf, str.data() + offset, tail);
        return create_key(StringData(buf, tail + 1));
    }
    // else fallback
    return create_key(str.substr(offset));
}

template <class T>
void StringIndex::insert(size_t row_ndx, T value, size_t num_rows, bool is_append)
{
    REALM_ASSERT_3(row_ndx, !=, npos);

    // If the new row is inserted after the last row in the table, we don't need
    // to adjust any row indexes.
    if (!is_append) {
        for (size_t i = 0; i < num_rows; ++i) {
            size_t row_ndx_2 = row_ndx + i;
            adjust_row_indexes(row_ndx_2, 1); // Throws
        }
    }

    StringConversionBuffer buffer;

    for (size_t i = 0; i < num_rows; ++i) {
        size_t row_ndx_2 = row_ndx + i;
        size_t offset = 0;                                            // First key from beginning of string
        insert_with_offset(row_ndx_2, to_str(value, buffer), offset); // Throws
    }
}

template <class T>
void StringIndex::insert(size_t row_ndx, util::Optional<T> value, size_t num_rows, bool is_append)
{
    if (value) {
        insert(row_ndx, *value, num_rows, is_append);
    }
    else {
        insert(row_ndx, null{}, num_rows, is_append);
    }
}

template <class T>
void StringIndex::set(size_t row_ndx, T new_value)
{
    StringConversionBuffer buffer;
    StringConversionBuffer buffer2;
    StringData old_value = get(row_ndx, buffer);
    StringData new_value2 = to_str(new_value, buffer2);

    // Note that insert_with_offset() throws UniqueConstraintViolation.

    if (REALM_LIKELY(new_value2 != old_value)) {
        // We must erase this row first because erase uses find_first which
        // might find the duplicate if we insert before erasing.
        bool is_last = true;        // To avoid updating refs
        erase<T>(row_ndx, is_last); // Throws

        size_t offset = 0;                               // First key from beginning of string
        insert_with_offset(row_ndx, new_value2, offset); // Throws
    }
}

template <class T>
void StringIndex::set(size_t row_ndx, util::Optional<T> new_value)
{
    if (new_value) {
        set(row_ndx, *new_value);
    }
    else {
        set(row_ndx, null{});
    }
}

template <class T>
void StringIndex::erase(size_t row_ndx, bool is_last)
{
    StringConversionBuffer buffer;
    StringData value = get(row_ndx, buffer);

    do_delete(row_ndx, value, 0);

    // Collapse top nodes with single item
    while (m_array->is_inner_bptree_node()) {
        REALM_ASSERT(m_array->size() > 1); // node cannot be empty
        if (m_array->size() > 2)
            break;

        ref_type ref = m_array->get_as_ref(1);
        m_array->set(1, 1); // avoid destruction of the extracted ref
        m_array->destroy_deep();
        m_array->init_from_ref(ref);
        m_array->update_parent();
    }

    // If it is last item in column, we don't have to update refs
    if (!is_last)
        adjust_row_indexes(row_ndx, -1);
}

template <class T>
size_t StringIndex::find_first(T value) const
{
    // Use direct access method
    StringConversionBuffer buffer;
    return m_array->index_string_find_first(to_str(value, buffer), m_target_column);
}

template <class T>
void StringIndex::find_all(IntegerColumn& result, T value, bool case_insensitive) const
{
    // Use direct access method
    StringConversionBuffer buffer;
    return m_array->index_string_find_all(result, to_str(value, buffer), m_target_column, case_insensitive);
}

template <class T>
FindRes StringIndex::find_all_no_copy(T value, InternalFindResult& result) const
{
    // Use direct access method
    StringConversionBuffer buffer;
    return m_array->index_string_find_all_no_copy(to_str(value, buffer), m_target_column, result);
}

template <class T>
size_t StringIndex::count(T value) const
{
    // Use direct access method
    StringConversionBuffer buffer;
    return m_array->index_string_count(to_str(value, buffer), m_target_column);
}

template <class T>
void StringIndex::update_ref(T value, size_t old_row_ndx, size_t new_row_ndx)
{
    StringConversionBuffer buffer;
    do_update_ref(to_str(value, buffer), old_row_ndx, new_row_ndx, 0);
}

inline void StringIndex::destroy() noexcept
{
    return m_array->destroy_deep();
}

inline bool StringIndex::is_attached() const noexcept
{
    return m_array->is_attached();
}

inline void StringIndex::refresh_accessor_tree(size_t, const Spec&)
{
    m_array->init_from_parent();
}

inline ref_type StringIndex::get_ref() const noexcept
{
    return m_array->get_ref();
}

inline void StringIndex::set_parent(ArrayParent* parent, size_t ndx_in_parent) noexcept
{
    m_array->set_parent(parent, ndx_in_parent);
}

inline size_t StringIndex::get_ndx_in_parent() const noexcept
{
    return m_array->get_ndx_in_parent();
}

inline void StringIndex::set_ndx_in_parent(size_t ndx_in_parent) noexcept
{
    m_array->set_ndx_in_parent(ndx_in_parent);
}

inline void StringIndex::update_from_parent(size_t old_baseline) noexcept
{
    m_array->update_from_parent(old_baseline);
}

} // namespace realm

#endif // REALM_INDEX_STRING_HPP
