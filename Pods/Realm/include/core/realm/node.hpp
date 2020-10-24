/*************************************************************************
 *
 * Copyright 2018 Realm Inc.
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

#ifndef REALM_NODE_HPP
#define REALM_NODE_HPP

#include <realm/node_header.hpp>
#include <realm/alloc.hpp>

namespace realm {

/// Special index value. It has various meanings depending on
/// context. It is returned by some search functions to indicate 'not
/// found'. It is similar in function to std::string::npos.
const size_t npos = size_t(-1);

/// Alias for realm::npos.
const size_t not_found = npos;

/// All accessor classes that logically contains other objects must inherit
/// this class.
///
/// A database node accessor contains information about the parent of the
/// referenced node. This 'reverse' reference is not explicitly present in the
/// underlying node hierarchy, but it is needed when modifying an array. A
/// modification may lead to relocation of the underlying array node, and the
/// parent must be updated accordingly. Since this applies recursivly all the
/// way to the root node, it is essential that the entire chain of parent
/// accessors is constructed and propperly maintained when a particular array is
/// modified.
class ArrayParent {
public:
    virtual ~ArrayParent() noexcept
    {
    }

    virtual ref_type get_child_ref(size_t child_ndx) const noexcept = 0;
    virtual void update_child_ref(size_t child_ndx, ref_type new_ref) = 0;
    // Used only by Array::to_dot().
    virtual std::pair<ref_type, size_t> get_to_dot_parent(size_t ndx_in_parent) const = 0;
};

/// Provides access to individual array nodes of the database.
///
/// This class serves purely as an accessor, it assumes no ownership of the
/// referenced memory.
///
/// An node accessor can be in one of two states: attached or unattached. It is
/// in the attached state if, and only if is_attached() returns true. Most
/// non-static member functions of this class have undefined behaviour if the
/// accessor is in the unattached state. The exceptions are: is_attached(),
/// detach(), create(), init_from_ref(), init_from_mem(), init_from_parent(),
/// has_parent(), get_parent(), set_parent(), get_ndx_in_parent(),
/// set_ndx_in_parent(), adjust_ndx_in_parent(), and get_ref_from_parent().
///
/// An node accessor contains information about the parent of the referenced
/// node. This 'reverse' reference is not explicitly present in the
/// underlying node hierarchy, but it is needed when modifying a node. A
/// modification may lead to relocation of the underlying node, and the
/// parent must be updated accordingly. Since this applies recursively all the
/// way to the root node, it is essential that the entire chain of parent
/// accessors is constructed and properly maintained when a particular node is
/// modified.
///
/// The parent reference (`pointer to parent`, `index in parent`) is updated
/// independently from the state of attachment to an underlying node. In
/// particular, the parent reference remains valid and is unaffected by changes
/// in attachment. These two aspects of the state of the accessor is updated
/// independently, and it is entirely the responsibility of the caller to update
/// them such that they are consistent with the underlying node hierarchy before
/// calling any method that modifies the underlying node.
///
/// FIXME: This class currently has fragments of ownership, in particular the
/// constructors that allocate underlying memory. On the other hand, the
/// destructor never frees the memory. This is a problematic situation, because
/// it so easily becomes an obscure source of leaks. There are three options for
/// a fix of which the third is most attractive but hardest to implement: (1)
/// Remove all traces of ownership semantics, that is, remove the constructors
/// that allocate memory, but keep the trivial copy constructor. For this to
/// work, it is important that the constness of the accessor has nothing to do
/// with the constness of the underlying memory, otherwise constness can be
/// violated simply by copying the accessor. (2) Disallov copying but associate
/// the constness of the accessor with the constness of the underlying
/// memory. (3) Provide full ownership semantics like is done for Table
/// accessors, and provide a proper copy constructor that really produces a copy
/// of the node. For this to work, the class should assume ownership if, and
/// only if there is no parent. A copy produced by a copy constructor will not
/// have a parent. Even if the original was part of a database, the copy will be
/// free-standing, that is, not be part of any database. For intra, or inter
/// database copying, one would have to also specify the target allocator.
class Node : public NodeHeader {
public:
    // FIXME: Should not be public
    char* m_data = nullptr; // Points to first byte after header

    /*********************** Constructor / destructor ************************/

    // The object will not be fully initialized when using this constructor
    explicit Node(Allocator& allocator) noexcept
        : m_alloc(allocator)
    {
    }

    virtual ~Node()
    {
    }

    /**************************** Initializers *******************************/

    /// Same as init_from_ref(ref_type) but avoid the mapping of 'ref' to memory
    /// pointer.
    char* init_from_mem(MemRef mem) noexcept
    {
        char* header = mem.get_addr();
        m_ref = mem.get_ref();
        m_data = get_data_from_header(header);
        m_width = get_width_from_header(header);
        m_size = get_size_from_header(header);

        return header;
    }

    /************************** access functions *****************************/

    bool is_attached() const noexcept
    {
        return m_data != nullptr;
    }

    inline bool is_read_only() const noexcept
    {
        REALM_ASSERT_DEBUG(is_attached());
        return m_alloc.is_read_only(m_ref);
    }

    size_t size() const noexcept
    {
        REALM_ASSERT_DEBUG(is_attached());
        return m_size;
    }

    bool is_empty() const noexcept
    {
        return size() == 0;
    }

    ref_type get_ref() const noexcept
    {
        return m_ref;
    }
    MemRef get_mem() const noexcept
    {
        return MemRef(get_header_from_data(m_data), m_ref, m_alloc);
    }
    Allocator& get_alloc() const noexcept
    {
        return m_alloc;
    }
    /// Get the address of the header of this array.
    char* get_header() const noexcept
    {
        return get_header_from_data(m_data);
    }

    bool has_parent() const noexcept
    {
        return m_parent != nullptr;
    }
    ArrayParent* get_parent() const noexcept
    {
        return m_parent;
    }
    size_t get_ndx_in_parent() const noexcept
    {
        return m_ndx_in_parent;
    }
    bool has_missing_parent_update() const noexcept
    {
        return m_missing_parent_update;
    }

    /// Get the ref of this array as known to the parent. The caller must ensure
    /// that the parent information ('pointer to parent' and 'index in parent')
    /// is correct before calling this function.
    ref_type get_ref_from_parent() const noexcept
    {
        REALM_ASSERT_DEBUG(m_parent);
        ref_type ref = m_parent->get_child_ref(m_ndx_in_parent);
        return ref;
    }

    /// The meaning of 'width' depends on the context in which this
    /// array is used.
    size_t get_width() const noexcept
    {
        return m_width;
    }

    /***************************** modifiers *********************************/

    /// Detach from the underlying array node. This method has no effect if the
    /// accessor is currently unattached (idempotency).
    void detach() noexcept
    {
        m_data = nullptr;
    }

    /// Destroy only the array that this accessor is attached to, not the
    /// children of that array. See non-static destroy_deep() for an
    /// alternative. If this accessor is already in the detached state, this
    /// function has no effect (idempotency).
    void destroy() noexcept
    {
        if (!is_attached())
            return;
        char* header = get_header_from_data(m_data);
        m_alloc.free_(m_ref, header);
        m_data = nullptr;
    }

    /// Shorthand for `destroy(MemRef(ref, alloc), alloc)`.
    static void destroy(ref_type ref, Allocator& alloc) noexcept
    {
        destroy(MemRef(ref, alloc), alloc);
    }

    /// Destroy only the specified array node, not its children. See also
    /// destroy_deep(MemRef, Allocator&).
    static void destroy(MemRef mem, Allocator& alloc) noexcept
    {
        alloc.free_(mem);
    }


    /// Setting a new parent affects ownership of the attached array node, if
    /// any. If a non-null parent is specified, and there was no parent
    /// originally, then the caller passes ownership to the parent, and vice
    /// versa. This assumes, of course, that the change in parentship reflects a
    /// corresponding change in the list of children in the affected parents.
    void set_parent(ArrayParent* parent, size_t ndx_in_parent) noexcept
    {
        m_parent = parent;
        m_ndx_in_parent = ndx_in_parent;
    }
    void set_ndx_in_parent(size_t ndx) noexcept
    {
        m_ndx_in_parent = ndx;
    }

    void clear_missing_parent_update()
    {
        m_missing_parent_update = false;
    }

    /// Update the parents reference to this child. This requires, of course,
    /// that the parent information stored in this child is up to date. If the
    /// parent pointer is set to null, this function has no effect.
    void update_parent()
    {
        if (m_parent) {
            m_parent->update_child_ref(m_ndx_in_parent, m_ref);
        }
        else {
            m_missing_parent_update = true;
        }
    }

