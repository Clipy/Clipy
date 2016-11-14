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

#ifndef REALM_COLUMN_STRING_HPP
#define REALM_COLUMN_STRING_HPP

#include <memory>
#include <realm/array_string.hpp>
#include <realm/array_string_long.hpp>
#include <realm/array_blobs_big.hpp>
#include <realm/column.hpp>
#include <realm/column_tpl.hpp>

namespace realm {

// Pre-declarations
class StringIndex;


/// A string column (StringColumn) is a single B+-tree, and
/// the root of the column is the root of the B+-tree. Leaf nodes are
/// either of type ArrayString (array of small strings),
/// ArrayStringLong (array of medium strings), or ArrayBigBlobs (array
/// of big strings).
///
/// A string column can optionally be equipped with a search index. If
/// it is, then the root ref of the index is stored in
/// Table::m_columns immediately after the root ref of the string
/// column.
class StringColumn : public ColumnBaseSimple {
public:
    typedef StringData value_type;

    StringColumn(Allocator&, ref_type, bool nullable = false, size_t column_ndx = npos);
    ~StringColumn() noexcept override;

    void destroy() noexcept override;

    size_t size() const noexcept final;
    bool is_empty() const noexcept
    {
        return size() == 0;
    }

    bool is_null(size_t ndx) const noexcept final;
    void set_null(size_t ndx) final;
    StringData get(size_t ndx) const noexcept;
    void set(size_t ndx, StringData);
    void add();
    void add(StringData value);
    void insert(size_t ndx);
    void insert(size_t ndx, StringData value);
    void erase(size_t row_ndx);
    void move_last_over(size_t row_ndx);
    void swap_rows(size_t row_ndx_1, size_t row_ndx_2) override;
    void clear();

    size_t count(StringData value) const;
    size_t find_first(StringData value, size_t begin = 0, size_t end = npos) const;
    void find_all(IntegerColumn& result, StringData value, size_t begin = 0, size_t end = npos) const;
    FindRes find_all_no_copy(StringData value, InternalFindResult& result) const;

    int compare_values(size_t, size_t) const noexcept override;

    //@{
    /// Find the lower/upper bound for the specified value assuming
    /// that the elements are already sorted in ascending order
    /// according to StringData::operator<().
    size_t lower_bound_string(StringData value) const noexcept;
    size_t upper_bound_string(StringData value) const noexcept;
    //@}

    void set_string(size_t, StringData) override;

    bool is_nullable() const noexcept final;

    // Search index
    StringData get_index_data(size_t ndx, StringIndex::StringConversionBuffer& buffer) const noexcept final;
    bool has_search_index() const noexcept override;
    void set_search_index_ref(ref_type, ArrayParent*, size_t, bool) override;
    void set_search_index_allow_duplicate_values(bool) noexcept override;
    StringIndex* get_search_index() noexcept override;
    const StringIndex* get_search_index() const noexcept override;
    std::unique_ptr<StringIndex> release_search_index() noexcept;
    bool supports_search_index() const noexcept final
    {
        return true;
    }
    StringIndex* create_search_index() override;

    // Simply inserts all column values in the index in a loop
    void populate_search_index();
    void destroy_search_index() noexcept override;

    // Optimizing data layout. enforce == true will enforce enumeration;
    // enforce == false will auto-evaluate if it should be enumerated or not
    bool auto_enumerate(ref_type& keys, ref_type& values, bool enforce = false) const;

    /// Compare two string columns for equality.
    bool compare_string(const StringColumn&) const;

    enum LeafType {
        leaf_type_Small,  ///< ArrayString
        leaf_type_Medium, ///< ArrayStringLong
        leaf_type_Big     ///< ArrayBigBlobs
    };

    std::unique_ptr<const ArrayParent> get_leaf(size_t ndx, size_t& out_ndx_in_parent, LeafType& out_leaf_type) const;

    static ref_type create(Allocator&, size_t size = 0);

    static size_t get_size_from_ref(ref_type root_ref, Allocator&) noexcept;

    // Overrriding method in ColumnBase
    ref_type write(size_t, size_t, size_t, _impl::OutputStream&) const override;

