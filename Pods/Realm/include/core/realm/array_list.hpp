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

#ifndef REALM_ARRAY_LIST_HPP
#define REALM_ARRAY_LIST_HPP

#include <realm/array.hpp>

namespace realm {

class ArrayList : public ArrayPayload, private Array {
public:
    using value_type = ref_type;

    using Array::Array;
    using Array::init_from_parent;
    using Array::update_parent;
    using Array::get_ref;
    using Array::size;
    using Array::erase;

    static ref_type default_value(bool)
    {
        return 0;
    }

    void init_from_ref(ref_type ref) noexcept override
    {
        Array::init_from_ref(ref);
    }

    void set_parent(ArrayParent* parent, size_t ndx_in_parent) noexcept override
    {
        Array::set_parent(parent, ndx_in_parent);
    }

    void create()
    {
        Array::create(type_HasRefs);
    }

    void add(ref_type value)
    {
        Array::add(from_ref(value));
    }
    void set(size_t ndx, ref_type value)
    {
        Array::set_as_ref(ndx, value);
    }

    void set_null(size_t ndx)
    {
        Array::set(ndx, 0);
    }
    void insert(size_t ndx, ref_type value)
    {
        Array::insert(ndx, from_ref(value));
    }
    ref_type get(size_t ndx) const
    {
        return Array::get_as_ref(ndx);
    }
    bool is_null(size_t ndx) const
    {
        return Array::get(ndx) == 0;
    }
    void truncate_and_destroy_children(size_t ndx)
    {
        Array::truncate(ndx);
    }

    size_t find_first(ref_type value, size_t begin, size_t end) const noexcept
    {
        return Array::find_first(from_ref(value), begin, end);
    }
};
}

#endif /* REALM_ARRAY_LIST_HPP */
