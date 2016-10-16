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
class BasicArray : public Array {
public:
    explicit BasicArray(Allocator&) noexcept;
    ~BasicArray() noexcept override
    {
    }

    T get(size_t ndx) const noexcept;
    bool is_null(size_t ndx) const noexcept;
    void add(T value);
    void set(size_t ndx, T value);
    void set_null(size_t ndx);
    void insert(size_t ndx, T value);
    void erase(size_t ndx);
    void truncate(size_t size);
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

    ref_type bptree_leaf_insert(size_t ndx, T, TreeInsertBase& state);

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

    /// Construct a copy of the specified slice of this basic array
    /// using the specified target allocator.
    MemRef slice(size_t offset, size_t size, Allocator& target_alloc) const;
    MemRef slice_and_clone_children(size_t offset, size_t size, Allocator& target_alloc) const;

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


// Class typedefs for BasicArray's: ArrayFloat and ArrayDouble
typedef BasicArray<float> ArrayFloat;
typedef BasicArray<double> ArrayDouble;

} // namespace realm

#include <realm/array_basic_tpl.hpp>

#endif // REALM_ARRAY_BASIC_HPP
