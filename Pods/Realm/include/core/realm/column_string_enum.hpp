/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2015] Realm Inc
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of Realm Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Realm Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Realm Incorporated.
 *
 **************************************************************************/
#ifndef REALM_COLUMN_STRING_ENUM_HPP
#define REALM_COLUMN_STRING_ENUM_HPP

#include <realm/column_string.hpp>

namespace realm {

// Pre-declarations
class StringIndex;


/// From the point of view of the application, an enumerated strings column
/// (StringEnumColumn) is like a string column (StringColumn), yet it manages
/// its strings in such a way that each unique string is stored only once. In
/// fact, an enumerated strings column is a combination of two subcolumns; a
/// regular string column (StringColumn) that stores the unique strings, and an
/// integer column that stores one unique string index for each entry in the
/// enumerated strings column.
///
/// In terms of the underlying node structure, the subcolumn containing the
/// unique strings is not a true part of the enumerated strings column. Instead
/// it is a part of the spec structure that describes the table of which the
/// enumerated strings column is a part. This way, the unique strings can be
/// shared across enumerated strings columns of multiple subtables. This also
/// means that the root of an enumerated strings column coincides with the root
/// of the integer subcolumn, and in some sense, an enumerated strings column is
/// just the integer subcolumn.
///
/// An enumerated strings column can optionally be equipped with a
/// search index. If it is, then the root ref of the index is stored
/// in Table::m_columns immediately after the root ref of the
/// enumerated strings column.
class StringEnumColumn: public IntegerColumn {
public:
    typedef StringData value_type;

    StringEnumColumn(Allocator&, ref_type ref, ref_type keys_ref, bool nullable);
    ~StringEnumColumn() noexcept override;
    void destroy() noexcept override;
    MemRef clone_deep(Allocator& alloc) const override;

    int compare_values(size_t row1, size_t row2) const override
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

    StringData get(size_t ndx) const noexcept;
    bool is_null(size_t ndx) const noexcept final;
    void set(size_t ndx, StringData value);
    void set_null(size_t ndx) override;
    void add();
    void add(StringData value);
    void insert(size_t ndx);
    void insert(size_t ndx, StringData value);
    void erase(size_t row_ndx);
    void move_last_over(size_t row_ndx);
    void clear();
    bool is_nullable() const noexcept final;

    size_t count(StringData value) const;
    size_t find_first(StringData value, size_t begin = 0, size_t end = npos) const;
    void find_all(IntegerColumn& res, StringData value,
                  size_t begin = 0, size_t end = npos) const;
    FindRes find_all_indexref(StringData value, size_t& dst) const;

    size_t count(size_t key_index) const;
    size_t find_first(size_t key_index, size_t begin=0, size_t end=-1) const;
    void find_all(IntegerColumn& res, size_t key_index, size_t begin = 0, size_t end = -1) const;

    //@{
    /// Find the lower/upper bound for the specified value assuming
    /// that the elements are already sorted in ascending order
    /// according to StringData::operator<().
    size_t lower_bound_string(StringData value) const noexcept;
    size_t upper_bound_string(StringData value) const noexcept;
    //@}

    void set_string(size_t, StringData) override;

    void adjust_keys_ndx_in_parent(int diff) noexcept;

    // Search index
    StringData get_index_data(size_t ndx, StringIndex::StringConversionBuffer& buffer) const noexcept final;
    void set_search_index_allow_duplicate_values(bool) noexcept override;
    bool supports_search_index() const noexcept final { return true; }
    StringIndex* create_search_index() override;
    void install_search_index(std::unique_ptr<StringIndex>) noexcept;
    void destroy_search_index() noexcept override;

    // Compare two string columns for equality
    bool compare_string(const StringColumn&) const;
    bool compare_string(const StringEnumColumn&) const;

    void insert_rows(size_t, size_t, size_t, bool) override;
    void erase_rows(size_t, size_t, size_t, bool) override;
    void move_last_row_over(size_t, size_t, bool) override;
    void clear(size_t, bool) override;
    void update_from_parent(size_t) noexcept override;
    void refresh_accessor_tree(size_t, const Spec&) override;

    size_t get_key_ndx(StringData value) const;
    size_t get_key_ndx_or_add(StringData value);

    StringColumn& get_keys();
    const StringColumn& get_keys() const;

#ifdef REALM_DEBUG
    void verify() const override;
    void verify(const Table&, size_t) const override;
    void do_dump_node_structure(std::ostream&, int) const override;
    void to_dot(std::ostream&, StringData title) const override;
#endif

private:
    // Member variables
    StringColumn m_keys;
    bool m_nullable;

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

