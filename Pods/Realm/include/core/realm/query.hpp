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

#ifndef REALM_QUERY_HPP
#define REALM_QUERY_HPP

#include <cstdint>
#include <cstdio>
#include <climits>
#include <algorithm>
#include <string>
#include <vector>

#define REALM_MULTITHREAD_QUERY 0

#if REALM_MULTITHREAD_QUERY
// FIXME: Use our C++ thread abstraction API since it provides a much
// higher level of encapsulation and safety.
#include <pthread.h>
#endif

#include <realm/views.hpp>
#include <realm/table_ref.hpp>
#include <realm/binary_data.hpp>
#include <realm/olddatetime.hpp>
#include <realm/handover_defs.hpp>
#include <realm/link_view_fwd.hpp>
#include <realm/descriptor_fwd.hpp>
#include <realm/row.hpp>

namespace realm {


// Pre-declarations
class ParentNode;
class Table;
class TableView;
class TableViewBase;
class ConstTableView;
class Array;
class Expression;
class SequentialGetterBase;
class Group;

struct QueryGroup {
    enum class State {
        Default,
        OrCondition,
        OrConditionChildren,
    };

    QueryGroup() = default;

    QueryGroup(const QueryGroup&);
    QueryGroup& operator=(const QueryGroup&);

    QueryGroup(QueryGroup&&) = default;
    QueryGroup& operator=(QueryGroup&&) = default;

    QueryGroup(const QueryGroup&, QueryNodeHandoverPatches&);

    std::unique_ptr<ParentNode> m_root_node;

    bool m_pending_not = false;
    size_t m_subtable_column = not_found;
    State m_state = State::Default;
};

class Query final {
public:
    Query(const Table& table, TableViewBase* tv = nullptr);
    Query(const Table& table, std::unique_ptr<TableViewBase>);
    Query(const Table& table, const LinkViewRef& lv);
    Query();
    Query(std::unique_ptr<Expression>);
    ~Query() noexcept;

    Query(const Query& copy);
    Query& operator=(const Query& source);

    Query(Query&&);
    Query& operator=(Query&&);

    // Find links that point to a specific target row
    Query& links_to(size_t column_ndx, const ConstRow& target_row);

    // Conditions: null
    Query& equal(size_t column_ndx, null);
    Query& not_equal(size_t column_ndx, null);

    // Conditions: int64_t
    Query& equal(size_t column_ndx, int64_t value);
    Query& not_equal(size_t column_ndx, int64_t value);
    Query& greater(size_t column_ndx, int64_t value);
    Query& greater_equal(size_t column_ndx, int64_t value);
    Query& less(size_t column_ndx, int64_t value);
    Query& less_equal(size_t column_ndx, int64_t value);
    Query& between(size_t column_ndx, int64_t from, int64_t to);

    // Conditions: int (we need those because conversion from '1234' is ambiguous with float/double)
    Query& equal(size_t column_ndx, int value);
    Query& not_equal(size_t column_ndx, int value);
    Query& greater(size_t column_ndx, int value);
    Query& greater_equal(size_t column_ndx, int value);
    Query& less(size_t column_ndx, int value);
    Query& less_equal(size_t column_ndx, int value);
    Query& between(size_t column_ndx, int from, int to);

    // Conditions: 2 int columns
    Query& equal_int(size_t column_ndx1, size_t column_ndx2);
    Query& not_equal_int(size_t column_ndx1, size_t column_ndx2);
    Query& greater_int(size_t column_ndx1, size_t column_ndx2);
    Query& less_int(size_t column_ndx1, size_t column_ndx2);
    Query& greater_equal_int(size_t column_ndx1, size_t column_ndx2);
    Query& less_equal_int(size_t column_ndx1, size_t column_ndx2);

    // Conditions: float
    Query& equal(size_t column_ndx, float value);
    Query& not_equal(size_t column_ndx, float value);
    Query& greater(size_t column_ndx, float value);
    Query& greater_equal(size_t column_ndx, float value);
    Query& less(size_t column_ndx, float value);
    Query& less_equal(size_t column_ndx, float value);
    Query& between(size_t column_ndx, float from, float to);

    // Conditions: 2 float columns
    Query& equal_float(size_t column_ndx1, size_t column_ndx2);
    Query& not_equal_float(size_t column_ndx1, size_t column_ndx2);
    Query& greater_float(size_t column_ndx1, size_t column_ndx2);
    Query& greater_equal_float(size_t column_ndx1, size_t column_ndx2);
    Query& less_float(size_t column_ndx1, size_t column_ndx2);
    Query& less_equal_float(size_t column_ndx1, size_t column_ndx2);

