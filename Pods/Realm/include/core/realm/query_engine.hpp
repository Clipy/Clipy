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

/*
A query consists of node objects, one for each query condition. Each node contains pointers to all other nodes:

node1        node2         node3
------       -----         -----
node2*       node1*        node1*
node3*       node3*        node2*

The construction of all this takes part in query.cpp. Each node has two important functions:

    aggregate(start, end)
    aggregate_local(start, end)

The aggregate() function executes the aggregate of a query. You can call the method on any of the nodes
(except children nodes of OrNode and SubtableNode) - it has the same behaviour. The function contains
scheduling that calls aggregate_local(start, end) on different nodes with different start/end ranges,
depending on what it finds is most optimal.

The aggregate_local() function contains a tight loop that tests the condition of its own node, and upon match
it tests all other conditions at that index to report a full match or not. It will remain in the tight loop
after a full match.

So a call stack with 2 and 9 being local matches of a node could look like this:

aggregate(0, 10)
    node1->aggregate_local(0, 3)
        node2->find_first_local(2, 3)
        node3->find_first_local(2, 3)
    node3->aggregate_local(3, 10)
        node1->find_first_local(4, 5)
        node2->find_first_local(4, 5)
        node1->find_first_local(7, 8)
        node2->find_first_local(7, 8)

find_first_local(n, n + 1) is a function that can be used to test a single row of another condition. Note that
this is very simplified. There are other statistical arguments to the methods, and also, find_first_local() can be
called from a callback function called by an integer Array.


Template arguments in methods:
----------------------------------------------------------------------------------------------------

TConditionFunction: Each node has a condition from query_conditions.c such as Equal, GreaterEqual, etc

TConditionValue:    Type of values in condition column. That is, int64_t, float, int, bool, etc

TAction:            What to do with each search result, from the enums act_ReturnFirst, act_Count, act_Sum, etc

TResult:            Type of result of actions - float, double, int64_t, etc. Special notes: For act_Count it's
                    int64_t, for RLM_FIND_ALL it's int64_t which points at destination array.

TSourceColumn:      Type of source column used in actions, or *ignored* if no source column is used (like for
                    act_Count, act_ReturnFirst)


There are two important classes used in queries:
----------------------------------------------------------------------------------------------------
SequentialGetter    Column iterator used to get successive values with leaf caching. Used both for condition columns
                    and aggregate source column

AggregateState      State of the aggregate - contains a state variable that stores intermediate sum, max, min,
                    etc, etc.

*/

#ifndef REALM_QUERY_ENGINE_HPP
#define REALM_QUERY_ENGINE_HPP

#include <algorithm>
#include <functional>
#include <string>
#include <array>

#include <realm/util/meta.hpp>
#include <realm/util/miscellaneous.hpp>
#include <realm/util/shared_ptr.hpp>
#include <realm/utilities.hpp>
#include <realm/array_basic.hpp>
#include <realm/array_string.hpp>
#include <realm/column_binary.hpp>
#include <realm/column_fwd.hpp>
#include <realm/column_link.hpp>
#include <realm/column_linklist.hpp>
#include <realm/column_mixed.hpp>
#include <realm/column_string.hpp>
#include <realm/column_string_enum.hpp>
#include <realm/column_table.hpp>
#include <realm/column_timestamp.hpp>
#include <realm/column_type_traits.hpp>
#include <realm/column_type_traits.hpp>
#include <realm/link_view.hpp>
#include <realm/query_conditions.hpp>
#include <realm/query_expression.hpp>
#include <realm/table.hpp>
#include <realm/table_view.hpp>
#include <realm/unicode.hpp>

#include <map>

#if defined(_MSC_FULL_VER) && _MSC_FULL_VER >= 160040219
#include <immintrin.h>
#endif

namespace realm {

// Number of matches to find in best condition loop before breaking out to probe other conditions. Too low value gives
// too many constant time overheads everywhere in the query engine. Too high value makes it adapt less rapidly to
// changes in match frequencies.
const size_t findlocals = 64;

// Average match distance in linear searches where further increase in distance no longer increases query speed
// (because time spent on handling each match becomes insignificant compared to time spent on the search).
const size_t bestdist = 512;

// Minimum number of matches required in a certain condition before it can be used to compute statistics. Too high
// value can spent too much time in a bad node (with high match frequency). Too low value gives inaccurate statistics.
const size_t probe_matches = 4;

const size_t bitwidth_time_unit = 64;

typedef bool (*CallbackDummy)(int64_t);


class ParentNode {
    typedef ParentNode ThisType;

public:
    ParentNode() = default;
    virtual ~ParentNode() = default;

    void gather_children(std::vector<ParentNode*>& v)
    {
        m_children.clear();
        size_t i = v.size();
        v.push_back(this);

        if (m_child)
            m_child->gather_children(v);

        m_children = v;
        m_children.erase(m_children.begin() + i);
        m_children.insert(m_children.begin(), this);
    }

    double cost() const
    {
        return 8 * bitwidth_time_unit / m_dD +
               m_dT; // dt = 1/64 to 1. Match dist is 8 times more important than bitwidth
    }

    size_t find_first(size_t start, size_t end);

    virtual void init()
    {
        if (m_child)
            m_child->init();
        m_column_action_specializer = nullptr;
    }

    void set_table(const Table& table)
    {
        if (&table == m_table)
            return;

        m_table.reset(&table);
        if (m_child)
            m_child->set_table(table);
        table_changed();
    }

    virtual size_t find_first_local(size_t start, size_t end) = 0;

    virtual void aggregate_local_prepare(Action TAction, DataType col_id, bool nullable);

    template <Action TAction, class TSourceColumn>
    bool column_action_specialization(QueryStateBase* st, SequentialGetterBase* source_column, size_t r)
    {
        // TResult: type of query result
        // TSourceValue: type of aggregate source
        using TSourceValue = typename TSourceColumn::value_type;
        using TResult = typename ColumnTypeTraitsSum<TSourceValue, TAction>::sum_type;

        // Sum of float column must accumulate in double
        static_assert(!(TAction == act_Sum &&
                        (std::is_same<TSourceColumn, float>::value && !std::is_same<TResult, double>::value)),
                      "");

        TSourceValue av{};
        // uses_val test because compiler cannot see that IntegerColumn::get has no side effect and result is
        // discarded
        if (static_cast<QueryState<TResult>*>(st)->template uses_val<TAction>() && source_column != nullptr) {
            REALM_ASSERT_DEBUG(dynamic_cast<SequentialGetter<TSourceColumn>*>(source_column) != nullptr);
            av = static_cast<SequentialGetter<TSourceColumn>*>(source_column)->get_next(r);
        }
        REALM_ASSERT_DEBUG(dynamic_cast<QueryState<TResult>*>(st) != nullptr);
        bool cont = static_cast<QueryState<TResult>*>(st)->template match<TAction, 0>(r, 0, av);
        return cont;
    }

    virtual size_t aggregate_local(QueryStateBase* st, size_t start, size_t end, size_t local_limit,
                                   SequentialGetterBase* source_column);


    virtual std::string validate()
    {
        if (error_code != "")
            return error_code;
        if (m_child == nullptr)
            return "";
        else
            return m_child->validate();
    }

    ParentNode(const ParentNode& from)
        : ParentNode(from, nullptr)
    {
    }