    void insert_rows(size_t, size_t, size_t, bool) override;
    void erase_rows(size_t, size_t, size_t, bool) override;
    void move_last_row_over(size_t, size_t, bool) override;
    void clear(size_t, bool) override;
    void set_ndx_in_parent(size_t ndx_in_parent) noexcept override;
    void update_from_parent(size_t old_baseline) noexcept override;
    void refresh_accessor_tree(size_t, const Spec&) override;

    void verify() const override;
    void verify(const Table&, size_t) const override;
    void to_dot(std::ostream&, StringData title) const override;
    void do_dump_node_structure(std::ostream&, int) const override;

private:
    std::unique_ptr<StringIndex> m_search_index;
    bool m_nullable;

    LeafType get_block(size_t ndx, ArrayParent**, size_t& off, bool use_retval = false) const;

    /// If you are appending and have the size of the column readily available,
    /// call the 4 argument version instead. If you are not appending, either
    /// one is fine.
    ///
    /// \param row_ndx Must be `realm::npos` if appending.
    void do_insert(size_t row_ndx, StringData value, size_t num_rows);

    /// If you are appending and you do not have the size of the column readily
    /// available, call the 3 argument version instead. If you are not
    /// appending, either one is fine.
    ///
    /// \param is_append Must be true if, and only if `row_ndx` is equal to the
    /// size of the column (before insertion).
    void do_insert(size_t row_ndx, StringData value, size_t num_rows, bool is_append);

    /// \param row_ndx Must be `realm::npos` if appending.
    void bptree_insert(size_t row_ndx, StringData value, size_t num_rows);

    // Called by Array::bptree_insert().
    static ref_type leaf_insert(MemRef leaf_mem, ArrayParent&, size_t ndx_in_parent, Allocator&, size_t insert_ndx,
                                BpTreeNode::TreeInsert<StringColumn>& state);

    class EraseLeafElem;
    class CreateHandler;
    class SliceHandler;

    void do_erase(size_t row_ndx, bool is_last);
    void do_move_last_over(size_t row_ndx, size_t last_row_ndx);
    void do_swap_rows(size_t row_ndx_1, size_t row_ndx_2);
    void do_clear();

    /// Root must be a leaf. Upgrades the root leaf as
    /// necessary. Returns the type of the root leaf as it is upon
    /// return.
    LeafType upgrade_root_leaf(size_t value_size);

    void refresh_root_accessor();

    void leaf_to_dot(MemRef, ArrayParent*, size_t ndx_in_parent, std::ostream&) const override;

