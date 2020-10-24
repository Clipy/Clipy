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

#ifndef REALM_ARRAY_BACKLINK_HPP
#define REALM_ARRAY_BACKLINK_HPP

#include <realm/cluster.hpp>

namespace realm {
class ArrayBacklink : public ArrayPayload, private Array {
public:
    using Array::Array;
    using Array::init_from_parent;
    using Array::copy_on_write;
    using Array::update_parent;
    using Array::get_ref;
    using Array::size;

    static int64_t default_value(bool)
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

    void insert(size_t ndx, int64_t val)
    {
        Array::insert(ndx, val);
    }

    int64_t get(size_t ndx) const
    {
        return Array::get(ndx);
    }

    void add(int64_t val)
    {
        Array::add(val);
    }

    // nullify forward links corresponding to any backward links at index 'ndx'
    void nullify_fwd_links(size_t ndx, CascadeState& state);
    void add(size_t ndx, ObjKey key);
    bool remove(size_t ndx, ObjKey key);
    void erase(size_t ndx);
    size_t get_backlink_count(size_t ndx) const;
    ObjKey get_backlink(size_t ndx, size_t index) const;
    void move(ArrayBacklink& dst, size_t ndx)
    {
        Array::move(dst, ndx);
    }
    void clear()
    {
        Array::truncate_and_destroy_children(0);
    }
    void verify() const;
};
}

#endif /* SRC_REALM_ARRAY_KEY_HPP_ */
