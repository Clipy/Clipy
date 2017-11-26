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

#ifndef REALM_COLUMN_HPP
#define REALM_COLUMN_HPP

#include <cstdint> // unint8_t etc
#include <cstdlib> // size_t
#include <vector>
#include <memory>

#include <realm/array_integer.hpp>
#include <realm/column_type.hpp>
#include <realm/column_fwd.hpp>
#include <realm/spec.hpp>
#include <realm/impl/output_stream.hpp>
#include <realm/query_conditions.hpp>
#include <realm/bptree.hpp>
#include <realm/index_string.hpp>
#include <realm/impl/destroy_guard.hpp>
#include <realm/exceptions.hpp>
#include <realm/table_ref.hpp>

namespace realm {


// Pre-definitions
struct CascadeState;
class StringIndex;

template <class T>
struct ImplicitNull;

template <class T>
struct ImplicitNull<util::Optional<T>> {
    static constexpr bool value = true;
};

template <>
struct ImplicitNull<int64_t> {
    static constexpr bool value = false;
};

template <>
struct ImplicitNull<float> {
    static constexpr bool value = true;
};

template <>
struct ImplicitNull<double> {
    static constexpr bool value = true;
};

// FIXME: Add specialization for ImplicitNull for float, double, StringData, BinaryData.

template <class T, class R, Action action, class Condition, class ColType>
R aggregate(const ColType& column, T target, size_t start, size_t end, size_t limit, size_t* return_ndx);


// Iterator with random access for Columns
template <typename ColumnDataType>
class ColumnRandIterator : public std::iterator<std::random_access_iterator_tag, ColumnDataType, ptrdiff_t, size_t> {
public:
    ColumnRandIterator(const Column<ColumnDataType>* src_col, size_t ndx = 0);
    bool operator==(const ColumnRandIterator<ColumnDataType>& rhs) const;
    bool operator!=(const ColumnRandIterator<ColumnDataType>& rhs) const;
    bool operator<(const ColumnRandIterator<ColumnDataType>& rhs) const;
    bool operator>(const ColumnRandIterator<ColumnDataType>& rhs) const;
    bool operator<=(const ColumnRandIterator<ColumnDataType>& rhs) const;
    bool operator>=(const ColumnRandIterator<ColumnDataType>& rhs) const;
    ColumnRandIterator<ColumnDataType>& operator+=(ptrdiff_t movement);
    ColumnRandIterator<ColumnDataType>& operator-=(ptrdiff_t movement);
    ColumnRandIterator<ColumnDataType>& operator++();
    ColumnRandIterator<ColumnDataType>& operator--();
    ColumnRandIterator<ColumnDataType> operator++(int);
    ColumnRandIterator<ColumnDataType> operator--(int);
    ColumnRandIterator<ColumnDataType> operator+(ptrdiff_t movement);
    ColumnRandIterator<ColumnDataType> operator-(ptrdiff_t movement);
    ptrdiff_t operator-(const ColumnRandIterator<ColumnDataType>& right) const;
    const ColumnDataType operator*() const;
    const ColumnDataType operator->() const;
    const ColumnDataType operator[](ptrdiff_t offset) const;
    size_t get_col_ndx() const;

protected:
    size_t m_col_ndx;
    const Column<ColumnDataType>* m_col;
};

/// Base class for all column types.
class ColumnBase {
public:
    /// Get the number of entries in this column. This operation is relatively
    /// slow.
    virtual size_t size() const noexcept = 0;

    /// \throw LogicError Thrown if this column is not string valued.
    virtual void set_string(size_t row_ndx, StringData value);

    /// Whether or not this column is nullable.
    virtual bool is_nullable() const noexcept;

    /// Whether or not the value at \a row_ndx is NULL. If the column is not
    /// nullable, always returns false.
    virtual bool is_null(size_t row_ndx) const noexcept;

    /// Sets the value at \a row_ndx to be NULL.
    /// \throw LogicError Thrown if this column is not nullable.
    virtual void set_null(size_t row_ndx);

    /// Inserts the specified number of elements into this column
    /// starting at the specified row index. The new elements will have the
    /// default value for the column type.
    ///
    /// \param row_ndx The row to start insertion at. If the row_ndx is less
    /// than prior_num_rows then previous rows from row_ndx onwards will be
    /// moved ahead by num_rows_to_insert.
    ///
    /// \param num_rows_to_insert The number of rows to insert. There is no
    /// restriction on this value.
    ///
    /// \param prior_num_rows The number of elements in this column prior to the
    /// modification.
    ///
    /// \param nullable Specifies whether or not this column is nullable. This
    /// function may assert if nullable does not agree with \a is_nullable()
    virtual void insert_rows(size_t row_ndx, size_t num_rows_to_insert, size_t prior_num_rows, bool nullable) = 0;

    /// Removes the specified number of consecutive elements from
    /// this column, starting at the specified row index.
    ///
    /// \param row_ndx The row to start removal at (inclusive). This must be
    /// less than prior_num_rows.
    ///
    /// \param num_rows_to_erase The number of rows to erase.
    /// The row_ndx + num_rows_to_erase must be less than prior_num_rows.
    ///
    /// \param prior_num_rows The number of elements in this column prior to the
    /// modification.
    ///
    /// \param broken_reciprocal_backlinks If true, link columns must assume
    /// that reciprocal backlinks have already been removed. Non-link columns
    /// should ignore this argument.
    virtual void erase_rows(size_t row_ndx, size_t num_rows_to_erase, size_t prior_num_rows,
                            bool broken_reciprocal_backlinks) = 0;

    /// Removes the element at the specified row index by
    /// moving the element at the last row index over it. This reduces the
    /// number of elements by one.
    ///
    /// \param row_ndx The row to erase. Must be less than prior_num_rows.
    ///
    /// \param prior_num_rows The number of elements in this column prior to the
    /// modification.
    ///
    /// \param broken_reciprocal_backlinks If true, link columns must assume
    /// that reciprocal backlinks have already been removed. Non-link columns
    /// should ignore this argument.
    virtual void move_last_row_over(size_t row_ndx, size_t prior_num_rows, bool broken_reciprocal_backlinks) = 0;

    /// Remove all elements from this column.
    ///
    /// \param num_rows The total number of rows in this column.
    ///
    /// \param broken_reciprocal_backlinks If true, link columns must assume
    /// that reciprocal backlinks have already been removed. Non-link columns
    /// should ignore this argument.
    virtual void clear(size_t num_rows, bool broken_reciprocal_backlinks) = 0;

    /// \brief Swap the elements at the specified indices.
    ///
    /// Behaviour is undefined if:
    /// - \a row_ndx_1 or \a row_ndx_2 point to an invalid element (out-of
    /// bounds)
    /// - \a row_ndx_1 and \a row_ndx_2 point to the same value
    virtual void swap_rows(size_t row_ndx_1, size_t row_ndx_2) = 0;

    virtual void destroy() noexcept = 0;
    void move_assign(ColumnBase& col) noexcept;

    virtual ~ColumnBase() noexcept
    {
    }

    // Disable copying, this is not supported.
    ColumnBase& operator=(const ColumnBase&) = delete;
    ColumnBase(const ColumnBase&) = delete;

    // Getter function for index. For integer index, the caller must supply a
    // buffer that we can store the extracted value in (it may be bitpacked, so
    // we cannot return a pointer in to the Array as we do with String index).
    virtual StringData get_index_data(size_t, StringIndex::StringConversionBuffer& buffer) const noexcept = 0;

