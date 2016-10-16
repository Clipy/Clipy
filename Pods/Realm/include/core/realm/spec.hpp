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

#ifndef REALM_SPEC_HPP
#define REALM_SPEC_HPP

#include <realm/util/features.h>
#include <realm/array.hpp>
#include <realm/array_string.hpp>
#include <realm/array_integer.hpp>
#include <realm/data_type.hpp>
#include <realm/column_type.hpp>

namespace realm {

class Table;
class SubspecRef;
class ConstSubspecRef;

class Spec {
public:
    Spec(SubspecRef) noexcept;
    ~Spec() noexcept;

    Allocator& get_alloc() const noexcept;

    bool has_strong_link_columns() noexcept;

    void insert_column(size_t column_ndx, ColumnType type, StringData name, ColumnAttr attr = col_attr_None);
    void rename_column(size_t column_ndx, StringData new_name);
    void move_column(size_t from, size_t to);

    /// Erase the column at the specified index, and move columns at
    /// succeeding indexes to the next lower index.
    ///
    /// This function is guaranteed to *never* throw if the spec is
    /// used in a non-transactional context, or if the spec has
    /// already been successfully modified within the current write
    /// transaction.
    void erase_column(size_t column_ndx);

    //@{
    // If a new Spec is constructed from the returned subspec
    // reference, it is the responsibility of the application that the
    // parent Spec object (this) is kept alive for at least as long as
    // the new Spec object.
    SubspecRef get_subtable_spec(size_t column_ndx) noexcept;
    ConstSubspecRef get_subtable_spec(size_t column_ndx) const noexcept;
    //@}

    // Column info
    size_t get_column_count() const noexcept;
    size_t get_public_column_count() const noexcept;
    DataType get_public_column_type(size_t column_ndx) const noexcept;
    ColumnType get_column_type(size_t column_ndx) const noexcept;
    StringData get_column_name(size_t column_ndx) const noexcept;

    /// Returns size_t(-1) if the specified column is not found.
    size_t get_column_index(StringData name) const noexcept;

    // Column Attributes
    ColumnAttr get_column_attr(size_t column_ndx) const noexcept;

    size_t get_subspec_ndx(size_t column_ndx) const noexcept;
    ref_type get_subspec_ref(size_t subspec_ndx) const noexcept;
    SubspecRef get_subspec_by_ndx(size_t subspec_ndx) noexcept;
    ConstSubspecRef get_subspec_by_ndx(size_t subspec_ndx) const noexcept;

    // Auto Enumerated string columns
    void upgrade_string_to_enum(size_t column_ndx, ref_type keys_ref, ArrayParent*& keys_parent, size_t& keys_ndx);
    size_t get_enumkeys_ndx(size_t column_ndx) const noexcept;
    ref_type get_enumkeys_ref(size_t column_ndx, ArrayParent** keys_parent = nullptr,
                              size_t* keys_ndx = nullptr) noexcept;

    // Links
    size_t get_opposite_link_table_ndx(size_t column_ndx) const noexcept;
    void set_opposite_link_table_ndx(size_t column_ndx, size_t table_ndx);
    bool has_backlinks() const noexcept;
    void set_backlink_origin_column(size_t backlink_col_ndx, size_t origin_col_ndx);
    size_t get_origin_column_ndx(size_t backlink_col_ndx) const noexcept;
    size_t find_backlink_column(size_t origin_table_ndx, size_t origin_col_ndx) const noexcept;

    /// Get position in `Table::m_columns` of the specified column. It may be
    /// different from the specified logical column index due to the presence of
    /// search indexes, since their top refs are stored in Table::m_columns as
    /// well.
    size_t get_column_ndx_in_parent(size_t column_ndx) const;

    //@{
    /// Compare two table specs for equality.
    bool operator==(const Spec&) const noexcept;
    bool operator!=(const Spec&) const noexcept;
    //@}

    void destroy() noexcept;

    size_t get_ndx_in_parent() const noexcept;
    void set_ndx_in_parent(size_t) noexcept;

#ifdef REALM_DEBUG
    void verify() const;
    void to_dot(std::ostream&, StringData title = StringData()) const;
#endif

private:
    // Underlying array structure.
    //
    // `m_subspecs` contains one entry for each subtable column, one entry for
    // each link or link list columns, two entries for each backlink column, and
    // zero entries for all other column types. For subtable columns the entry
    // is a ref pointing to the subtable spec, for link and link list columns it
    // is the group-level table index of the target table, and for backlink
    // columns the first entry is the group-level table index of the origin
    // table, and the second entry is the index of the origin column in the
    // origin table.
    Array m_top;
    ArrayInteger m_types; // 1st slot in m_top
    ArrayString m_names;  // 2nd slot in m_top
    ArrayInteger m_attr;  // 3rd slot in m_top
    Array m_subspecs;     // 4th slot in m_top (optional)
    Array m_enumkeys;     // 5th slot in m_top (optional)
    bool m_has_strong_link_columns;

    Spec(Allocator&) noexcept; // Unattached

    void init(ref_type) noexcept;
    void init(MemRef) noexcept;
    void init(SubspecRef) noexcept;
    void update_has_strong_link_columns() noexcept;

