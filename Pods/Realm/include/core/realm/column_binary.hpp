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

#ifndef REALM_COLUMN_BINARY_HPP
#define REALM_COLUMN_BINARY_HPP

#include <realm/column.hpp>
#include <realm/array_binary.hpp>
#include <realm/array_blobs_big.hpp>

namespace realm {


/// A binary column (BinaryColumn) is a single B+-tree, and the root
/// of the column is the root of the B+-tree. Leaf nodes are either of
/// type ArrayBinary (array of small blobs) or ArrayBigBlobs (array of
/// big blobs).
class BinaryColumn : public ColumnBaseSimple {
public:
    typedef BinaryData value_type;

    BinaryColumn(Allocator&, ref_type, bool nullable = false, size_t column_ndx = npos);

    size_t size() const noexcept final;
    bool is_empty() const noexcept
    {
        return size() == 0;
    }
    bool is_nullable() const noexcept override;

    BinaryData get(size_t ndx) const noexcept;

    /// Return data from position 'pos' and onwards. If the blob is distributed
    /// across multiple arrays (if bigger than ~ 16M), you will only get data
    /// from one array. 'pos' will be updated to be an index to next available
    /// data. It will be 0 if no more data.
    BinaryData get_at(size_t ndx, size_t& pos) const noexcept;

    bool is_null(size_t ndx) const noexcept override;
    StringData get_index_data(size_t, StringIndex::StringConversionBuffer&) const noexcept final;

    void add(BinaryData value);
    void set(size_t ndx, BinaryData value, bool add_zero_term = false);
    void set_null(size_t ndx) override;
    void insert(size_t ndx, BinaryData value);
    void erase(size_t row_ndx);
    void erase(size_t row_ndx, bool is_last);
    void move_last_over(size_t row_ndx);
    void swap_rows(size_t row_ndx_1, size_t row_ndx_2) override;
    void clear();
    size_t find_first(BinaryData value) const;

    // Requires that the specified entry was inserted as StringData.
    StringData get_string(size_t ndx) const noexcept;

    void add_string(StringData value);
    void set_string(size_t ndx, StringData value) override;
    void insert_string(size_t ndx, StringData value);

    /// Compare two binary columns for equality.
    bool compare_binary(const BinaryColumn&) const;

    int compare_values(size_t row1, size_t row2) const noexcept override;

    static ref_type create(Allocator&, size_t size, bool nullable);

    static size_t get_size_from_ref(ref_type root_ref, Allocator&) noexcept;

    // Overrriding method in ColumnBase
    ref_type write(size_t, size_t, size_t, _impl::OutputStream&) const override;

    void insert_rows(size_t, size_t, size_t, bool) override;
    void erase_rows(size_t, size_t, size_t, bool) override;
    void move_last_row_over(size_t, size_t, bool) override;
    void clear(size_t, bool) override;
    void update_from_parent(size_t) noexcept override;
    void refresh_accessor_tree(size_t, const Spec&) override;

    /// In contrast to update_from_parent(), this function is able to handle
    /// cases where the accessed payload data has changed. In particular, it
    /// handles cases where the B+-tree switches from having one level (root is
    /// a leaf node), to having multiple levels (root is an inner node). Note
    /// that this is at the expense of loosing the `noexcept` guarantee.
    void update_from_ref(ref_type ref);

    void verify() const override;
    void to_dot(std::ostream&, StringData title) const override;
    void do_dump_node_structure(std::ostream&, int) const override;

private:
    /// \param row_ndx Must be `realm::npos` if appending.
    void do_insert(size_t row_ndx, BinaryData value, bool add_zero_term, size_t num_rows);

    // Called by Array::bptree_insert().
    static ref_type leaf_insert(MemRef leaf_mem, ArrayParent&, size_t ndx_in_parent, Allocator&, size_t insert_ndx,
                                BpTreeNode::TreeInsert<BinaryColumn>& state);

    struct InsertState : BpTreeNode::TreeInsert<BinaryColumn> {
        bool m_add_zero_term;
    };

    class EraseLeafElem;
    class CreateHandler;
    class SliceHandler;

    void do_move_last_over(size_t row_ndx, size_t last_row_ndx);
    void do_clear();

    /// Root must be a leaf. Upgrades the root leaf if
    /// necessary. Returns true if, and only if the root is a 'big
    /// blobs' leaf upon return.
    bool upgrade_root_leaf(size_t value_size);

    bool m_nullable = false;

    void leaf_to_dot(MemRef, ArrayParent*, size_t ndx_in_parent, std::ostream&) const override;

    friend class BpTreeNode;
    friend class ColumnBase;
};

class BinaryIterator {
public:
    BinaryIterator()
    {
    }
    // TODO: When WriteLogCollector is removed, there is no need for this
    BinaryIterator(BinaryData binary)
        : m_binary(binary)
    {
    }

    BinaryIterator(const BinaryColumn* col, size_t ndx)
        : m_binary_col(col)
        , m_ndx(ndx)
    {
    }