    ParentNode(const ParentNode& from, QueryNodeHandoverPatches* patches)
        : m_child(from.m_child ? from.m_child->clone(patches) : nullptr)
        , m_condition_column_idx(from.m_condition_column_idx)
        , m_dD(from.m_dD)
        , m_dT(from.m_dT)
        , m_probes(from.m_probes)
        , m_matches(from.m_matches)
        , m_table(patches ? ConstTableRef{} : from.m_table)
    {
    }

    void add_child(std::unique_ptr<ParentNode> child)
    {
        if (m_child)
            m_child->add_child(std::move(child));
        else
            m_child = std::move(child);
    }

    virtual std::unique_ptr<ParentNode> clone(QueryNodeHandoverPatches* = nullptr) const = 0;

    virtual void apply_handover_patch(QueryNodeHandoverPatches& patches, Group& group)
    {
        if (m_child)
            m_child->apply_handover_patch(patches, group);
    }

    std::unique_ptr<ParentNode> m_child;
    std::vector<ParentNode*> m_children;
    size_t m_condition_column_idx = npos; // Column of search criteria

    double m_dD;       // Average row distance between each local match at current position
    double m_dT = 0.0; // Time overhead of testing index i + 1 if we have just tested index i. > 1 for linear scans, 0
    // for index/tableview

    size_t m_probes = 0;
    size_t m_matches = 0;

protected:
    typedef bool (ParentNode::*Column_action_specialized)(QueryStateBase*, SequentialGetterBase*, size_t);
    Column_action_specialized m_column_action_specializer;
    ConstTableRef m_table;
    std::string error_code;

    const ColumnBase& get_column_base(size_t ndx)
    {
        return m_table->get_column_base(ndx);
    }

    template <class ColType>
    const ColType& get_column(size_t ndx)
    {
        auto& col = m_table->get_column_base(ndx);
        REALM_ASSERT_DEBUG(dynamic_cast<const ColType*>(&col));
        return static_cast<const ColType&>(col);
    }

    ColumnType get_real_column_type(size_t ndx)
    {
        return m_table->get_real_column_type(ndx);
    }

    template <class ColType>
    void copy_getter(SequentialGetter<ColType>& dst, size_t& dst_idx, const SequentialGetter<ColType>& src,
                     const QueryNodeHandoverPatches* patches)
    {
        if (src.m_column) {
            if (patches)
                dst_idx = src.m_column->get_column_index();
            else
                dst.init(src.m_column);
        }
    }

private:
    virtual void table_changed() = 0;
};

// For conditions on a subtable (encapsulated in subtable()...end_subtable()). These return the parent row as match if
// and
// only if one or more subtable rows match the condition.
class SubtableNode : public ParentNode {
public:
    SubtableNode(size_t column, std::unique_ptr<ParentNode> condition)
        : m_condition(std::move(condition))
    {
        m_dT = 100.0;
        m_condition_column_idx = column;
    }

    void init() override
    {
        m_dD = 10.0;

        // m_condition is first node in condition of subtable query.
        if (m_condition) {
            // Can't call init() here as usual since the subtable can be degenerate
            // m_condition->init(table);
            std::vector<ParentNode*> v;
            m_condition->gather_children(v);
        }

        // m_child is next node of parent query
        if (m_child)
            m_child->init();
    }

    void table_changed() override
    {
        m_col_type = m_table->get_real_column_type(m_condition_column_idx);
        REALM_ASSERT(m_col_type == col_type_Table || m_col_type == col_type_Mixed);
        if (m_col_type == col_type_Table)
            m_column = &m_table->get_column_table(m_condition_column_idx);
        else // Mixed
            m_column = &m_table->get_column_mixed(m_condition_column_idx);
    }

    std::string validate() override
    {
        if (error_code != "")
            return error_code;
        if (m_condition == nullptr)
            return "Unbalanced subtable/end_subtable block";
        else
            return m_condition->validate();
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        REALM_ASSERT(m_table);
        REALM_ASSERT(m_condition);

        for (size_t s = start; s < end; ++s) {
            const Table* subtable;
            if (m_col_type == col_type_Table)
                subtable = static_cast<const SubtableColumn*>(m_column)->get_subtable_ptr(s);
            else {
                subtable = static_cast<const MixedColumn*>(m_column)->get_subtable_ptr(s);
                if (!subtable)
                    continue;
            }

            if (subtable->is_degenerate())
                return not_found;

            m_condition->set_table(*subtable);
            m_condition->init();
            const size_t subsize = subtable->size();
            const size_t sub = m_condition->find_first(0, subsize);

            if (sub != not_found)
                return s;
        }
        return not_found;
    }

    std::unique_ptr<ParentNode> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<ParentNode>(new SubtableNode(*this, patches));
    }

    SubtableNode(const SubtableNode& from, QueryNodeHandoverPatches* patches)
        : ParentNode(from, patches)
        , m_condition(from.m_condition ? from.m_condition->clone(patches) : nullptr)
        , m_column(from.m_column)
        , m_col_type(from.m_col_type)
    {
        if (m_column && patches)
            m_condition_column_idx = m_column->get_column_index();
    }

    void apply_handover_patch(QueryNodeHandoverPatches& patches, Group& group) override
    {
        m_condition->apply_handover_patch(patches, group);
        ParentNode::apply_handover_patch(patches, group);
    }

    std::unique_ptr<ParentNode> m_condition;
    const ColumnBase* m_column = nullptr;
    ColumnType m_col_type;
};

namespace _impl {

template <class ColType>
struct CostHeuristic;

template <>
struct CostHeuristic<IntegerColumn> {
    static constexpr double dD()
    {
        return 100.0;
    }
    static constexpr double dT()
    {
        return 1.0 / 4.0;
    }
};

template <>
struct CostHeuristic<IntNullColumn> {
    static constexpr double dD()
    {
        return 100.0;
    }
    static constexpr double dT()
    {
        return 1.0 / 4.0;
    }
};

// FIXME: Add AdaptiveStringColumn, BasicColumn, etc.
}

class ColumnNodeBase : public ParentNode {
protected:
    ColumnNodeBase(size_t column_idx)
    {
        m_condition_column_idx = column_idx;
    }

    ColumnNodeBase(const ColumnNodeBase& from, QueryNodeHandoverPatches* patches)
        : ParentNode(from, patches)
        , m_last_local_match(from.m_last_local_match)
        , m_local_matches(from.m_local_matches)
        , m_local_limit(from.m_local_limit)
        , m_fastmode_disabled(from.m_fastmode_disabled)
        , m_action(from.m_action)
        , m_state(from.m_state)
        , m_source_column(from.m_source_column)
    {
    }

    template <Action TAction, class ColType>
    bool match_callback(int64_t v)
    {
        using TSourceValue = typename ColType::value_type;
        using QueryStateType = typename ColumnTypeTraitsSum<TSourceValue, TAction>::sum_type;

        size_t i = to_size_t(v);
        m_last_local_match = i;
        m_local_matches++;

        auto state = static_cast<QueryState<QueryStateType>*>(m_state);
        auto source_column = static_cast<SequentialGetter<ColType>*>(m_source_column);

        // Test remaining sub conditions of this node. m_children[0] is the node that called match_callback(), so skip
        // it
        for (size_t c = 1; c < m_children.size(); c++) {
            m_children[c]->m_probes++;
            size_t m = m_children[c]->find_first_local(i, i + 1);
            if (m != i)
                return true;
        }

        bool b;
        if (state->template uses_val<TAction>()) { // Compiler cannot see that IntegerColumn::Get has no side effect
            // and result is discarded
            TSourceValue av = source_column->get_next(i);
            b = state->template match<TAction, false>(i, 0, av);
        }
        else {
            b = state->template match<TAction, false>(i, 0, TSourceValue{});
        }

        return b;
    }

