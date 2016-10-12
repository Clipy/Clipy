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
#ifndef REALM_COLUMN_TPL_HPP
#define REALM_COLUMN_TPL_HPP

#include <cstdlib>

#include <realm/util/features.h>
#include <realm/array.hpp>
#include <realm/array_basic.hpp>

namespace realm {

template<class T, class cond>
class FloatDoubleNode;
template<class ColType, class Cond>
class IntegerNode;
template<class T>
class SequentialGetter;

template<class cond, class T>
struct ColumnTypeTraits2;

template<class cond>
struct ColumnTypeTraits2<cond, int64_t> {
    typedef IntegerColumn column_type;
    typedef ArrayInteger array_type;
};
template<class cond>
struct ColumnTypeTraits2<cond, bool> {
    typedef IntegerColumn column_type;
    typedef ArrayInteger array_type;
};
template<class cond>
struct ColumnTypeTraits2<cond, float> {
    typedef FloatColumn column_type;
    typedef ArrayFloat array_type;
};
template<class cond>
struct ColumnTypeTraits2<cond, double> {
    typedef DoubleColumn column_type;
    typedef ArrayDouble array_type;
};


namespace _impl {

template<class ColType>
struct FindInLeaf {
    using LeafType = typename ColType::LeafType;

    template<Action action, class Condition, class T, class R>
    static bool find(const LeafType& leaf, T target, size_t local_start, size_t local_end, size_t leaf_start, QueryState<R>& state)
    {
        Condition cond;
        bool cont = true;
        // todo, make an additional loop with hard coded `false` instead of is_null(v) for non-nullable columns
        bool null_target = null::is_null_float(target);
        for (size_t local_index = local_start; cont && local_index < local_end; local_index++) {
            auto v = leaf.get(local_index);
            if (cond(v, target, null::is_null_float(v), null_target)) {
                cont = state.template match<action, false>(leaf_start + local_index , 0, static_cast<R>(v));
            }
        }
        return cont;
    }
};

template<>
struct FindInLeaf<IntegerColumn> {
    using LeafType = IntegerColumn::LeafType;

    template<Action action, class Condition, class T, class R>
    static bool find(const LeafType& leaf, T target, size_t local_start, size_t local_end, size_t leaf_start, QueryState<R>& state)
    {
        const int c = Condition::condition;
        return leaf.find(c, action, target, local_start, local_end, leaf_start, &state);
    }
};

template<>
struct FindInLeaf<IntNullColumn> {
    using LeafType = IntNullColumn::LeafType;

    template<Action action, class Condition, class T, class R>
    static bool find(const LeafType& leaf, T target, size_t local_start, size_t local_end, size_t leaf_start, QueryState<R>& state)
    {
        constexpr int cond = Condition::condition;
        return leaf.find(cond, action, target, local_start, local_end, leaf_start, &state);
    }
};

} // namespace _impl

template<class T, class R, Action action, class Condition, class ColType>
R aggregate(const ColType& column, T target, size_t start, size_t end,
            size_t limit, size_t* return_ndx)
{
    if (end == npos)
        end = column.size();

    QueryState<R> state;
    state.init(action, nullptr, limit);
    SequentialGetter<ColType> sg { &column };

    bool cont = true;
    for (size_t s = start; cont && s < end; ) {
        sg.cache_next(s);
        size_t start2 = s - sg.m_leaf_start;
        size_t end2 = sg.local_end(end);
        cont = _impl::FindInLeaf<ColType>::template find<action, Condition>(*sg.m_leaf_ptr, target, start2, end2, sg.m_leaf_start, state);
        s = sg.m_leaf_start + end2;
    }

    if (return_ndx)
        *return_ndx = action == act_Sum ? state.m_match_count : state.m_minmax_index;

    return state.m_state;
}


} // namespace realm

#endif // REALM_COLUMN_TPL_HPP
