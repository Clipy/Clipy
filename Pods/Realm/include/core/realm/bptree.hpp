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

#ifndef REALM_BPTREE_HPP
#define REALM_BPTREE_HPP

#include <memory> // std::unique_ptr
#include <realm/array.hpp>
#include <realm/array_basic.hpp>
#include <realm/column_type_traits.hpp>
#include <realm/impl/destroy_guard.hpp>
#include <realm/impl/output_stream.hpp>

namespace realm {

/// Specialize BpTree to implement column types.
template <class T>
class BpTree;

class ArrayInteger;
class ArrayIntNull;

class BpTreeNode : public Array {
public:
    using Array::Array;
    /// Get the number of elements in the B+-tree rooted at this array
    /// node. The root must not be a leaf.
    ///
    /// Please avoid using this function (consider it deprecated). It
    /// will have to be removed if we choose to get rid of the last
    /// element of the main array of an inner B+-tree node that stores
    /// the total number of elements in the subtree. The motivation
    /// for removing it, is that it will significantly improve the
    /// efficiency when inserting after, and erasing the last element.
    size_t get_bptree_size() const noexcept;

    /// The root must not be a leaf.
    static size_t get_bptree_size_from_header(const char* root_header) noexcept;


    /// Find the leaf node corresponding to the specified element
    /// index index. The specified element index must refer to an
    /// element that exists in the tree. This function must be called
    /// on an inner B+-tree node, never a leaf. Note that according to
    /// invar:bptree-nonempty-inner and invar:bptree-nonempty-leaf, an
    /// inner B+-tree node can never be empty.
    ///
    /// This function is not obliged to instantiate intermediate array
    /// accessors. For this reason, this function cannot be used for
    /// operations that modify the tree, as that requires an unbroken
    /// chain of parent array accessors between the root and the
    /// leaf. Thus, despite the fact that the returned MemRef object
    /// appears to allow modification of the referenced memory, the
    /// caller must handle the memory reference as if it was
    /// const-qualified.
    ///
    /// \return (`leaf_header`, `ndx_in_leaf`) where `leaf_header`
    /// points to the the header of the located leaf, and
    /// `ndx_in_leaf` is the local index within that leaf
    /// corresponding to the specified element index.
    std::pair<MemRef, size_t> get_bptree_leaf(size_t elem_ndx) const noexcept;


    class NodeInfo;
    class VisitHandler;

    /// Visit leaves of the B+-tree rooted at this inner node,
    /// starting with the leaf that contains the element at the
    /// specified element index start offset, and ending when the
    /// handler returns false. The specified element index offset must
    /// refer to an element that exists in the tree. This function
    /// must be called on an inner B+-tree node, never a leaf. Note
    /// that according to invar:bptree-nonempty-inner and
    /// invar:bptree-nonempty-leaf, an inner B+-tree node can never be
    /// empty.
    ///
    /// \param elem_ndx_offset The start position (must be valid).
    ///
    /// \param elems_in_tree The total number of elements in the tree.
    ///
    /// \param handler The callback which will get called for each leaf.
    ///
    /// \return True if, and only if the handler has returned true for
    /// all visited leafs.
    bool visit_bptree_leaves(size_t elem_ndx_offset, size_t elems_in_tree, VisitHandler& handler);


    class UpdateHandler;

    /// Call the handler for every leaf. This function must be called
    /// on an inner B+-tree node, never a leaf.
    void update_bptree_leaves(UpdateHandler&);

    /// Call the handler for the leaf that contains the element at the
    /// specified index. This function must be called on an inner
    /// B+-tree node, never a leaf.
    void update_bptree_elem(size_t elem_ndx, UpdateHandler&);


    class EraseHandler;

    /// Erase the element at the specified index in the B+-tree with
    /// the specified root. When erasing the last element, you must
    /// pass npos in place of the index. This function must be called
    /// with a root that is an inner B+-tree node, never a leaf.
    ///
    /// This function is guaranteed to succeed (not throw) if the
    /// specified element was inserted during the current transaction,
    /// and no other modifying operation has been carried out since
    /// then (noexcept:bptree-erase-alt).
    ///
    /// FIXME: ExceptionSafety: The exception guarantee explained
    /// above is not as powerfull as we would like it to be. Here is
    /// what we would like: This function is guaranteed to succeed
    /// (not throw) if the specified element was inserted during the
    /// current transaction (noexcept:bptree-erase). This must be true
    /// even if the element is modified after insertion, and/or if
    /// other elements are inserted or erased around it. There are two
    /// aspects of the current design that stand in the way of this
    /// guarantee: (A) The fact that the node accessor, that is cached
    /// in the column accessor, has to be reallocated/reinstantiated
    /// when the root switches between being a leaf and an inner
    /// node. This problem would go away if we always cached the last
    /// used leaf accessor in the column accessor instead. (B) The
    /// fact that replacing one child ref with another can fail,
    /// because it may require reallocation of memory to expand the
    /// bit-width. This can be fixed in two ways: Either have the
    /// inner B+-tree nodes always have a bit-width of 64, or allow
    /// the root node to be discarded and the column ref to be set to
    /// zero in Table::m_columns.
    static void erase_bptree_elem(BpTreeNode* root, size_t elem_ndx, EraseHandler&);

    template <class TreeTraits>
    struct TreeInsert : TreeInsertBase {
        typename TreeTraits::value_type m_value;
        bool m_nullable;
    };

    /// Same as bptree_insert() but insert after the last element.
    template <class TreeTraits>
    ref_type bptree_append(TreeInsert<TreeTraits>& state);