    // Aggregate bookkeeping
    size_t m_last_local_match = npos;
    size_t m_local_matches = 0;
    size_t m_local_limit = 0;
    bool m_fastmode_disabled = false;
    Action m_action;
    QueryStateBase* m_state = nullptr;
    SequentialGetterBase* m_source_column =
        nullptr; // Column of values used in aggregate (act_FindAll, actReturnFirst, act_Sum, etc)
};

template <class ColType>
class IntegerNodeBase : public ColumnNodeBase {
    using ThisType = IntegerNodeBase<ColType>;

public:
    using TConditionValue = typename ColType::value_type;
    static const bool nullable = ColType::nullable;

    template <class TConditionFunction, Action TAction, DataType TDataType, bool Nullable>
    bool find_callback_specialization(size_t s, size_t end_in_leaf)
    {
        using AggregateColumnType = typename GetColumnType<TDataType, Nullable>::type;
        bool cont;
        size_t start_in_leaf = s - this->m_leaf_start;
        cont = this->m_leaf_ptr->template find<TConditionFunction, act_CallbackIdx>(
            m_value, start_in_leaf, end_in_leaf, this->m_leaf_start, nullptr,
            std::bind(std::mem_fn(&ThisType::template match_callback<TAction, AggregateColumnType>), this,
                      std::placeholders::_1));
        return cont;
    }

protected:
    using LeafType = typename ColType::LeafType;
    using LeafInfo = typename ColType::LeafInfo;

    size_t aggregate_local_impl(QueryStateBase* st, size_t start, size_t end, size_t local_limit,
                                SequentialGetterBase* source_column, int c)
    {
        REALM_ASSERT(m_children.size() > 0);
        m_local_matches = 0;
        m_local_limit = local_limit;
        m_last_local_match = start - 1;
        m_state = st;

        // If there are no other nodes than us (m_children.size() == 1) AND the column used for our condition is
        // the same as the column used for the aggregate action, then the entire query can run within scope of that
        // column only, with no references to other columns:
        bool fastmode = should_run_in_fastmode(source_column);
        for (size_t s = start; s < end;) {
            cache_leaf(s);

            size_t end_in_leaf;
            if (end > m_leaf_end)
                end_in_leaf = m_leaf_end - m_leaf_start;
            else
                end_in_leaf = end - m_leaf_start;

            if (fastmode) {
                bool cont;
                size_t start_in_leaf = s - m_leaf_start;
                cont = m_leaf_ptr->find(c, m_action, m_value, start_in_leaf, end_in_leaf, m_leaf_start,
                                        static_cast<QueryState<int64_t>*>(st));
                if (!cont)
                    return not_found;
            }
            // Else, for each match in this node, call our IntegerNodeBase::match_callback to test remaining nodes
            // and/or extract
            // aggregate payload from aggregate column:
            else {
                m_source_column = source_column;
                bool cont = (this->*m_find_callback_specialized)(s, end_in_leaf);
                if (!cont)
                    return not_found;
            }

            if (m_local_matches == m_local_limit)
                break;

            s = end_in_leaf + m_leaf_start;
        }

        if (m_local_matches == m_local_limit) {
            m_dD = (m_last_local_match + 1 - start) / (m_local_matches + 1.0);
            return m_last_local_match + 1;
        }
        else {
            m_dD = (end - start) / (m_local_matches + 1.0);
            return end;
        }
    }

    IntegerNodeBase(TConditionValue value, size_t column_idx)
        : ColumnNodeBase(column_idx)
        , m_value(std::move(value))
    {
    }

    IntegerNodeBase(const ThisType& from, QueryNodeHandoverPatches* patches)
        : ColumnNodeBase(from, patches)
        , m_value(from.m_value)
        , m_condition_column(from.m_condition_column)
        , m_find_callback_specialized(from.m_find_callback_specialized)
    {
        if (m_condition_column && patches)
            m_condition_column_idx = m_condition_column->get_column_index();
    }

    void table_changed() override
    {
        m_condition_column = &get_column<ColType>(m_condition_column_idx);
    }

    void init() override
    {
        ColumnNodeBase::init();

        m_dT = _impl::CostHeuristic<ColType>::dT();
        m_dD = _impl::CostHeuristic<ColType>::dD();

        // Clear leaf cache
        m_leaf_end = 0;
        m_array_ptr.reset(); // Explicitly destroy the old one first, because we're reusing the memory.
        m_array_ptr.reset(new (&m_leaf_cache_storage) LeafType(m_table->get_alloc()));

        if (m_child)
            m_child->init();
    }

    void get_leaf(const ColType& col, size_t ndx)
    {
        size_t ndx_in_leaf;
        LeafInfo leaf_info{&m_leaf_ptr, m_array_ptr.get()};
        col.get_leaf(ndx, ndx_in_leaf, leaf_info);
        m_leaf_start = ndx - ndx_in_leaf;
        m_leaf_end = m_leaf_start + m_leaf_ptr->size();
    }

    void cache_leaf(size_t s)
    {
        if (s >= m_leaf_end || s < m_leaf_start) {
            get_leaf(*m_condition_column, s);
            size_t w = m_leaf_ptr->get_width();
            m_dT = (w == 0 ? 1.0 / REALM_MAX_BPNODE_SIZE : w / float(bitwidth_time_unit));
        }
    }

    bool should_run_in_fastmode(SequentialGetterBase* source_column) const
    {
        return (m_children.size() == 1 &&
                (source_column == nullptr ||
                 (!m_fastmode_disabled &&
                  static_cast<SequentialGetter<ColType>*>(source_column)->m_column == m_condition_column)));
    }

    // Search value:
    TConditionValue m_value;

    // Column on which search criteria are applied
    const ColType* m_condition_column = nullptr;

    // Leaf cache
    using LeafCacheStorage = typename std::aligned_storage<sizeof(LeafType), alignof(LeafType)>::type;
    LeafCacheStorage m_leaf_cache_storage;
    std::unique_ptr<LeafType, PlacementDelete> m_array_ptr;
    const LeafType* m_leaf_ptr = nullptr;
    size_t m_leaf_start = npos;
    size_t m_leaf_end = 0;
    size_t m_local_end;

    // Aggregate optimization
    using TFind_callback_specialized = bool (ThisType::*)(size_t, size_t);
    TFind_callback_specialized m_find_callback_specialized = nullptr;
};

// FIXME: Add specialization that uses index for TConditionFunction = Equal
template <class ColType, class TConditionFunction>
class IntegerNode : public IntegerNodeBase<ColType> {
    using BaseType = IntegerNodeBase<ColType>;
    using ThisType = IntegerNode<ColType, TConditionFunction>;

public:
    static const bool special_null_node = false;
    using TConditionValue = typename BaseType::TConditionValue;

    IntegerNode(TConditionValue value, size_t column_ndx)
        : BaseType(value, column_ndx)
    {
    }
    IntegerNode(const IntegerNode& from, QueryNodeHandoverPatches* patches)
        : BaseType(from, patches)
    {
    }

    void aggregate_local_prepare(Action action, DataType col_id, bool nullable) override
    {
        this->m_fastmode_disabled = (col_id == type_Float || col_id == type_Double);
        this->m_action = action;
        this->m_find_callback_specialized = get_specialized_callback(action, col_id, nullable);
    }