    BinaryData get_next() noexcept
    {
        if (!end_of_data) {
            if (m_binary_col) {
                BinaryData ret = m_binary_col->get_at(m_ndx, m_pos);
                end_of_data = (m_pos == 0);
                return ret;
            }
            else if (!m_binary.is_null()) {
                end_of_data = true;
                return m_binary;
            }
        }
        return {};
    }

private:
    bool end_of_data = false;
    const BinaryColumn* m_binary_col = nullptr;
    size_t m_ndx = 0;
    size_t m_pos = 0;
    BinaryData m_binary;
};


// Implementation

// LCOV_EXCL_START
inline StringData BinaryColumn::get_index_data(size_t, StringIndex::StringConversionBuffer&) const noexcept
{
    REALM_ASSERT(false && "Index not implemented for BinaryColumn.");
    REALM_UNREACHABLE();
}
// LCOV_EXCL_STOP

inline size_t BinaryColumn::size() const noexcept
{
    if (root_is_leaf()) {
        bool is_big = m_array->get_context_flag();
        if (!is_big) {
            // Small blobs root leaf
            ArrayBinary* leaf = static_cast<ArrayBinary*>(m_array.get());
            return leaf->size();
        }
        // Big blobs root leaf
        ArrayBigBlobs* leaf = static_cast<ArrayBigBlobs*>(m_array.get());
        return leaf->size();
    }
    // Non-leaf root
    return static_cast<BpTreeNode*>(m_array.get())->get_bptree_size();
}

inline bool BinaryColumn::is_nullable() const noexcept
{
    return m_nullable;
}

inline void BinaryColumn::update_from_parent(size_t old_baseline) noexcept
{
    if (root_is_leaf()) {
        bool is_big = m_array->get_context_flag();
        if (!is_big) {
            // Small blobs root leaf
            REALM_ASSERT(dynamic_cast<ArrayBinary*>(m_array.get()));
            ArrayBinary* leaf = static_cast<ArrayBinary*>(m_array.get());
            leaf->update_from_parent(old_baseline);
            return;
        }
        // Big blobs root leaf
        REALM_ASSERT(dynamic_cast<ArrayBigBlobs*>(m_array.get()));
        ArrayBigBlobs* leaf = static_cast<ArrayBigBlobs*>(m_array.get());
        leaf->update_from_parent(old_baseline);
        return;
    }
    // Non-leaf root
    m_array->update_from_parent(old_baseline);
}

inline BinaryData BinaryColumn::get(size_t ndx) const noexcept
{
    REALM_ASSERT_DEBUG(ndx < size());
    if (root_is_leaf()) {
        bool is_big = m_array->get_context_flag();
        BinaryData ret;
        if (!is_big) {
            // Small blobs root leaf
            ArrayBinary* leaf = static_cast<ArrayBinary*>(m_array.get());
            ret = leaf->get(ndx);
        }
        else {
            // Big blobs root leaf
            ArrayBigBlobs* leaf = static_cast<ArrayBigBlobs*>(m_array.get());
            ret = leaf->get(ndx);
        }
        if (!m_nullable && ret.is_null())
            return BinaryData("", 0); // return empty string (non-null)
        return ret;
    }

    // Non-leaf root
    std::pair<MemRef, size_t> p = static_cast<BpTreeNode*>(m_array.get())->get_bptree_leaf(ndx);
    const char* leaf_header = p.first.get_addr();
    size_t ndx_in_leaf = p.second;
    Allocator& alloc = m_array->get_alloc();
    bool is_big = Array::get_context_flag_from_header(leaf_header);
    if (!is_big) {
        // Small blobs
        return ArrayBinary::get(leaf_header, ndx_in_leaf, alloc);
    }
    // Big blobs
    return ArrayBigBlobs::get(leaf_header, ndx_in_leaf, alloc);
}

inline bool BinaryColumn::is_null(size_t ndx) const noexcept
{
    return m_nullable && get(ndx).is_null();
}

inline StringData BinaryColumn::get_string(size_t ndx) const noexcept
{
    BinaryData bin = get(ndx);
    REALM_ASSERT_3(0, <, bin.size());
    return StringData(bin.data(), bin.size() - 1);
}

inline void BinaryColumn::set_string(size_t ndx, StringData value)
{
    if (value.is_null() && !m_nullable)
        throw LogicError(LogicError::column_not_nullable);

    BinaryData bin(value.data(), value.size());
    bool add_zero_term = true;
    set(ndx, bin, add_zero_term);
}

inline void BinaryColumn::add(BinaryData value)
{
    if (value.is_null() && !m_nullable)
        throw LogicError(LogicError::column_not_nullable);

    size_t row_ndx = realm::npos;
    bool add_zero_term = false;
    size_t num_rows = 1;
    do_insert(row_ndx, value, add_zero_term, num_rows); // Throws
}

inline void BinaryColumn::insert(size_t row_ndx, BinaryData value)
{
    if (value.is_null() && !m_nullable)
        throw LogicError(LogicError::column_not_nullable);

    size_t column_size = this->size(); // Slow
    REALM_ASSERT_3(row_ndx, <=, column_size);
    size_t row_ndx_2 = row_ndx == column_size ? realm::npos : row_ndx;
    bool add_zero_term = false;
    size_t num_rows = 1;
    do_insert(row_ndx_2, value, add_zero_term, num_rows); // Throws
}

inline void BinaryColumn::set_null(size_t row_ndx)
{
    set(row_ndx, BinaryData{});
}

inline size_t BinaryColumn::find_first(BinaryData value) const
{
    for (size_t t = 0; t < size(); t++)
        if (get(t) == value)
            return t;

    return not_found;
}


inline void BinaryColumn::erase(size_t row_ndx)
{
    size_t last_row_ndx = size() - 1; // Note that size() is slow
    bool is_last = row_ndx == last_row_ndx;
    erase(row_ndx, is_last); // Throws
}

inline void BinaryColumn::move_last_over(size_t row_ndx)
{
    size_t last_row_ndx = size() - 1;         // Note that size() is slow
    do_move_last_over(row_ndx, last_row_ndx); // Throws
}

inline void BinaryColumn::clear()
{
    do_clear(); // Throws
}

// Implementing pure virtual method of ColumnBase.
inline void BinaryColumn::insert_rows(size_t row_ndx, size_t num_rows_to_insert, size_t prior_num_rows,
                                      bool insert_nulls)
{
    REALM_ASSERT_DEBUG(prior_num_rows == size());
    REALM_ASSERT(row_ndx <= prior_num_rows);
    REALM_ASSERT(!insert_nulls || m_nullable);

    size_t row_ndx_2 = (row_ndx == prior_num_rows ? realm::npos : row_ndx);
    BinaryData value = m_nullable ? BinaryData() : BinaryData("", 0);
    bool add_zero_term = false;
    do_insert(row_ndx_2, value, add_zero_term, num_rows_to_insert); // Throws
}

// Implementing pure virtual method of ColumnBase.
inline void BinaryColumn::erase_rows(size_t row_ndx, size_t num_rows_to_erase, size_t prior_num_rows, bool)
{
    REALM_ASSERT_DEBUG(prior_num_rows == size());
    REALM_ASSERT(num_rows_to_erase <= prior_num_rows);
    REALM_ASSERT(row_ndx <= prior_num_rows - num_rows_to_erase);

    bool is_last = (row_ndx + num_rows_to_erase == prior_num_rows);
    for (size_t i = num_rows_to_erase; i > 0; --i) {
        size_t row_ndx_2 = row_ndx + i - 1;
        erase(row_ndx_2, is_last); // Throws
    }
}

// Implementing pure virtual method of ColumnBase.
inline void BinaryColumn::move_last_row_over(size_t row_ndx, size_t prior_num_rows, bool)
{
    REALM_ASSERT_DEBUG(prior_num_rows == size());
    REALM_ASSERT(row_ndx < prior_num_rows);

    size_t last_row_ndx = prior_num_rows - 1;
    do_move_last_over(row_ndx, last_row_ndx); // Throws
}

// Implementing pure virtual method of ColumnBase.
inline void BinaryColumn::clear(size_t, bool)
{
    do_clear(); // Throws
}

inline void BinaryColumn::add_string(StringData value)
{
    size_t row_ndx = realm::npos;
    BinaryData value_2(value.data(), value.size());
    bool add_zero_term = true;
    size_t num_rows = 1;
    do_insert(row_ndx, value_2, add_zero_term, num_rows); // Throws
}

inline void BinaryColumn::insert_string(size_t row_ndx, StringData value)
{
    size_t column_size = this->size(); // Slow
    REALM_ASSERT_3(row_ndx, <=, column_size);
    size_t row_ndx_2 = row_ndx == column_size ? realm::npos : row_ndx;
    BinaryData value_2(value.data(), value.size());
    bool add_zero_term = false;
    size_t num_rows = 1;
    do_insert(row_ndx_2, value_2, add_zero_term, num_rows); // Throws
}

inline size_t BinaryColumn::get_size_from_ref(ref_type root_ref, Allocator& alloc) noexcept
{
    const char* root_header = alloc.translate(root_ref);
    bool root_is_leaf = !Array::get_is_inner_bptree_node_from_header(root_header);
    if (root_is_leaf) {
        bool is_big = Array::get_context_flag_from_header(root_header);
        if (!is_big) {
            // Small blobs leaf
            return ArrayBinary::get_size_from_header(root_header, alloc);
        }
        // Big blobs leaf
        return ArrayBigBlobs::get_size_from_header(root_header);
    }
    return BpTreeNode::get_bptree_size_from_header(root_header);
}


} // namespace realm

#endif // REALM_COLUMN_BINARY_HPP