    /// Insert an element into the B+-subtree rooted at this array
    /// node. The element is inserted before the specified element
    /// index. This function must be called on an inner B+-tree node,
    /// never a leaf. If this inner node had to be split, this
    /// function returns the `ref` of the new sibling.
    template <class TreeTraits>
    ref_type bptree_insert(size_t elem_ndx, TreeInsert<TreeTraits>& state);

protected:
    /// Insert a new child after original. If the parent has to be
    /// split, this function returns the `ref` of the new parent node.
    ref_type insert_bptree_child(Array& offsets, size_t orig_child_ndx, ref_type new_sibling_ref,
                                 TreeInsertBase& state);

    void ensure_bptree_offsets(Array& offsets);
    void create_bptree_offsets(Array& offsets, int_fast64_t first_value);

    bool do_erase_bptree_elem(size_t elem_ndx, EraseHandler&);
};

class BpTreeBase {
public:
    struct unattached_tag {
    };

    // Disable copying, this is not allowed.
    BpTreeBase& operator=(const BpTreeBase&) = delete;
    BpTreeBase(const BpTreeBase&) = delete;

    // Accessor concept:
    Allocator& get_alloc() const noexcept;
    void destroy() noexcept;
    void detach();
    bool is_attached() const noexcept;
    void set_parent(ArrayParent* parent, size_t ndx_in_parent) noexcept;
    size_t get_ndx_in_parent() const noexcept;
    void set_ndx_in_parent(size_t ndx) noexcept;
    void update_from_parent(size_t old_baseline) noexcept;
    MemRef clone_deep(Allocator& alloc) const;

    // BpTree interface:
    const Array& root() const noexcept;
    Array& root() noexcept;
    bool root_is_leaf() const noexcept;
    BpTreeNode& root_as_node();
    const BpTreeNode& root_as_node() const;
    void introduce_new_root(ref_type new_sibling_ref, TreeInsertBase& state, bool is_append);
    void replace_root(std::unique_ptr<Array> leaf);

protected:
    explicit BpTreeBase(std::unique_ptr<Array> root);
    explicit BpTreeBase(BpTreeBase&&) = default;
    BpTreeBase& operator=(BpTreeBase&&) = default;
    std::unique_ptr<Array> m_root;

    struct SliceHandler {
        virtual MemRef slice_leaf(MemRef leaf_mem, size_t offset, size_t size, Allocator& target_alloc) = 0;
        ~SliceHandler() noexcept
        {
        }
    };
    static ref_type write_subtree(const BpTreeNode& root, size_t slice_offset, size_t slice_size, size_t table_size,
                                  SliceHandler&, _impl::OutputStream&);
    friend class ColumnBase;
    friend class ColumnBaseSimple;

private:
    struct WriteSliceHandler;

    // FIXME: Move B+Tree functionality from Array to this class.
};


// Default implementation of BpTree. This should work for all types that have monomorphic
// leaves (i.e. all leaves are of the same type).
template <class T>
class BpTree : public BpTreeBase {
public:
    using value_type = T;
    using LeafType = typename ColumnTypeTraits<T>::leaf_type;

    /// LeafInfo is used by get_leaf() to provide access to a leaf
    /// without instantiating unnecessary nodes along the way.
    /// Upon return, out_leaf with hold a pointer to the leaf containing
    /// the index given to get_leaf(). If the index happens to be
    /// in the root node (i.e., the root is a leaf), it will point
    /// to the root node.
    /// If the index isn't in the root node, fallback will be initialized
    /// to represent the leaf holding the node, and out_leaf will be set
    /// to point to fallback.
    struct LeafInfo {
        const LeafType** out_leaf;
        LeafType* fallback;
    };

    BpTree();
    explicit BpTree(BpTreeBase::unattached_tag);
    explicit BpTree(Allocator& alloc);
    REALM_DEPRECATED("Initialize with MemRef instead")
    explicit BpTree(std::unique_ptr<Array> init_root)
        : BpTreeBase(std::move(init_root))
    {
    }
    explicit BpTree(Allocator& alloc, MemRef mem)
        : BpTreeBase(std::unique_ptr<Array>(new LeafType(alloc)))
    {
        init_from_mem(alloc, mem);
    }
    BpTree(BpTree&&) = default;
    BpTree& operator=(BpTree&&) = default;

    // Disable copying, this is not allowed.
    BpTree& operator=(const BpTree&) = delete;
    BpTree(const BpTree&) = delete;

    void init_from_ref(Allocator& alloc, ref_type ref);
    void init_from_mem(Allocator& alloc, MemRef mem);
    void init_from_parent();

    size_t size() const noexcept;
    bool is_empty() const noexcept
    {
        return size() == 0;
    }

    T get(size_t ndx) const noexcept;
    bool is_null(size_t ndx) const noexcept;
    void set(size_t, T value);
    void set_null(size_t);
    void insert(size_t ndx, T value, size_t num_rows = 1);
    void erase(size_t ndx, bool is_last = false);
    void move_last_over(size_t ndx, size_t last_row_ndx);
    void clear();
    T front() const noexcept;
    T back() const noexcept;

    size_t find_first(T value, size_t begin = 0, size_t end = npos) const;
    void find_all(IntegerColumn& out_indices, T value, size_t begin = 0, size_t end = npos) const;

    static MemRef create_leaf(Array::Type leaf_type, size_t size, T value, Allocator&);

    /// See LeafInfo for information about what to put in the inout_leaf
    /// parameter.
    ///
    /// This function cannot be used for modifying operations as it
    /// does not ensure the presence of an unbroken chain of parent
    /// accessors. For this reason, the identified leaf should always
    /// be accessed through the returned const-qualified reference,
    /// and never directly through the specfied fallback accessor.
    void get_leaf(size_t ndx, size_t& out_ndx_in_leaf, LeafInfo& inout_leaf) const noexcept;