    size_t aggregate_local(QueryStateBase* st, size_t start, size_t end, size_t local_limit,
                           SequentialGetterBase* source_column) override
    {
        constexpr int cond = TConditionFunction::condition;
        return this->aggregate_local_impl(st, start, end, local_limit, source_column, cond);
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        REALM_ASSERT(this->m_table);

        while (start < end) {

            // Cache internal leaves
            if (start >= this->m_leaf_end || start < this->m_leaf_start) {
                this->get_leaf(*this->m_condition_column, start);
            }

            // FIXME: Create a fast bypass when you just need to check 1 row, which is used alot from within core.
            // It should just call array::get and save the initial overhead of find_first() which has become quite
            // big. Do this when we have cleaned up core a bit more.

            size_t end2;
            if (end > this->m_leaf_end)
                end2 = this->m_leaf_end - this->m_leaf_start;
            else
                end2 = end - this->m_leaf_start;

            size_t s;
            s = this->m_leaf_ptr->template find_first<TConditionFunction>(this->m_value, start - this->m_leaf_start,
                                                                          end2);

            if (s == not_found) {
                start = this->m_leaf_end;
                continue;
            }
            else
                return s + this->m_leaf_start;
        }

        return not_found;
    }

    std::unique_ptr<ParentNode> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<ParentNode>(new IntegerNode<ColType, TConditionFunction>(*this, patches));
    }

protected:
    using TFind_callback_specialized = typename BaseType::TFind_callback_specialized;

    static TFind_callback_specialized get_specialized_callback(Action action, DataType col_id, bool nullable)
    {
        switch (action) {
            case act_Count:
                return get_specialized_callback_2_int<act_Count>(col_id, nullable);
            case act_Sum:
                return get_specialized_callback_2<act_Sum>(col_id, nullable);
            case act_Max:
                return get_specialized_callback_2<act_Max>(col_id, nullable);
            case act_Min:
                return get_specialized_callback_2<act_Min>(col_id, nullable);
            case act_FindAll:
                return get_specialized_callback_2_int<act_FindAll>(col_id, nullable);
            case act_CallbackIdx:
                return get_specialized_callback_2_int<act_CallbackIdx>(col_id, nullable);
            default:
                break;
        }
        REALM_ASSERT(false); // Invalid aggregate function
        return nullptr;
    }

    template <Action TAction>
    static TFind_callback_specialized get_specialized_callback_2(DataType col_id, bool nullable)
    {
        switch (col_id) {
            case type_Int:
                return get_specialized_callback_3<TAction, type_Int>(nullable);
            case type_Float:
                return get_specialized_callback_3<TAction, type_Float>(nullable);
            case type_Double:
                return get_specialized_callback_3<TAction, type_Double>(nullable);
            default:
                break;
        }
        REALM_ASSERT(false); // Invalid aggregate source column
        return nullptr;
    }

    template <Action TAction>
    static TFind_callback_specialized get_specialized_callback_2_int(DataType col_id, bool nullable)
    {
        if (col_id == type_Int) {
            return get_specialized_callback_3<TAction, type_Int>(nullable);
        }
        REALM_ASSERT(false); // Invalid aggregate source column
        return nullptr;
    }

    template <Action TAction, DataType TDataType>
    static TFind_callback_specialized get_specialized_callback_3(bool nullable)
    {
        if (nullable) {
            return &BaseType::template find_callback_specialization<TConditionFunction, TAction, TDataType, true>;
        }
        else {
            return &BaseType::template find_callback_specialization<TConditionFunction, TAction, TDataType, false>;
        }
    }
};

// This node is currently used for floats and doubles only
template <class ColType, class TConditionFunction>
class FloatDoubleNode : public ParentNode {
public:
    using TConditionValue = typename ColType::value_type;
    static const bool special_null_node = false;

    FloatDoubleNode(TConditionValue v, size_t column_ndx)
        : m_value(v)
    {
        m_condition_column_idx = column_ndx;
        m_dT = 1.0;
    }
    FloatDoubleNode(null, size_t column_ndx)
        : m_value(null::get_null_float<TConditionValue>())
    {
        m_condition_column_idx = column_ndx;
        m_dT = 1.0;
    }

    void table_changed() override
    {
        m_condition_column.init(&get_column<ColType>(m_condition_column_idx));
    }

    void init() override
    {
        ParentNode::init();
        m_dD = 100.0;
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        TConditionFunction cond;

        auto find = [&](bool nullability) {
            bool m_value_nan = nullability ? null::is_null_float(m_value) : false;
            for (size_t s = start; s < end; ++s) {
                TConditionValue v = m_condition_column.get_next(s);
                REALM_ASSERT(!(null::is_null_float(v) && !nullability));
                if (cond(v, m_value, nullability ? null::is_null_float<TConditionValue>(v) : false, m_value_nan))
                    return s;
            }
            return not_found;
        };

        // This will inline the second case but no the first. Todo, use templated lambda when switching to c++14
        if (m_table->is_nullable(m_condition_column_idx))
            return find(true);
        else
            return find(false);
    }

    std::unique_ptr<ParentNode> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<ParentNode>(new FloatDoubleNode(*this, patches));
    }

    FloatDoubleNode(const FloatDoubleNode& from, QueryNodeHandoverPatches* patches)
        : ParentNode(from, patches)
        , m_value(from.m_value)
    {
        copy_getter(m_condition_column, m_condition_column_idx, from.m_condition_column, patches);
    }

protected:
    TConditionValue m_value;
    SequentialGetter<ColType> m_condition_column;
};


template <class TConditionFunction>
class BinaryNode : public ParentNode {
public:
    using TConditionValue = BinaryData;
    static const bool special_null_node = false;

    BinaryNode(BinaryData v, size_t column)
        : m_value(v)
    {
        m_dT = 100.0;
        m_condition_column_idx = column;
    }

    BinaryNode(null, size_t column)
        : BinaryNode(BinaryData{}, column)
    {
    }

    void table_changed() override
    {
        m_condition_column = &get_column<BinaryColumn>(m_condition_column_idx);
    }

    void init() override
    {
        m_dD = 100.0;

        if (m_child)
            m_child->init();
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        TConditionFunction condition;
        for (size_t s = start; s < end; ++s) {
            BinaryData value = m_condition_column->get(s);
            if (condition(m_value.get(), value))
                return s;
        }
        return not_found;
    }

    std::unique_ptr<ParentNode> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<ParentNode>(new BinaryNode(*this, patches));
    }

    BinaryNode(const BinaryNode& from, QueryNodeHandoverPatches* patches)
        : ParentNode(from, patches)
        , m_value(from.m_value)
        , m_condition_column(from.m_condition_column)
    {
        if (m_condition_column && patches)
            m_condition_column_idx = m_condition_column->get_column_index();
    }

private:
    OwnedBinaryData m_value;
    const BinaryColumn* m_condition_column;
};


template <class TConditionFunction>
class TimestampNode : public ParentNode {
public:
    using TConditionValue = Timestamp;
    static const bool special_null_node = false;

    TimestampNode(Timestamp v, size_t column)
        : m_value(v)
    {
        m_condition_column_idx = column;
    }

    TimestampNode(null, size_t column)
        : TimestampNode(Timestamp{}, column)
    {
    }

    void table_changed() override
    {
        m_condition_column = &get_column<TimestampColumn>(m_condition_column_idx);
    }

    void init() override
    {
        m_dD = 100.0;

        if (m_child)
            m_child->init();
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        size_t ret = m_condition_column->find<TConditionFunction>(m_value, start, end);
        return ret;
    }