    // Search index
    virtual bool supports_search_index() const noexcept;
    virtual bool has_search_index() const noexcept;
    virtual StringIndex* create_search_index();
    virtual void destroy_search_index() noexcept;
    virtual const StringIndex* get_search_index() const noexcept;
    virtual StringIndex* get_search_index() noexcept;
    virtual void set_search_index_ref(ref_type, ArrayParent*, size_t ndx_in_parent);

    virtual Allocator& get_alloc() const noexcept = 0;

    /// Returns the 'ref' of the root array.
    virtual ref_type get_ref() const noexcept = 0;
    virtual MemRef get_mem() const noexcept = 0;

    virtual void replace_root_array(std::unique_ptr<Array> leaf) = 0;
    virtual MemRef clone_deep(Allocator& alloc) const = 0;
    virtual void detach(void) = 0;
    virtual bool is_attached(void) const noexcept = 0;

    static size_t get_size_from_type_and_ref(ColumnType, ref_type, Allocator&, bool) noexcept;

    // These assume that the right column compile-time type has been
    // figured out.
    static size_t get_size_from_ref(ref_type root_ref, Allocator&);
    static size_t get_size_from_ref(ref_type spec_ref, ref_type columns_ref, Allocator&);

    /// Write a slice of this column to the specified output stream.
    virtual ref_type write(size_t slice_offset, size_t slice_size, size_t table_size, _impl::OutputStream&) const = 0;

    /// Get this column's logical index within the containing table, or npos
    /// for free-standing or non-top-level columns.
    size_t get_column_index() const noexcept
    {
        return m_column_ndx;
    }

    virtual void set_parent(ArrayParent*, size_t ndx_in_parent) noexcept = 0;
    virtual size_t get_ndx_in_parent() const noexcept = 0;
    virtual void set_ndx_in_parent(size_t ndx_in_parent) noexcept = 0;

    /// Called to update refs and memory pointers of this column accessor and
    /// all its nested accessors, but only in cases where the logical contents
    /// in strictly unchanged. Group::commit(), and
    /// SharedGroup::commit_and_continue_as_read()() are examples of such
    /// cases. In both those cases, the purpose is to keep user visible
    /// accessors in a valid state across a commit.
    virtual void update_from_parent(size_t old_baseline) noexcept = 0;

    //@{

    /// cascade_break_backlinks_to() is called iteratively for each column by
    /// Table::cascade_break_backlinks_to() with the same arguments as are
    /// passed to Table::cascade_break_backlinks_to(). Link columns must
    /// override it. The same is true for cascade_break_backlinks_to_all_rows(),
    /// except that it is called from
    /// Table::cascade_break_backlinks_to_all_rows(), and that it expects
    /// Table::cascade_break_backlinks_to_all_rows() to pass the number of rows
    /// in the table as \a num_rows.

    virtual void cascade_break_backlinks_to(size_t row_ndx, CascadeState&);
    virtual void cascade_break_backlinks_to_all_rows(size_t num_rows, CascadeState&);

    //@}

    void discard_child_accessors() noexcept;

    /// For columns that are able to contain subtables, this function returns
    /// the pointer to the subtable accessor at the specified row index if it
    /// exists, otherwise it returns null. For other column types, this function
    /// returns null.
    virtual TableRef get_subtable_accessor(size_t row_ndx) const noexcept;

    /// Detach and remove the subtable accessor at the specified row if it
    /// exists. For column types that are unable to contain subtable, this
    /// function does nothing.
    virtual void discard_subtable_accessor(size_t row_ndx) noexcept;

    virtual void adj_acc_insert_rows(size_t row_ndx, size_t num_rows) noexcept;
    virtual void adj_acc_erase_row(size_t row_ndx) noexcept;
    /// See Table::adj_acc_move_over()
    virtual void adj_acc_move_over(size_t from_row_ndx, size_t to_row_ndx) noexcept;
    virtual void adj_acc_swap_rows(size_t row_ndx_1, size_t row_ndx_2) noexcept;
    virtual void adj_acc_move_row(size_t from_ndx, size_t to_ndx) noexcept;
    virtual void adj_acc_merge_rows(size_t old_row_ndx, size_t new_row_ndx) noexcept;
    virtual void adj_acc_clear_root_table() noexcept;

    enum {
        mark_Recursive = 0x01,
        mark_LinkTargets = 0x02,
        mark_LinkOrigins = 0x04,
    };

    virtual void mark(int type) noexcept;

    virtual void bump_link_origin_table_version() noexcept;

    virtual int compare_values(size_t row1, size_t row2) const noexcept = 0;

    /// Refresh the dirty part of the accessor subtree rooted at this column
    /// accessor.
    ///
    /// The following conditions are necessary and sufficient for the proper
    /// operation of this function:
    ///
    ///  - The parent table accessor (excluding its column accessors) is in a
    ///    valid state (already refreshed).
    ///
    ///  - Every subtable accessor in the subtree is marked dirty if it needs to
    ///    be refreshed, or if it has a descendant accessor that needs to be
    ///    refreshed.
    ///
    ///  - This column accessor, as well as all its descendant accessors, are in
    ///    structural correspondence with the underlying node hierarchy whose
    ///    root ref is stored in the parent (`Table::m_columns`) (see
    ///    AccessorConsistencyLevels).
    ///
    ///  - The 'index in parent' property of the cached root array
    ///    (`root->m_ndx_in_parent`) is valid.
    virtual void refresh_accessor_tree(size_t new_col_ndx, const Spec&);

    virtual void verify() const = 0;
    virtual void verify(const Table&, size_t col_ndx) const;
    virtual void to_dot(std::ostream&, StringData title = StringData()) const = 0;
    virtual void do_dump_node_structure(std::ostream&, int level) const = 0;

#ifdef REALM_DEBUG
    void dump_node_structure() const; // To std::cerr (for GDB)
    void bptree_to_dot(const Array* root, std::ostream& out) const;
#endif

protected:
    using SliceHandler = BpTreeBase::SliceHandler;

    ColumnBase(size_t column_ndx = npos)
        : m_column_ndx(column_ndx)
    {
    }
    ColumnBase(ColumnBase&&) = default;

    // Must not assume more than minimal consistency (see
    // AccessorConsistencyLevels).
    virtual void do_discard_child_accessors() noexcept
    {
    }

    //@{
    /// \tparam L Any type with an appropriate `value_type`, %size(),
    /// and %get() members.
    template <class L, class T>
    size_t lower_bound(const L& list, T value) const noexcept;

    template <class L, class T>
    size_t upper_bound(const L& list, T value) const noexcept;
    //@}

    // Node functions

    class CreateHandler {
    public:
        virtual ref_type create_leaf(size_t size) = 0;
        ~CreateHandler() noexcept
        {
        }
    };

    static ref_type create(Allocator&, size_t size, CreateHandler&);

    class LeafToDot;
    virtual void leaf_to_dot(MemRef, ArrayParent*, size_t ndx_in_parent, std::ostream&) const = 0;

    template <class Column>
    static int compare_values(const Column* column, size_t row1, size_t row2) noexcept;

private:
    size_t m_column_ndx = npos;

