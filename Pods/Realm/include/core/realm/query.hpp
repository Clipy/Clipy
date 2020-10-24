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

#include <realm/obj_list.hpp>
#include <realm/table_ref.hpp>
#include <realm/binary_data.hpp>
#include <realm/timestamp.hpp>
#include <realm/handover_defs.hpp>
#include <realm/util/serializer.hpp>

namespace realm {


// Pre-declarations
class ParentNode;
class Table;
class TableView;
class ConstTableView;
class Array;
class Expression;
class Group;
class Transaction;

namespace metrics {
class QueryInfo;
}

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

    std::unique_ptr<ParentNode> m_root_node;

    bool m_pending_not = false;
    State m_state = State::Default;
};

class Query final {
public:
    Query(ConstTableRef table, ConstTableView* tv = nullptr);
    Query(ConstTableRef table, std::unique_ptr<ConstTableView>);
    Query(ConstTableRef table, const LnkLst& list);
    Query(ConstTableRef table, LnkLstPtr&& list);
    Query();
    Query(std::unique_ptr<Expression>);
    ~Query() noexcept;

    Query(const Query& copy);
    Query& operator=(const Query& source);

    Query(Query&&);
    Query& operator=(Query&&);

    // Find links that point to a specific target row
    Query& links_to(ColKey column_key, ObjKey target_key);
    // Find links that point to specific target objects
    Query& links_to(ColKey column_key, const std::vector<ObjKey>& target_obj);

    // Conditions: null
    Query& equal(ColKey column_key, null);
    Query& not_equal(ColKey column_key, null);

    // Conditions: int64_t
    Query& equal(ColKey column_key, int64_t value);
    Query& not_equal(ColKey column_key, int64_t value);
    Query& greater(ColKey column_key, int64_t value);
    Query& greater_equal(ColKey column_key, int64_t value);
    Query& less(ColKey column_key, int64_t value);
    Query& less_equal(ColKey column_key, int64_t value);
    Query& between(ColKey column_key, int64_t from, int64_t to);

    // Conditions: int (we need those because conversion from '1234' is ambiguous with float/double)
    Query& equal(ColKey column_key, int value);
    Query& not_equal(ColKey column_key, int value);
    Query& greater(ColKey column_key, int value);
    Query& greater_equal(ColKey column_key, int value);
    Query& less(ColKey column_key, int value);
    Query& less_equal(ColKey column_key, int value);
    Query& between(ColKey column_key, int from, int to);

    // Conditions: 2 int columns
    Query& equal_int(ColKey column_key1, ColKey column_key2);
    Query& not_equal_int(ColKey column_key1, ColKey column_key2);
    Query& greater_int(ColKey column_key1, ColKey column_key2);
    Query& less_int(ColKey column_key1, ColKey column_key2);
    Query& greater_equal_int(ColKey column_key1, ColKey column_key2);
    Query& less_equal_int(ColKey column_key1, ColKey column_key2);

    // Conditions: float
    Query& equal(ColKey column_key, float value);
    Query& not_equal(ColKey column_key, float value);
    Query& greater(ColKey column_key, float value);
    Query& greater_equal(ColKey column_key, float value);
    Query& less(ColKey column_key, float value);
    Query& less_equal(ColKey column_key, float value);
    Query& between(ColKey column_key, float from, float to);

    // Conditions: 2 float columns
    Query& equal_float(ColKey column_key1, ColKey column_key2);
    Query& not_equal_float(ColKey column_key1, ColKey column_key2);
    Query& greater_float(ColKey column_key1, ColKey column_key2);
    Query& greater_equal_float(ColKey column_key1, ColKey column_key2);
    Query& less_float(ColKey column_key1, ColKey column_key2);
    Query& less_equal_float(ColKey column_key1, ColKey column_key2);

    // Conditions: double
    Query& equal(ColKey column_key, double value);
    Query& not_equal(ColKey column_key, double value);
    Query& greater(ColKey column_key, double value);
    Query& greater_equal(ColKey column_key, double value);
    Query& less(ColKey column_key, double value);
    Query& less_equal(ColKey column_key, double value);
    Query& between(ColKey column_key, double from, double to);

