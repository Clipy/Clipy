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
#include <realm/cluster_tree.hpp>

/*
The StringIndex class is used for both type_String and all integral types, such as type_Bool, type_Timestamp and
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
class ClusterColumn;

template <class T>
class BPlusTree;

/// Each StringIndex node contains an array of this type
class IndexArray : public Array {
public:
    IndexArray(Allocator& allocator)
        : Array(allocator)
    {
    }

    ObjKey index_string_find_first(StringData value, const ClusterColumn& column) const;
    void index_string_find_all(std::vector<ObjKey>& result, StringData value, const ClusterColumn& column,
                               bool case_insensitive = false) const;
    FindRes index_string_find_all_no_copy(StringData value, const ClusterColumn& column,
                                          InternalFindResult& result) const;
    size_t index_string_count(StringData value, const ClusterColumn& column) const;

private:
    template <IndexMethod>
    int64_t from_list(StringData value, InternalFindResult& result_ref, const IntegerColumn& key_values,
                      const ClusterColumn& column) const;

    void from_list_all(StringData value, std::vector<ObjKey>& result, const IntegerColumn& rows,
                       const ClusterColumn& column) const;

    void from_list_all_ins(StringData value, std::vector<ObjKey>& result, const IntegerColumn& rows,
                           const ClusterColumn& column) const;

    template <IndexMethod method>
    int64_t index_string(StringData value, InternalFindResult& result_ref, const ClusterColumn& column) const;

    void index_string_all(StringData value, std::vector<ObjKey>& result, const ClusterColumn& column) const;

    void index_string_all_ins(StringData value, std::vector<ObjKey>& result, const ClusterColumn& column) const;
};

// 12 is the biggest element size of any non-string/binary Realm type
constexpr size_t string_conversion_buffer_size = 12;
using StringConversionBuffer = std::array<char, string_conversion_buffer_size>;

// The purpose of this class is to get easy access to fields in a specific column in the
// cluster. When you have an object like this, you can get a string version of the relevant
// field based on the key for the object.
class ClusterColumn {
public:
    ClusterColumn(const ClusterTree* cluster_tree, ColKey column_key)
        : m_cluster_tree(cluster_tree)
        , m_column_key(column_key)
    {
    }
    size_t size() const
    {
        return m_cluster_tree->size();
    }
    ClusterTree::ConstIterator begin() const
    {
        return ClusterTree::ConstIterator(*m_cluster_tree, 0);
    }

    ClusterTree::ConstIterator end() const
    {
        return ClusterTree::ConstIterator(*m_cluster_tree, size());
    }


    DataType get_data_type() const;
    ColKey get_column_key() const
    {
        return m_column_key;
    }
    bool is_nullable() const;
    StringData get_index_data(ObjKey key, StringConversionBuffer& buffer) const;

private:
    const ClusterTree* m_cluster_tree;
    ColKey m_column_key;
};

class StringIndex {
public:
    StringIndex(const ClusterColumn& target_column, Allocator&);
    StringIndex(ref_type, ArrayParent*, size_t ndx_in_parent, const ClusterColumn& target_column, Allocator&);
    ~StringIndex() noexcept
    {
    }

    ColKey get_column_key() const
    {
        return m_target_column.get_column_key();
    }

    template <class T>
    static bool type_supported()
    {
        return realm::is_any<T, int64_t, int, StringData, bool, Timestamp>::value;
    }
    static bool type_supported(realm::DataType type)
    {
        return (type == type_Int || type == type_String || type == type_Bool || type == type_Timestamp);
    }

    static ref_type create_empty(Allocator& alloc);

    void set_target(const ClusterColumn& target_column) noexcept;

    // Accessor concept:
    Allocator& get_alloc() const noexcept;
    void destroy() noexcept;
    void detach();
    bool is_attached() const noexcept;
    void set_parent(ArrayParent* parent, size_t ndx_in_parent) noexcept;
    size_t get_ndx_in_parent() const noexcept;
    void set_ndx_in_parent(size_t ndx_in_parent) noexcept;
    void update_from_parent(size_t old_baseline) noexcept;
    void refresh_accessor_tree(const ClusterColumn& target_column);
    ref_type get_ref() const noexcept;

    // StringIndex interface:

    bool is_empty() const;

    template <class T>
    void insert(ObjKey key, T value);
    template <class T>
    void insert(ObjKey key, util::Optional<T> value);

    template <class T>
    void set(ObjKey key, T new_value);
    template <class T>
    void set(ObjKey key, util::Optional<T> new_value);

    void erase(ObjKey key);

    template <class T>
    ObjKey find_first(T value) const;
    template <class T>
    void find_all(std::vector<ObjKey>& result, T value, bool case_insensitive = false) const;
    template <class T>
    FindRes find_all_no_copy(T value, InternalFindResult& result) const;
    template <class T>
    size_t count(T value) const;
    template <class T>
    void update_ref(T value, size_t old_row_ndx, size_t new_row_ndx);

    void clear();

    void distinct(BPlusTree<ObjKey>& result) const;
    bool has_duplicate_values() const noexcept;

    void verify() const;
#ifdef REALM_DEBUG
    template <class T>
    void verify_entries(const ClusterColumn& column) const;
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
    ClusterColumn m_target_column;

    struct inner_node_tag {
    };
    StringIndex(inner_node_tag, Allocator&);

    static IndexArray* create_node(Allocator&, bool is_leaf);

    void insert_with_offset(ObjKey key, StringData value, size_t offset);
    void insert_row_list(size_t ref, size_t offset, StringData value);
    void insert_to_existing_list(ObjKey key, StringData value, IntegerColumn& list);
    void insert_to_existing_list_at_lower(ObjKey key, StringData value, IntegerColumn& list,
                                          const IntegerColumnIterator& lower);
    key_type get_last_key() const;

    struct NodeChange {
        size_t ref1;
        size_t ref2;
        enum ChangeType { change_None, change_InsertBefore, change_InsertAfter, change_Split } type;
        NodeChange(ChangeType t, size_t r1 = 0, size_t r2 = 0)
            : ref1(r1)
            , ref2(r2)
            , type(t)
        {
        }
        NodeChange()
            : ref1(0)
            , ref2(0)
            , type(change_None)
        {
        }
    };

    // B-Tree functions
    void TreeInsert(ObjKey obj_key, key_type, size_t offset, StringData value);
    NodeChange do_insert(ObjKey, key_type, size_t offset, StringData value);
    /// Returns true if there is room or it can join existing entries
    bool leaf_insert(ObjKey obj_key, key_type, size_t offset, StringData value, bool noextend = false);
    void node_insert_split(size_t ndx, size_t new_ref);
    void node_insert(size_t ndx, size_t ref);
    void do_delete(ObjKey key, StringData, size_t offset);

    StringData get(ObjKey key, StringConversionBuffer& buffer) const;

    void node_add_key(ref_type ref);

#ifdef REALM_DEBUG
    static void dump_node_structure(const Array& node, std::ostream&, int level);
    static void array_to_dot(std::ostream&, const Array&);
    static void keys_to_dot(std::ostream&, const Array&, StringData title = StringData());
#endif
};

class SortedListComparator {
public:
    SortedListComparator(const ClusterTree* cluster_tree, ColKey column_key)
        : m_column(cluster_tree, column_key)
    {
    }
    SortedListComparator(const ClusterColumn& column)
        : m_column(column)
    {
    }

    bool operator()(int64_t key_value, StringData needle);
    bool operator()(StringData needle, int64_t key_value);

private:
    const ClusterColumn m_column;
};


// Implementation:

template <class T>
struct GetIndexData;

template <>
struct GetIndexData<Timestamp> {
    static StringData get_index_data(const Timestamp& dt, StringConversionBuffer& buffer);
};

template <>
struct GetIndexData<int64_t> {
    static StringData get_index_data(const int64_t& value, StringConversionBuffer& buffer)
    {
        const char* c = reinterpret_cast<const char*>(&value);
        realm::safe_copy_n(c, sizeof(int64_t), buffer.data());
        return StringData{buffer.data(), sizeof(int64_t)};
    }
};

template <>
struct GetIndexData<bool> {
    static StringData get_index_data(const bool& value, StringConversionBuffer& buffer)
    {
        int64_t value2 = value ? 1 : 0;
        const char* c = reinterpret_cast<const char*>(&value2);
        realm::safe_copy_n(c, sizeof(int64_t), buffer.data());
        return StringData{buffer.data(), sizeof(int64_t)};
    }
};

template <>
struct GetIndexData<StringData> {
    static StringData get_index_data(StringData data, StringConversionBuffer&)
    {
        return data;
    }
};

template <>
struct GetIndexData<null> {
    static StringData get_index_data(null, StringConversionBuffer&)
    {
        return null{};
    }
};

template <class T>
struct GetIndexData<util::Optional<T>> {
    static StringData get_index_data(const util::Optional<T>& value, StringConversionBuffer& buffer)
    {
        if (value)
            return GetIndexData<T>::get_index_data(*value, buffer);
        return null{};
    }
};

template <>
struct GetIndexData<float> {
    static StringData get_index_data(float, StringConversionBuffer&)
    {
        REALM_ASSERT_RELEASE(false); // LCOV_EXCL_LINE; Index on float not supported
        return {};
    }
};

template <>
struct GetIndexData<double> {
    static StringData get_index_data(double, StringConversionBuffer&)
    {
        REALM_ASSERT_RELEASE(false); // LCOV_EXCL_LINE; Index on float not supported
        return {};
    }
};

template <>
struct GetIndexData<BinaryData> {
    static StringData get_index_data(BinaryData, StringConversionBuffer&)
    {
        REALM_ASSERT_RELEASE(false); // LCOV_EXCL_LINE; Index on float not supported
        return {};
    }
};

template <>
struct GetIndexData<const char*> : GetIndexData<StringData> {
};

// to_str() is used by the integer index. The existing StringIndex is re-used for this
// by making IntegerColumn convert its integers to strings by calling to_str().

template <class T>
inline StringData to_str(T&& value, StringConversionBuffer& buffer)
{
    return GetIndexData<typename std::remove_reference<T>::type>::get_index_data(value, buffer);
}


inline StringIndex::StringIndex(const ClusterColumn& target_column, Allocator& alloc)
    : m_array(create_node(alloc, true)) // Throws
    , m_target_column(target_column)
{
}

inline StringIndex::StringIndex(ref_type ref, ArrayParent* parent, size_t ndx_in_parent,
                                const ClusterColumn& target_column, Allocator& alloc)
    : m_array(new IndexArray(alloc))
    , m_target_column(target_column)
{
    REALM_ASSERT_EX(Array::get_context_flag_from_header(alloc.translate(ref)), ref, size_t(alloc.translate(ref)));
    m_array->init_from_ref(ref);
    set_parent(parent, ndx_in_parent);
}

inline StringIndex::StringIndex(inner_node_tag, Allocator& alloc)
    : m_array(create_node(alloc, false)) // Throws
    , m_target_column(ClusterColumn(nullptr, {}))
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
void StringIndex::insert(ObjKey key, T value)
{
    StringConversionBuffer buffer;
    size_t offset = 0;                                      // First key from beginning of string
    insert_with_offset(key, to_str(value, buffer), offset); // Throws
}

template <class T>
void StringIndex::insert(ObjKey key, util::Optional<T> value)
{
    if (value) {
        insert(key, *value);
    }
    else {
        insert(key, null{});
    }
}

template <class T>
void StringIndex::set(ObjKey key, T new_value)
{
    StringConversionBuffer buffer;
    StringConversionBuffer buffer2;
    StringData old_value = get(key, buffer);
    StringData new_value2 = to_str(new_value, buffer2);

    // Note that insert_with_offset() throws UniqueConstraintViolation.

    if (REALM_LIKELY(new_value2 != old_value)) {
        // We must erase this row first because erase uses find_first which
        // might find the duplicate if we insert before erasing.
        erase(key); // Throws

        size_t offset = 0;                               // First key from beginning of string
        insert_with_offset(key, new_value2, offset);     // Throws
    }
}

template <class T>
void StringIndex::set(ObjKey key, util::Optional<T> new_value)
{
    if (new_value) {
        set(key, *new_value);
    }
    else {
        set(key, null{});
    }
}

template <class T>
ObjKey StringIndex::find_first(T value) const
{
    // Use direct access method
    StringConversionBuffer buffer;
    return m_array->index_string_find_first(to_str(value, buffer), m_target_column);
}

template <class T>
void StringIndex::find_all(std::vector<ObjKey>& result, T value, bool case_insensitive) const
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

inline void StringIndex::refresh_accessor_tree(const ClusterColumn& target_column)
{
    m_array->init_from_parent();
    m_target_column = target_column;
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