    std::unique_ptr<ParentNode> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<ParentNode>(new TimestampNode(*this, patches));
    }

    TimestampNode(const TimestampNode& from, QueryNodeHandoverPatches* patches)
        : ParentNode(from, patches)
        , m_value(from.m_value)
        , m_condition_column(from.m_condition_column)
    {
        if (m_condition_column && patches)
            m_condition_column_idx = m_condition_column->get_column_index();
    }

private:
    Timestamp m_value;
    const TimestampColumn* m_condition_column;
};

class StringNodeBase : public ParentNode {
public:
    using TConditionValue = StringData;
    static const bool special_null_node = true;

    StringNodeBase(StringData v, size_t column)
        : m_value(v.is_null() ? util::none : util::make_optional(std::string(v)))
    {
        m_condition_column_idx = column;
    }

    void table_changed() override
    {
        m_condition_column = &get_column_base(m_condition_column_idx);
        m_column_type = get_real_column_type(m_condition_column_idx);
    }

    void init() override
    {
        m_dT = 10.0;
        m_probes = 0;
        m_matches = 0;
        m_end_s = 0;
        m_leaf_start = 0;
        m_leaf_end = 0;
    }

    void clear_leaf_state()
    {
        m_leaf.reset(nullptr);
    }

    StringNodeBase(const StringNodeBase& from, QueryNodeHandoverPatches* patches)
        : ParentNode(from, patches)
        , m_value(from.m_value)
        , m_condition_column(from.m_condition_column)
        , m_column_type(from.m_column_type)
        , m_leaf_type(from.m_leaf_type)
    {
        if (m_condition_column && patches)
            m_condition_column_idx = m_condition_column->get_column_index();
    }

protected:
    util::Optional<std::string> m_value;

    const ColumnBase* m_condition_column = nullptr;
    ColumnType m_column_type;

    // Used for linear scan through short/long-string
    std::unique_ptr<const ArrayParent> m_leaf;
    StringColumn::LeafType m_leaf_type;
    size_t m_end_s = 0;
    size_t m_leaf_start = 0;
    size_t m_leaf_end = 0;
    
    inline StringData get_string(size_t s)
    {
        StringData t;
        
        if (m_column_type == col_type_StringEnum) {
            // enum
            t = static_cast<const StringEnumColumn*>(m_condition_column)->get(s);
        }
        else {
            // short or long
            const StringColumn* asc = static_cast<const StringColumn*>(m_condition_column);
            REALM_ASSERT_3(s, <, asc->size());
            if (s >= m_end_s || s < m_leaf_start) {
                // we exceeded current leaf's range
                clear_leaf_state();
                size_t ndx_in_leaf;
                m_leaf = asc->get_leaf(s, ndx_in_leaf, m_leaf_type);
                m_leaf_start = s - ndx_in_leaf;
                
                if (m_leaf_type == StringColumn::leaf_type_Small)
                    m_end_s = m_leaf_start + static_cast<const ArrayString&>(*m_leaf).size();
                else if (m_leaf_type == StringColumn::leaf_type_Medium)
                    m_end_s = m_leaf_start + static_cast<const ArrayStringLong&>(*m_leaf).size();
                else
                    m_end_s = m_leaf_start + static_cast<const ArrayBigBlobs&>(*m_leaf).size();
            }
            
            if (m_leaf_type == StringColumn::leaf_type_Small)
                t = static_cast<const ArrayString&>(*m_leaf).get(s - m_leaf_start);
            else if (m_leaf_type == StringColumn::leaf_type_Medium)
                t = static_cast<const ArrayStringLong&>(*m_leaf).get(s - m_leaf_start);
            else
                t = static_cast<const ArrayBigBlobs&>(*m_leaf).get_string(s - m_leaf_start);
        }
        return t;
    }
};

// Conditions for strings. Note that Equal is specialized later in this file!
template <class TConditionFunction>
class StringNode : public StringNodeBase {
public:
    StringNode(StringData v, size_t column)
        : StringNodeBase(v, column)
    {
        auto upper = case_map(v, true);
        auto lower = case_map(v, false);
        if (!upper || !lower) {
            error_code = "Malformed UTF-8: " + std::string(v);
        }
        else {
            m_ucase = std::move(*upper);
            m_lcase = std::move(*lower);
        }
    }

    void init() override
    {
        clear_leaf_state();

        m_dD = 100.0;

        StringNodeBase::init();

        if (m_child)
            m_child->init();
    }


    size_t find_first_local(size_t start, size_t end) override
    {
        TConditionFunction cond;

        for (size_t s = start; s < end; ++s) {
            StringData t = get_string(s);
            
            if (cond(StringData(m_value), m_ucase.data(), m_lcase.data(), t))
                return s;
        }
        return not_found;
    }

    std::unique_ptr<ParentNode> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<ParentNode>(new StringNode<TConditionFunction>(*this, patches));
    }

    StringNode(const StringNode& from, QueryNodeHandoverPatches* patches)
        : StringNodeBase(from, patches)
        , m_ucase(from.m_ucase)
        , m_lcase(from.m_lcase)
    {
    }

protected:
    std::string m_ucase;
    std::string m_lcase;
};

// Specialization for Contains condition on Strings - we specialize because we can utilize Boyer-Moore
template <>
class StringNode<Contains> : public StringNodeBase {
public:
    StringNode(StringData v, size_t column)
    : StringNodeBase(v, column), m_charmap()
    {
        if (v.size() == 0)
            return;
        
        // Build a dictionary of char-to-last distances in the search string
        // (zero indicates that the char is not in needle)
        size_t last_char_pos = v.size()-1;
        for (size_t i = 0; i < last_char_pos; ++i) {
            // we never jump longer increments than 255 chars, even if needle is longer (to fit in one byte)
            uint8_t jump = last_char_pos-i < 255 ? static_cast<uint8_t>(last_char_pos-i) : 255;
            
            unsigned char c = v[i];
            m_charmap[c] = jump;
        }
    }
    
    void init() override
    {
        clear_leaf_state();
        
        m_dD = 100.0;
        
        StringNodeBase::init();
        
        if (m_child)
            m_child->init();
    }
    
    
    size_t find_first_local(size_t start, size_t end) override
    {
        Contains cond;
        
        for (size_t s = start; s < end; ++s) {
            StringData t = get_string(s);
            
            if (cond(StringData(m_value), m_charmap, t))
                return s;
        }
        return not_found;
    }
    
    std::unique_ptr<ParentNode> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<ParentNode>(new StringNode<Contains>(*this, patches));
    }
    
    StringNode(const StringNode& from, QueryNodeHandoverPatches* patches)
    : StringNodeBase(from, patches)
    , m_charmap(from.m_charmap)
    {
    }
    
protected:
    std::array<uint8_t, 256> m_charmap;
};

// Specialization for ContainsIns condition on Strings - we specialize because we can utilize Boyer-Moore
template <>
class StringNode<ContainsIns> : public StringNodeBase {
public:
    StringNode(StringData v, size_t column)
    : StringNodeBase(v, column), m_charmap()
    {
        auto upper = case_map(v, true);
        auto lower = case_map(v, false);
        if (!upper || !lower) {
            error_code = "Malformed UTF-8: " + std::string(v);
        }
        else {
            m_ucase = std::move(*upper);
            m_lcase = std::move(*lower);
        }
        
        if (v.size() == 0)
            return;
        
        // Build a dictionary of char-to-last distances in the search string
        // (zero indicates that the char is not in needle)
        size_t last_char_pos = m_ucase.size()-1;
        for (size_t i = 0; i < last_char_pos; ++i) {
            // we never jump longer increments than 255 chars, even if needle is longer (to fit in one byte)
            uint8_t jump = last_char_pos-i < 255 ? static_cast<uint8_t>(last_char_pos-i) : 255;
            
            unsigned char uc = m_ucase[i];
            unsigned char lc = m_lcase[i];
            m_charmap[uc] = jump;
            m_charmap[lc] = jump;
        }

    }
    