    void update_each(BpTreeNode::UpdateHandler&);
    void update_elem(size_t, BpTreeNode::UpdateHandler&);

    void adjust(size_t ndx, T diff);
    void adjust(T diff);
    void adjust_ge(T limit, T diff);

    ref_type write(size_t slice_offset, size_t slice_size, size_t table_size, _impl::OutputStream& out) const;

#if defined(REALM_DEBUG)
    void verify() const;
    static size_t verify_leaf(MemRef mem, Allocator& alloc);
#endif
    static void leaf_to_dot(MemRef mem, ArrayParent* parent, size_t ndx_in_parent, std::ostream& out,
                            Allocator& alloc);

private:
    LeafType& root_as_leaf();
    const LeafType& root_as_leaf() const;

    std::unique_ptr<Array> create_root_from_ref(Allocator& alloc, ref_type ref);
    std::unique_ptr<Array> create_root_from_mem(Allocator& alloc, MemRef mem);

    struct EraseHandler;
    struct UpdateHandler;
    struct SetNullHandler;
    struct SliceHandler;
    struct AdjustHandler;
    struct AdjustGEHandler;

    struct LeafValueInserter;
    struct LeafNullInserter;

    template <class TreeTraits>
    void bptree_insert(size_t row_ndx, BpTreeNode::TreeInsert<TreeTraits>& state, size_t num_rows);
};


class BpTreeNode::NodeInfo {
public:
    MemRef m_mem;
    Array* m_parent;
    size_t m_ndx_in_parent;
    size_t m_offset, m_size;
};

class BpTreeNode::VisitHandler {
public:
    virtual bool visit(const NodeInfo& leaf_info) = 0;
    virtual ~VisitHandler() noexcept
    {
    }
};


class BpTreeNode::UpdateHandler {
public:
    virtual void update(MemRef, ArrayParent*, size_t leaf_ndx_in_parent, size_t elem_ndx_in_leaf) = 0;
    virtual ~UpdateHandler() noexcept
    {
    }
};


class BpTreeNode::EraseHandler {
public:
    /// If the specified leaf has more than one element, this function
    /// must erase the specified element from the leaf and return
    /// false. Otherwise, when the leaf has a single element, this
    /// function must return true without modifying the leaf. If \a
    /// elem_ndx_in_leaf is `npos`, it refers to the last element in
    /// the leaf. The implementation of this function must be
    /// exception safe. This function is guaranteed to be called at
    /// most once during each execution of Array::erase_bptree_elem(),
    /// and *exactly* once during each *successful* execution of
    /// Array::erase_bptree_elem().
    virtual bool erase_leaf_elem(MemRef, ArrayParent*, size_t leaf_ndx_in_parent, size_t elem_ndx_in_leaf) = 0;

    virtual void destroy_leaf(MemRef leaf_mem) noexcept = 0;

    /// Must replace the current root with the specified leaf. The
    /// implementation of this function must not destroy the
    /// underlying root node, or any of its children, as that will be
    /// done by Array::erase_bptree_elem(). The implementation of this
    /// function must be exception safe.
    virtual void replace_root_by_leaf(MemRef leaf_mem) = 0;

    /// Same as replace_root_by_leaf(), but must replace the root with
    /// an empty leaf. Also, if this function is called during an
    /// execution of Array::erase_bptree_elem(), it is guaranteed that
    /// it will be preceeded by a call to erase_leaf_elem().
    virtual void replace_root_by_empty_leaf() = 0;

