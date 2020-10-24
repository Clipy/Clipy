/*************************************************************************
 *
 * Copyright 2018 Realm Inc.
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

#ifndef REALM_ARRAY_MIXED_HPP
#define REALM_ARRAY_MIXED_HPP

#include <realm/data_type.hpp>
#include <realm/mixed.hpp>
#include <realm/obj.hpp>
#include <realm/array_binary.hpp>
#include <realm/array_string.hpp>
#include <realm/array_timestamp.hpp>
#include <realm/array_integer.hpp>

namespace realm {

class ArrayMixed : public ArrayPayload, private Array {
public:
    using value_type = Mixed;

    using Array::detach;
    using Array::is_attached;
    using Array::get_ref;
    using Array::set_parent;
    using Array::update_parent;
    using Array::get_parent;

    explicit ArrayMixed(Allocator&);

    static Mixed default_value(bool)
    {
        return Mixed{};
    }

    void create();
    void destroy()
    {
        Array::destroy_deep();
    }

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
        ref_type ref = get_ref_from_parent();
        init_from_ref(ref);
    }

    size_t size() const
    {
        return m_composite.size();
    }

    void add(Mixed value);
    void set(size_t ndx, Mixed value);
    void set_null(size_t ndx);
    void insert(size_t ndx, Mixed value);
    Mixed get(size_t ndx) const;
    bool is_null(size_t ndx) const
    {
        return m_composite.get(ndx) == 0;
    }

    void erase(size_t ndx);
    void truncate_and_destroy_children(size_t ndx);
    void move(ArrayMixed& dst, size_t ndx);

    size_t find_first(Mixed value, size_t begin = 0, size_t end = realm::npos) const noexcept;

private:
    enum { payload_idx_type, payload_idx_int, payload_idx_pair, payload_idx_str, payload_idx_size };

    static constexpr int64_t s_data_type_mask = 0b0001'1111;
    static constexpr int64_t s_payload_idx_mask = 0b1110'0000;
    static constexpr int64_t s_payload_idx_shift = 5;
    static constexpr int64_t s_data_shift = 8;

    // This primary array contains an aggregation of the actual value - which can be
    // either the value itself or an index into one of the payload arrays - the index
    // of the payload array and the data_type.
    //
    // value << s_data_shift | payload_idx << s_payload_idx_shift | data_type
    //
    // payload_idx one of PayloadIdx
    Array m_composite;

    // Used to store big ints, floats and doubles
    mutable Array m_ints;
    // Used to store timestamps
    mutable Array m_int_pairs;
    // Used to store String and Binary
    mutable ArrayString m_strings;

    DataType get_type(size_t ndx) const
    {
        return DataType((m_composite.get(ndx) & s_data_type_mask) - 1);
    }
    int64_t store(const Mixed&);
    void ensure_array_accessor(Array& arr, size_t ndx_in_parent) const;
    void ensure_int_array() const;
    void ensure_int_pair_array() const;
    void ensure_string_array() const;
    void replace_index(size_t old_ndx, size_t new_ndx, size_t payload_index);
    void erase_linked_payload(size_t ndx);
};
} // namespace realm

#endif /* REALM_ARRAY_MIXED_HPP */