    // Conditions: double
    Query& equal(size_t column_ndx, double value);
    Query& not_equal(size_t column_ndx, double value);
    Query& greater(size_t column_ndx, double value);
    Query& greater_equal(size_t column_ndx, double value);
    Query& less(size_t column_ndx, double value);
    Query& less_equal(size_t column_ndx, double value);
    Query& between(size_t column_ndx, double from, double to);

    // Conditions: 2 double columns
    Query& equal_double(size_t column_ndx1, size_t column_ndx2);
    Query& not_equal_double(size_t column_ndx1, size_t column_ndx2);
    Query& greater_double(size_t column_ndx1, size_t column_ndx2);
    Query& greater_equal_double(size_t column_ndx1, size_t column_ndx2);
    Query& less_double(size_t column_ndx1, size_t column_ndx2);
    Query& less_equal_double(size_t column_ndx1, size_t column_ndx2);

    // Conditions: timestamp
    Query& equal(size_t column_ndx, Timestamp value);
    Query& not_equal(size_t column_ndx, Timestamp value);
    Query& greater(size_t column_ndx, Timestamp value);
    Query& greater_equal(size_t column_ndx, Timestamp value);
    Query& less_equal(size_t column_ndx, Timestamp value);
    Query& less(size_t column_ndx, Timestamp value);

    // Conditions: bool
    Query& equal(size_t column_ndx, bool value);

    // Conditions: date
    Query& equal_olddatetime(size_t column_ndx, OldDateTime value)
    {
        return equal(column_ndx, int64_t(value.get_olddatetime()));
    }
    Query& not_equal_olddatetime(size_t column_ndx, OldDateTime value)
    {
        return not_equal(column_ndx, int64_t(value.get_olddatetime()));
    }
    Query& greater_olddatetime(size_t column_ndx, OldDateTime value)
    {
        return greater(column_ndx, int64_t(value.get_olddatetime()));
    }
    Query& greater_equal_olddatetime(size_t column_ndx, OldDateTime value)
    {
        return greater_equal(column_ndx, int64_t(value.get_olddatetime()));
    }
    Query& less_olddatetime(size_t column_ndx, OldDateTime value)
    {
        return less(column_ndx, int64_t(value.get_olddatetime()));
    }
    Query& less_equal_olddatetime(size_t column_ndx, OldDateTime value)
    {
        return less_equal(column_ndx, int64_t(value.get_olddatetime()));
    }
    Query& between_olddatetime(size_t column_ndx, OldDateTime from, OldDateTime to)
    {
        return between(column_ndx, int64_t(from.get_olddatetime()), int64_t(to.get_olddatetime()));
    }

    // Conditions: strings
    Query& equal(size_t column_ndx, StringData value, bool case_sensitive = true);
    Query& not_equal(size_t column_ndx, StringData value, bool case_sensitive = true);
    Query& begins_with(size_t column_ndx, StringData value, bool case_sensitive = true);
    Query& ends_with(size_t column_ndx, StringData value, bool case_sensitive = true);
    Query& contains(size_t column_ndx, StringData value, bool case_sensitive = true);
    Query& like(size_t column_ndx, StringData value, bool case_sensitive = true);

    // These are shortcuts for equal(StringData(c_str)) and
    // not_equal(StringData(c_str)), and are needed to avoid unwanted
    // implicit conversion of char* to bool.
    Query& equal(size_t column_ndx, const char* c_str, bool case_sensitive = true);
    Query& not_equal(size_t column_ndx, const char* c_str, bool case_sensitive = true);

    // Conditions: binary data
    Query& equal(size_t column_ndx, BinaryData value);
    Query& not_equal(size_t column_ndx, BinaryData value);
    Query& begins_with(size_t column_ndx, BinaryData value);
    Query& ends_with(size_t column_ndx, BinaryData value);
    Query& contains(size_t column_ndx, BinaryData value);

    // Negation
    Query& Not();

    // Grouping
    Query& group();
    Query& end_group();
    Query& subtable(size_t column);
    Query& end_subtable();
    Query& Or();

    Query& and_query(const Query& q);
    Query& and_query(Query&& q);
    Query operator||(const Query& q);
    Query operator&&(const Query& q);
    Query operator!();


