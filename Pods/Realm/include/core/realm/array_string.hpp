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

#ifndef REALM_ARRAY_STRING_HPP
#define REALM_ARRAY_STRING_HPP

#include <realm/array_string_short.hpp>
#include <realm/array_blobs_small.hpp>
#include <realm/array_blobs_big.hpp>

namespace realm {

class Spec;

class ArrayString : public ArrayPayload {
public:
    using value_type = StringData;

    explicit ArrayString(Allocator&);

    static StringData default_value(bool nullable)
    {
        return nullable ? StringData{} : StringData{""};
    }

    // This is only used in the upgrade process
    void set_nullability(bool n)
    {
        m_nullable = n;
    }
    void create();

    bool is_attached() const
    {
        return m_arr->is_attached();
    }

    ref_type get_ref() const
    {
        return m_arr->get_ref();
    }
    ArrayParent* get_parent() const
    {
        return m_arr->get_parent();
    }
    size_t get_ndx_in_parent() const
    {
        return m_arr->get_ndx_in_parent();
    }
    void set_parent(ArrayParent* p, size_t n) noexcept override
    {
        m_arr->set_parent(p, n);
    }
    bool need_spec() const override
    {
        return true;
    }
    void set_spec(Spec* spec, size_t col_ndx) const override
    {
        m_spec = spec;
        m_col_ndx = col_ndx;
    }

    void update_parent()
    {
        m_arr->update_parent();
    }

    void init_from_mem(MemRef mem) noexcept;
    void init_from_ref(ref_type ref) noexcept override
    {
        init_from_mem(MemRef(m_alloc.translate(ref), ref, m_alloc));
    }
    void init_from_parent();
    void destroy();

    size_t size() const;

    void add(StringData value);
    void set(size_t ndx, StringData value);
    void set_null(size_t ndx)
    {
        set(ndx, StringData{});
    }
    void insert(size_t ndx, StringData value);
    StringData get(size_t ndx) const;
    StringData get_legacy(size_t ndx) const;
    bool is_null(size_t ndx) const;
    void erase(size_t ndx);
    void move(ArrayString& dst, size_t ndx);
    void clear();

    size_t find_first(StringData value, size_t begin, size_t end) const noexcept;

    size_t lower_bound(StringData value);

    /// Get the specified element without the cost of constructing an
    /// array instance. If an array instance is already available, or
    /// you need to get multiple values, then this method will be
    /// slower.
    static StringData get(const char* header, size_t ndx, Allocator& alloc) noexcept;

    void verify() const;

private:
    static constexpr size_t small_string_max_size = 15;  // ArrayStringShort
    static constexpr size_t medium_string_max_size = 63; // ArrayStringLong
    union Storage {
        std::aligned_storage<sizeof(ArrayStringShort), alignof(ArrayStringShort)>::type m_string_short;
        std::aligned_storage<sizeof(ArraySmallBlobs), alignof(ArraySmallBlobs)>::type m_string_long;
        std::aligned_storage<sizeof(ArrayBigBlobs), alignof(ArrayBigBlobs)>::type m_big_blobs;
        std::aligned_storage<sizeof(ArrayInteger), alignof(ArrayInteger)>::type m_enum;
    };
    enum class Type { small_strings, medium_strings, big_strings, enum_strings };

    Type m_type = Type::small_strings;

    Allocator& m_alloc;
    Storage m_storage;
    Array* m_arr;
    mutable Spec* m_spec = nullptr;
    mutable size_t m_col_ndx = realm::npos;
    bool m_nullable = true;

    std::unique_ptr<ArrayString> m_string_enum_values;

    Type upgrade_leaf(size_t value_size);
};

inline StringData ArrayString::get(const char* header, size_t ndx, Allocator& alloc) noexcept
{
    bool long_strings = Array::get_hasrefs_from_header(header);
    if (!long_strings) {
        return ArrayStringShort::get(header, ndx, true);
    }
    else {
        bool is_big = Array::get_context_flag_from_header(header);
        if (!is_big) {
            return ArraySmallBlobs::get_string(header, ndx, alloc);
        }
        else {
            return ArrayBigBlobs::get_string(header, ndx, alloc);
        }
    }
}

template <>
class QueryState<StringData> : public QueryStateBase {
public:
    StringData m_state;

    template <Action action>
    bool uses_val()
    {
        return (action == act_Count);
    }

    QueryState(Action, Array* = nullptr, size_t limit = -1)
        : QueryStateBase(limit)
    {
    }

    template <Action action, bool pattern>
    inline bool match(size_t, uint64_t, StringData)
    {
        if (pattern)
            return false;

        if (action == act_Count) {
            ++m_match_count;
        }
        else {
            REALM_ASSERT_DEBUG(false);
        }

        return (m_limit > m_match_count);
    }
};
}

#endif /* REALM_ARRAY_STRING_HPP */