    static ref_type build(size_t* rest_size_ptr, size_t fixed_height, Allocator&, CreateHandler&);
};


// FIXME: Temporary class until all column types have been migrated to use BpTree interface
class ColumnBaseSimple : public ColumnBase {
public:
    //@{
    /// Returns the array node at the root of this column, but note
    /// that there is no guarantee that this node is an inner B+-tree
    /// node or a leaf. This is the case for a MixedColumn in
    /// particular.
    Array* get_root_array() noexcept
    {
        return m_array.get();
    }
    const Array* get_root_array() const noexcept
    {
        return m_array.get();
    }
    //@}

    Allocator& get_alloc() const noexcept final
    {
        return m_array->get_alloc();
    }
    void destroy() noexcept override
    {
        if (m_array)
            m_array->destroy_deep();
    }
    ref_type get_ref() const noexcept final
    {
        return m_array->get_ref();
    }
    MemRef get_mem() const noexcept final
    {
        return m_array->get_mem();
    }
    void detach() noexcept final
    {
        m_array->detach();
    }
    bool is_attached() const noexcept final
    {
        return m_array->is_attached();
    }
    void set_parent(ArrayParent* parent, size_t ndx_in_parent) noexcept final
    {
        m_array->set_parent(parent, ndx_in_parent);
    }
    size_t get_ndx_in_parent() const noexcept final
    {
        return m_array->get_ndx_in_parent();
    }
    void set_ndx_in_parent(size_t ndx_in_parent) noexcept override
    {
        m_array->set_ndx_in_parent(ndx_in_parent);
    }
    void update_from_parent(size_t old_baseline) noexcept override
    {
        m_array->update_from_parent(old_baseline);
    }
    MemRef clone_deep(Allocator& alloc) const override
    {
        return m_array->clone_deep(alloc);
    }

protected:
    ColumnBaseSimple(size_t column_ndx)
        : ColumnBase(column_ndx)
    {
    }
    ColumnBaseSimple(Array* root)
        : m_array(root)
    {
    }
    std::unique_ptr<Array> m_array;

    void replace_root_array(std::unique_ptr<Array> new_root) final;
    bool root_is_leaf() const noexcept
    {
        return !m_array->is_inner_bptree_node();
    }

    /// Introduce a new root node which increments the height of the
    /// tree by one.
    void introduce_new_root(ref_type new_sibling_ref, TreeInsertBase& state, bool is_append);

    static ref_type write(const Array* root, size_t slice_offset, size_t slice_size, size_t table_size, SliceHandler&,
                          _impl::OutputStream&);

#if defined(REALM_DEBUG)
    void tree_to_dot(std::ostream&) const;
#endif
};

class ColumnBaseWithIndex : public ColumnBase {
public:
    ~ColumnBaseWithIndex() noexcept override
    {
    }
    void set_ndx_in_parent(size_t ndx) noexcept override;
    void update_from_parent(size_t old_baseline) noexcept override;
    void refresh_accessor_tree(size_t, const Spec&) override;
    void move_assign(ColumnBaseWithIndex& col) noexcept;
    void destroy() noexcept override;

    virtual bool supports_search_index() const noexcept override
    {
        return true;
    }
    bool has_search_index() const noexcept final
    {
        return bool(m_search_index);
    }
    StringIndex* get_search_index() noexcept final
    {
        return m_search_index.get();
    }
    const StringIndex* get_search_index() const noexcept final
    {
        return m_search_index.get();
    }
    void destroy_search_index() noexcept override;
    void set_search_index_ref(ref_type ref, ArrayParent* parent, size_t ndx_in_parent) final;
    StringIndex* create_search_index() override = 0;

protected:
    using ColumnBase::ColumnBase;
    ColumnBaseWithIndex(ColumnBaseWithIndex&&) = default;
    std::unique_ptr<StringIndex> m_search_index;
};


/// A column (Column) is a single B+-tree, and the root of
/// the column is the root of the B+-tree. All leaf nodes are arrays.
template <class T>
class Column : public ColumnBaseWithIndex {
public:
    using value_type = T;
    using LeafInfo = typename BpTree<T>::LeafInfo;
    using LeafType = typename BpTree<T>::LeafType;

    static constexpr bool nullable = ImplicitNull<T>::value;

    struct unattached_root_tag {
    };

    explicit Column() noexcept
        : ColumnBaseWithIndex(npos)
        , m_tree(Allocator::get_default())
    {
    }
    REALM_DEPRECATED("Initialize with ref instead") explicit Column(std::unique_ptr<Array> root) noexcept;
    Column(Allocator&, ref_type, size_t column_ndx = npos);
    Column(unattached_root_tag, Allocator&);
    Column(Column&&) noexcept = default;
    ~Column() noexcept override;

    void init_from_parent();
    void init_from_ref(Allocator&, ref_type);
    void init_from_mem(Allocator&, MemRef);
    // Accessor concept:
    void destroy() noexcept override;
    Allocator& get_alloc() const noexcept final;
    ref_type get_ref() const noexcept final;
    MemRef get_mem() const noexcept final;
    void set_parent(ArrayParent* parent, size_t ndx_in_parent) noexcept override;
    size_t get_ndx_in_parent() const noexcept final;
    void set_ndx_in_parent(size_t ndx) noexcept final;
    void update_from_parent(size_t old_baseline) noexcept override;
    void refresh_accessor_tree(size_t, const Spec&) override;
    void detach() noexcept final;
    bool is_attached() const noexcept final;
    MemRef clone_deep(Allocator&) const override;

    void move_assign(Column&);

    static size_t get_size_from_ref(ref_type root_ref, Allocator& alloc)
    {
        return ColumnBase::get_size_from_ref(root_ref, alloc);
    }

    size_t size() const noexcept override;
    bool is_empty() const noexcept
    {
        return size() == 0;
    }
    bool is_nullable() const noexcept override;

    /// Provides access to the leaf that contains the element at the
    /// specified index. Upon return \a ndx_in_leaf will be set to the
    /// corresponding index relative to the beginning of the leaf.
    ///
    /// LeafInfo is a struct defined by the underlying BpTree<T>
    /// data structure, that provides a way for the caller to do
    /// leaf caching without instantiating too many objects along
    /// the way.
    ///
    /// This function cannot be used for modifying operations as it
    /// does not ensure the presence of an unbroken chain of parent
    /// accessors. For this reason, the identified leaf should always
    /// be accessed through the returned const-qualified reference,
    /// and never directly through the specfied fallback accessor.
    void get_leaf(size_t ndx, size_t& ndx_in_leaf, LeafInfo& inout_leaf) const noexcept;

    // Getting and setting values
    T get(size_t ndx) const noexcept;
    bool is_null(size_t ndx) const noexcept override;
    T back() const noexcept;
    void set(size_t, T value);
    void set_null(size_t) override;
    void add(T value = T{});
    void insert(size_t ndx, T value = T{}, size_t num_rows = 1);
    void erase(size_t row_ndx);
    void erase(size_t row_ndx, bool is_last);
    void move_last_over(size_t row_ndx, size_t last_row_ndx);
    void clear();

    // Index support
    StringData get_index_data(size_t ndx, StringIndex::StringConversionBuffer& buffer) const noexcept override;

    // FIXME: Remove these
    uint64_t get_uint(size_t ndx) const noexcept;
    ref_type get_as_ref(size_t ndx) const noexcept;
    void set_uint(size_t ndx, uint64_t value);
    void set_as_ref(size_t ndx, ref_type value);

    template <class U>
    void adjust(size_t ndx, U diff);