    // Searching
    size_t find(size_t begin_at_table_row = size_t(0));
    TableView find_all(size_t start = 0, size_t end = size_t(-1), size_t limit = size_t(-1));
    ConstTableView find_all(size_t start = 0, size_t end = size_t(-1), size_t limit = size_t(-1)) const;

    // Aggregates
    size_t count(size_t start = 0, size_t end = size_t(-1), size_t limit = size_t(-1)) const;

    int64_t sum_int(size_t column_ndx, size_t* resultcount = nullptr, size_t start = 0, size_t end = size_t(-1),
                    size_t limit = size_t(-1)) const;

    double average_int(size_t column_ndx, size_t* resultcount = nullptr, size_t start = 0, size_t end = size_t(-1),
                       size_t limit = size_t(-1)) const;

    int64_t maximum_int(size_t column_ndx, size_t* resultcount = nullptr, size_t start = 0, size_t end = size_t(-1),
                        size_t limit = size_t(-1), size_t* return_ndx = nullptr) const;

    int64_t minimum_int(size_t column_ndx, size_t* resultcount = nullptr, size_t start = 0, size_t end = size_t(-1),
                        size_t limit = size_t(-1), size_t* return_ndx = nullptr) const;

    double sum_float(size_t column_ndx, size_t* resultcount = nullptr, size_t start = 0, size_t end = size_t(-1),
                     size_t limit = size_t(-1)) const;

    double average_float(size_t column_ndx, size_t* resultcount = nullptr, size_t start = 0, size_t end = size_t(-1),
                         size_t limit = size_t(-1)) const;

    float maximum_float(size_t column_ndx, size_t* resultcount = nullptr, size_t start = 0, size_t end = size_t(-1),
                        size_t limit = size_t(-1), size_t* return_ndx = nullptr) const;

    float minimum_float(size_t column_ndx, size_t* resultcount = nullptr, size_t start = 0, size_t end = size_t(-1),
                        size_t limit = size_t(-1), size_t* return_ndx = nullptr) const;

    double sum_double(size_t column_ndx, size_t* resultcount = nullptr, size_t start = 0, size_t end = size_t(-1),
                      size_t limit = size_t(-1)) const;

    double average_double(size_t column_ndx, size_t* resultcount = nullptr, size_t start = 0, size_t end = size_t(-1),
                          size_t limit = size_t(-1)) const;

    double maximum_double(size_t column_ndx, size_t* resultcount = nullptr, size_t start = 0, size_t end = size_t(-1),
                          size_t limit = size_t(-1), size_t* return_ndx = nullptr) const;

    double minimum_double(size_t column_ndx, size_t* resultcount = nullptr, size_t start = 0, size_t end = size_t(-1),
                          size_t limit = size_t(-1), size_t* return_ndx = nullptr) const;

    OldDateTime maximum_olddatetime(size_t column_ndx, size_t* resultcount = nullptr, size_t start = 0,
                                    size_t end = size_t(-1), size_t limit = size_t(-1),
                                    size_t* return_ndx = nullptr) const;

    OldDateTime minimum_olddatetime(size_t column_ndx, size_t* resultcount = nullptr, size_t start = 0,
                                    size_t end = size_t(-1), size_t limit = size_t(-1),
                                    size_t* return_ndx = nullptr) const;

    Timestamp maximum_timestamp(size_t column_ndx, size_t* return_ndx, size_t start = 0, size_t end = size_t(-1),
                                size_t limit = size_t(-1));

    Timestamp minimum_timestamp(size_t column_ndx, size_t* return_ndx, size_t start = 0, size_t end = size_t(-1),
                                size_t limit = size_t(-1));

    // Deletion
    size_t remove();

#if REALM_MULTITHREAD_QUERY
    // Multi-threading
    TableView find_all_multi(size_t start = 0, size_t end = size_t(-1));
    ConstTableView find_all_multi(size_t start = 0, size_t end = size_t(-1)) const;
    int set_threads(unsigned int threadcount);
#endif

    const TableRef& get_table()
    {
        return m_table;
    }

    // True if matching rows are guaranteed to be returned in table order.
    bool produces_results_in_table_order() const
    {
        return !m_view;
    }

    // Calls sync_if_needed on the restricting view, if present.
    // Returns the current version of the table(s) this query depends on,
    // or util::none if the query is not associated with a table.
    util::Optional<uint_fast64_t> sync_view_if_needed() const;

    std::string validate();

private:
    Query(Table& table, TableViewBase* tv = nullptr);
    void create();