protected:
    /// The total size in bytes (including the header) of a new empty
    /// array. Must be a multiple of 8 (i.e., 64-bit aligned).
    static const size_t initial_capacity = 128;

    size_t m_ref;
    Allocator& m_alloc;
    size_t m_size = 0;         // Number of elements currently stored.
    uint_least8_t m_width = 0; // Size of an element (meaning depend on type of array).

#if REALM_ENABLE_MEMDEBUG
    // If m_no_relocation is false, then copy_on_write() will always relocate this array, regardless if it's
    // required or not. If it's true, then it will never relocate, which is currently only expeted inside
    // GroupWriter::write_group() due to a unique chicken/egg problem (see description there).
    bool m_no_relocation = false;
#endif

    void alloc(size_t init_size, size_t new_width);
    void copy_on_write()
    {
#if REALM_ENABLE_MEMDEBUG
        // We want to relocate this array regardless if there is a need or not, in order to catch use-after-free bugs.
        // Only exception is inside GroupWriter::write_group() (see explanation at the definition of the
        // m_no_relocation
        // member)
        if (!m_no_relocation) {
#else
        if (is_read_only()) {
#endif
            do_copy_on_write();
        }
    }
    void copy_on_write(size_t min_size)
    {
#if REALM_ENABLE_MEMDEBUG
        // We want to relocate this array regardless if there is a need or not, in order to catch use-after-free bugs.
        // Only exception is inside GroupWriter::write_group() (see explanation at the definition of the
        // m_no_relocation
        // member)
        if (!m_no_relocation) {
#else
        if (is_read_only()) {
#endif
            do_copy_on_write(min_size);
        }
    }
    void ensure_size(size_t min_size)
    {
        char* header = get_header_from_data(m_data);
        size_t orig_capacity_bytes = get_capacity_from_header(header);
        if (orig_capacity_bytes < min_size) {
            do_copy_on_write(min_size);
        }
    }

    static MemRef create_node(size_t size, Allocator& alloc, bool context_flag = false, Type type = type_Normal,
                              WidthType width_type = wtype_Ignore, int width = 1);

    void set_header_size(size_t value) noexcept
    {
        set_size_in_header(value, get_header());
    }

    // Includes array header. Not necessarily 8-byte aligned.
    virtual size_t calc_byte_len(size_t num_items, size_t width) const;
    virtual size_t calc_item_count(size_t bytes, size_t width) const noexcept;
    static void init_header(char* header, bool is_inner_bptree_node, bool has_refs, bool context_flag,
                            WidthType width_type, int width, size_t size, size_t capacity) noexcept;

private:
    ArrayParent* m_parent = nullptr;
    size_t m_ndx_in_parent = 0; // Ignored if m_parent is null.
    bool m_missing_parent_update = false;

    void do_copy_on_write(size_t minimum_size = 0);
};

class Spec;

/// Base class for all nodes holding user data
class ArrayPayload {
public:
    virtual ~ArrayPayload();
    virtual void init_from_ref(ref_type) noexcept = 0;
    virtual void set_parent(ArrayParent* parent, size_t ndx_in_parent) noexcept = 0;
    virtual bool need_spec() const
    {
        return false;
    }
    virtual void set_spec(Spec*, size_t) const
    {
    }
};


inline void Node::init_header(char* header, bool is_inner_bptree_node, bool has_refs, bool context_flag,
                              WidthType width_type, int width, size_t size, size_t capacity) noexcept
{
    // Note: Since the header layout contains unallocated bit and/or
    // bytes, it is important that we put the entire header into a
    // well defined state initially.
    std::fill(header, header + header_size, 0);
    set_is_inner_bptree_node_in_header(is_inner_bptree_node, header);
    set_hasrefs_in_header(has_refs, header);
    set_context_flag_in_header(context_flag, header);
    set_wtype_in_header(width_type, header);
    set_width_in_header(width, header);
    set_size_in_header(size, header);
    set_capacity_in_header(capacity, header);
}
}

#endif /* REALM_NODE_HPP */