    template <class U>
    void adjust(U diff);

    template <class U>
    void adjust_ge(T limit, U diff);

    size_t count(T target) const;

    typename ColumnTypeTraits<T>::sum_type sum(size_t start = 0, size_t end = npos, size_t limit = npos,
                                               size_t* return_ndx = nullptr) const;

    typename ColumnTypeTraits<T>::minmax_type maximum(size_t start = 0, size_t end = npos, size_t limit = npos,
                                                      size_t* return_ndx = nullptr) const;

    typename ColumnTypeTraits<T>::minmax_type minimum(size_t start = 0, size_t end = npos, size_t limit = npos,
                                                      size_t* return_ndx = nullptr) const;

    double average(size_t start = 0, size_t end = npos, size_t limit = npos, size_t* return_ndx = nullptr) const;

    size_t find_first(T value, size_t begin = 0, size_t end = npos) const;
    void find_all(Column<int64_t>& out_indices, T value, size_t begin = 0, size_t end = npos) const;

    void populate_search_index();
    StringIndex* create_search_index() override;
    inline bool supports_search_index() const noexcept override
    {
        if (realm::is_any<T, float, double>::value)
            return false;
        else
            return true;
    }


    //@{
    /// Find the lower/upper bound for the specified value assuming
    /// that the elements are already sorted in ascending order
    /// according to ordinary integer comparison.
    size_t lower_bound(T value) const noexcept;
    size_t upper_bound(T value) const noexcept;
    //@}

    using const_iterator = ColumnRandIterator<T>;

    const_iterator cbegin() const
    {
        return const_iterator(this, 0); // `this` is const in a const method
    }
    const_iterator cend() const
    {
        return const_iterator(this, size());
    }

    size_t find_gte(T target, size_t start) const;

    bool compare(const Column&) const noexcept;
    int compare_values(size_t row1, size_t row2) const noexcept override;

    static ref_type create(Allocator&, Array::Type leaf_type = Array::type_Normal, size_t size = 0, T value = T{});

    // Overriding method in ColumnBase
    ref_type write(size_t, size_t, size_t, _impl::OutputStream&) const override;

    void insert_rows(size_t, size_t, size_t, bool) override;
    void erase_rows(size_t, size_t, size_t, bool) override;
    void move_last_row_over(size_t, size_t, bool) override;

    /// \brief Swap the elements at the specified indices.
    ///
    /// If this \c Column has a search index defined, it will be updated to
    /// reflect the changes induced by the swap.
    ///
    /// Behaviour is undefined if:
    /// - \a row_ndx_1 or \a row_ndx_2 point to an invalid element (out-of
    /// bounds)
    /// - \a row_ndx_1 and \a row_ndx_2 point to the same value
    void swap_rows(size_t, size_t) override;
    void clear(size_t, bool) override;

    /// \param row_ndx Must be `realm::npos` if appending.
    /// \param value The value to store at the specified row.
    /// \param num_rows The number of rows to insert.
    void insert_without_updating_index(size_t row_ndx, T value, size_t num_rows);

    void verify() const override;
    void to_dot(std::ostream&, StringData title) const override;
    void do_dump_node_structure(std::ostream&, int) const override;
#ifdef REALM_DEBUG
    using ColumnBase::verify;
    void tree_to_dot(std::ostream&) const;
    MemStats stats() const;
#endif

    //@{
    /// Returns the array node at the root of this column, but note
    /// that there is no guarantee that this node is an inner B+-tree
    /// node or a leaf. This is the case for a MixedColumn in
    /// particular.
    Array* get_root_array() noexcept
    {
        return &m_tree.root();
    }
    const Array* get_root_array() const noexcept
    {
        return &m_tree.root();
    }
    //@}

protected:
    bool root_is_leaf() const noexcept
    {
        return m_tree.root_is_leaf();
    }
    void replace_root_array(std::unique_ptr<Array> leaf) final
    {
        m_tree.replace_root(std::move(leaf));
    }

    void set_without_updating_index(size_t row_ndx, T value);
    void erase_without_updating_index(size_t row_ndx, bool is_last);
    void move_last_over_without_updating_index(size_t row_ndx, size_t last_row_ndx);
    void swap_rows_without_updating_index(size_t row_ndx_1, size_t row_ndx_2);

    /// If any element points to an array node, this function recursively
    /// destroys that array node. Note that the same is **not** true for
    /// IntegerColumn::do_erase() and IntegerColumn::do_move_last_over().
    ///
    /// FIXME: Be careful, clear_without_updating_index() currently forgets
    /// if the leaf type is Array::type_HasRefs.
    void clear_without_updating_index();

    void leaf_to_dot(MemRef, ArrayParent*, size_t ndx_in_parent, std::ostream&) const override;
#ifdef REALM_DEBUG
    static void dump_node_structure(const Array& root, std::ostream&, int level);
#endif
    std::pair<ref_type, size_t> get_to_dot_parent(size_t ndx_in_parent) const;

private:
    class EraseLeafElem;
    class CreateHandler;
    class SliceHandler;

    friend class Array;
    friend class ColumnBase;
    friend class StringIndex;

    BpTree<T> m_tree;