    // Similar in function to Array::init_from_parent().
    void init_from_parent() noexcept;

    ref_type get_ref() const noexcept;

    /// Called in the context of Group::commit() to ensure that
    /// attached table accessors stay valid across a commit. Please
    /// note that this works only for non-transactional commits. Table
    /// accessors obtained during a transaction are always detached
    /// when the transaction ends.
    void update_from_parent(size_t old_baseline) noexcept;

    void set_parent(ArrayParent*, size_t ndx_in_parent) noexcept;

    void set_column_type(size_t column_ndx, ColumnType type);
    void set_column_attr(size_t column_ndx, ColumnAttr attr);

    /// Construct an empty spec and return just the reference to the
    /// underlying memory.
    static MemRef create_empty_spec(Allocator&);

    struct ColumnInfo {
        size_t m_column_ref_ndx = 0; ///< Index within Table::m_columns
        bool m_has_search_index = false;
    };

    ColumnInfo get_column_info(size_t column_ndx) const noexcept;

    size_t get_subspec_ndx_after(size_t column_ndx, size_t skip_column_ndx) const noexcept;
    size_t get_subspec_entries_for_col_type(ColumnType type) const noexcept;
    bool has_subspec() const noexcept;

    // Returns false if the spec has no columns, otherwise it returns
    // true and sets `type` to the type of the first column.
    static bool get_first_column_type_from_ref(ref_type, Allocator&, ColumnType& type) noexcept;

    friend class Replication;
    friend class Group;
    friend class Table;
};


class SubspecRef {
public:
    struct const_cast_tag {
    };
    SubspecRef(const_cast_tag, ConstSubspecRef r) noexcept;
    ~SubspecRef() noexcept
    {
    }
    Allocator& get_alloc() const noexcept
    {
        return m_parent->get_alloc();
    }

private:
    Array* const m_parent;
    size_t const m_ndx_in_parent;

    SubspecRef(Array* parent, size_t ndx_in_parent) noexcept;

    friend class Spec;
    friend class ConstSubspecRef;
};

class ConstSubspecRef {
public:
    ConstSubspecRef(SubspecRef r) noexcept;
    ~ConstSubspecRef() noexcept
    {
    }
    Allocator& get_alloc() const noexcept
    {
        return m_parent->get_alloc();
    }

private:
    const Array* const m_parent;
    size_t const m_ndx_in_parent;

    ConstSubspecRef(const Array* parent, size_t ndx_in_parent) noexcept;