    void init() override
    {
        clear_leaf_state();
        
        m_dD = 100.0;
        
        StringNodeBase::init();
        
        if (m_child)
            m_child->init();
    }
    
    
    size_t find_first_local(size_t start, size_t end) override
    {
        ContainsIns cond;
        
        for (size_t s = start; s < end; ++s) {
            StringData t = get_string(s);
            
            if (cond(StringData(m_value), m_ucase.data(), m_lcase.data(), m_charmap, t))
                return s;
        }
        return not_found;
    }
    
    std::unique_ptr<ParentNode> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<ParentNode>(new StringNode<ContainsIns>(*this, patches));
    }
    
    StringNode(const StringNode& from, QueryNodeHandoverPatches* patches)
    : StringNodeBase(from, patches)
    , m_charmap(from.m_charmap)
    , m_ucase(from.m_ucase)
    , m_lcase(from.m_lcase)
    {
    }
    
protected:
    std::array<uint8_t, 256> m_charmap;
    std::string m_ucase;
    std::string m_lcase;
};

// Specialization for Equal condition on Strings - we specialize because we can utilize indexes (if they exist) for
// Equal.
// Future optimization: make specialization for greater, notequal, etc
template <>
class StringNode<Equal> : public StringNodeBase {
public:
    StringNode(StringData v, size_t column)
        : StringNodeBase(v, column)
    {
    }
    ~StringNode() noexcept override
    {
        deallocate();
    }

    void deallocate() noexcept
    {
        // Must be called after each query execution too free temporary resources used by the execution. Run in
        // destructor, but also in Init because a user could define a query once and execute it multiple times.
        clear_leaf_state();

        if (m_index_matches_destroy)
            m_index_matches->destroy();

        m_index_matches_destroy = false;
        m_index_matches.reset();
        m_index_getter.reset();
    }

    void init() override
    {
        deallocate();
        m_dD = 10.0;
        StringNodeBase::init();

        if (m_column_type == col_type_StringEnum) {
            m_dT = 1.0;
            m_key_ndx = static_cast<const StringEnumColumn*>(m_condition_column)->get_key_ndx(m_value);
        }
        else if (m_condition_column->has_search_index()) {
            m_dT = 0.0;
        }
        else {
            m_dT = 10.0;
        }

        if (m_condition_column->has_search_index()) {
            FindRes fr;
            InternalFindResult res;

            if (m_column_type == col_type_StringEnum) {
                fr = static_cast<const StringEnumColumn*>(m_condition_column)->find_all_no_copy(m_value, res);
            }
            else {
                fr = static_cast<const StringColumn*>(m_condition_column)->find_all_no_copy(m_value, res);
            }

            m_index_matches_destroy = false;
            m_last_start = size_t(-1);

            switch (fr) {
                case FindRes_single:
                    m_index_matches.reset(
                        new IntegerColumn(IntegerColumn::unattached_root_tag(), Allocator::get_default())); // Throws
                    m_index_matches->get_root_array()->create(Array::type_Normal);                          // Throws
                    m_index_matches->add(res.payload);
                    m_index_matches_destroy = true; // we own m_index_matches, so we must destroy it
                    m_results_start = 0;
                    m_results_end = 1;
                    break;
                case FindRes_column:
                    // todo: Apparently we can't use m_index.get_alloc() because it uses default allocator which
                    // simply makes
                    // translate(x) = x. Shouldn't it inherit owner column's allocator?!
                    m_index_matches.reset(new IntegerColumn(m_condition_column->get_alloc(), res.payload)); // Throws
                    m_results_start = res.start_ndx;
                    m_results_end = res.end_ndx;

                    // FIXME: handle start and end of find_result!
                    break;
                case FindRes_not_found:
                    m_index_matches.reset();
                    m_index_getter.reset();
                    break;
            }

            if (m_index_matches) {
                m_index_getter.reset(new SequentialGetter<IntegerColumn>(m_index_matches.get()));
            }
        }
        else if (m_column_type != col_type_String) {
            REALM_ASSERT_DEBUG(dynamic_cast<const StringEnumColumn*>(m_condition_column));
            m_cse.init(static_cast<const StringEnumColumn*>(m_condition_column));
        }

        if (m_child)
            m_child->init();
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        REALM_ASSERT(m_table);

        if (m_condition_column->has_search_index()) {
            // Indexed string column
            if (!m_index_getter)
                return not_found; // no matches in the index

            if (m_last_start > start)
                m_last_indexed = m_results_start;
            m_last_start = start;

            while (m_last_indexed < m_results_end) {
                m_index_getter->cache_next(m_last_indexed);
                size_t f = m_index_getter->m_leaf_ptr->find_gte(start, m_last_indexed - m_index_getter->m_leaf_start,
                                                                m_results_end - m_index_getter->m_leaf_start);

                if (f == not_found) {
                    // Not found in this leaf - move on to next
                    m_last_indexed = m_index_getter->m_leaf_end;
                }
                else if (f >= (m_results_end - m_index_getter->m_leaf_start)) {
                    // Found outside valid range
                    return not_found;
                }
                else {
                    size_t found_index = to_size_t(m_index_getter->m_leaf_ptr->get(f));
                    if (found_index >= end)
                        return not_found;
                    else {
                        m_last_indexed = f + m_index_getter->m_leaf_start;
                        return found_index;
                    }
                }
            }
            return not_found;
        }

        if (m_column_type != col_type_String) {
            // Enum string column
            if (m_key_ndx == not_found)
                return not_found; // not in key set

            for (size_t s = start; s < end; ++s) {
                m_cse.cache_next(s);
                s = m_cse.m_leaf_ptr->find_first(m_key_ndx, s - m_cse.m_leaf_start, m_cse.local_end(end));
                if (s == not_found)
                    s = m_cse.m_leaf_end - 1;
                else
                    return s + m_cse.m_leaf_start;
            }

            return not_found;
        }

        // Normal string column, with long or short leaf
        for (size_t s = start; s < end; ++s) {
            const StringColumn* asc = static_cast<const StringColumn*>(m_condition_column);
            if (s >= m_leaf_end || s < m_leaf_start) {
                clear_leaf_state();
                size_t ndx_in_leaf;
                m_leaf = asc->get_leaf(s, ndx_in_leaf, m_leaf_type);
                m_leaf_start = s - ndx_in_leaf;
                if (m_leaf_type == StringColumn::leaf_type_Small)
                    m_leaf_end = m_leaf_start + static_cast<const ArrayString&>(*m_leaf).size();
                else if (m_leaf_type == StringColumn::leaf_type_Medium)
                    m_leaf_end = m_leaf_start + static_cast<const ArrayStringLong&>(*m_leaf).size();
                else
                    m_leaf_end = m_leaf_start + static_cast<const ArrayBigBlobs&>(*m_leaf).size();
                REALM_ASSERT(m_leaf);
            }
            size_t end2 = (end > m_leaf_end ? m_leaf_end - m_leaf_start : end - m_leaf_start);

            if (m_leaf_type == StringColumn::leaf_type_Small)
                s = static_cast<const ArrayString&>(*m_leaf).find_first(m_value, s - m_leaf_start, end2);
            else if (m_leaf_type == StringColumn::leaf_type_Medium)
                s = static_cast<const ArrayStringLong&>(*m_leaf).find_first(m_value, s - m_leaf_start, end2);
            else
                s = static_cast<const ArrayBigBlobs&>(*m_leaf).find_first(str_to_bin(m_value), true, s - m_leaf_start,
                                                                          end2);

            if (s == not_found)
                s = m_leaf_end - 1;
            else
                return s + m_leaf_start;
        }

        return not_found;
    }