    virtual ~EraseHandler() noexcept
    {
    }
};


/// Implementation:

inline BpTreeBase::BpTreeBase(std::unique_ptr<Array> init_root)
    : m_root(std::move(init_root))
{
}

inline Allocator& BpTreeBase::get_alloc() const noexcept
{
    return m_root->get_alloc();
}

inline void BpTreeBase::destroy() noexcept
{
    if (m_root)
        m_root->destroy_deep();
}

inline void BpTreeBase::detach()
{
    m_root->detach();
}

inline bool BpTreeBase::is_attached() const noexcept
{
    return m_root->is_attached();
}

inline bool BpTreeBase::root_is_leaf() const noexcept
{
    return !m_root->is_inner_bptree_node();
}

inline BpTreeNode& BpTreeBase::root_as_node()
{
    REALM_ASSERT_DEBUG(!root_is_leaf());
    REALM_ASSERT_DEBUG(dynamic_cast<BpTreeNode*>(m_root.get()) != nullptr);
    return static_cast<BpTreeNode&>(root());
}

inline const BpTreeNode& BpTreeBase::root_as_node() const
{
    Array* arr = m_root.get();
    REALM_ASSERT_DEBUG(!root_is_leaf());
    REALM_ASSERT_DEBUG(dynamic_cast<const BpTreeNode*>(arr) != nullptr);
    return static_cast<const BpTreeNode&>(*arr);
}

inline void BpTreeBase::set_parent(ArrayParent* parent, size_t ndx_in_parent) noexcept
{
    m_root->set_parent(parent, ndx_in_parent);
}

inline size_t BpTreeBase::get_ndx_in_parent() const noexcept
{
    return m_root->get_ndx_in_parent();
}

inline void BpTreeBase::set_ndx_in_parent(size_t ndx) noexcept
{
    m_root->set_ndx_in_parent(ndx);
}

inline void BpTreeBase::update_from_parent(size_t old_baseline) noexcept
{
    m_root->update_from_parent(old_baseline);
}

inline MemRef BpTreeBase::clone_deep(Allocator& alloc) const
{
    return m_root->clone_deep(alloc);
}

inline const Array& BpTreeBase::root() const noexcept
{
    return *m_root;
}

inline Array& BpTreeBase::root() noexcept
{
    return *m_root;
}

inline size_t BpTreeNode::get_bptree_size() const noexcept
{
    REALM_ASSERT_DEBUG(is_inner_bptree_node());
    int_fast64_t v = back();
    return size_t(v / 2); // v = 1 + 2*total_elems_in_tree
}

inline size_t BpTreeNode::get_bptree_size_from_header(const char* root_header) noexcept
{
    REALM_ASSERT_DEBUG(get_is_inner_bptree_node_from_header(root_header));
    size_t root_size = get_size_from_header(root_header);
    int_fast64_t v = get(root_header, root_size - 1);
    return size_t(v / 2); // v = 1 + 2*total_elems_in_tree
}

inline void BpTreeNode::ensure_bptree_offsets(Array& offsets)
{
    int_fast64_t first_value = get(0);
    if (first_value % 2 == 0) {
        offsets.init_from_ref(to_ref(first_value));
    }
    else {
        create_bptree_offsets(offsets, first_value); // Throws
    }
    offsets.set_parent(this, 0);
}


template <class TreeTraits>
ref_type BpTreeNode::bptree_append(TreeInsert<TreeTraits>& state)
{
    // FIXME: Consider exception safety. Especially, how can the split
    // be carried out in an exception safe manner?
    //
    // Can split be done as a separate preparation step, such that if
    // the actual insert fails, the split will still have occured.
    //
    // Unfortunately, it requires a rather significant rearrangement
    // of the insertion flow. Instead of returning the sibling ref
    // from insert functions, the leaf-insert functions must instead
    // call the special bptree_insert() function on the parent, which
    // will then cascade the split towards the root as required.
    //
    // At each level where a split is required (starting at the leaf):
    //
    //  1. Create the new sibling.
    //
    //  2. Copy relevant entries over such that new sibling is in
    //     its final state.
    //
    //  3. Call Array::bptree_insert() on parent with sibling ref.
    //
    //  4. Rearrange entries in original sibling and truncate as
    //     required (must not throw).
    //
    // What about the 'offsets' array? It will always be
    // present. Consider this carefully.

    REALM_ASSERT_DEBUG(size() >= 1 + 1 + 1); // At least one child

    ArrayParent& childs_parent = *this;
    size_t child_ref_ndx = size() - 2;
    ref_type child_ref = get_as_ref(child_ref_ndx), new_sibling_ref;
    char* child_header = static_cast<char*>(m_alloc.translate(child_ref));

    bool child_is_leaf = !get_is_inner_bptree_node_from_header(child_header);
    if (child_is_leaf) {
        size_t elem_ndx_in_child = npos; // Append
        new_sibling_ref = TreeTraits::leaf_insert(MemRef(child_header, child_ref, m_alloc), childs_parent,
                                                  child_ref_ndx, m_alloc, elem_ndx_in_child, state); // Throws
    }
    else {
        BpTreeNode child(m_alloc);
        child.init_from_mem(MemRef(child_header, child_ref, m_alloc));
        child.set_parent(&childs_parent, child_ref_ndx);
        new_sibling_ref = child.bptree_append(state); // Throws
    }

    if (REALM_LIKELY(!new_sibling_ref)) {
        // +2 because stored value is 1 + 2*total_elems_in_subtree
        adjust(size() - 1, +2); // Throws
        return 0;               // Child was not split, so parent was not split either
    }

    Array offsets(m_alloc);
    int_fast64_t first_value = get(0);
    if (first_value % 2 == 0) {
        // Offsets array is present (general form)
        offsets.init_from_ref(to_ref(first_value));
        offsets.set_parent(this, 0);
    }
    size_t child_ndx = child_ref_ndx - 1;
    return insert_bptree_child(offsets, child_ndx, new_sibling_ref, state); // Throws
}


template <class TreeTraits>
ref_type BpTreeNode::bptree_insert(size_t elem_ndx, TreeInsert<TreeTraits>& state)
{
    REALM_ASSERT_3(size(), >=, 1 + 1 + 1); // At least one child

    // Conversion to general form if in compact form. Since this
    // conversion will occur from root to leaf, it will maintain
    // invar:bptree-node-form.
    Array offsets(m_alloc);
    ensure_bptree_offsets(offsets); // Throws

    size_t child_ndx, elem_ndx_in_child;
    if (elem_ndx == 0) {
        // Optimization for prepend
        child_ndx = 0;
        elem_ndx_in_child = 0;
    }
    else {
        // There is a choice to be made when the element is to be
        // inserted between two subtrees. It can either be appended to
        // the first subtree, or it can be prepended to the second
        // one. We currently always append to the first subtree. It is
        // essentially a matter of using the lower vs. the upper bound
        // when searching through the offsets array.
        child_ndx = offsets.lower_bound_int(elem_ndx);
        REALM_ASSERT_3(child_ndx, <, size() - 2);
        size_t elem_ndx_offset = child_ndx == 0 ? 0 : to_size_t(offsets.get(child_ndx - 1));
        elem_ndx_in_child = elem_ndx - elem_ndx_offset;
    }

    ArrayParent& childs_parent = *this;
    size_t child_ref_ndx = child_ndx + 1;
    ref_type child_ref = get_as_ref(child_ref_ndx), new_sibling_ref;
    char* child_header = static_cast<char*>(m_alloc.translate(child_ref));
    bool child_is_leaf = !get_is_inner_bptree_node_from_header(child_header);
    if (child_is_leaf) {
        REALM_ASSERT_3(elem_ndx_in_child, <=, REALM_MAX_BPNODE_SIZE);
        new_sibling_ref = TreeTraits::leaf_insert(MemRef(child_header, child_ref, m_alloc), childs_parent,
                                                  child_ref_ndx, m_alloc, elem_ndx_in_child, state); // Throws
    }
    else {
        BpTreeNode child(m_alloc);
        child.init_from_mem(MemRef(child_header, child_ref, m_alloc));
        child.set_parent(&childs_parent, child_ref_ndx);
        new_sibling_ref = child.bptree_insert(elem_ndx_in_child, state); // Throws
    }

    if (REALM_LIKELY(!new_sibling_ref)) {
        // +2 because stored value is 1 + 2*total_elems_in_subtree
        adjust(size() - 1, +2); // Throws
        offsets.adjust(child_ndx, offsets.size(), +1);
        return 0; // Child was not split, so parent was not split either
    }

    return insert_bptree_child(offsets, child_ndx, new_sibling_ref, state); // Throws
}

template <class T>
BpTree<T>::BpTree()
    : BpTree(Allocator::get_default())
{
}

template <class T>
BpTree<T>::BpTree(Allocator& alloc)
    : BpTreeBase(std::unique_ptr<Array>(new LeafType(alloc)))
{
}

template <class T>
BpTree<T>::BpTree(BpTreeBase::unattached_tag)
    : BpTreeBase(nullptr)
{
}

template <class T>
std::unique_ptr<Array> BpTree<T>::create_root_from_mem(Allocator& alloc, MemRef mem)
{
    const char* header = mem.get_addr();
    std::unique_ptr<Array> new_root;
    bool is_inner_bptree_node = Array::get_is_inner_bptree_node_from_header(header);

    bool can_reuse_root_accessor =
        m_root && &m_root->get_alloc() == &alloc && m_root->is_inner_bptree_node() == is_inner_bptree_node;
    if (can_reuse_root_accessor) {
        if (is_inner_bptree_node) {
            m_root->init_from_mem(mem);
        }
        else {
            static_cast<LeafType&>(*m_root).init_from_mem(mem);
        }
        return std::move(m_root); // Same root will be reinstalled.
    }

    // Not reusing root note, allocating a new one.
    if (is_inner_bptree_node) {
        new_root.reset(new BpTreeNode{alloc});
        new_root->init_from_mem(mem);
    }
    else {
        std::unique_ptr<LeafType> leaf{new LeafType{alloc}};
        leaf->init_from_mem(mem);
        new_root = std::move(leaf);
    }
    return new_root;
}

template <class T>
std::unique_ptr<Array> BpTree<T>::create_root_from_ref(Allocator& alloc, ref_type ref)
{
    MemRef mem = MemRef{alloc.translate(ref), ref, alloc};
    return create_root_from_mem(alloc, mem);
}

template <class T>
void BpTree<T>::init_from_ref(Allocator& alloc, ref_type ref)
{
    auto new_root = create_root_from_ref(alloc, ref);
    replace_root(std::move(new_root));
}

template <class T>
void BpTree<T>::init_from_mem(Allocator& alloc, MemRef mem)
{
    auto new_root = create_root_from_mem(alloc, mem);
    replace_root(std::move(new_root));
}

template <class T>
void BpTree<T>::init_from_parent()
{
    ref_type ref = root().get_ref_from_parent();
    if (ref) {
        ArrayParent* parent = m_root->get_parent();
        size_t ndx_in_parent = m_root->get_ndx_in_parent();
        auto new_root = create_root_from_ref(get_alloc(), ref);
        new_root->set_parent(parent, ndx_in_parent);
        m_root = std::move(new_root);
    }
    else {
        m_root->detach();
    }
}

template <class T>
typename BpTree<T>::LeafType& BpTree<T>::root_as_leaf()
{
    REALM_ASSERT_DEBUG(root_is_leaf());
    REALM_ASSERT_DEBUG(dynamic_cast<LeafType*>(m_root.get()) != nullptr);
    return static_cast<LeafType&>(root());
}

template <class T>
const typename BpTree<T>::LeafType& BpTree<T>::root_as_leaf() const
{
    REALM_ASSERT_DEBUG(root_is_leaf());
    REALM_ASSERT_DEBUG(dynamic_cast<const LeafType*>(m_root.get()) != nullptr);
    return static_cast<const LeafType&>(root());
}

template <class T>
size_t BpTree<T>::size() const noexcept
{
    if (root_is_leaf()) {
        return root_as_leaf().size();
    }
    return root_as_node().get_bptree_size();
}

template <class T>
T BpTree<T>::back() const noexcept
{
    // FIXME: slow
    return get(size() - 1);
}

namespace _impl {

// NullableOrNothing encapsulates the behavior of nullable and
// non-nullable leaf types, so that non-nullable leaf types
// don't have to implement is_null/set_null but BpTree can still
// support the interface (and return false / assert when null
// is not supported).
template <class Leaf>
struct NullableOrNothing {
    static bool is_null(const Leaf& leaf, size_t ndx)
    {
        return leaf.is_null(ndx);
    }
    static void set_null(Leaf& leaf, size_t ndx)
    {
        leaf.set_null(ndx);
    }
};
template <>
struct NullableOrNothing<ArrayInteger> {
    static bool is_null(const ArrayInteger&, size_t)
    {
        return false;
    }
    static void set_null(ArrayInteger&, size_t)
    {
        REALM_ASSERT_RELEASE(false);
    }
};
}

template <class T>
bool BpTree<T>::is_null(size_t ndx) const noexcept
{
    if (root_is_leaf()) {
        return _impl::NullableOrNothing<LeafType>::is_null(root_as_leaf(), ndx);
    }
    LeafType fallback(get_alloc());
    const LeafType* leaf;
    LeafInfo leaf_info{&leaf, &fallback};
    size_t ndx_in_leaf;
    get_leaf(ndx, ndx_in_leaf, leaf_info);
    return _impl::NullableOrNothing<LeafType>::is_null(*leaf, ndx_in_leaf);
}

template <class T>
T BpTree<T>::get(size_t ndx) const noexcept
{
    REALM_ASSERT_DEBUG_EX(ndx < size(), ndx, size());
    if (root_is_leaf()) {
        return root_as_leaf().get(ndx);
    }

    // Use direct getter to avoid initializing leaf array:
    std::pair<MemRef, size_t> p = root_as_node().get_bptree_leaf(ndx);
    const char* leaf_header = p.first.get_addr();
    size_t ndx_in_leaf = p.second;
    return LeafType::get(leaf_header, ndx_in_leaf);
}

template <class T>
template <class TreeTraits>
void BpTree<T>::bptree_insert(size_t row_ndx, BpTreeNode::TreeInsert<TreeTraits>& state, size_t num_rows)
{
    ref_type new_sibling_ref;
    for (size_t i = 0; i < num_rows; ++i) {
        size_t row_ndx_2 = row_ndx == realm::npos ? realm::npos : row_ndx + i;
        if (root_is_leaf()) {
            REALM_ASSERT_DEBUG(row_ndx_2 == realm::npos || row_ndx_2 < REALM_MAX_BPNODE_SIZE);
            new_sibling_ref = root_as_leaf().bptree_leaf_insert(row_ndx_2, state.m_value, state);
        }
        else {
            if (row_ndx_2 == realm::npos) {
                new_sibling_ref = root_as_node().bptree_append(state); // Throws
            }
            else {
                new_sibling_ref = root_as_node().bptree_insert(row_ndx_2, state); // Throws
            }
        }

        if (REALM_UNLIKELY(new_sibling_ref)) {
            bool is_append = row_ndx_2 == realm::npos;
            introduce_new_root(new_sibling_ref, state, is_append);
        }
    }
}

template <class T>
struct BpTree<T>::LeafValueInserter {
    using value_type = T;
    T m_value;
    LeafValueInserter(T value)
        : m_value(std::move(value))
    {
    }