    // Conditions: 2 double columns
    Query& equal_double(ColKey column_key1, ColKey column_key2);
    Query& not_equal_double(ColKey column_key1, ColKey column_key2);
    Query& greater_double(ColKey column_key1, ColKey column_key2);
    Query& greater_equal_double(ColKey column_key1, ColKey column_key2);
    Query& less_double(ColKey column_key1, ColKey column_key2);
    Query& less_equal_double(ColKey column_key1, ColKey column_key2);

    // Conditions: timestamp
    Query& equal(ColKey column_key, Timestamp value);
    Query& not_equal(ColKey column_key, Timestamp value);
    Query& greater(ColKey column_key, Timestamp value);
    Query& greater_equal(ColKey column_key, Timestamp value);
    Query& less_equal(ColKey column_key, Timestamp value);
    Query& less(ColKey column_key, Timestamp value);

    // Conditions: size
    Query& size_equal(ColKey column_key, int64_t value);
    Query& size_not_equal(ColKey column_key, int64_t value);
    Query& size_greater(ColKey column_key, int64_t value);
    Query& size_greater_equal(ColKey column_key, int64_t value);
    Query& size_less_equal(ColKey column_key, int64_t value);
    Query& size_less(ColKey column_key, int64_t value);
    Query& size_between(ColKey column_key, int64_t from, int64_t to);

    // Conditions: bool
    Query& equal(ColKey column_key, bool value);
    Query& not_equal(ColKey column_key, bool value);

    // Conditions: strings
    Query& equal(ColKey column_key, StringData value, bool case_sensitive = true);
    Query& not_equal(ColKey column_key, StringData value, bool case_sensitive = true);
    Query& begins_with(ColKey column_key, StringData value, bool case_sensitive = true);
    Query& ends_with(ColKey column_key, StringData value, bool case_sensitive = true);
    Query& contains(ColKey column_key, StringData value, bool case_sensitive = true);
    Query& like(ColKey column_key, StringData value, bool case_sensitive = true);

    // These are shortcuts for equal(StringData(c_str)) and
    // not_equal(StringData(c_str)), and are needed to avoid unwanted
    // implicit conversion of char* to bool.
    Query& equal(ColKey column_key, const char* c_str, bool case_sensitive = true);
    Query& not_equal(ColKey column_key, const char* c_str, bool case_sensitive = true);

    // Conditions: binary data
    Query& equal(ColKey column_key, BinaryData value, bool case_sensitive = true);
    Query& not_equal(ColKey column_key, BinaryData value, bool case_sensitive = true);
    Query& begins_with(ColKey column_key, BinaryData value, bool case_sensitive = true);
    Query& ends_with(ColKey column_key, BinaryData value, bool case_sensitive = true);
    Query& contains(ColKey column_key, BinaryData value, bool case_sensitive = true);
    Query& like(ColKey column_key, BinaryData b, bool case_sensitive = true);

    // Negation
    Query& Not();

    // Grouping
    Query& group();
    Query& end_group();
    Query& Or();

    Query& and_query(const Query& q);
    Query& and_query(Query&& q);
    Query operator||(const Query& q);
    Query operator&&(const Query& q);
    Query operator!();


    // Searching
    ObjKey find();
    TableView find_all(size_t start = 0, size_t end = size_t(-1), size_t limit = size_t(-1));

    // Aggregates
    size_t count() const;
    TableView find_all(const DescriptorOrdering& descriptor);
    size_t count(const DescriptorOrdering& descriptor);
    int64_t sum_int(ColKey column_key) const;
    double average_int(ColKey column_key, size_t* resultcount = nullptr) const;
    int64_t maximum_int(ColKey column_key, ObjKey* return_ndx = nullptr) const;
    int64_t minimum_int(ColKey column_key, ObjKey* return_ndx = nullptr) const;
    double sum_float(ColKey column_key) const;
    double average_float(ColKey column_key, size_t* resultcount = nullptr) const;
    float maximum_float(ColKey column_key, ObjKey* return_ndx = nullptr) const;
    float minimum_float(ColKey column_key, ObjKey* return_ndx = nullptr) const;
    double sum_double(ColKey column_key) const;
    double average_double(ColKey column_key, size_t* resultcount = nullptr) const;
    double maximum_double(ColKey column_key, ObjKey* return_ndx = nullptr) const;
    double minimum_double(ColKey column_key, ObjKey* return_ndx = nullptr) const;
    Timestamp maximum_timestamp(ColKey column_key, ObjKey* return_ndx = nullptr);
    Timestamp minimum_timestamp(ColKey column_key, ObjKey* return_ndx = nullptr);

