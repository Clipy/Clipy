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

#ifndef REALM_ARRAY_BOOL_HPP
#define REALM_ARRAY_BOOL_HPP

#include <realm/array.hpp>

namespace realm {

/// ArrayBool supports both nullable and non-nullable arrays with respect to
/// adding and inserting values. In this way we don't need to distinguish
/// between the two type when adding a row and when adding a column.
/// add, insert and getting of non-nullable values are taken care of by the
/// respective functions in Array.
class ArrayBool : public Array, public ArrayPayload {
public:
    using value_type = bool;

    using Array::Array;
    using Array::init_from_ref;
    using Array::init_from_parent;
    using Array::update_parent;
    using Array::get_ref;
    using Array::size;
    using Array::erase;
    using Array::truncate_and_destroy_children;

    static bool default_value(bool)
    {
        return false;
    }
    void create()
    {
        Array::create(type_Normal);
    }
    void init_from_ref(ref_type ref) noexcept override
    {
        Array::init_from_ref(ref);
    }
    void set_parent(ArrayParent* parent, size_t ndx_in_parent) noexcept override
    {
        Array::set_parent(parent, ndx_in_parent);
    }
    bool is_null(size_t) const
    {
        return false;
    }
    void set(size_t ndx, bool value)
    {
        Array::set(ndx, value ? 1 : 0);
    }
    bool get(size_t ndx) const
    {
        return Array::get(ndx) != 0;
    }
    void add(bool value)
    {
        Array::add(value);
    }
    void insert(size_t ndx, bool value)
    {
        Array::insert(ndx, value);
    }

    size_t find_first(util::Optional<bool> value, size_t begin = 0, size_t end = npos) const noexcept
    {
        if (value) {
            return Array::find_first(*value, begin, end);
        }
        else {
            return Array::find_first(null_value, begin, end);
        }
    }

protected:
    // We can still be in two bits as small values are considered unsigned
    static constexpr int null_value = 3;
};

class ArrayBoolNull : public ArrayBool {
public:
    using value_type = util::Optional<bool>;
    using ArrayBool::ArrayBool;

    static util::Optional<bool> default_value(bool nullable)
    {
        return nullable ? util::none : util::some<bool>(false);
    }
    void set(size_t ndx, util::Optional<bool> value)
    {
        if (value) {
            Array::set(ndx, *value);
        }
        else {
            Array::set(ndx, null_value);
        }
    }
    void add(util::Optional<bool> value)
    {
        if (value) {
            Array::add(*value);
        }
        else {
            Array::add(null_value);
        }
    }
    void insert(size_t ndx, util::Optional<bool> value)
    {
        if (value) {
            Array::insert(ndx, *value);
        }
        else {
            Array::insert(ndx, null_value);
        }
    }
    void set_null(size_t ndx)
    {
        Array::set(ndx, null_value);
    }
    bool is_null(size_t ndx) const
    {
        return Array::get(ndx) == null_value;
    }
    util::Optional<bool> get(size_t ndx) const
    {
        int64_t val = Array::get(ndx);
        return (val == null_value) ? util::none : util::make_optional(val != 0);
    }
};
}

#endif /* REALM_ARRAY_BOOL_HPP */
