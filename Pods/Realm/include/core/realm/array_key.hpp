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

#ifndef REALM_ARRAY_KEY_HPP
#define REALM_ARRAY_KEY_HPP

#include <realm/array.hpp>
#include <realm/cluster.hpp>
#include <realm/impl/destroy_guard.hpp>

namespace realm {

// If this class is used directly in a cluster leaf, the links are stored as the
// link value +1 in order to represent the null_key (-1) as 0. If the class is used
// in BPlusTree<ObjKey> class, the values should not be adjusted.
template <int adj>
class ArrayKeyBase : public ArrayPayload, private Array {
public:
    using value_type = ObjKey;

    using Array::is_attached;
    using Array::init_from_mem;
    using Array::init_from_parent;
    using Array::update_parent;
    using Array::get_ref;
    using Array::size;
    using Array::erase;
    using Array::clear;
    using Array::destroy;
    using Array::verify;

    ArrayKeyBase(Allocator& allocator)
        : Array(allocator)
    {
    }

    static ObjKey default_value(bool)
    {
        return {};
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
        Array::create(type_Normal);
    }

    void add(ObjKey value)
    {
        Array::add(value.value + adj);
    }
    void set(size_t ndx, ObjKey value)
    {
        Array::set(ndx, value.value + adj);
    }

    void set_null(size_t ndx)
    {
        Array::set(ndx, 0);
    }
    void insert(size_t ndx, ObjKey value)
    {
        Array::insert(ndx, value.value + adj);
    }
    ObjKey get(size_t ndx) const
    {
        return ObjKey{Array::get(ndx) - adj};
    }
    bool is_null(size_t ndx) const
    {
        return Array::get(ndx) == 0;
    }
    void move(ArrayKeyBase& dst, size_t ndx)
    {
        Array::move(dst, ndx);
    }

    size_t find_first(ObjKey value, size_t begin, size_t end) const noexcept
    {
        return Array::find_first(value.value + adj, begin, end);
    }

    void nullify(ObjKey key)
    {
        size_t begin = find_first(key, 0, Array::size());
        // There must be one
        REALM_ASSERT(begin != realm::npos);
        Array::erase(begin);
    }
};

class ArrayKey : public ArrayKeyBase<1> {
public:
    using ArrayKeyBase::ArrayKeyBase;
};

class ArrayKeyNonNullable : public ArrayKeyBase<0> {
public:
    using ArrayKeyBase::ArrayKeyBase;
};
}

#endif /* SRC_REALM_ARRAY_KEY_HPP_ */