    std::unique_ptr<ParentNode> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<ParentNode>(new StringNode<Equal>(*this, patches));
    }

    StringNode(const StringNode& from, QueryNodeHandoverPatches* patches)
        : StringNodeBase(from, patches)
    {
    }

private:
    inline BinaryData str_to_bin(const StringData& s) noexcept
    {
        return BinaryData(s.data(), s.size());
    }

    size_t m_key_ndx = not_found;
    size_t m_last_indexed;

    // Used for linear scan through enum-string
    SequentialGetter<StringEnumColumn> m_cse;

    // Used for index lookup
    std::unique_ptr<IntegerColumn> m_index_matches;
    bool m_index_matches_destroy = false;
    std::unique_ptr<SequentialGetter<IntegerColumn>> m_index_getter;
    size_t m_results_start;
    size_t m_results_end;
    size_t m_last_start;
};

// OR node contains at least two node pointers: Two or more conditions to OR
// together in m_conditions, and the next AND condition (if any) in m_child.
//
// For 'second.equal(23).begin_group().first.equal(111).Or().first.equal(222).end_group().third().equal(555)', this
// will first set m_conditions[0] = left-hand-side through constructor, and then later, when .first.equal(222) is
// invoked,
// invocation will set m_conditions[1] = right-hand-side through Query& Query::Or() (see query.cpp). In there, m_child
// is
// also set to next AND condition (if any exists) following the OR.
class OrNode : public ParentNode {
public:
    OrNode(std::unique_ptr<ParentNode> condition)
    {
        m_dT = 50.0;
        if (condition)
            m_conditions.emplace_back(std::move(condition));
    }

    OrNode(const OrNode& other, QueryNodeHandoverPatches* patches)
        : ParentNode(other, patches)
    {
        for (const auto& condition : other.m_conditions) {
            m_conditions.emplace_back(condition->clone(patches));
        }
    }

    void table_changed() override
    {
        for (auto& condition : m_conditions) {
            condition->set_table(*m_table);
        }
    }

    void init() override
    {
        m_dD = 10.0;

        m_start.clear();
        m_start.resize(m_conditions.size(), 0);

        m_last.clear();
        m_last.resize(m_conditions.size(), 0);

        m_was_match.clear();
        m_was_match.resize(m_conditions.size(), false);

        std::vector<ParentNode*> v;
        for (auto& condition : m_conditions) {
            condition->init();
            v.clear();
            condition->gather_children(v);
        }

        if (m_child)
            m_child->init();
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        if (start >= end)
            return not_found;

        size_t index = not_found;

        for (size_t c = 0; c < m_conditions.size(); ++c) {
            // out of order search; have to discard cached results
            if (start < m_start[c]) {
                m_last[c] = 0;
                m_was_match[c] = false;
            }
            // already searched this range and didn't match
            else if (m_last[c] >= end)
                continue;
            // already search this range and *did* match
            else if (m_was_match[c] && m_last[c] >= start) {
                if (index > m_last[c])
                    index = m_last[c];
                continue;
            }

            m_start[c] = start;
            size_t fmax = std::max(m_last[c], start);
            size_t f = m_conditions[c]->find_first(fmax, end);
            m_was_match[c] = f != not_found;
            m_last[c] = f == not_found ? end : f;
            if (f != not_found && index > m_last[c])
                index = m_last[c];
        }

        return index;
    }

    std::string validate() override
    {
        if (error_code != "")
            return error_code;
        if (m_conditions.size() == 0)
            return "Missing left-hand side of OR";
        if (m_conditions.size() == 1)
            return "Missing right-hand side of OR";
        std::string s;
        if (m_child != 0)
            s = m_child->validate();
        if (s != "")
            return s;
        for (size_t i = 0; i < m_conditions.size(); ++i) {
            s = m_conditions[i]->validate();
            if (s != "")
                return s;
        }
        return "";
    }

    std::unique_ptr<ParentNode> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<ParentNode>(new OrNode(*this, patches));
    }

    void apply_handover_patch(QueryNodeHandoverPatches& patches, Group& group) override
    {
        for (auto it = m_conditions.rbegin(); it != m_conditions.rend(); ++it)
            (*it)->apply_handover_patch(patches, group);

        ParentNode::apply_handover_patch(patches, group);
    }

    std::vector<std::unique_ptr<ParentNode>> m_conditions;

private:
    // start index of the last find for each cond
    std::vector<size_t> m_start;
    // last looked at index of the lasft find for each cond
    // is a matching index if m_was_match is true
    std::vector<size_t> m_last;
    std::vector<bool> m_was_match;
};


class NotNode : public ParentNode {
public:
    NotNode(std::unique_ptr<ParentNode> condition)
        : m_condition(std::move(condition))
    {
        m_dT = 50.0;
    }

    void table_changed() override
    {
        m_condition->set_table(*m_table);
    }

    void init() override
    {
        m_dD = 10.0;

        std::vector<ParentNode*> v;

        m_condition->init();
        v.clear();
        m_condition->gather_children(v);

        // Heuristics bookkeeping:
        m_known_range_start = 0;
        m_known_range_end = 0;
        m_first_in_known_range = not_found;

        if (m_child)
            m_child->init();
    }

    size_t find_first_local(size_t start, size_t end) override;

    std::string validate() override
    {
        if (error_code != "")
            return error_code;
        if (m_condition == 0)
            return "Missing argument to Not";
        std::string s;
        if (m_child != 0)
            s = m_child->validate();
        if (s != "")
            return s;
        s = m_condition->validate();
        if (s != "")
            return s;
        return "";
    }

    std::unique_ptr<ParentNode> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<ParentNode>(new NotNode(*this, patches));
    }

    NotNode(const NotNode& from, QueryNodeHandoverPatches* patches)
        : ParentNode(from, patches)
        , m_condition(from.m_condition ? from.m_condition->clone(patches) : nullptr)
        , m_known_range_start(from.m_known_range_start)
        , m_known_range_end(from.m_known_range_end)
        , m_first_in_known_range(from.m_first_in_known_range)
    {
    }

    void apply_handover_patch(QueryNodeHandoverPatches& patches, Group& group) override
    {
        m_condition->apply_handover_patch(patches, group);
        ParentNode::apply_handover_patch(patches, group);
    }

    std::unique_ptr<ParentNode> m_condition;

private:
    // FIXME This heuristic might as well be reused for all condition nodes.
    size_t m_known_range_start;
    size_t m_known_range_end;
    size_t m_first_in_known_range;

    bool evaluate_at(size_t rowndx);
    void update_known(size_t start, size_t end, size_t first);
    size_t find_first_loop(size_t start, size_t end);
    size_t find_first_covers_known(size_t start, size_t end);
    size_t find_first_covered_by_known(size_t start, size_t end);
    size_t find_first_overlap_lower(size_t start, size_t end);
    size_t find_first_overlap_upper(size_t start, size_t end);
    size_t find_first_no_overlap(size_t start, size_t end);
};


