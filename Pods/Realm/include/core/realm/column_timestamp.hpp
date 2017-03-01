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

#ifndef REALM_COLUMN_TIMESTAMP_HPP
#define REALM_COLUMN_TIMESTAMP_HPP

#include <realm/column.hpp>
#include <realm/timestamp.hpp>

namespace realm {

// Inherits from ColumnTemplate to get a compare_values() that can be called without knowing the
// column type
class TimestampColumn : public ColumnBaseSimple {
public:
    TimestampColumn(bool nullable, Allocator& alloc, ref_type ref, size_t col_ndx = npos);

    static ref_type create(Allocator& alloc, size_t size, bool nullable);
    static size_t get_size_from_ref(ref_type root_ref, Allocator& alloc) noexcept;

    /// Get the number of entries in this column. This operation is relatively
    /// slow.
    size_t size() const noexcept override;
    /// Whether or not this column is nullable.
    bool is_nullable() const noexcept override;
    /// Whether or not the value at \a row_ndx is NULL. If the column is not
    /// nullable, always returns false.
    bool is_null(size_t row_ndx) const noexcept override;
    /// Sets the value at \a row_ndx to be NULL.
    /// \throw LogicError Thrown if this column is not nullable.
    void set_null(size_t row_ndx) override;
    void insert_rows(size_t row_ndx, size_t num_rows_to_insert, size_t prior_num_rows, bool nullable) override;
    void erase_rows(size_t row_ndx, size_t num_rows_to_erase, size_t prior_num_rows,
                    bool broken_reciprocal_backlinks) override;
    void move_last_row_over(size_t row_ndx, size_t prior_num_rows, bool broken_reciprocal_backlinks) override;
    void clear(size_t num_rows, bool broken_reciprocal_backlinks) override;
    void swap_rows(size_t row_ndx_1, size_t row_ndx_2) override;
    void destroy() noexcept override;

    bool has_search_index() const noexcept final
    {
        return bool(m_search_index);
    }
    StringIndex* get_search_index() noexcept final
    {
        return m_search_index.get();
    }
    StringIndex* get_search_index() const noexcept final
    {
        return m_search_index.get();
    }
    void destroy_search_index() noexcept override;
    void set_search_index_ref(ref_type ref, ArrayParent* parent, size_t ndx_in_parent,
                              bool allow_duplicate_values) final;
    void populate_search_index();
    StringIndex* create_search_index() override;
    bool supports_search_index() const noexcept final
    {
        return true;
    }

    StringData get_index_data(size_t, StringIndex::StringConversionBuffer& buffer) const noexcept override;
    ref_type write(size_t slice_offset, size_t slice_size, size_t table_size, _impl::OutputStream&) const override;
    void update_from_parent(size_t old_baseline) noexcept override;
    void set_ndx_in_parent(size_t ndx) noexcept override;
    void refresh_accessor_tree(size_t new_col_ndx, const Spec&) override;

    void verify() const override;
    void to_dot(std::ostream&, StringData title = StringData()) const override;
    void do_dump_node_structure(std::ostream&, int level) const override;
    void leaf_to_dot(MemRef, ArrayParent*, size_t ndx_in_parent, std::ostream&) const override;

    void add(const Timestamp& ts = Timestamp{});
    Timestamp get(size_t row_ndx) const noexcept;
    void set(size_t row_ndx, const Timestamp& ts);
    bool compare(const TimestampColumn& c) const noexcept;
    int compare_values(size_t row1, size_t row2) const noexcept override;

    Timestamp maximum(size_t* result_index) const;
    Timestamp minimum(size_t* result_index) const;
    size_t count(Timestamp) const;
    void erase(size_t row_ndx, bool is_last);

    template <class Condition>
    size_t find(Timestamp value, size_t begin, size_t end) const noexcept
    {
        // FIXME: Here we can do all sorts of clever optimizations. Use bithack-search on seconds, then for each match
        // check nanoseconds, etc. Lots of possibilities. Below code is naive and slow but works.

        Condition cond;
        for (size_t t = begin; t < end; t++) {
            Timestamp ts = get(t);
            if (cond(ts, value, ts.is_null(), value.is_null()))
                return t;
        }
        return npos;
    }

    typedef Timestamp value_type;

private:
    std::unique_ptr<BpTree<util::Optional<int64_t>>> m_seconds;
    std::unique_ptr<BpTree<int64_t>> m_nanoseconds;

    std::unique_ptr<StringIndex> m_search_index;
    bool m_nullable;

    template <class BT>
    class CreateHandler;

    template <class Condition>
    Timestamp minmax(size_t* result_index) const noexcept
    {
        // Condition is realm::Greater for maximum and realm::Less for minimum. Any non-null value is both larger
        // and smaller than a null value.
        if (size() == 0) {
            if (result_index)
                *result_index = npos;
            return Timestamp{};
        }

        Timestamp best = get(0);
        size_t best_index = best.is_null() ? npos : 0;

        for (size_t i = 1; i < size(); ++i) {
            Timestamp candidate = get(i);
            // Condition() will return false if any of the two values are null.
            if ((best.is_null() && !candidate.is_null()) || Condition()(candidate, best, candidate.is_null(), best.is_null())) {
                best = candidate;
                best_index = i;
            }
        }
        if (result_index)
            *result_index = best_index;
        return best;
    }
};

} // namespace realm

#endif // REALM_COLUMN_TIMESTAMP_HPP
