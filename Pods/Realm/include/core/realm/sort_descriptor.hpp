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

#ifndef REALM_SORT_DESCRIPTOR_HPP
#define REALM_SORT_DESCRIPTOR_HPP

#include <vector>
#include <unordered_set>
#include <realm/cluster.hpp>
#include <realm/mixed.hpp>

namespace realm {

class SortDescriptor;
class ConstTableRef;
class Group;

enum class DescriptorType { Sort, Distinct, Limit, Include };

struct LinkPathPart {
    // Constructor for forward links
    LinkPathPart(ColKey col_key)
        : column_key(col_key)
    {
    }
    // Constructor for backward links. Source table must be a valid table.
    LinkPathPart(ColKey col_key, ConstTableRef source);
    // Each step in the path can be a forward or a backward link.
    // In case of a backlink, the column_key indicates the origin link column
    // (the forward link column in the origin table), not the backlink column
    // itself.
    ColKey column_key;
    // "from" is omitted to indicate forward links, if it is valid then
    // this path describes a backlink originating from the column from[column_key]
    TableKey from;
};

class BaseDescriptor {
public:
    struct IndexPair {
        IndexPair(ObjKey k, size_t i)
            : key_for_object(k)
            , index_in_view(i)
        {
        }
        bool operator<(const IndexPair& other) const
        {
            return index_in_view < other.index_in_view;
        }
        ObjKey key_for_object;
        size_t index_in_view;
        Mixed cached_value;
    };
    class IndexPairs : public std::vector<BaseDescriptor::IndexPair> {
    public:
        size_t m_removed_by_limit = 0;
    };
    class Sorter {
    public:
        Sorter(std::vector<std::vector<ColKey>> const& columns, std::vector<bool> const& ascending,
               Table const& root_table, const IndexPairs& indexes);
        Sorter()
        {
        }

        bool operator()(IndexPair i, IndexPair j, bool total_ordering = true) const;

        bool has_links() const
        {
            return std::any_of(m_columns.begin(), m_columns.end(),
                               [](auto&& col) { return !col.translated_keys.empty(); });
        }

        bool any_is_null(IndexPair i) const
        {
            return std::any_of(m_columns.begin(), m_columns.end(), [=](auto&& col) {
                return col.is_null.empty() ? false : col.is_null[i.index_in_view];
            });
        }
        void cache_first_column(IndexPairs& v);

    private:
        struct SortColumn {
            SortColumn(const Table* t, ColKey c, bool a)
                : table(t)
                , col_key(c)
                , ascending(a)
            {
            }
            std::vector<bool> is_null;
            std::vector<ObjKey> translated_keys;

            const Table* table;
            ColKey col_key;
            bool ascending;
        };
        std::vector<SortColumn> m_columns;
        friend class ObjList;
    };

    BaseDescriptor() = default;
    virtual ~BaseDescriptor() = default;
    virtual bool is_valid() const noexcept = 0;
    virtual std::string get_description(ConstTableRef attached_table) const = 0;
    virtual std::unique_ptr<BaseDescriptor> clone() const = 0;
    virtual DescriptorType get_type() const = 0;
    virtual void collect_dependencies(const Table* table, std::vector<TableKey>& table_keys) const = 0;
    virtual Sorter sorter(Table const& table, const IndexPairs& indexes) const = 0;
    // Do what you have to do
    virtual void execute(IndexPairs& v, const Sorter& predicate, const BaseDescriptor* next) const = 0;
};


// ColumnsDescriptor encapsulates a reference to a set of columns (possibly over
// links), which is used to indicate the criteria columns for sort and distinct.
class ColumnsDescriptor : public BaseDescriptor {
public:
    ColumnsDescriptor() = default;

    // Create a descriptor for the given columns on the given table.
    // Each vector in `column_keys` represents a chain of columns, where
    // all but the last are Link columns (n.b.: LinkList and Backlink are not
    // supported), and the final is any column type that can be sorted on.
    // `column_keys` must be non-empty, and each vector within it must also
    // be non-empty.
    ColumnsDescriptor(std::vector<std::vector<ColKey>> column_keys);

    // returns whether this descriptor is valid and can be used for sort or distinct
    bool is_valid() const noexcept override
    {
        return !m_column_keys.empty();
    }
    void collect_dependencies(const Table* table, std::vector<TableKey>& table_keys) const override;

protected:
    std::vector<std::vector<ColKey>> m_column_keys;
};

class DistinctDescriptor : public ColumnsDescriptor {
public:
    DistinctDescriptor() = default;
    DistinctDescriptor(std::vector<std::vector<ColKey>> column_keys)
        : ColumnsDescriptor(column_keys)
    {
    }

    std::unique_ptr<BaseDescriptor> clone() const override;

    DescriptorType get_type() const override
    {
        return DescriptorType::Distinct;
    }

    Sorter sorter(Table const& table, const IndexPairs& indexes) const override;
    void execute(IndexPairs& v, const Sorter& predicate, const BaseDescriptor* next) const override;

    std::string get_description(ConstTableRef attached_table) const override;
};


class SortDescriptor : public ColumnsDescriptor {
public:
    // Create a sort descriptor for the given columns on the given table.
    // See ColumnsDescriptor for restrictions on `column_keys`.
    // The sort order can be specified by using `ascending` which must either be
    // empty or have one entry for each column index chain.
    SortDescriptor(std::vector<std::vector<ColKey>> column_indices, std::vector<bool> ascending = {});
    SortDescriptor() = default;
    ~SortDescriptor() = default;
    std::unique_ptr<BaseDescriptor> clone() const override;

