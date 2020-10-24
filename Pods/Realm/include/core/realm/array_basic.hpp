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

#ifndef REALM_ARRAY_BASIC_HPP
#define REALM_ARRAY_BASIC_HPP

#include <realm/array.hpp>

namespace realm {

/// A BasicArray can currently only be used for simple unstructured
/// types like float, double.
template <class T>
class BasicArray : public Array, public ArrayPayload {
public:
    using value_type = T;

    explicit BasicArray(Allocator&) noexcept;
    ~BasicArray() noexcept override
    {
    }

    static T default_value(bool)
    {
        return T(0.0);
    }

    void init_from_ref(ref_type ref) noexcept override
    {
        Array::init_from_ref(ref);
    }

    void set_parent(ArrayParent* parent, size_t ndx_in_parent) noexcept override
    {
        Array::set_parent(parent, ndx_in_parent);
    }

    // Disable copying, this is not allowed.
    BasicArray& operator=(const BasicArray&) = delete;
    BasicArray(const BasicArray&) = delete;

    T get(size_t ndx) const noexcept;
    bool is_null(size_t ndx) const noexcept
    {
        // FIXME: This assumes BasicArray will only ever be instantiated for float-like T.
        static_assert(realm::is_any<T, float, double>::value, "T can only be float or double");
        auto x = BasicArray<T>::get(ndx);
        return null::is_null_float(x);
    }
    void add(T value);
    void set(size_t ndx, T value);
    void insert(size_t ndx, T value);
    void erase(size_t ndx);
    void truncate(size_t size);
    void move(BasicArray& dst, size_t ndx)
    {
        for (size_t i = ndx; i < m_size; i++) {
            dst.add(get(i));
        }
        truncate(ndx);
    }
    void clear();

    size_t find_first(T value, size_t begin = 0, size_t end = npos) const;
    void find_all(IntegerColumn* result, T value, size_t add_offset = 0, size_t begin = 0, size_t end = npos) const;

    size_t count(T value, size_t begin = 0, size_t end = npos) const;
    bool maximum(T& result, size_t begin = 0, size_t end = npos) const;
    bool minimum(T& result, size_t begin = 0, size_t end = npos) const;

    /// Compare two arrays for equality.
    bool compare(const BasicArray<T>&) const;

    /// Get the specified element without the cost of constructing an
    /// array instance. If an array instance is already available, or
    /// you need to get multiple values, then this method will be
    /// slower.
    static T get(const char* header, size_t ndx) noexcept;

    size_t lower_bound(T value) const noexcept;
    size_t upper_bound(T value) const noexcept;

    /// Construct a basic array of the specified size and return just
    /// the reference to the underlying memory. All elements will be
    /// initialized to `T()`.
    static MemRef create_array(size_t size, Allocator&);

    static MemRef create_array(Array::Type leaf_type, bool context_flag, size_t size, T value, Allocator&);

    /// Create a new empty array and attach this accessor to it. This
    /// does not modify the parent reference information of this
    /// accessor.
    ///
    /// Note that the caller assumes ownership of the allocated
    /// underlying node. It is not owned by the accessor.
    void create(Array::Type = type_Normal, bool context_flag = false);

#ifdef REALM_DEBUG
    void to_dot(std::ostream&, StringData title = StringData()) const;
#endif

private:
    size_t find(T target, size_t begin, size_t end) const;

    size_t calc_byte_len(size_t count, size_t width) const override;
    virtual size_t calc_item_count(size_t bytes, size_t width) const noexcept override;

    template <bool find_max>
    bool minmax(T& result, size_t begin, size_t end) const;

    /// Calculate the total number of bytes needed for a basic array
    /// with the specified number of elements. This includes the size
    /// of the header. The result will be upwards aligned to the
    /// closest 8-byte boundary.
    static size_t calc_aligned_byte_size(size_t size);
};

template <class T>
class BasicArrayNull : public BasicArray<T> {
public:
    using BasicArray<T>::BasicArray;

    static T default_value(bool nullable)
    {
        return nullable ? null::get_null_float<T>() : T(0.0);
    }
    void set(size_t ndx, util::Optional<T> value)
    {
        if (value) {
            BasicArray<T>::set(ndx, *value);
        }
        else {
            BasicArray<T>::set(ndx, null::get_null_float<T>());
        }
    }
    void add(util::Optional<T> value)
    {
        if (value) {
            BasicArray<T>::add(*value);
        }
        else {
            BasicArray<T>::add(null::get_null_float<T>());
        }
    }
    void insert(size_t ndx, util::Optional<T> value)
    {
        if (value) {
            BasicArray<T>::insert(ndx, *value);
        }
        else {
            BasicArray<T>::insert(ndx, null::get_null_float<T>());
        }
    }

    void set_null(size_t ndx)
    {
        // FIXME: This assumes BasicArray will only ever be instantiated for float-like T.
        set(ndx, null::get_null_float<T>());
    }

    util::Optional<T> get(size_t ndx) const noexcept
    {
        T val = BasicArray<T>::get(ndx);
        return null::is_null_float(val) ? util::none : util::make_optional(val);
    }
    size_t find_first(util::Optional<T> value, size_t begin = 0, size_t end = npos) const
    {
        if (value) {
            return BasicArray<T>::find_first(*value, begin, end);
        }
        else {
            return find_first_null(begin, end);
        }
    }
    void find_all(IntegerColumn* result, util::Optional<T> value, size_t add_offset = 0, size_t begin = 0,
                  size_t end = npos) const
    {
        if (value) {
            return BasicArray<T>::find_all(result, *value, add_offset, begin, end);
        }
        else {
            return find_all_null(result, add_offset, begin, end);
        }
    }
    size_t find_first_null(size_t begin = 0, size_t end = npos) const;
    void find_all_null(IntegerColumn* result, size_t add_offset = 0, size_t begin = 0, size_t end = npos) const;
};


// Class typedefs for BasicArray's: ArrayFloat and ArrayDouble
typedef BasicArray<float> ArrayFloat;
typedef BasicArray<double> ArrayDouble;
typedef BasicArrayNull<float> ArrayFloatNull;
typedef BasicArrayNull<double> ArrayDoubleNull;

} // namespace realm

#include <realm/array_basic_tpl.hpp>

#endif // REALM_ARRAY_BASIC_HPP
