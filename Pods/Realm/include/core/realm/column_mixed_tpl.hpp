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

namespace realm {

inline MixedColumn::MixedColumn(Allocator& alloc, ref_type ref,
                                Table* table, size_t column_ndx)
{
    create(alloc, ref, table, column_ndx);
}

inline void MixedColumn::adj_acc_insert_rows(size_t row_ndx, size_t num_rows) noexcept
{
    m_data->adj_acc_insert_rows(row_ndx, num_rows);
}

inline void MixedColumn::adj_acc_erase_row(size_t row_ndx) noexcept
{
    m_data->adj_acc_erase_row(row_ndx);
}

inline void MixedColumn::adj_acc_swap_rows(size_t row_ndx_1, size_t row_ndx_2) noexcept
{
    m_data->adj_acc_swap_rows(row_ndx_1, row_ndx_2);
}

inline void MixedColumn::adj_acc_move_over(size_t from_row_ndx, size_t to_row_ndx) noexcept
{
    m_data->adj_acc_move_over(from_row_ndx, to_row_ndx);
}

inline void MixedColumn::adj_acc_clear_root_table() noexcept
{
    m_data->adj_acc_clear_root_table();
}

inline ref_type MixedColumn::get_subtable_ref(size_t row_ndx) const noexcept
{
    REALM_ASSERT_3(row_ndx, <, m_types->size());
    if (m_types->get(row_ndx) != type_Table)
        return 0;
    return m_data->get_as_ref(row_ndx);
}

inline size_t MixedColumn::get_subtable_size(size_t row_ndx) const noexcept
{
    ref_type top_ref = get_subtable_ref(row_ndx);
    if (top_ref == 0)
        return 0;
    return _impl::TableFriend::get_size_from_ref(top_ref, m_data->get_alloc());
}

inline Table* MixedColumn::get_subtable_accessor(size_t row_ndx) const noexcept
{
    return m_data->get_subtable_accessor(row_ndx);
}

inline void MixedColumn::discard_subtable_accessor(size_t row_ndx) noexcept
{
    m_data->discard_subtable_accessor(row_ndx);
}

inline Table* MixedColumn::get_subtable_ptr(size_t row_ndx)
{
    REALM_ASSERT_3(row_ndx, <, m_types->size());
    if (m_types->get(row_ndx) != type_Table)
        return 0;
    return m_data->get_subtable_ptr(row_ndx); // Throws
}

inline const Table* MixedColumn::get_subtable_ptr(size_t subtable_ndx) const
{
    return const_cast<MixedColumn*>(this)->get_subtable_ptr(subtable_ndx);
}

inline void MixedColumn::discard_child_accessors() noexcept
{
    m_data->discard_child_accessors();
}


//
// Getters
//

#define REALM_BIT63 0x8000000000000000ULL

inline int64_t MixedColumn::get_value(size_t ndx) const noexcept
{
    REALM_ASSERT_3(ndx, <, m_types->size());

    // Shift the unsigned value right - ensuring 0 gets in from left.
    // Shifting signed integers right doesn't ensure 0's.
    uint64_t value = uint64_t(m_data->get(ndx)) >> 1;
    return int64_t(value);
}

inline int64_t MixedColumn::get_int(size_t ndx) const noexcept
{
    // Get first 63 bits of the integer value
    int64_t value = get_value(ndx);

    // restore 'sign'-bit from the column-type
    MixedColType col_type = MixedColType(m_types->get(ndx));
    if (col_type == mixcol_IntNeg) {
        // FIXME: Bad cast of result of '|' from unsigned to signed
        value |= REALM_BIT63; // set sign bit (63)
    }
    else {
        REALM_ASSERT_3(col_type, ==, mixcol_Int);
    }
    return value;
}

inline bool MixedColumn::get_bool(size_t ndx) const noexcept
{
    REALM_ASSERT_3(m_types->get(ndx), ==, mixcol_Bool);

    return (get_value(ndx) != 0);
}

inline OldDateTime MixedColumn::get_olddatetime(size_t ndx) const noexcept
{
    REALM_ASSERT_3(m_types->get(ndx), ==, mixcol_OldDateTime);

    return OldDateTime(get_value(ndx));
}

inline float MixedColumn::get_float(size_t ndx) const noexcept
{
    static_assert(std::numeric_limits<float>::is_iec559, "'float' is not IEEE");
    static_assert((sizeof (float) * CHAR_BIT == 32), "Assume 32 bit float.");
    REALM_ASSERT_3(m_types->get(ndx), ==, mixcol_Float);

    return type_punning<float>(get_value(ndx));
}

inline double MixedColumn::get_double(size_t ndx) const noexcept
{
    static_assert(std::numeric_limits<double>::is_iec559, "'double' is not IEEE");
    static_assert((sizeof (double) * CHAR_BIT == 64), "Assume 64 bit double.");

    int64_t int_val = get_value(ndx);

    // restore 'sign'-bit from the column-type
    MixedColType col_type = MixedColType(m_types->get(ndx));
    if (col_type == mixcol_DoubleNeg)
        int_val |= REALM_BIT63; // set sign bit (63)
    else {
        REALM_ASSERT_3(col_type, ==, mixcol_Double);
    }
    return type_punning<double>(int_val);
}

inline StringData MixedColumn::get_string(size_t ndx) const noexcept
{
    REALM_ASSERT_3(ndx, <, m_types->size());
    REALM_ASSERT_3(m_types->get(ndx), ==, mixcol_String);
    REALM_ASSERT(m_binary_data);

    size_t data_ndx = size_t(int64_t(m_data->get(ndx)) >> 1);
    return m_binary_data->get_string(data_ndx);
}

inline BinaryData MixedColumn::get_binary(size_t ndx) const noexcept
{
    REALM_ASSERT_3(ndx, <, m_types->size());
    REALM_ASSERT_3(m_types->get(ndx), ==, mixcol_Binary);
    REALM_ASSERT(m_binary_data);

    size_t data_ndx = size_t(uint64_t(m_data->get(ndx)) >> 1);
    return m_binary_data->get(data_ndx);
}

inline Timestamp MixedColumn::get_timestamp(size_t ndx) const noexcept
{
    REALM_ASSERT_3(ndx, <, m_types->size());
    REALM_ASSERT_3(m_types->get(ndx), ==, mixcol_Timestamp);
    REALM_ASSERT(m_timestamp_data);
    size_t data_ndx = size_t(uint64_t(m_data->get(ndx)) >> 1);
    return m_timestamp_data->get(data_ndx);
}

//
// Setters
//

// Set a int64 value.
// Store 63 bit of the value in m_data. Store sign bit in m_types.

inline void MixedColumn::set_int64(size_t ndx, int64_t value, MixedColType pos_type, MixedColType neg_type)
{
    REALM_ASSERT_3(ndx, <, m_types->size());

    // If sign-bit is set in value, 'store' it in the column-type
    MixedColType coltype = ((value & REALM_BIT63) == 0) ? pos_type : neg_type;

    // Remove refs or binary data (sets type to double)
    clear_value_and_discard_subtab_acc(ndx, coltype); // Throws

    // Shift value one bit and set lowest bit to indicate that this is not a ref
    value = (value << 1) + 1;
    m_data->set(ndx, value);
}

inline void MixedColumn::set_int(size_t ndx, int64_t value)
{
    set_int64(ndx, value, mixcol_Int, mixcol_IntNeg); // Throws
}

inline void MixedColumn::set_double(size_t ndx, double value)
{
    int64_t val64 = type_punning<int64_t>(value);
    set_int64(ndx, val64, mixcol_Double, mixcol_DoubleNeg); // Throws
}

inline void MixedColumn::set_value(size_t ndx, int64_t value, MixedColType coltype)
{
    REALM_ASSERT_3(ndx, <, m_types->size());

    // Remove refs or binary data (sets type to float)
    clear_value_and_discard_subtab_acc(ndx, coltype); // Throws

    // Shift value one bit and set lowest bit to indicate that this is not a ref
    int64_t v = (value << 1) + 1;
    m_data->set(ndx, v); // Throws
}

inline void MixedColumn::set_float(size_t ndx, float value)
{
    int64_t val64 = type_punning<int64_t>(value);
    set_value(ndx, val64, mixcol_Float); // Throws
}

inline void MixedColumn::set_bool(size_t ndx, bool value)
{
    set_value(ndx, (value ? 1 : 0), mixcol_Bool); // Throws
}

inline void MixedColumn::set_olddatetime(size_t ndx, OldDateTime value)
{
    set_value(ndx, int64_t(value.get_olddatetime()), mixcol_OldDateTime); // Throws
}

inline void MixedColumn::set_subtable(size_t ndx, const Table* t)
{
    REALM_ASSERT_3(ndx, <, m_types->size());
    typedef _impl::TableFriend tf;
    ref_type ref;
    if (t) {
        ref = tf::clone(*t, get_alloc()); // Throws
    }
    else {
        ref = tf::create_empty_table(get_alloc()); // Throws
    }
    // Remove any previous refs or binary data
    clear_value_and_discard_subtab_acc(ndx, mixcol_Table); // Throws
    m_data->set(ndx, ref); // Throws
}

//
// Inserts
//

inline void MixedColumn::insert_value(size_t row_ndx, int_fast64_t types_value,
                                      int_fast64_t data_value)
{
    size_t types_size = m_types->size(); // Slow
    bool is_append = row_ndx == types_size;
    size_t row_ndx_2 = is_append ? realm::npos : row_ndx;
    size_t num_rows = 1;
    m_types->insert_without_updating_index(row_ndx_2, types_value, num_rows); // Throws
    m_data->do_insert(row_ndx_2, data_value, num_rows); // Throws
}

// Insert a int64 value.
// Store 63 bit of the value in m_data. Store sign bit in m_types.

inline void MixedColumn::insert_int(size_t ndx, int_fast64_t value, MixedColType type)
{
    int_fast64_t types_value = type;
    // Shift value one bit and set lowest bit to indicate that this is not a ref
    int_fast64_t data_value =  1 + (value << 1);
    insert_value(ndx, types_value, data_value); // Throws
}

inline void MixedColumn::insert_pos_neg(size_t ndx, int_fast64_t value, MixedColType pos_type,
                                        MixedColType neg_type)
{
    // 'store' the sign-bit in the integer-type
    MixedColType type = (value & REALM_BIT63) == 0 ? pos_type : neg_type;
    int_fast64_t types_value = type;
    // Shift value one bit and set lowest bit to indicate that this is not a ref
    int_fast64_t data_value =  1 + (value << 1);
    insert_value(ndx, types_value, data_value); // Throws
}

inline void MixedColumn::insert_int(size_t ndx, int_fast64_t value)
{
    insert_pos_neg(ndx, value, mixcol_Int, mixcol_IntNeg); // Throws
}

inline void MixedColumn::insert_double(size_t ndx, double value)
{
    int_fast64_t value_2 = type_punning<int64_t>(value);
    insert_pos_neg(ndx, value_2, mixcol_Double, mixcol_DoubleNeg); // Throws
}

inline void MixedColumn::insert_float(size_t ndx, float value)
{
    int_fast64_t value_2 = type_punning<int32_t>(value);
    insert_int(ndx, value_2, mixcol_Float); // Throws
}

inline void MixedColumn::insert_bool(size_t ndx, bool value)
{
    int_fast64_t value_2 = int_fast64_t(value);
    insert_int(ndx, value_2, mixcol_Bool); // Throws
}

inline void MixedColumn::insert_olddatetime(size_t ndx, OldDateTime value)
{
    int_fast64_t value_2 = int_fast64_t(value.get_olddatetime());
    insert_int(ndx, value_2, mixcol_OldDateTime); // Throws
}

inline void MixedColumn::insert_timestamp(size_t ndx, Timestamp value)
{
    ensure_timestamp_column();
    size_t data_ndx = m_timestamp_data->size();
    m_timestamp_data->add(value); // Throws
    insert_int(ndx, int_fast64_t(data_ndx), mixcol_Timestamp);
}

inline void MixedColumn::insert_string(size_t ndx, StringData value)
{
    ensure_binary_data_column();
    size_t blob_ndx = m_binary_data->size();
    m_binary_data->add_string(value); // Throws

    int_fast64_t value_2 = int_fast64_t(blob_ndx);
    insert_int(ndx, value_2, mixcol_String); // Throws
}

inline void MixedColumn::insert_binary(size_t ndx, BinaryData value)
{
    ensure_binary_data_column();
    size_t blob_ndx = m_binary_data->size();
    m_binary_data->add(value); // Throws

    int_fast64_t value_2 = int_fast64_t(blob_ndx);
    insert_int(ndx, value_2, mixcol_Binary); // Throws
}

inline void MixedColumn::insert_subtable(size_t ndx, const Table* t)
{
    typedef _impl::TableFriend tf;
    ref_type ref;
    if (t) {
        ref = tf::clone(*t, get_alloc()); // Throws
    }
    else {
        ref = tf::create_empty_table(get_alloc()); // Throws
    }
    int_fast64_t types_value = mixcol_Table;
    int_fast64_t data_value = int_fast64_t(ref);
    insert_value(ndx, types_value, data_value); // Throws
}

inline void MixedColumn::erase(size_t row_ndx)
{
    size_t num_rows_to_erase = 1;
    size_t prior_num_rows = size(); // Note that size() is slow
    do_erase(row_ndx, num_rows_to_erase, prior_num_rows); // Throws
}

inline void MixedColumn::move_last_over(size_t row_ndx)
{
    size_t prior_num_rows = size(); // Note that size() is slow
    do_move_last_over(row_ndx, prior_num_rows); // Throws
}

inline void MixedColumn::swap_rows(size_t row_ndx_1, size_t row_ndx_2)
{
    do_swap_rows(row_ndx_1, row_ndx_2);
}

inline void MixedColumn::clear()
{
    size_t num_rows = size(); // Note that size() is slow
    do_clear(num_rows); // Throws
}

inline size_t MixedColumn::get_size_from_ref(ref_type root_ref,
                                                  Allocator& alloc) noexcept
{
    const char* root_header = alloc.translate(root_ref);
    ref_type types_ref = to_ref(Array::get(root_header, 0));
    return IntegerColumn::get_size_from_ref(types_ref, alloc);
}

inline void MixedColumn::clear_value_and_discard_subtab_acc(size_t row_ndx,
                                                            MixedColType new_type)
{
    MixedColType old_type = clear_value(row_ndx, new_type);
    if (old_type == mixcol_Table)
        m_data->discard_subtable_accessor(row_ndx);
}

// Implementing pure virtual method of ColumnBase.
inline void MixedColumn::insert_rows(size_t row_ndx, size_t num_rows_to_insert,
                                     size_t prior_num_rows, bool insert_nulls)
{
    REALM_ASSERT_DEBUG(prior_num_rows == size());
    REALM_ASSERT(row_ndx <= prior_num_rows);
    REALM_ASSERT(!insert_nulls);

    size_t row_ndx_2 = (row_ndx == prior_num_rows ? realm::npos : row_ndx);

    int_fast64_t type_value = mixcol_Int;
    m_types->insert_without_updating_index(row_ndx_2, type_value, num_rows_to_insert); // Throws

    // The least significant bit indicates that the rest of the bits form an
    // integer value, so 1 is actually zero.
    int_fast64_t data_value = 1;
    m_data->do_insert(row_ndx_2, data_value, num_rows_to_insert); // Throws
}

// Implementing pure virtual method of ColumnBase.
inline void MixedColumn::erase_rows(size_t row_ndx, size_t num_rows_to_erase,
                                    size_t prior_num_rows, bool)
{
    do_erase(row_ndx, num_rows_to_erase, prior_num_rows); // Throws
}

// Implementing pure virtual method of ColumnBase.
inline void MixedColumn::move_last_row_over(size_t row_ndx, size_t prior_num_rows, bool)
{
    do_move_last_over(row_ndx, prior_num_rows); // Throws
}

// Implementing pure virtual method of ColumnBase.
inline void MixedColumn::clear(size_t num_rows, bool)
{
    do_clear(num_rows); // Throws
}

inline void MixedColumn::mark(int type) noexcept
{
    m_data->mark(type);
}

inline void MixedColumn::refresh_accessor_tree(size_t col_ndx, const Spec& spec)
{
    get_root_array()->init_from_parent();
    m_types->refresh_accessor_tree(col_ndx, spec); // Throws
    m_data->refresh_accessor_tree(col_ndx, spec); // Throws
    if (m_binary_data) {
        REALM_ASSERT_3(get_root_array()->size(), >=, 3);
        m_binary_data->refresh_accessor_tree(col_ndx, spec); // Throws
    }
    if (m_timestamp_data) {
        REALM_ASSERT_3(get_root_array()->size(), >=, 4);
        m_timestamp_data->refresh_accessor_tree(col_ndx, spec); // Throws
    }


    // See if m_binary_data needs to be created.
    if (get_root_array()->size() >= 3) {
        ref_type ref = get_root_array()->get_as_ref(2);
        m_binary_data.reset(new BinaryColumn(get_alloc(), ref)); // Throws
        m_binary_data->set_parent(get_root_array(), 2);
    }

    // See if m_timestamp_data needs to be created.
    if (get_root_array()->size() >= 4) {
        ref_type ref = get_root_array()->get_as_ref(3);
        m_timestamp_data.reset(new TimestampColumn(get_alloc(), ref)); // Throws
        m_timestamp_data->set_parent(get_root_array(), 3);
    }
}

inline void MixedColumn::RefsColumn::refresh_accessor_tree(size_t col_ndx, const Spec& spec)
{
    SubtableColumnBase::refresh_accessor_tree(col_ndx, spec); // Throws
    size_t spec_ndx_in_parent = 0; // Ignored because these are root tables
    m_subtable_map.refresh_accessor_tree(spec_ndx_in_parent); // Throws
}

} // namespace realm