    // Deletion
    size_t remove();

#if REALM_MULTITHREAD_QUERY
    // Multi-threading
    TableView find_all_multi(size_t start = 0, size_t end = size_t(-1));
    ConstTableView find_all_multi(size_t start = 0, size_t end = size_t(-1)) const;
    int set_threads(unsigned int threadcount);
#endif

    ConstTableRef& get_table()
    {
        return m_table;
    }

    void get_outside_versions(TableVersions&) const;

    // True if matching rows are guaranteed to be returned in table order.
    bool produces_results_in_table_order() const
    {
        return !m_view;
    }

    // Calls sync_if_needed on the restricting view, if present.
    // Returns the current version of the table(s) this query depends on,
    // or empty vector if the query is not associated with a table.
    TableVersions sync_view_if_needed() const;

    std::string validate();

    std::string get_description() const;
    std::string get_description(util::serializer::SerialisationState& state) const;

    bool eval_object(ConstObj& obj) const;

private:
    void create();

    void init() const;
    size_t find_internal(size_t start = 0, size_t end = size_t(-1)) const;
    void handle_pending_not();
    void set_table(TableRef tr);
public:
    std::unique_ptr<Query> clone_for_handover(Transaction* tr, PayloadPolicy policy) const
    {
        return std::make_unique<Query>(this, tr, policy);
    }

    Query(const Query* source, Transaction* tr, PayloadPolicy policy);
    Query(const Query& source, Transaction* tr, PayloadPolicy policy)
        : Query(&source, tr, policy)
    {
    }

private:
    void add_expression_node(std::unique_ptr<Expression>);

    template <class ColumnType>
    Query& equal(ColKey column_key1, ColKey column_key2);

    template <class ColumnType>
    Query& less(ColKey column_key1, ColKey column_key2);

    template <class ColumnType>
    Query& less_equal(ColKey column_key1, ColKey column_key2);

    template <class ColumnType>
    Query& greater(ColKey column_key1, ColKey column_key2);

    template <class ColumnType>
    Query& greater_equal(ColKey column_key1, ColKey column_key2);

    template <class ColumnType>
    Query& not_equal(ColKey column_key1, ColKey column_key2);

    template <typename TConditionFunction, class T>
    Query& add_condition(ColKey column_key, T value);

    template <typename TConditionFunction>
    Query& add_size_condition(ColKey column_key, int64_t value);

    template <typename T, bool Nullable>
    double average(ColKey column_key, size_t* resultcount = nullptr) const;

    template <Action action, typename T, typename R>
    R aggregate(ColKey column_key, size_t* resultcount = nullptr, ObjKey* return_ndx = nullptr) const;

    size_t find_best_node(ParentNode* pn) const;
    void aggregate_internal(ParentNode* pn, QueryStateBase* st, size_t start, size_t end,
                            ArrayPayload* source_column) const;

    void find_all(ConstTableView& tv, size_t start = 0, size_t end = size_t(-1), size_t limit = size_t(-1)) const;
    size_t do_count(size_t limit = size_t(-1)) const;
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
    friend class ConstTableView;
    friend class SubQueryCount;
    friend class metrics::QueryInfo;

    std::string error_code;

    std::vector<QueryGroup> m_groups;
    mutable std::vector<TableKey> m_table_keys;

    TableRef m_table;

    // points to the base class of the restricting view. If the restricting
    // view is a link view, m_source_link_list is non-zero. If it is a table view,
    // m_source_table_view is non-zero.
    ObjList* m_view = nullptr;

    // At most one of these can be non-zero, and if so the non-zero one indicates the restricting view.
    LnkLstPtr m_source_link_list;                  // link lists are owned by the query.
    ConstTableView* m_source_table_view = nullptr; // table views are not refcounted, and not owned by the query.
    std::unique_ptr<ConstTableView> m_owned_source_table_view; // <--- except when indicated here
};

// Implementation:

inline Query& Query::equal(ColKey column_key, const char* c_str, bool case_sensitive)
{
    return equal(column_key, StringData(c_str), case_sensitive);
}

inline Query& Query::not_equal(ColKey column_key, const char* c_str, bool case_sensitive)
{
    return not_equal(column_key, StringData(c_str), case_sensitive);
}

} // namespace realm

#endif // REALM_QUERY_HPP
