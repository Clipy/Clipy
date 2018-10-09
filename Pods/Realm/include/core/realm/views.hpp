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

#ifndef REALM_VIEWS_HPP
#define REALM_VIEWS_HPP

#include <realm/column.hpp>
#include <realm/handover_defs.hpp>

#include <realm/util/optional.hpp>

#include <vector>

namespace realm {

const int64_t detached_ref = -1;

class RowIndexes;

class BaseDescriptor {
public:
    BaseDescriptor() = default;
    virtual ~BaseDescriptor() = default;
    virtual bool is_valid() const noexcept = 0;
    virtual std::string get_description(TableRef attached_table) const = 0;
    virtual std::unique_ptr<BaseDescriptor> clone() const = 0;
    virtual DescriptorExport export_for_handover() const = 0;
    virtual DescriptorType get_type() const = 0;
};

// Forward declaration needed for deleted ColumnsDescriptor constructor
class SortDescriptor;

// ColumnsDescriptor encapsulates a reference to a set of columns (possibly over
// links), which is used to indicate the criteria columns for sort and distinct.
// Although the input is column indices, it does not rely on those indices
// remaining stable as long as the columns continue to exist.
class ColumnsDescriptor : public BaseDescriptor {
public:
    ColumnsDescriptor() = default;
    // Enforce type saftey to prevent automatic conversion of derived class
    // SortDescriptor into ColumnsDescriptor at compile time.
    ColumnsDescriptor(const SortDescriptor&) = delete;
    virtual ~ColumnsDescriptor() = default;

    // Create a descriptor for the given columns on the given table.
    // Each vector in `column_indices` represents a chain of columns, where
    // all but the last are Link columns (n.b.: LinkList and Backlink are not
    // supported), and the final is any column type that can be sorted on.
    // `column_indices` must be non-empty, and each vector within it must also
    // be non-empty.
    ColumnsDescriptor(Table const& table, std::vector<std::vector<size_t>> column_indices);
    std::unique_ptr<BaseDescriptor> clone() const override;

    // returns whether this descriptor is valid and can be used to sort
    bool is_valid() const noexcept override
    {
        return !m_columns.empty();
    }
    DescriptorType get_type() const override
    {
        return DescriptorType::Distinct;
    }

    class Sorter;
    virtual Sorter sorter(IntegerColumn const& row_indexes) const;

    // handover support
    DescriptorExport export_for_handover() const override;

    std::string get_description(TableRef attached_table) const override;

protected:
    std::vector<std::vector<const ColumnBase*>> m_columns;
};

class SortDescriptor : public ColumnsDescriptor {
public:
    // Create a sort descriptor for the given columns on the given table.
    // See ColumnsDescriptor for restrictions on `column_indices`.
    // The sort order can be specified by using `ascending` which must either be
    // empty or have one entry for each column index chain.
    SortDescriptor(Table const& table, std::vector<std::vector<size_t>> column_indices,
                   std::vector<bool> ascending = {});
    SortDescriptor() = default;
    ~SortDescriptor() = default;
    std::unique_ptr<BaseDescriptor> clone() const override;
    DescriptorType get_type() const override
    {
        return DescriptorType::Sort;
    }

    void merge_with(SortDescriptor&& other);

    Sorter sorter(IntegerColumn const& row_indexes) const override;

    // handover support
    DescriptorExport export_for_handover() const override;
    std::string get_description(TableRef attached_table) const override;

private:
    std::vector<bool> m_ascending;
};

class LimitDescriptor : public BaseDescriptor {
public:
    LimitDescriptor(size_t limit);
    virtual ~LimitDescriptor() = default;
    bool is_valid() const noexcept override { return true; }
    std::string get_description(TableRef attached_table) const override;
    std::unique_ptr<BaseDescriptor> clone() const override;
    DescriptorExport export_for_handover() const override;
    size_t get_limit() const noexcept { return m_limit; }
    DescriptorType get_type() const override
    {
        return DescriptorType::Limit;
    }

private:
    size_t m_limit = 0;
};


// Distinct uses the same syntax as sort except that the order is meaningless.
typedef ColumnsDescriptor DistinctDescriptor;

class DescriptorOrdering {
public:
    DescriptorOrdering() = default;
    DescriptorOrdering(const DescriptorOrdering&);
    DescriptorOrdering(DescriptorOrdering&&) = default;
    DescriptorOrdering& operator=(const DescriptorOrdering&);
    DescriptorOrdering& operator=(DescriptorOrdering&&) = default;

    void append_sort(SortDescriptor sort);
    void append_distinct(DistinctDescriptor distinct);
    void append_limit(LimitDescriptor limit);
    bool descriptor_is_sort(size_t index) const;
    bool descriptor_is_distinct(size_t index) const;
    bool descriptor_is_limit(size_t index) const;
    DescriptorType get_type(size_t index) const;
    bool is_empty() const { return m_descriptors.empty(); }
    size_t size() const { return m_descriptors.size(); }
    const BaseDescriptor* operator[](size_t ndx) const;
    bool will_apply_sort() const;
    bool will_apply_distinct() const;
    bool will_apply_limit() const;
    realm::util::Optional<size_t> get_min_limit() const;
    bool will_limit_to_zero() const;
    std::string get_description(TableRef target_table) const;

    // handover support
    using HandoverPatch = std::unique_ptr<DescriptorOrderingHandoverPatch>;
    static void generate_patch(DescriptorOrdering const&, HandoverPatch&);
    static DescriptorOrdering create_from_and_consume_patch(HandoverPatch&, Table const&);

private:
    std::vector<std::unique_ptr<BaseDescriptor>> m_descriptors;
};

// This class is for common functionality of ListView and LinkView which inherit from it. Currently it only
// supports sorting and distinct.
class RowIndexes {
public:
    RowIndexes(IntegerColumn::unattached_root_tag urt, realm::Allocator& alloc);
    RowIndexes(IntegerColumn&& col);
    RowIndexes(const RowIndexes& source, ConstSourcePayload mode);
    RowIndexes(RowIndexes& source, MutableSourcePayload mode);

    virtual ~RowIndexes()
    {
#ifdef REALM_COOKIE_CHECK
        m_debug_cookie = 0x7765697633333333; // 0x77656976 = 'view'; 0x33333333 = '3333' = destructed
#endif
    }

    // Disable copying, this is not supported.
    RowIndexes& operator=(const RowIndexes&) = delete;
    RowIndexes(const RowIndexes&) = delete;

    // Return a column of the table that m_row_indexes are pointing at (which is the target table for LinkList and
    // parent table for TableView)
    virtual const ColumnBase& get_column_base(size_t index) const = 0;

    virtual size_t size() const = 0;
    // Get the number of total results which have been filtered out because a number of "LIMIT" operations have
    // been applied. This number only applies to the last sync.
    virtual size_t get_num_results_excluded_by_limit() const noexcept { return m_limit_count; }

    // These two methods are overridden by TableView and LinkView.
    virtual uint_fast64_t sync_if_needed() const = 0;
    virtual bool is_in_sync() const
    {
        return true;
    }

    void check_cookie() const
    {
#ifdef REALM_COOKIE_CHECK
        REALM_ASSERT_RELEASE(m_debug_cookie == cookie_expected);
#endif
    }

    IntegerColumn m_row_indexes;

protected:
    void do_sort(const DescriptorOrdering& ordering);

    static const uint64_t cookie_expected = 0x7765697677777777ull; // 0x77656976 = 'view'; 0x77777777 = '7777' = alive
    size_t m_limit_count = 0;
    uint64_t m_debug_cookie;
};

} // namespace realm

#endif // REALM_VIEWS_HPP