    void do_erase(size_t row_ndx, size_t num_rows_to_erase, bool is_last);
};

// Implementation:


template <>
inline size_t IntNullColumn::get_size_from_ref(ref_type root_ref, Allocator& alloc)
{
    // FIXME: Speed improvement possible by not creating instance, but tricky! This slow method is OK so far
    // because it's only invoked by Table::get_size_from_ref() which is only used for subtables which we
    // currently 2016) do not expose publicly.
    IntNullColumn inc(alloc, root_ref);
    return inc.size();
}


inline bool ColumnBase::supports_search_index() const noexcept
{
    REALM_ASSERT(!has_search_index());
    return false;
}

inline bool ColumnBase::has_search_index() const noexcept
{
    return get_search_index() != nullptr;
}

inline StringIndex* ColumnBase::create_search_index()
{
    return nullptr;
}

inline void ColumnBase::destroy_search_index() noexcept
{
}

inline const StringIndex* ColumnBase::get_search_index() const noexcept
{
    return nullptr;
}

inline StringIndex* ColumnBase::get_search_index() noexcept
{
    return nullptr;
}

inline void ColumnBase::set_search_index_ref(ref_type, ArrayParent*, size_t)
{
}

inline void ColumnBase::discard_child_accessors() noexcept
{
    do_discard_child_accessors();
}

inline void ColumnBase::discard_subtable_accessor(size_t) noexcept
{
    // Noop
}

inline void ColumnBase::adj_acc_insert_rows(size_t, size_t) noexcept
{
    // Noop
}

inline void ColumnBase::adj_acc_erase_row(size_t) noexcept
{
    // Noop
}

inline void ColumnBase::adj_acc_move_over(size_t, size_t) noexcept
{
    // Noop
}

inline void ColumnBase::adj_acc_swap_rows(size_t, size_t) noexcept
{
    // Noop
}

inline void ColumnBase::adj_acc_move_row(size_t, size_t) noexcept
{
    // Noop
}

inline void ColumnBase::adj_acc_merge_rows(size_t, size_t) noexcept
{
    // Noop
}

inline void ColumnBase::adj_acc_clear_root_table() noexcept
{
    // Noop
}

inline void ColumnBase::mark(int) noexcept
{
    // Noop
}

inline void ColumnBase::bump_link_origin_table_version() noexcept
{
    // Noop
}

template <class Column>
int ColumnBase::compare_values(const Column* column, size_t row1, size_t row2) noexcept
{
    // we negate nullability such that the two ternary statements in this method can look identical to reduce
    // risk of bugs
    bool v1 = !column->is_null(row1);
    bool v2 = !column->is_null(row2);

    if (!v1 || !v2)
        return v1 == v2 ? 0 : v1 < v2 ? 1 : -1;

    auto a = column->get(row1);
    auto b = column->get(row2);
    return a == b ? 0 : a < b ? 1 : -1;
}

template <class T>
void Column<T>::set_without_updating_index(size_t ndx, T value)
{
    m_tree.set(ndx, std::move(value));
}

template <class T>
void Column<T>::set(size_t ndx, T value)
{
    REALM_ASSERT_DEBUG(ndx < size());
    if (has_search_index()) {
        m_search_index->set(ndx, value);
    }
    set_without_updating_index(ndx, std::move(value));
}

template <class T>
void Column<T>::set_null(size_t ndx)
{
    REALM_ASSERT_DEBUG(ndx < size());
    if (!is_nullable()) {
        throw LogicError{LogicError::column_not_nullable};
    }
    if (has_search_index()) {
        m_search_index->set(ndx, null{});
    }
    m_tree.set_null(ndx);
}

// When a value of a signed type is converted to an unsigned type, the C++ standard guarantees that negative values
// are converted from the native representation to 2's complement, but the opposite conversion is left as undefined.
// realm::util::from_twos_compl() is used here to perform the correct opposite unsigned-to-signed conversion,
// which reduces to a no-op when 2's complement is the native representation of negative values.
template <class T>
void Column<T>::set_uint(size_t ndx, uint64_t value)
{
    set(ndx, util::from_twos_compl<int_fast64_t>(value));
}

template <class T>
void Column<T>::set_as_ref(size_t ndx, ref_type ref)
{
    set(ndx, from_ref(ref));
}

template <class T>
template <class U>
void Column<T>::adjust(size_t ndx, U diff)
{
    REALM_ASSERT_3(ndx, <, size());
    m_tree.adjust(ndx, diff);
}

template <class T>
template <class U>
void Column<T>::adjust(U diff)
{
    m_tree.adjust(diff);
}

template <class T>
template <class U>
void Column<T>::adjust_ge(T limit, U diff)
{
    m_tree.adjust_ge(limit, diff);
}

template <class T>
size_t Column<T>::count(T target) const
{
    if (has_search_index()) {
        return m_search_index->count(target);
    }
    return to_size_t(aggregate<T, int64_t, act_Count, Equal>(*this, target, 0, size(), npos, nullptr));
}

template <class T>
typename ColumnTypeTraits<T>::sum_type Column<T>::sum(size_t start, size_t end, size_t limit,
                                                      size_t* return_ndx) const
{
    using sum_type = typename ColumnTypeTraits<T>::sum_type;
    if (nullable)
        return aggregate<T, sum_type, act_Sum, NotNull>(*this, 0, start, end, limit, return_ndx);
    else
        return aggregate<T, sum_type, act_Sum, None>(*this, 0, start, end, limit, return_ndx);
}

template <class T>
double Column<T>::average(size_t start, size_t end, size_t limit, size_t* return_ndx) const
{
    if (end == size_t(-1))
        end = size();

    auto s = sum(start, end, limit);
    size_t cnt = to_size_t(aggregate<T, int64_t, act_Count, NotNull>(*this, 0, start, end, limit, nullptr));
    if (return_ndx)
        *return_ndx = cnt;
    double avg = double(s) / (cnt == 0 ? 1 : cnt);
    return avg;
}

template <class T>
typename ColumnTypeTraits<T>::minmax_type Column<T>::minimum(size_t start, size_t end, size_t limit,
                                                             size_t* return_ndx) const
{
    using R = typename ColumnTypeTraits<T>::minmax_type;
    return aggregate<T, R, act_Min, NotNull>(*this, 0, start, end, limit, return_ndx);
}

template <class T>
typename ColumnTypeTraits<T>::minmax_type Column<T>::maximum(size_t start, size_t end, size_t limit,
                                                             size_t* return_ndx) const
{
    using R = typename ColumnTypeTraits<T>::minmax_type;
    return aggregate<T, R, act_Max, NotNull>(*this, 0, start, end, limit, return_ndx);
}

template <class T>
void Column<T>::get_leaf(size_t ndx, size_t& ndx_in_leaf, LeafInfo& inout_leaf_info) const noexcept
{
    m_tree.get_leaf(ndx, ndx_in_leaf, inout_leaf_info);
}

template <class T>
StringData Column<T>::get_index_data(size_t ndx, StringIndex::StringConversionBuffer& buffer) const noexcept
{
    T x = get(ndx);
    return to_str(x, buffer);
}

template <class T>
void Column<T>::populate_search_index()
{
    REALM_ASSERT(has_search_index());
    // Populate the index
    size_t num_rows = size();
    for (size_t row_ndx = 0; row_ndx != num_rows; ++row_ndx) {
        bool is_append = true;
        if (is_null(row_ndx)) {
            m_search_index->insert(row_ndx, null{}, 1, is_append); // Throws
        }
        else {
            T value = get(row_ndx);
            m_search_index->insert(row_ndx, value, 1, is_append); // Throws
        }
    }
}

template <class T>
StringIndex* Column<T>::create_search_index()
{
    if (realm::is_any<T, float, double>::value)
        return nullptr;

    REALM_ASSERT(!has_search_index());
    REALM_ASSERT(supports_search_index());
    m_search_index.reset(new StringIndex(this, get_alloc())); // Throws
    populate_search_index();
    return m_search_index.get();
}

template <class T>
size_t Column<T>::find_first(T value, size_t begin, size_t end) const
{
    REALM_ASSERT_3(begin, <=, size());
    REALM_ASSERT(end == npos || (begin <= end && end <= size()));

    if (m_search_index && begin == 0 && end == npos)
        return m_search_index->find_first(value);
    return m_tree.find_first(value, begin, end);
}

template <class T>
void Column<T>::find_all(IntegerColumn& result, T value, size_t begin, size_t end) const
{
    REALM_ASSERT_3(begin, <=, size());
    REALM_ASSERT(end == npos || (begin <= end && end <= size()));

    if (m_search_index && begin == 0 && end == npos)
        return m_search_index->find_all(result, value);
    return m_tree.find_all(result, value, begin, end);
}

inline size_t ColumnBase::get_size_from_ref(ref_type root_ref, Allocator& alloc)
{
    const char* root_header = alloc.translate(root_ref);
    bool root_is_leaf = !Array::get_is_inner_bptree_node_from_header(root_header);
    if (root_is_leaf)
        return Array::get_size_from_header(root_header);
    return BpTreeNode::get_bptree_size_from_header(root_header);
}

template <class L, class T>
size_t ColumnBase::lower_bound(const L& list, T value) const noexcept
{
    size_t i = 0;
    size_t list_size = list.size();
    while (0 < list_size) {
        size_t half = list_size / 2;
        size_t mid = i + half;
        typename L::value_type probe = list.get(mid);
        if (probe < value) {
            i = mid + 1;
            list_size -= half + 1;
        }
        else {
            list_size = half;
        }
    }
    return i;
}

template <class L, class T>
size_t ColumnBase::upper_bound(const L& list, T value) const noexcept
{
    size_t i = 0;
    size_t list_size = list.size();
    while (0 < list_size) {
        size_t half = list_size / 2;
        size_t mid = i + half;
        typename L::value_type probe = list.get(mid);
        if (!(value < probe)) {
            i = mid + 1;
            list_size -= half + 1;
        }
        else {
            list_size = half;
        }
    }
    return i;
}


inline ref_type ColumnBase::create(Allocator& alloc, size_t column_size, CreateHandler& handler)
{
    size_t rest_size = column_size;
    size_t fixed_height = 0; // Not fixed
    return build(&rest_size, fixed_height, alloc, handler);
}

template <class T>
Column<T>::Column(Allocator& alloc, ref_type ref, size_t column_ndx)
    : ColumnBaseWithIndex(column_ndx)
    , m_tree(BpTreeBase::unattached_tag{})
{
    // fixme, must m_search_index be copied here?
    m_tree.init_from_ref(alloc, ref);
}

template <class T>
Column<T>::Column(unattached_root_tag, Allocator& alloc)
    : ColumnBaseWithIndex(npos)
    , m_tree(alloc)
{
}

template <class T>
Column<T>::Column(std::unique_ptr<Array> root) noexcept
    : m_tree(std::move(root))
{
}

template <class T>
Column<T>::~Column() noexcept
{
}

template <class T>
void Column<T>::init_from_parent()
{
    m_tree.init_from_parent();
}

template <class T>
void Column<T>::init_from_ref(Allocator& alloc, ref_type ref)
{
    m_tree.init_from_ref(alloc, ref);
}

template <class T>
void Column<T>::init_from_mem(Allocator& alloc, MemRef mem)
{
    m_tree.init_from_mem(alloc, mem);
}

template <class T>
void Column<T>::destroy() noexcept
{
    ColumnBaseWithIndex::destroy();
    m_tree.destroy();
}

template <class T>
void Column<T>::move_assign(Column<T>& col)
{
    ColumnBaseWithIndex::move_assign(col);
    m_tree = std::move(col.m_tree);
}

template <class T>
Allocator& Column<T>::get_alloc() const noexcept
{
    return m_tree.get_alloc();
}

template <class T>
void Column<T>::set_parent(ArrayParent* parent, size_t ndx_in_parent) noexcept
{
    m_tree.set_parent(parent, ndx_in_parent);
}

template <class T>
size_t Column<T>::get_ndx_in_parent() const noexcept
{
    return m_tree.get_ndx_in_parent();
}

template <class T>
void Column<T>::set_ndx_in_parent(size_t ndx_in_parent) noexcept
{
    ColumnBaseWithIndex::set_ndx_in_parent(ndx_in_parent);
    m_tree.set_ndx_in_parent(ndx_in_parent);
}

template <class T>
void Column<T>::detach() noexcept
{
    m_tree.detach();
}

template <class T>
bool Column<T>::is_attached() const noexcept
{
    return m_tree.is_attached();
}

template <class T>
ref_type Column<T>::get_ref() const noexcept
{
    return get_root_array()->get_ref();
}

template <class T>
MemRef Column<T>::get_mem() const noexcept
{
    return get_root_array()->get_mem();
}

template <class T>
void Column<T>::update_from_parent(size_t old_baseline) noexcept
{
    ColumnBaseWithIndex::update_from_parent(old_baseline);
    m_tree.update_from_parent(old_baseline);
}

template <class T>
MemRef Column<T>::clone_deep(Allocator& alloc) const
{
    return m_tree.clone_deep(alloc);
}

template <class T>
size_t Column<T>::size() const noexcept
{
    return m_tree.size();
}

template <class T>
bool Column<T>::is_nullable() const noexcept
{
    return nullable;
}

template <class T>
T Column<T>::get(size_t ndx) const noexcept
{
    return m_tree.get(ndx);
}

template <class T>
bool Column<T>::is_null(size_t ndx) const noexcept
{
    return nullable && m_tree.is_null(ndx);
}

template <class T>
T Column<T>::back() const noexcept
{
    return m_tree.back();
}

template <class T>
ref_type Column<T>::get_as_ref(size_t ndx) const noexcept
{
    return to_ref(get(ndx));
}

template <class T>
uint64_t Column<T>::get_uint(size_t ndx) const noexcept
{
    static_assert(std::is_convertible<T, uint64_t>::value, "T is not convertible to uint.");
    return static_cast<uint64_t>(get(ndx));
}

template <class T>
void Column<T>::add(T value)
{
    insert(npos, std::move(value));
}

template <class T>
void Column<T>::insert_without_updating_index(size_t row_ndx, T value, size_t num_rows)
{
    size_t column_size = this->size(); // Slow
    bool is_append = row_ndx == column_size || row_ndx == npos;
    size_t ndx_or_npos_if_append = is_append ? npos : row_ndx;

    m_tree.insert(ndx_or_npos_if_append, std::move(value), num_rows); // Throws
}

template <class T>
void Column<T>::insert(size_t row_ndx, T value, size_t num_rows)
{
    size_t column_size = this->size(); // Slow
    bool is_append = row_ndx == column_size || row_ndx == npos;
    size_t ndx_or_npos_if_append = is_append ? npos : row_ndx;

    m_tree.insert(ndx_or_npos_if_append, value, num_rows); // Throws

    if (has_search_index()) {
        row_ndx = is_append ? column_size : row_ndx;
        m_search_index->insert(row_ndx, value, num_rows, is_append); // Throws
    }
}

template <class T>
void Column<T>::erase_without_updating_index(size_t row_ndx, bool is_last)
{
    m_tree.erase(row_ndx, is_last);
}

template <class T>
void Column<T>::erase(size_t row_ndx)
{
    REALM_ASSERT(size() >= 1);
    size_t last_row_ndx = size() - 1; // Note that size() is slow
    bool is_last = (row_ndx == last_row_ndx);
    erase(row_ndx, is_last); // Throws
}

template <class T>
void Column<T>::erase(size_t row_ndx, bool is_last)
{
    size_t num_rows_to_erase = 1;
    do_erase(row_ndx, num_rows_to_erase, is_last); // Throws
}

template <class T>
void Column<T>::move_last_over_without_updating_index(size_t row_ndx, size_t last_row_ndx)
{
    m_tree.move_last_over(row_ndx, last_row_ndx);
}

template <class T>
void Column<T>::move_last_over(size_t row_ndx, size_t last_row_ndx)
{
    REALM_ASSERT_3(row_ndx, <=, last_row_ndx);
    REALM_ASSERT_DEBUG(last_row_ndx + 1 == size());

    if (has_search_index()) {
        // remove the value to be overwritten from index
        bool is_last = true; // This tells StringIndex::erase() to not adjust subsequent indexes
        m_search_index->erase<StringData>(row_ndx, is_last); // Throws

        // update index to point to new location
        if (row_ndx != last_row_ndx) {
            T moved_value = get(last_row_ndx);
            m_search_index->update_ref(moved_value, last_row_ndx, row_ndx); // Throws
        }
    }

    move_last_over_without_updating_index(row_ndx, last_row_ndx);
}

template <class T>
void Column<T>::swap_rows(size_t row_ndx_1, size_t row_ndx_2)
{
    REALM_ASSERT_3(row_ndx_1, <, size());
    REALM_ASSERT_3(row_ndx_2, <, size());
    REALM_ASSERT_DEBUG(row_ndx_1 != row_ndx_2);

    if (has_search_index()) {
        T value_1 = get(row_ndx_1);
        T value_2 = get(row_ndx_2);
        size_t column_size = this->size();
        bool row_ndx_1_is_last = row_ndx_1 == column_size - 1;
        bool row_ndx_2_is_last = row_ndx_2 == column_size - 1;
        m_search_index->erase<StringData>(row_ndx_1, row_ndx_1_is_last);
        m_search_index->insert(row_ndx_1, value_2, 1, row_ndx_1_is_last);

        m_search_index->erase<StringData>(row_ndx_2, row_ndx_2_is_last);
        m_search_index->insert(row_ndx_2, value_1, 1, row_ndx_2_is_last);
    }

    swap_rows_without_updating_index(row_ndx_1, row_ndx_2);
}

template <class T>
void Column<T>::swap_rows_without_updating_index(size_t row_ndx_1, size_t row_ndx_2)
{
    // FIXME: This can be optimized with direct getters and setters.
    T value_1 = get(row_ndx_1);
    T value_2 = get(row_ndx_2);
    m_tree.set(row_ndx_1, value_2);
    m_tree.set(row_ndx_2, value_1);
}

template <class T>
void Column<T>::clear_without_updating_index()
{
    m_tree.clear(); // Throws
}

template <class T>
void Column<T>::clear()
{
    if (has_search_index()) {
        m_search_index->clear();
    }
    clear_without_updating_index();
}

template <class T, class Enable = void>
struct NullOrDefaultValue;
template <class T>
struct NullOrDefaultValue<T, typename std::enable_if<std::is_floating_point<T>::value>::type> {
    static T null_or_default_value(bool is_null)
    {
        if (is_null) {
            return null::get_null_float<T>();
        }
        else {
            return T{};
        }
    }
};
template <class T>
struct NullOrDefaultValue<util::Optional<T>, void> {
    static util::Optional<T> null_or_default_value(bool is_null)
    {
        if (is_null) {
            return util::none;
        }
        else {
            return util::some<T>(T{});
        }
    }
};
template <class T>
struct NullOrDefaultValue<T, typename std::enable_if<!ImplicitNull<T>::value>::type> {
    static T null_or_default_value(bool is_null)
    {
        REALM_ASSERT(!is_null);
        return T{};
    }
};

// Implementing pure virtual method of ColumnBase.
template <class T>
void Column<T>::insert_rows(size_t row_ndx, size_t num_rows_to_insert, size_t prior_num_rows, bool insert_nulls)
{
    REALM_ASSERT_DEBUG(prior_num_rows == size());
    REALM_ASSERT(row_ndx <= prior_num_rows);

    size_t row_ndx_2 = (row_ndx == prior_num_rows ? realm::npos : row_ndx);
    T value = NullOrDefaultValue<T>::null_or_default_value(insert_nulls);
    insert(row_ndx_2, value, num_rows_to_insert); // Throws
}

// Implementing pure virtual method of ColumnBase.
template <class T>
void Column<T>::erase_rows(size_t row_ndx, size_t num_rows_to_erase, size_t prior_num_rows, bool)
{
    REALM_ASSERT_DEBUG(prior_num_rows == size());
    REALM_ASSERT(num_rows_to_erase <= prior_num_rows);
    REALM_ASSERT(row_ndx <= prior_num_rows - num_rows_to_erase);

    bool is_last = (row_ndx + num_rows_to_erase == prior_num_rows);
    do_erase(row_ndx, num_rows_to_erase, is_last); // Throws
}

// Implementing pure virtual method of ColumnBase.
template <class T>
void Column<T>::move_last_row_over(size_t row_ndx, size_t prior_num_rows, bool)
{
    REALM_ASSERT_DEBUG(prior_num_rows == size());
    REALM_ASSERT(row_ndx < prior_num_rows);

    size_t last_row_ndx = prior_num_rows - 1;
    move_last_over(row_ndx, last_row_ndx); // Throws
}

// Implementing pure virtual method of ColumnBase.
template <class T>
void Column<T>::clear(size_t, bool)
{
    clear(); // Throws
}


template <class T>
size_t Column<T>::lower_bound(T value) const noexcept
{
    if (root_is_leaf()) {
        auto root = static_cast<const LeafType*>(get_root_array());
        return root->lower_bound(value);
    }
    return ColumnBase::lower_bound(*this, value);
}

template <class T>
size_t Column<T>::upper_bound(T value) const noexcept
{
    if (root_is_leaf()) {
        auto root = static_cast<const LeafType*>(get_root_array());
        return root->upper_bound(value);
    }
    return ColumnBase::upper_bound(*this, value);
}

// For a *sorted* Column, return first element E for which E >= target or return -1 if none
template <class T>
size_t Column<T>::find_gte(T target, size_t start) const
{
    // fixme: slow reference implementation. See Array::find_gte for faster version
    size_t ref = 0;
    size_t idx;
    for (idx = start; idx < size(); ++idx) {
        if (get(idx) >= target) {
            ref = idx;
            break;
        }
    }
    if (idx == size())
        ref = not_found;

    return ref;
}


template <class T>
bool Column<T>::compare(const Column<T>& c) const noexcept
{
    size_t n = size();
    if (c.size() != n)
        return false;
    for (size_t i = 0; i < n; ++i) {
        bool left_is_null = is_null(i);
        bool right_is_null = c.is_null(i);
        if (left_is_null != right_is_null) {
            return false;
        }
        if (!left_is_null) {
            if (get(i) != c.get(i))
                return false;
        }
    }
    return true;
}

template <class T>
int Column<T>::compare_values(size_t row1, size_t row2) const noexcept
{
    return ColumnBase::compare_values(this, row1, row2);
}

template <class T>
class Column<T>::CreateHandler : public ColumnBase::CreateHandler {
public:
    CreateHandler(Array::Type leaf_type, T value, Allocator& alloc)
        : m_value(value)
        , m_alloc(alloc)
        , m_leaf_type(leaf_type)
    {
    }
    ref_type create_leaf(size_t size) override
    {
        MemRef mem = BpTree<T>::create_leaf(m_leaf_type, size, m_value, m_alloc); // Throws
        return mem.get_ref();
    }

private:
    const T m_value;
    Allocator& m_alloc;
    Array::Type m_leaf_type;
};

template <class T>
ref_type Column<T>::create(Allocator& alloc, Array::Type leaf_type, size_t size, T value)
{
    CreateHandler handler(leaf_type, std::move(value), alloc);
    return ColumnBase::create(alloc, size, handler);
}

template <class T>
ref_type Column<T>::write(size_t slice_offset, size_t slice_size, size_t table_size, _impl::OutputStream& out) const
{
    return m_tree.write(slice_offset, slice_size, table_size, out);
}

template <class T>
void Column<T>::refresh_accessor_tree(size_t new_col_ndx, const Spec& spec)
{
    m_tree.init_from_parent();
    ColumnBaseWithIndex::refresh_accessor_tree(new_col_ndx, spec);
}

template <class T>
void Column<T>::do_erase(size_t row_ndx, size_t num_rows_to_erase, bool is_last)
{
    if (has_search_index()) {
        for (size_t i = num_rows_to_erase; i > 0; --i) {
            size_t row_ndx_2 = row_ndx + i - 1;
            m_search_index->erase<T>(row_ndx_2, is_last); // Throws
        }
    }
    for (size_t i = num_rows_to_erase; i > 0; --i) {
        size_t row_ndx_2 = row_ndx + i - 1;
        erase_without_updating_index(row_ndx_2, is_last); // Throws
    }
}

template <class T>
void Column<T>::verify() const
{
#ifdef REALM_DEBUG
    m_tree.verify();
#endif
}

// LCOV_EXCL_START

template <class T>
void Column<T>::to_dot(std::ostream& out, StringData title) const
{
#ifdef REALM_DEBUG
    ref_type ref = get_root_array()->get_ref();
    out << "subgraph cluster_integer_column" << ref << " {" << std::endl;
    out << " label = \"Integer column";
    if (title.size() != 0)
        out << "\\n'" << title << "'";
    out << "\";" << std::endl;
    tree_to_dot(out);
    out << "}" << std::endl;
#else
    static_cast<void>(out);
    static_cast<void>(title);
#endif
}

template <class T>
void Column<T>::leaf_to_dot(MemRef leaf_mem, ArrayParent* parent, size_t ndx_in_parent, std::ostream& out) const
{
#ifdef REALM_DEBUG
    BpTree<T>::leaf_to_dot(leaf_mem, parent, ndx_in_parent, out, get_alloc());
#else
    static_cast<void>(leaf_mem);
    static_cast<void>(parent);
    static_cast<void>(ndx_in_parent);
    static_cast<void>(out);
#endif
}

template <class T>
void Column<T>::do_dump_node_structure(std::ostream& out, int level) const
{
#ifdef REALM_DEBUG
    dump_node_structure(*get_root_array(), out, level);
#else
    static_cast<void>(out);
    static_cast<void>(level);
#endif
}

#ifdef REALM_DEBUG

template <class T>
void Column<T>::tree_to_dot(std::ostream& out) const
{
    ColumnBase::bptree_to_dot(get_root_array(), out);
}


template <class T>
MemStats Column<T>::stats() const
{
    MemStats mem_stats;
    get_root_array()->stats(mem_stats);
    return mem_stats;
}

namespace _impl {
void leaf_dumper(MemRef mem, Allocator& alloc, std::ostream& out, int level);
}

template <class T>
void Column<T>::dump_node_structure(const Array& root, std::ostream& out, int level)
{
    root.dump_bptree_structure(out, level, &_impl::leaf_dumper);
}

#endif

template <class T>
std::pair<ref_type, size_t> Column<T>::get_to_dot_parent(size_t ndx_in_parent) const
{
    auto root = get_root_array();
    if (root->is_inner_bptree_node()) {
        std::pair<MemRef, size_t> p = static_cast<const BpTreeNode*>(root)->get_bptree_leaf(ndx_in_parent);
        return std::make_pair(p.first.get_ref(), p.second);
    }
    else {
        return std::make_pair(root->get_ref(), ndx_in_parent);
    }
}

// LCOV_EXCL_STOP ignore debug functions


template <class ColumnDataType>
ColumnRandIterator<ColumnDataType>::ColumnRandIterator(const Column<ColumnDataType>* src_col, size_t ndx)
    : m_col_ndx(ndx)
    , m_col(src_col)
{
}

template <class ColumnDataType>
bool ColumnRandIterator<ColumnDataType>::operator==(const ColumnRandIterator<ColumnDataType>& rhs) const
{
    return (m_col_ndx == rhs.m_col_ndx);
}

template <class ColumnDataType>
bool ColumnRandIterator<ColumnDataType>::operator!=(const ColumnRandIterator<ColumnDataType>& rhs) const
{
    return !(*this == rhs);
}

template <class ColumnDataType>
bool ColumnRandIterator<ColumnDataType>::operator<(const ColumnRandIterator<ColumnDataType>& rhs) const
{
    return m_col_ndx < rhs.m_col_ndx;
}

template <class ColumnDataType>
bool ColumnRandIterator<ColumnDataType>::operator>(const ColumnRandIterator<ColumnDataType>& rhs) const
{
    return rhs < *this;
}

template <class ColumnDataType>
bool ColumnRandIterator<ColumnDataType>::operator<=(const ColumnRandIterator<ColumnDataType>& rhs) const
{
    return !(rhs < *this);
}

template <class ColumnDataType>
bool ColumnRandIterator<ColumnDataType>::operator>=(const ColumnRandIterator<ColumnDataType>& rhs) const
{
    return !(*this < rhs);
}

template <class ColumnDataType>
ColumnRandIterator<ColumnDataType>& ColumnRandIterator<ColumnDataType>::operator+=(ptrdiff_t movement)
{
    m_col_ndx += movement;
    return (*this);
}

template <class ColumnDataType>
ColumnRandIterator<ColumnDataType>& ColumnRandIterator<ColumnDataType>::operator-=(ptrdiff_t movement)
{
    m_col_ndx -= movement;
    return (*this);
}

template <class ColumnDataType>
ColumnRandIterator<ColumnDataType>& ColumnRandIterator<ColumnDataType>::operator++()
{
    ++m_col_ndx;
    return (*this);
}

template <class ColumnDataType>
ColumnRandIterator<ColumnDataType>& ColumnRandIterator<ColumnDataType>::operator--()
{
    --m_col_ndx;
    return (*this);
}

template <class ColumnDataType>
ColumnRandIterator<ColumnDataType> ColumnRandIterator<ColumnDataType>::operator++(int)
{
    auto temp(*this);
    ++m_col_ndx;
    return temp;
}

template <class ColumnDataType>
ColumnRandIterator<ColumnDataType> ColumnRandIterator<ColumnDataType>::operator--(int)
{
    auto temp(*this);
    --m_col_ndx;
    return temp;
}

template <class ColumnDataType>
ColumnRandIterator<ColumnDataType> ColumnRandIterator<ColumnDataType>::operator+(ptrdiff_t movement)
{
    return ColumnRandIterator(m_col, m_col_ndx + movement);
}

template <class ColumnDataType>
ColumnRandIterator<ColumnDataType> ColumnRandIterator<ColumnDataType>::operator-(ptrdiff_t movement)
{
    return ColumnRandIterator(m_col, m_col_ndx - movement);
}

template <class ColumnDataType>
ptrdiff_t ColumnRandIterator<ColumnDataType>::operator-(const ColumnRandIterator<ColumnDataType>& right) const
{
    return m_col_ndx - right.m_col_ndx;
}

template <class ColumnDataType>
const ColumnDataType ColumnRandIterator<ColumnDataType>::operator*() const
{
    return m_col->get(m_col_ndx);
}

template <class ColumnDataType>
const ColumnDataType ColumnRandIterator<ColumnDataType>::operator->() const
{
    return m_col->get(m_col_ndx);
}

template <class ColumnDataType>
const ColumnDataType ColumnRandIterator<ColumnDataType>::operator[](ptrdiff_t offset) const
{
    return m_col->get(m_col_ndx + offset);
}

template <class ColumnDataType>
size_t ColumnRandIterator<ColumnDataType>::get_col_ndx() const
{
    return m_col_ndx;
}

template <class ColumnDataType>
std::ostream& operator<<(std::ostream& out, const ColumnRandIterator<ColumnDataType>& it)
{
    out << "ColumnRandIterator at index: " << it.get_col_ndx();
    return out;
}

} // namespace realm

#endif // REALM_COLUMN_HPP