    // TreeTraits concept:
    static ref_type leaf_insert(MemRef leaf_mem, ArrayParent& parent, size_t ndx_in_parent, Allocator& alloc,
                                size_t ndx_in_leaf, BpTreeNode::TreeInsert<LeafValueInserter>& state)
    {
        LeafType leaf{alloc};
        leaf.init_from_mem(leaf_mem);
        leaf.set_parent(&parent, ndx_in_parent);
        // Should not move out of m_value, because the same inserter may be used to perform
        // multiple insertions (for example, if num_rows > 1).
        return leaf.bptree_leaf_insert(ndx_in_leaf, state.m_value, state);
    }
};

template <class T>
struct BpTree<T>::LeafNullInserter {
    using value_type = null;
    // TreeTraits concept:
    static ref_type leaf_insert(MemRef leaf_mem, ArrayParent& parent, size_t ndx_in_parent, Allocator& alloc,
                                size_t ndx_in_leaf, BpTreeNode::TreeInsert<LeafNullInserter>& state)
    {
        LeafType leaf{alloc};
        leaf.init_from_mem(leaf_mem);
        leaf.set_parent(&parent, ndx_in_parent);
        return leaf.bptree_leaf_insert(ndx_in_leaf, null{}, state);
    }
};

template <class T>
void BpTree<T>::insert(size_t row_ndx, T value, size_t num_rows)
{
    REALM_ASSERT_DEBUG(row_ndx == npos || row_ndx < size());
    BpTreeNode::TreeInsert<LeafValueInserter> inserter;
    inserter.m_value = std::move(value);
    inserter.m_nullable = std::is_same<T, util::Optional<int64_t>>::value; // FIXME
    bptree_insert(row_ndx, inserter, num_rows);                            // Throws
}

template <class T>
struct BpTree<T>::UpdateHandler : BpTreeNode::UpdateHandler {
    LeafType m_leaf;
    const T m_value;
    UpdateHandler(BpTreeBase& tree, T value) noexcept
        : m_leaf(tree.get_alloc())
        , m_value(std::move(value))
    {
    }
    void update(MemRef mem, ArrayParent* parent, size_t ndx_in_parent, size_t elem_ndx_in_leaf) override
    {
        m_leaf.init_from_mem(mem);
        m_leaf.set_parent(parent, ndx_in_parent);
        m_leaf.set(elem_ndx_in_leaf, m_value); // Throws
    }
};

template <class T>
struct BpTree<T>::SetNullHandler : BpTreeNode::UpdateHandler {
    LeafType m_leaf;
    SetNullHandler(BpTreeBase& tree) noexcept
        : m_leaf(tree.get_alloc())
    {
    }
    void update(MemRef mem, ArrayParent* parent, size_t ndx_in_parent, size_t elem_ndx_in_leaf) override
    {
        m_leaf.init_from_mem(mem);
        m_leaf.set_parent(parent, ndx_in_parent);
        _impl::NullableOrNothing<LeafType>::set_null(m_leaf, elem_ndx_in_leaf); // Throws
    }
};

template <class T>
void BpTree<T>::set(size_t ndx, T value)
{
    if (root_is_leaf()) {
        root_as_leaf().set(ndx, std::move(value));
    }
    else {
        UpdateHandler set_leaf_elem(*this, std::move(value));
        static_cast<BpTreeNode*>(m_root.get())->update_bptree_elem(ndx, set_leaf_elem); // Throws
    }
}

template <class T>
void BpTree<T>::set_null(size_t ndx)
{
    if (root_is_leaf()) {
        _impl::NullableOrNothing<LeafType>::set_null(root_as_leaf(), ndx);
    }
    else {
        SetNullHandler set_leaf_elem(*this);
        static_cast<BpTreeNode*>(m_root.get())->update_bptree_elem(ndx, set_leaf_elem); // Throws;
    }
}

template <class T>
struct BpTree<T>::EraseHandler : BpTreeNode::EraseHandler {
    BpTreeBase& m_tree;
    LeafType m_leaf;
    bool m_leaves_have_refs; // FIXME: Should be able to eliminate this.
    EraseHandler(BpTreeBase& tree) noexcept
        : m_tree(tree)
        , m_leaf(tree.get_alloc())
        , m_leaves_have_refs(false)
    {
    }
    bool erase_leaf_elem(MemRef leaf_mem, ArrayParent* parent, size_t leaf_ndx_in_parent,
                         size_t elem_ndx_in_leaf) override
    {
        m_leaf.init_from_mem(leaf_mem);
        REALM_ASSERT_3(m_leaf.size(), >=, 1);
        size_t last_ndx = m_leaf.size() - 1;
        if (last_ndx == 0) {
            m_leaves_have_refs = m_leaf.has_refs();
            return true;
        }
        m_leaf.set_parent(parent, leaf_ndx_in_parent);
        size_t ndx = elem_ndx_in_leaf;
        if (ndx == npos)
            ndx = last_ndx;
        m_leaf.erase(ndx); // Throws
        return false;
    }
    void destroy_leaf(MemRef leaf_mem) noexcept override
    {
        // FIXME: Seems like this would cause file space leaks if
        // m_leaves_have_refs is true, but consider carefully how
        // m_leaves_have_refs get its value.
        m_tree.get_alloc().free_(leaf_mem);
    }
    void replace_root_by_leaf(MemRef leaf_mem) override
    {
        std::unique_ptr<LeafType> leaf{new LeafType(m_tree.get_alloc())}; // Throws
        leaf->init_from_mem(leaf_mem);
        m_tree.replace_root(std::move(leaf)); // Throws
    }
    void replace_root_by_empty_leaf() override
    {
        std::unique_ptr<LeafType> leaf{new LeafType(m_tree.get_alloc())};            // Throws
        leaf->create(m_leaves_have_refs ? Array::type_HasRefs : Array::type_Normal); // Throws
        m_tree.replace_root(std::move(leaf));                                        // Throws
    }
};

template <class T>
void BpTree<T>::erase(size_t ndx, bool is_last)
{
    REALM_ASSERT_DEBUG_EX(ndx < size(), ndx, size());
    REALM_ASSERT_DEBUG(is_last == (ndx == size() - 1));
    if (root_is_leaf()) {
        root_as_leaf().erase(ndx);
    }
    else {
        size_t ndx_2 = is_last ? npos : ndx;
        EraseHandler handler(*this);
        BpTreeNode::erase_bptree_elem(&root_as_node(), ndx_2, handler);
    }
}

template <class T>
void BpTree<T>::move_last_over(size_t row_ndx, size_t last_row_ndx)
{
    // Copy value from last row over
    T value = get(last_row_ndx);
    set(row_ndx, value);
    erase(last_row_ndx, true);
}

template <class T>
void BpTree<T>::clear()
{
    if (root_is_leaf()) {
        if (std::is_same<T, int64_t>::value && root().get_type() == Array::type_HasRefs) {
            // FIXME: This is because some column types rely on integer columns
            // to contain refs.
            root().clear_and_destroy_children();
        }
        else {
            root_as_leaf().clear();
        }
    }
    else {
        Allocator& alloc = get_alloc();
        root().destroy_deep();

        std::unique_ptr<LeafType> new_root(new LeafType(alloc));
        new_root->create();
        replace_root(std::move(new_root));
    }
}


template <class T>
struct BpTree<T>::AdjustHandler : BpTreeNode::UpdateHandler {
    LeafType m_leaf;
    const T m_diff;
    AdjustHandler(BpTreeBase& tree, T diff)
        : m_leaf(tree.get_alloc())
        , m_diff(diff)
    {
    }