    void init() const;
    size_t find_internal(size_t start = 0, size_t end = size_t(-1)) const;
    size_t peek_tablerow(size_t row) const;
    void handle_pending_not();
    void set_table(TableRef tr);

    static bool comp(const std::pair<size_t, size_t>& a, const std::pair<size_t, size_t>& b);

public:
    using HandoverPatch = QueryHandoverPatch;

    std::unique_ptr<Query> clone_for_handover(std::unique_ptr<HandoverPatch>& patch, ConstSourcePayload mode) const
    {
        patch.reset(new HandoverPatch);
        return std::make_unique<Query>(*this, *patch, mode);
    }

    std::unique_ptr<Query> clone_for_handover(std::unique_ptr<HandoverPatch>& patch, MutableSourcePayload mode)
    {
        patch.reset(new HandoverPatch);
        return std::make_unique<Query>(*this, *patch, mode);
    }

    void apply_and_consume_patch(std::unique_ptr<HandoverPatch>& patch, Group& dest_group)
    {
        apply_patch(*patch, dest_group);
        patch.reset();
    }

    void apply_patch(HandoverPatch& patch, Group& dest_group);
    Query(const Query& source, HandoverPatch& patch, ConstSourcePayload mode);
    Query(Query& source, HandoverPatch& patch, MutableSourcePayload mode);

private:
    void fetch_descriptor();

    void add_expression_node(std::unique_ptr<Expression>);

    template <class ColumnType>
    Query& equal(size_t column_ndx1, size_t column_ndx2);

    template <class ColumnType>
    Query& less(size_t column_ndx1, size_t column_ndx2);

    template <class ColumnType>
    Query& less_equal(size_t column_ndx1, size_t column_ndx2);

    template <class ColumnType>
    Query& greater(size_t column_ndx1, size_t column_ndx2);

    template <class ColumnType>
    Query& greater_equal(size_t column_ndx1, size_t column_ndx2);

    template <class ColumnType>
    Query& not_equal(size_t column_ndx1, size_t column_ndx2);

    template <typename TConditionFunction, class T>
    Query& add_condition(size_t column_ndx, T value);

    template <typename T, bool Nullable>
    double average(size_t column_ndx, size_t* resultcount = nullptr, size_t start = 0, size_t end = size_t(-1),
                   size_t limit = size_t(-1)) const;

    template <Action action, typename T, typename R, class ColClass>
    R aggregate(R (ColClass::*method)(size_t, size_t, size_t, size_t*) const, size_t column_ndx, size_t* resultcount,
                size_t start, size_t end, size_t limit, size_t* return_ndx = nullptr) const;

    void aggregate_internal(Action TAction, DataType TSourceColumn, bool nullable, ParentNode* pn, QueryStateBase* st,
                            size_t start, size_t end, SequentialGetterBase* source_column) const;

    void find_all(TableViewBase& tv, size_t start = 0, size_t end = size_t(-1), size_t limit = size_t(-1)) const;
    void delete_nodes() noexcept;

    bool has_conditions() const
    {
        return m_groups.size() > 0 && m_groups[0].m_root_node;
    }
    ParentNode* root_node() const
    {
        REALM_ASSERT(m_groups.size());
        return m_groups[0].m_root_node.get();
    }

    void add_node(std::unique_ptr<ParentNode>);

    friend class Table;
    friend class TableViewBase;

    std::string error_code;

    std::vector<QueryGroup> m_groups;

    // Used to access schema while building query:
    std::vector<size_t> m_subtable_path;

    ConstDescriptorRef m_current_descriptor;
    TableRef m_table;

    // points to the base class of the restricting view. If the restricting
    // view is a link view, m_source_link_view is non-zero. If it is a table view,
    // m_source_table_view is non-zero.
    RowIndexes* m_view = nullptr;

    // At most one of these can be non-zero, and if so the non-zero one indicates the restricting view.
    LinkViewRef m_source_link_view;               // link views are refcounted and shared.
    TableViewBase* m_source_table_view = nullptr; // table views are not refcounted, and not owned by the query.
    std::unique_ptr<TableViewBase> m_owned_source_table_view; // <--- except when indicated here
};

// Implementation:

inline Query& Query::equal(size_t column_ndx, const char* c_str, bool case_sensitive)
{
    return equal(column_ndx, StringData(c_str), case_sensitive);
}

inline Query& Query::not_equal(size_t column_ndx, const char* c_str, bool case_sensitive)
{
    return not_equal(column_ndx, StringData(c_str), case_sensitive);
}

} // namespace realm

#endif // REALM_QUERY_HPP