    DescriptorType get_type() const override
    {
        return DescriptorType::Sort;
    }

    util::Optional<bool> is_ascending(size_t ndx) const
    {
        if (ndx < m_ascending.size()) {
            return util::Optional<bool>(m_ascending[ndx]);
        }
        return util::none;
    }

    enum class MergeMode {
        /// If another sort has just been applied, merge before it, so it takes primary precedence
        /// this is used for time based scenarios where building the last applied sort is the most important
        /// default historical behaviour
        append,
        /// If another sort has just been applied, merge after it to take secondary precedence
        /// this is used to construct sorts in a builder pattern where the first applied sort remains the most
        /// important
        prepend,
        /// Replace this sort descriptor with another
        replace
    };

    void merge(SortDescriptor&& other, MergeMode mode);

    Sorter sorter(Table const& table, const IndexPairs& indexes) const override;

    void execute(IndexPairs& v, const Sorter& predicate, const BaseDescriptor* next) const override;

    std::string get_description(ConstTableRef attached_table) const override;

private:
    std::vector<bool> m_ascending;
};

class LimitDescriptor : public BaseDescriptor {
public:
    LimitDescriptor(size_t limit)
        : m_limit(limit)
    {
    }
    LimitDescriptor() = default;
    ~LimitDescriptor() = default;

    bool is_valid() const noexcept override
    {
        return m_limit != size_t(-1);
    }
    std::string get_description(ConstTableRef attached_table) const override;
    std::unique_ptr<BaseDescriptor> clone() const override;
    size_t get_limit() const noexcept
    {
        return m_limit;
    }

    DescriptorType get_type() const override
    {
        return DescriptorType::Limit;
    }

    Sorter sorter(Table const&, const IndexPairs&) const override
    {
        return Sorter();
    }

    void collect_dependencies(const Table*, std::vector<TableKey>&) const override
    {
    }
    void execute(IndexPairs& v, const Sorter& predicate, const BaseDescriptor* next) const override;

private:
    size_t m_limit = size_t(-1);
};

class IncludeDescriptor : public ColumnsDescriptor {
public:
    IncludeDescriptor() = default;
    // This constructor may throw an InvalidPathError exception if the path is not valid.
    // A valid path consists of any number of connected link/list/backlink paths and always ends with a backlink
    // column.
    IncludeDescriptor(ConstTableRef table, const std::vector<std::vector<LinkPathPart>>& link_paths);
    ~IncludeDescriptor() = default;
    std::string get_description(ConstTableRef attached_table) const override;
    std::unique_ptr<BaseDescriptor> clone() const override;
    DescriptorType get_type() const override
    {
        return DescriptorType::Include;
    }
    void append(const IncludeDescriptor& other);
    void report_included_backlinks(
        ConstTableRef origin, ObjKey object,
        util::FunctionRef<void(const Table*, const std::unordered_set<ObjKey>&)> reporter) const;

    Sorter sorter(Table const&, const IndexPairs&) const override
    {
        return Sorter();
    }

    void collect_dependencies(const Table*, std::vector<TableKey>&) const override
    {
    }
    void execute(IndexPairs& v, const Sorter& predicate, const BaseDescriptor* next) const override;

private:
    std::vector<std::vector<TableKey>> m_backlink_sources; // stores a default TableKey for non-backlink columns
};


class DescriptorOrdering {
public:
    DescriptorOrdering() = default;
    DescriptorOrdering(const DescriptorOrdering&);
    DescriptorOrdering(DescriptorOrdering&&) = default;
    DescriptorOrdering& operator=(const DescriptorOrdering&);
    DescriptorOrdering& operator=(DescriptorOrdering&&) = default;

    void append_sort(SortDescriptor sort, SortDescriptor::MergeMode mode = SortDescriptor::MergeMode::prepend);
    void append_distinct(DistinctDescriptor distinct);
    void append_limit(LimitDescriptor limit);
    void append_include(IncludeDescriptor include);
    realm::util::Optional<size_t> get_min_limit() const;
    /// Remove all LIMIT statements from this descriptor ordering, returning the
    /// minimum LIMIT value that existed. If there was no LIMIT statement,
    /// returns `none`.
    util::Optional<size_t> remove_all_limits();
    bool will_limit_to_zero() const;
    DescriptorType get_type(size_t index) const;
    bool is_empty() const
    {
        return m_descriptors.empty();
    }
    size_t size() const
    {
        return m_descriptors.size();
    }
    const BaseDescriptor* operator[](size_t ndx) const;
    bool will_apply_sort() const;
    bool will_apply_distinct() const;
    bool will_apply_limit() const;
    bool will_apply_include() const;
    std::string get_description(ConstTableRef target_table) const;
    IncludeDescriptor compile_included_backlinks() const;
    void collect_dependencies(const Table* table);
    void get_versions(const Group* group, TableVersions& versions) const;

private:
    std::vector<std::unique_ptr<BaseDescriptor>> m_descriptors;
    std::vector<TableKey> m_dependencies;
};
}

#endif /* REALM_SORT_DESCRIPTOR_HPP */