// Compare two columns with eachother row-by-row
template <class ColType, class TConditionFunction>
class TwoColumnsNode : public ParentNode {
public:
    using TConditionValue = typename ColType::value_type;

    TwoColumnsNode(size_t column1, size_t column2)
    {
        m_dT = 100.0;
        m_condition_column_idx1 = column1;
        m_condition_column_idx2 = column2;
    }

    ~TwoColumnsNode() noexcept override
    {
        delete[] m_value.data();
    }

    void table_changed() override
    {
        m_getter1.init(&get_column<ColType>(m_condition_column_idx1));
        m_getter2.init(&get_column<ColType>(m_condition_column_idx2));
    }

    void init() override
    {
        m_dD = 100.0;

        if (m_child)
            m_child->init();
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        size_t s = start;

        while (s < end) {
            if (std::is_same<TConditionValue, int64_t>::value) {
                // For int64_t we've created an array intrinsics named compare_leafs which template expands bitwidths
                // of boths arrays to make Get faster.
                m_getter1.cache_next(s);
                m_getter2.cache_next(s);

                QueryState<int64_t> qs;
                bool resume = m_getter1.m_leaf_ptr->template compare_leafs<TConditionFunction, act_ReturnFirst>(
                    m_getter2.m_leaf_ptr, s - m_getter1.m_leaf_start, m_getter1.local_end(end), 0, &qs,
                    CallbackDummy());

                if (resume)
                    s = m_getter1.m_leaf_end;
                else
                    return to_size_t(qs.m_state) + m_getter1.m_leaf_start;
            }
            else {
// This is for float and double.

#if 0 && defined(REALM_COMPILER_AVX)
// AVX has been disabled because of array alignment (see https://app.asana.com/0/search/8836174089724/5763107052506)
//
// For AVX you can call things like if (sseavx<1>()) to test for AVX, and then utilize _mm256_movemask_ps (VC)
// or movemask_cmp_ps (gcc/clang)
//
// See https://github.com/rrrlasse/realm/tree/AVX for an example of utilizing AVX for a two-column search which has
// been benchmarked to: floats: 288 ms vs 552 by using AVX compared to 2-level-unrolled FPU loop. doubles: 415 ms vs
// 475 (more bandwidth bound). Tests against SSE have not been performed; AVX may not pay off. Please benchmark
#endif

                TConditionValue v1 = m_getter1.get_next(s);
                TConditionValue v2 = m_getter2.get_next(s);
                TConditionFunction C;

                if (C(v1, v2))
                    return s;
                else
                    s++;
            }
        }
        return not_found;
    }

    std::unique_ptr<ParentNode> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<ParentNode>(new TwoColumnsNode<ColType, TConditionFunction>(*this, patches));
    }

    TwoColumnsNode(const TwoColumnsNode& from, QueryNodeHandoverPatches* patches)
        : ParentNode(from, patches)
        , m_value(from.m_value)
        , m_condition_column(from.m_condition_column)
        , m_column_type(from.m_column_type)
    {
        if (m_condition_column)
            m_condition_column_idx = m_condition_column->get_column_index();
        copy_getter(m_getter1, m_condition_column_idx1, from.m_getter1, patches);
        copy_getter(m_getter2, m_condition_column_idx2, from.m_getter2, patches);
    }

private:
    BinaryData m_value;
    const BinaryColumn* m_condition_column = nullptr;
    ColumnType m_column_type;

    size_t m_condition_column_idx1 = not_found;
    size_t m_condition_column_idx2 = not_found;

    SequentialGetter<ColType> m_getter1;
    SequentialGetter<ColType> m_getter2;
};


// For Next-Generation expressions like col1 / col2 + 123 > col4 * 100.
class ExpressionNode : public ParentNode {

public:
    ExpressionNode(std::unique_ptr<Expression> expression)
        : m_expression(std::move(expression))
    {
        m_dD = 10.0;
        m_dT = 50.0;
    }

    void table_changed() override
    {
        m_expression->set_base_table(m_table.get());
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        return m_expression->find_first(start, end);
    }

    std::unique_ptr<ParentNode> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<ParentNode>(new ExpressionNode(*this, patches));
    }

    void apply_handover_patch(QueryNodeHandoverPatches& patches, Group& group) override
    {
        m_expression->apply_handover_patch(patches, group);
        ParentNode::apply_handover_patch(patches, group);
    }

private:
    ExpressionNode(const ExpressionNode& from, QueryNodeHandoverPatches* patches)
        : ParentNode(from, patches)
        , m_expression(from.m_expression->clone(patches))
    {
    }

    std::unique_ptr<Expression> m_expression;
};


struct LinksToNodeHandoverPatch : public QueryNodeHandoverPatch {
    std::unique_ptr<RowBaseHandoverPatch> m_target_row;
    size_t m_origin_column;
};

class LinksToNode : public ParentNode {
public:
    LinksToNode(size_t origin_column_index, const ConstRow& target_row)
        : m_origin_column(origin_column_index)
        , m_target_row(target_row)
    {
        m_dD = 10.0;
        m_dT = 50.0;
    }

    void table_changed() override
    {
        m_column_type = m_table->get_column_type(m_origin_column);
        m_column = &const_cast<Table*>(m_table.get())->get_column_link_base(m_origin_column);
        REALM_ASSERT(m_column_type == type_Link || m_column_type == type_LinkList);
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        REALM_ASSERT(m_column);
        if (!m_target_row.is_attached())
            return not_found;

        if (m_column_type == type_Link) {
            LinkColumn& cl = static_cast<LinkColumn&>(*m_column);
            return cl.find_first(m_target_row.get_index() + 1, start,
                                 end); // LinkColumn stores link to row N as the integer N + 1
        }
        else if (m_column_type == type_LinkList) {
            LinkListColumn& cll = static_cast<LinkListColumn&>(*m_column);

            for (size_t i = start; i < end; i++) {
                LinkViewRef lv = cll.get(i);
                if (lv->find(m_target_row.get_index()) != not_found)
                    return i;
            }
        }

        return not_found;
    }

    std::unique_ptr<ParentNode> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<ParentNode>(patches ? new LinksToNode(*this, patches) : new LinksToNode(*this));
    }

    void apply_handover_patch(QueryNodeHandoverPatches& patches, Group& group) override
    {
        REALM_ASSERT(patches.size());
        std::unique_ptr<QueryNodeHandoverPatch> abstract_patch = std::move(patches.back());
        patches.pop_back();

        auto patch = dynamic_cast<LinksToNodeHandoverPatch*>(abstract_patch.get());
        REALM_ASSERT(patch);

        m_origin_column = patch->m_origin_column;
        m_target_row.apply_and_consume_patch(patch->m_target_row, group);

        ParentNode::apply_handover_patch(patches, group);
    }

private:
    size_t m_origin_column = npos;
    ConstRow m_target_row;
    LinkColumnBase* m_column = nullptr;
    DataType m_column_type;

    LinksToNode(const LinksToNode& source, QueryNodeHandoverPatches* patches)
        : ParentNode(source, patches)
    {
        auto patch = std::make_unique<LinksToNodeHandoverPatch>();
        patch->m_origin_column = source.m_column->get_column_index();
        ConstRow::generate_patch(source.m_target_row, patch->m_target_row);
        patches->push_back(std::move(patch));
    }
};

} // namespace realm

#endif // REALM_QUERY_ENGINE_HPP
