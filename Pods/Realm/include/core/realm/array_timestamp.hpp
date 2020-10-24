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

#ifndef REALM_ARRAY_TIMESTAMP_HPP
#define REALM_ARRAY_TIMESTAMP_HPP

#include <realm/array_integer.hpp>
#include <realm/timestamp.hpp>

namespace realm {

class ArrayTimestamp : public ArrayPayload, private Array {
public:
    using value_type = Timestamp;

    explicit ArrayTimestamp(Allocator&);

    using Array::update_parent;
    using Array::get_parent;
    using Array::get_ndx_in_parent;
    using Array::get_ref;

    static Timestamp default_value(bool nullable)
    {
        return nullable ? Timestamp{} : Timestamp{0, 0};
    }

    void create();

    void init_from_mem(MemRef mem) noexcept;
    void init_from_ref(ref_type ref) noexcept override
    {
        init_from_mem(MemRef(m_alloc.translate(ref), ref, m_alloc));
    }
    void set_parent(ArrayParent* parent, size_t ndx_in_parent) noexcept override
    {
        Array::set_parent(parent, ndx_in_parent);
    }
    void init_from_parent()
    {
        init_from_ref(Array::get_ref_from_parent());
    }

    size_t size() const
    {
        return m_seconds.size();
    }

    void add(Timestamp value)
    {
        insert(m_seconds.size(), value);
    }
    void set(size_t ndx, Timestamp value);
    void set_null(size_t ndx)
    {
        // Value in m_nanoseconds is irrelevant if m_seconds is null
        m_seconds.set_null(ndx); // Throws
    }
    void insert(size_t ndx, Timestamp value);
    Timestamp get(size_t ndx) const
    {
        util::Optional<int64_t> seconds = m_seconds.get(ndx);
        return seconds ? Timestamp(*seconds, int32_t(m_nanoseconds.get(ndx))) : Timestamp{};
    }
    bool is_null(size_t ndx) const
    {
        return m_seconds.is_null(ndx);
    }
    void erase(size_t ndx)
    {
        m_seconds.erase(ndx);
        m_nanoseconds.erase(ndx);
    }
    void move(ArrayTimestamp& dst, size_t ndx)
    {
        m_seconds.move(dst.m_seconds, ndx);
        m_nanoseconds.move(dst.m_nanoseconds, ndx);
    }
    void clear()
    {
        m_seconds.clear();
        m_nanoseconds.clear();
    }

    template <class Condition>
    size_t find_first(Timestamp value, size_t begin, size_t end) const noexcept;

    size_t find_first(Timestamp value, size_t begin, size_t end) const noexcept;

    void verify() const;

private:
    ArrayIntNull m_seconds;
    ArrayInteger m_nanoseconds;
};

template <>
size_t ArrayTimestamp::find_first<Equal>(Timestamp value, size_t begin, size_t end) const noexcept;
template <>
size_t ArrayTimestamp::find_first<NotEqual>(Timestamp value, size_t begin, size_t end) const noexcept;
template <>
size_t ArrayTimestamp::find_first<Less>(Timestamp value, size_t begin, size_t end) const noexcept;
template <>
size_t ArrayTimestamp::find_first<LessEqual>(Timestamp value, size_t begin, size_t end) const noexcept;
template <>
size_t ArrayTimestamp::find_first<GreaterEqual>(Timestamp value, size_t begin, size_t end) const noexcept;
template <>
size_t ArrayTimestamp::find_first<Greater>(Timestamp value, size_t begin, size_t end) const noexcept;

inline size_t ArrayTimestamp::find_first(Timestamp value, size_t begin, size_t end) const noexcept
{
    return find_first<Equal>(value, begin, end);
}


template <>
class QueryState<Timestamp> : public QueryStateBase {
public:
    Timestamp m_state;

    template <Action action>
    bool uses_val()
    {
        return (action == act_Max || action == act_Min || action == act_Sum || action == act_Count);
    }

    QueryState(Action action, Array* = nullptr, size_t limit = -1)
        : QueryStateBase(limit)
    {
        if (action == act_Max)
            m_state = Timestamp{std::numeric_limits<int64_t>::min(), 0};
        else if (action == act_Min)
            m_state = Timestamp{std::numeric_limits<int64_t>::max(), 0};
        else {
            REALM_ASSERT_DEBUG(false);
        }
    }

    template <Action action, bool pattern>
    inline bool match(size_t index, uint64_t /*indexpattern*/, Timestamp value)
    {
        if (pattern)
            return false;

        if (action == act_Count) {
            ++m_match_count;
        }
        else {
            ++m_match_count;
            if (action == act_Max) {
                if (value > m_state) {
                    m_state = value;
                    REALM_ASSERT(m_key_values);
                    m_minmax_index = m_key_values->get(index) + m_key_offset;
                }
            }
            else if (action == act_Min) {
                if (value < m_state) {
                    m_state = value;
                    REALM_ASSERT(m_key_values);
                    m_minmax_index = m_key_values->get(index) + m_key_offset;
                }
            }
            else {
                REALM_ASSERT_DEBUG(false);
            }
        }

        return (m_limit > m_match_count);
    }
};
}

#endif /* SRC_REALM_ARRAY_BINARY_HPP_ */