    friend class BpTreeNode;
    friend class ColumnBase;
};


// Implementation:

inline size_t StringColumn::size() const noexcept
{
    if (root_is_leaf()) {
        bool long_strings = m_array->has_refs();
        if (!long_strings) {
            // Small strings root leaf
            ArrayString* leaf = static_cast<ArrayString*>(m_array.get());
            return leaf->size();
        }
        bool is_big = m_array->get_context_flag();
        if (!is_big) {
            // Medium strings root leaf
            ArrayStringLong* leaf = static_cast<ArrayStringLong*>(m_array.get());
            return leaf->size();
        }
        // Big strings root leaf
        ArrayBigBlobs* leaf = static_cast<ArrayBigBlobs*>(m_array.get());
        return leaf->size();
    }
    // Non-leaf root
    BpTreeNode* node = static_cast<BpTreeNode*>(m_array.get());
    return node->get_bptree_size();
}

inline void StringColumn::add(StringData value)
{
    REALM_ASSERT(!(value.is_null() && !m_nullable));
    size_t row_ndx = realm::npos;
    size_t num_rows = 1;
    do_insert(row_ndx, value, num_rows); // Throws
}

inline void StringColumn::add()
{
    add(m_nullable ? realm::null() : StringData(""));
}

inline void StringColumn::insert(size_t row_ndx, StringData value)
{
    REALM_ASSERT(!(value.is_null() && !m_nullable));
    size_t column_size = this->size();
    REALM_ASSERT_3(row_ndx, <=, column_size);
    size_t num_rows = 1;
    bool is_append = row_ndx == column_size;
    do_insert(row_ndx, value, num_rows, is_append); // Throws
}

inline void StringColumn::insert(size_t row_ndx)
{
    insert(row_ndx, m_nullable ? realm::null() : StringData(""));
}

inline void StringColumn::erase(size_t row_ndx)
{
    size_t last_row_ndx = size() - 1; // Note that size() is slow
    bool is_last = row_ndx == last_row_ndx;
    do_erase(row_ndx, is_last); // Throws
}

inline void StringColumn::move_last_over(size_t row_ndx)
{
    size_t last_row_ndx = size() - 1;         // Note that size() is slow
    do_move_last_over(row_ndx, last_row_ndx); // Throws
}

inline void StringColumn::swap_rows(size_t row_ndx_1, size_t row_ndx_2)
{
    do_swap_rows(row_ndx_1, row_ndx_2); // Throws
}

inline void StringColumn::clear()
{
    do_clear(); // Throws
}

inline int StringColumn::compare_values(size_t row1, size_t row2) const noexcept
{
    StringData a = get(row1);
    StringData b = get(row2);

    if (a.is_null() && !b.is_null())
        return 1;
    else if (b.is_null() && !a.is_null())
        return -1;
    else if (a.is_null() && b.is_null())
        return 0;

    if (a == b)
        return 0;
    return utf8_compare(a, b) ? 1 : -1;
}

inline void StringColumn::set_string(size_t row_ndx, StringData value)
{
    REALM_ASSERT(!(value.is_null() && !m_nullable));
    set(row_ndx, value); // Throws
}

inline bool StringColumn::has_search_index() const noexcept
{
    return m_search_index != 0;
}

inline StringIndex* StringColumn::get_search_index() noexcept
{
    return m_search_index.get();
}

inline const StringIndex* StringColumn::get_search_index() const noexcept
{
    return m_search_index.get();
}

inline size_t StringColumn::get_size_from_ref(ref_type root_ref, Allocator& alloc) noexcept
{
    const char* root_header = alloc.translate(root_ref);
    bool root_is_leaf = !Array::get_is_inner_bptree_node_from_header(root_header);
    if (root_is_leaf) {
        bool long_strings = Array::get_hasrefs_from_header(root_header);
        if (!long_strings) {
            // Small strings leaf
            return ArrayString::get_size_from_header(root_header);
        }
        bool is_big = Array::get_context_flag_from_header(root_header);
        if (!is_big) {
            // Medium strings leaf
            return ArrayStringLong::get_size_from_header(root_header, alloc);
        }
        // Big strings leaf
        return ArrayBigBlobs::get_size_from_header(root_header);
    }

    return BpTreeNode::get_bptree_size_from_header(root_header);
}

// Implementing pure virtual method of ColumnBase.
inline void StringColumn::insert_rows(size_t row_ndx, size_t num_rows_to_insert, size_t prior_num_rows,
                                      bool insert_nulls)
{
    REALM_ASSERT_DEBUG(prior_num_rows == size());
    REALM_ASSERT(row_ndx <= prior_num_rows);
    REALM_ASSERT(!insert_nulls || m_nullable);

    StringData value = m_nullable ? realm::null() : StringData("");
    bool is_append = (row_ndx == prior_num_rows);
    do_insert(row_ndx, value, num_rows_to_insert, is_append); // Throws
}

// Implementing pure virtual method of ColumnBase.
inline void StringColumn::erase_rows(size_t row_ndx, size_t num_rows_to_erase, size_t prior_num_rows, bool)
{
    REALM_ASSERT_DEBUG(prior_num_rows == size());
    REALM_ASSERT(num_rows_to_erase <= prior_num_rows);
    REALM_ASSERT(row_ndx <= prior_num_rows - num_rows_to_erase);

    bool is_last = (row_ndx + num_rows_to_erase == prior_num_rows);
    for (size_t i = num_rows_to_erase; i > 0; --i) {
        size_t row_ndx_2 = row_ndx + i - 1;
        do_erase(row_ndx_2, is_last); // Throws
    }
}

// Implementing pure virtual method of ColumnBase.
inline void StringColumn::move_last_row_over(size_t row_ndx, size_t prior_num_rows, bool)
{
    REALM_ASSERT_DEBUG(prior_num_rows == size());
    REALM_ASSERT(row_ndx < prior_num_rows);

    size_t last_row_ndx = prior_num_rows - 1;
    do_move_last_over(row_ndx, last_row_ndx); // Throws
}

// Implementing pure virtual method of ColumnBase.
inline void StringColumn::clear(size_t, bool)
{
    do_clear(); // Throws
}

} // namespace realm

#endif // REALM_COLUMN_STRING_HPP