    void update(MemRef mem, ArrayParent* parent, size_t ndx_in_parent, size_t) final
    {
        m_leaf.init_from_mem(mem);
        m_leaf.set_parent(parent, ndx_in_parent);
        m_leaf.adjust(0, m_leaf.size(), m_diff);
    }
};

template <class T>
void BpTree<T>::adjust(T diff)
{
    if (root_is_leaf()) {
        root_as_leaf().adjust(0, m_root->size(), std::move(diff)); // Throws
    }
    else {
        AdjustHandler adjust_leaf_elem(*this, std::move(diff));
        root_as_node().update_bptree_leaves(adjust_leaf_elem); // Throws
    }
}

template <class T>
void BpTree<T>::adjust(size_t ndx, T diff)
{
    static_assert(std::is_arithmetic<T>::value, "adjust is undefined for non-arithmetic trees");
    set(ndx, get(ndx) + diff);
}

template <class T>
struct BpTree<T>::AdjustGEHandler : BpTreeNode::UpdateHandler {
    LeafType m_leaf;
    const T m_limit, m_diff;

    AdjustGEHandler(BpTreeBase& tree, T limit, T diff)
        : m_leaf(tree.get_alloc())
        , m_limit(limit)
        , m_diff(diff)
    {
    }

    void update(MemRef mem, ArrayParent* parent, size_t ndx_in_parent, size_t) final
    {
        m_leaf.init_from_mem(mem);
        m_leaf.set_parent(parent, ndx_in_parent);
        m_leaf.adjust_ge(m_limit, m_diff);
    }
};

template <class T>
void BpTree<T>::adjust_ge(T limit, T diff)
{
    if (root_is_leaf()) {
        root_as_leaf().adjust_ge(std::move(limit), std::move(diff)); // Throws
    }
    else {
        AdjustGEHandler adjust_leaf_elem(*this, std::move(limit), std::move(diff));
        root_as_node().update_bptree_leaves(adjust_leaf_elem); // Throws
    }
}

template <class T>
struct BpTree<T>::SliceHandler : public BpTreeBase::SliceHandler {
public:
    SliceHandler(Allocator& alloc)
        : m_leaf(alloc)
    {
    }
    MemRef slice_leaf(MemRef leaf_mem, size_t offset, size_t size, Allocator& target_alloc) override
    {
        m_leaf.init_from_mem(leaf_mem);
        return m_leaf.slice_and_clone_children(offset, size, target_alloc); // Throws
    }

private:
    LeafType m_leaf;
};

template <class T>
ref_type BpTree<T>::write(size_t slice_offset, size_t slice_size, size_t table_size, _impl::OutputStream& out) const
{
    ref_type ref;
    if (root_is_leaf()) {
        Allocator& alloc = Allocator::get_default();
        MemRef mem = root_as_leaf().slice_and_clone_children(slice_offset, slice_size, alloc); // Throws
        Array slice(alloc);
        _impl::DeepArrayDestroyGuard dg(&slice);
        slice.init_from_mem(mem);
        bool deep = true;
        bool only_when_modified = false;
        ref = slice.write(out, deep, only_when_modified); // Throws
    }
    else {
        SliceHandler handler(get_alloc());
        ref = write_subtree(root_as_node(), slice_offset, slice_size, table_size, handler, out); // Throws
    }
    return ref;
}

template <class T>
MemRef BpTree<T>::create_leaf(Array::Type leaf_type, size_t size, T value, Allocator& alloc)
{
    bool context_flag = false;
    MemRef mem = LeafType::create_array(leaf_type, context_flag, size, std::move(value), alloc);
    return mem;
}

template <class T>
void BpTree<T>::get_leaf(size_t ndx, size_t& ndx_in_leaf, LeafInfo& inout_leaf_info) const noexcept
{
    if (root_is_leaf()) {
        ndx_in_leaf = ndx;
        *inout_leaf_info.out_leaf = &root_as_leaf();
        return;
    }
    std::pair<MemRef, size_t> p = root_as_node().get_bptree_leaf(ndx);
    inout_leaf_info.fallback->init_from_mem(p.first);
    ndx_in_leaf = p.second;
    *inout_leaf_info.out_leaf = inout_leaf_info.fallback;
}

template <class T>
size_t BpTree<T>::find_first(T value, size_t begin, size_t end) const
{
    if (root_is_leaf()) {
        return root_as_leaf().find_first(value, begin, end);
    }

    // FIXME: It would be better to always require that 'end' is
    // specified explicitly, since Table has the size readily
    // available, and Array::get_bptree_size() is deprecated.
    if (end == npos)
        end = size();

    LeafType leaf_cache(get_alloc());
    size_t ndx_in_tree = begin;
    while (ndx_in_tree < end) {
        const LeafType* leaf;
        LeafInfo leaf_info{&leaf, &leaf_cache};
        size_t ndx_in_leaf;
        get_leaf(ndx_in_tree, ndx_in_leaf, leaf_info);
        size_t leaf_offset = ndx_in_tree - ndx_in_leaf;
        size_t end_in_leaf = std::min(leaf->size(), end - leaf_offset);
        size_t ndx = leaf->find_first(value, ndx_in_leaf, end_in_leaf); // Throws (maybe)
        if (ndx != not_found)
            return leaf_offset + ndx;
        ndx_in_tree = leaf_offset + end_in_leaf;
    }

    return not_found;
}

template <class T>
void BpTree<T>::find_all(IntegerColumn& result, T value, size_t begin, size_t end) const
{
    if (root_is_leaf()) {
        root_as_leaf().find_all(&result, value, 0, begin, end); // Throws
        return;
    }

    // FIXME: It would be better to always require that 'end' is
    // specified explicitely, since Table has the size readily
    // available, and Array::get_bptree_size() is deprecated.
    if (end == npos)
        end = size();

    LeafType leaf_cache(get_alloc());
    size_t ndx_in_tree = begin;
    while (ndx_in_tree < end) {
        const LeafType* leaf;
        LeafInfo leaf_info{&leaf, &leaf_cache};
        size_t ndx_in_leaf;
        get_leaf(ndx_in_tree, ndx_in_leaf, leaf_info);
        size_t leaf_offset = ndx_in_tree - ndx_in_leaf;
        size_t end_in_leaf = std::min(leaf->size(), end - leaf_offset);
        leaf->find_all(&result, value, leaf_offset, ndx_in_leaf, end_in_leaf); // Throws
        ndx_in_tree = leaf_offset + end_in_leaf;
    }
}

#if defined(REALM_DEBUG)
template <class T>
size_t BpTree<T>::verify_leaf(MemRef mem, Allocator& alloc)
{
    LeafType leaf(alloc);
    leaf.init_from_mem(mem);
    leaf.verify();
    return leaf.size();
}

template <class T>
void BpTree<T>::verify() const
{
    if (root_is_leaf()) {
        root_as_leaf().verify();
    }
    else {
        root().verify_bptree(&verify_leaf);
    }
}
#endif // REALM_DEBUG

template <class T>
void BpTree<T>::leaf_to_dot(MemRef leaf_mem, ArrayParent* parent, size_t ndx_in_parent, std::ostream& out,
                            Allocator& alloc)
{
    LeafType leaf(alloc);
    leaf.init_from_mem(leaf_mem);
    leaf.set_parent(parent, ndx_in_parent);
    leaf.to_dot(out);
}

} // namespace realm

#endif // REALM_BPTREE_HPP