    friend class Spec;
    friend class SubspecRef;
};


// Implementation:

inline Allocator& Spec::get_alloc() const noexcept
{
    return m_top.get_alloc();
}

inline bool Spec::has_strong_link_columns() noexcept
{
    return m_has_strong_link_columns;
}

inline ref_type Spec::get_subspec_ref(size_t subspec_ndx) const noexcept
{
    REALM_ASSERT(subspec_ndx < m_subspecs.size());

    // Note that this addresses subspecs directly, indexing
    // by number of sub-table columns
    return m_subspecs.get_as_ref(subspec_ndx);
}

inline Spec::Spec(SubspecRef r) noexcept
    : m_top(r.m_parent->get_alloc())
    , m_types(r.m_parent->get_alloc())
    , m_names(r.m_parent->get_alloc())
    , m_attr(r.m_parent->get_alloc())
    , m_subspecs(r.m_parent->get_alloc())
    , m_enumkeys(r.m_parent->get_alloc())
{
    init(r);
}

// Uninitialized Spec (call init() to init)
inline Spec::Spec(Allocator& alloc) noexcept
    : m_top(alloc)
    , m_types(alloc)
    , m_names(alloc)
    , m_attr(alloc)
    , m_subspecs(alloc)
    , m_enumkeys(alloc)
{
}

inline SubspecRef Spec::get_subtable_spec(size_t column_ndx) noexcept
{
    REALM_ASSERT(column_ndx < get_column_count());
    REALM_ASSERT(get_column_type(column_ndx) == col_type_Table);
    size_t subspec_ndx = get_subspec_ndx(column_ndx);
    return SubspecRef(&m_subspecs, subspec_ndx);
}

inline ConstSubspecRef Spec::get_subtable_spec(size_t column_ndx) const noexcept
{
    REALM_ASSERT(column_ndx < get_column_count());
    REALM_ASSERT(get_column_type(column_ndx) == col_type_Table);
    size_t subspec_ndx = get_subspec_ndx(column_ndx);
    return ConstSubspecRef(&m_subspecs, subspec_ndx);
}

inline SubspecRef Spec::get_subspec_by_ndx(size_t subspec_ndx) noexcept
{
    return SubspecRef(&m_subspecs, subspec_ndx);
}

inline ConstSubspecRef Spec::get_subspec_by_ndx(size_t subspec_ndx) const noexcept
{
    return const_cast<Spec*>(this)->get_subspec_by_ndx(subspec_ndx);
}

inline void Spec::init(ref_type ref) noexcept
{
    MemRef mem(ref, get_alloc());
    init(mem);
}

inline void Spec::init(SubspecRef r) noexcept
{
    m_top.set_parent(r.m_parent, r.m_ndx_in_parent);
    ref_type ref = r.m_parent->get_as_ref(r.m_ndx_in_parent);
    init(ref);
}

inline void Spec::init_from_parent() noexcept
{
    ref_type ref = m_top.get_ref_from_parent();
    init(ref);
}

inline void Spec::destroy() noexcept
{
    m_top.destroy_deep();
}

inline size_t Spec::get_ndx_in_parent() const noexcept
{
    return m_top.get_ndx_in_parent();
}

inline void Spec::set_ndx_in_parent(size_t ndx) noexcept
{
    m_top.set_ndx_in_parent(ndx);
}

inline ref_type Spec::get_ref() const noexcept
{
    return m_top.get_ref();
}

inline void Spec::set_parent(ArrayParent* parent, size_t ndx_in_parent) noexcept
{
    m_top.set_parent(parent, ndx_in_parent);
}

inline void Spec::rename_column(size_t column_ndx, StringData new_name)
{
    REALM_ASSERT(column_ndx < m_types.size());
    m_names.set(column_ndx, new_name);
}

inline size_t Spec::get_column_count() const noexcept
{
    // This is the total count of columns, including backlinks (not public)
    return m_types.size();
}

inline size_t Spec::get_public_column_count() const noexcept
{
    // Backlinks are the last columns, and do not have names, so getting
    // the number of names gives us the count of user facing columns
    return m_names.size();
}

inline ColumnType Spec::get_column_type(size_t ndx) const noexcept
{
    REALM_ASSERT(ndx < get_column_count());
    return ColumnType(m_types.get(ndx));
}

inline void Spec::set_column_type(size_t column_ndx, ColumnType type)
{
    REALM_ASSERT(column_ndx < get_column_count());

    // At this point we only support upgrading to string enum
    REALM_ASSERT(ColumnType(m_types.get(column_ndx)) == col_type_String);
    REALM_ASSERT(type == col_type_StringEnum);

    m_types.set(column_ndx, type); // Throws

    update_has_strong_link_columns();
}

inline ColumnAttr Spec::get_column_attr(size_t ndx) const noexcept
{
    REALM_ASSERT(ndx < get_column_count());
    return ColumnAttr(m_attr.get(ndx));
}

inline void Spec::set_column_attr(size_t column_ndx, ColumnAttr attr)
{
    REALM_ASSERT(column_ndx < get_column_count());

    // At this point we only allow one attr at a time
    // so setting it will overwrite existing. In the future
    // we will allow combinations.
    m_attr.set(column_ndx, attr);

    update_has_strong_link_columns();
}

inline StringData Spec::get_column_name(size_t ndx) const noexcept
{
    REALM_ASSERT(ndx < get_column_count());
    return m_names.get(ndx);
}

inline size_t Spec::get_column_index(StringData name) const noexcept
{
    return m_names.find_first(name);
}

inline bool Spec::get_first_column_type_from_ref(ref_type top_ref, Allocator& alloc, ColumnType& type) noexcept
{
    const char* top_header = alloc.translate(top_ref);
    ref_type types_ref = to_ref(Array::get(top_header, 0));
    const char* types_header = alloc.translate(types_ref);
    if (Array::get_size_from_header(types_header) == 0)
        return false;
    type = ColumnType(Array::get(types_header, 0));
    return true;
}

inline bool Spec::has_backlinks() const noexcept
{
    // backlinks are always last and do not have names.
    return m_names.size() < m_types.size();

    // Fixme: It's bad design that backlinks are stored and recognized like this. Backlink columns
    // should be a column type like any other, and we should find another way to hide them away from
    // the user.
}

// Spec will have a subspec when it contains a column which is one of:
// link, linklist, backlink, or subtable. It is possible for m_top.size()
// to contain an entry for m_subspecs (at index 3) but this reference
// may be empty if the spec contains enumkeys (at index 4) but no subspec types.
inline bool Spec::has_subspec() const noexcept
{
    return (m_top.size() >= 4) && (m_top.get_as_ref(3) != 0);
}

inline bool Spec::operator!=(const Spec& s) const noexcept
{
    return !(*this == s);
}


inline SubspecRef::SubspecRef(Array* parent, size_t ndx_in_parent) noexcept
    : m_parent(parent)
    , m_ndx_in_parent(ndx_in_parent)
{
}

inline SubspecRef::SubspecRef(const_cast_tag, ConstSubspecRef r) noexcept
    : m_parent(const_cast<Array*>(r.m_parent))
    , m_ndx_in_parent(r.m_ndx_in_parent)
{
}

inline ConstSubspecRef::ConstSubspecRef(const Array* parent, size_t ndx_in_parent) noexcept
    : m_parent(parent)
    , m_ndx_in_parent(ndx_in_parent)
{
}

inline ConstSubspecRef::ConstSubspecRef(SubspecRef r) noexcept
    : m_parent(r.m_parent)
    , m_ndx_in_parent(r.m_ndx_in_parent)
{
}


} // namespace realm

#endif // REALM_SPEC_HPP