    void do_erase(size_t row_ndx, bool is_last);
    void do_move_last_over(size_t row_ndx, size_t last_row_ndx);
    void do_clear();
};





// Implementation:

inline StringData StringEnumColumn::get(size_t ndx) const noexcept
{
    REALM_ASSERT_3(ndx, <, IntegerColumn::size());
    size_t key_ndx = to_size_t(IntegerColumn::get(ndx));
    StringData sd = m_keys.get(key_ndx);
    REALM_ASSERT_DEBUG(!(!m_nullable && sd.is_null()));
    return sd;
}

inline bool StringEnumColumn::is_null(size_t ndx) const noexcept
{
    return is_nullable() ? get(ndx).is_null() : false;
}

inline void StringEnumColumn::add()
{
    add(m_nullable ? realm::null() : StringData(""));
}

inline void StringEnumColumn::add(StringData value)
{
    REALM_ASSERT_DEBUG(!(!m_nullable && value.is_null()));
    size_t row_ndx = realm::npos;
    size_t num_rows = 1;
    do_insert(row_ndx, value, num_rows); // Throws
}

inline void StringEnumColumn::insert(size_t row_ndx)
{
    insert(row_ndx, m_nullable ? realm::null() : StringData(""));
}

inline void StringEnumColumn::insert(size_t row_ndx, StringData value)
{
    REALM_ASSERT_DEBUG(!(!m_nullable && value.is_null()));
    size_t column_size = this->size();
    REALM_ASSERT_3(row_ndx, <=, column_size);
    size_t num_rows = 1;
    bool is_append = row_ndx == column_size;
    do_insert(row_ndx, value, num_rows, is_append); // Throws
}

inline void StringEnumColumn::erase(size_t row_ndx)
{
    size_t last_row_ndx = size() - 1; // Note that size() is slow
    bool is_last = row_ndx == last_row_ndx;
    do_erase(row_ndx, is_last); // Throws
}

inline void StringEnumColumn::move_last_over(size_t row_ndx)
{
    size_t last_row_ndx = size() - 1; // Note that size() is slow
    do_move_last_over(row_ndx, last_row_ndx); // Throws
}

inline void StringEnumColumn::clear()
{
    do_clear(); // Throws
}

// Overriding virtual method of Column.
inline void StringEnumColumn::insert_rows(size_t row_ndx, size_t num_rows_to_insert,
                                          size_t prior_num_rows, bool insert_nulls)
{
    REALM_ASSERT_DEBUG(prior_num_rows == size());
    REALM_ASSERT(row_ndx <= prior_num_rows);
    REALM_ASSERT(!insert_nulls || m_nullable);

    StringData value = m_nullable ? realm::null() : StringData("");
    bool is_append = (row_ndx == prior_num_rows);
    do_insert(row_ndx, value, num_rows_to_insert, is_append); // Throws
}

// Overriding virtual method of Column.
inline void StringEnumColumn::erase_rows(size_t row_ndx, size_t num_rows_to_erase,
                                         size_t prior_num_rows, bool)
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

// Overriding virtual method of Column.
inline void StringEnumColumn::move_last_row_over(size_t row_ndx, size_t prior_num_rows, bool)
{
    REALM_ASSERT_DEBUG(prior_num_rows == size());
    REALM_ASSERT(row_ndx < prior_num_rows);

    size_t last_row_ndx = prior_num_rows - 1;
    do_move_last_over(row_ndx, last_row_ndx); // Throws
}

// Overriding virtual method of Column.
inline void StringEnumColumn::clear(size_t, bool)
{
    do_clear(); // Throws
}

inline size_t StringEnumColumn::lower_bound_string(StringData value) const noexcept
{
    return ColumnBase::lower_bound(*this, value);
}

inline size_t StringEnumColumn::upper_bound_string(StringData value) const noexcept
{
    return ColumnBase::upper_bound(*this, value);
}

inline void StringEnumColumn::set_string(size_t row_ndx, StringData value)
{
    set(row_ndx, value); // Throws
}

inline void StringEnumColumn::set_null(size_t row_ndx)
{
    set(row_ndx, realm::null{});
}

inline StringColumn& StringEnumColumn::get_keys()
{
    return m_keys;
}

inline const StringColumn& StringEnumColumn::get_keys() const
{
    return m_keys;
}


} // namespace realm

#endif // REALM_COLUMN_STRING_ENUM_HPP
