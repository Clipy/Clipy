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

#ifndef REALM_ARRAY_INTEGER_HPP
#define REALM_ARRAY_INTEGER_HPP

#include <realm/array.hpp>
#include <realm/util/safe_int_ops.hpp>
#include <realm/util/optional.hpp>

namespace realm {

class ArrayInteger : public Array, public ArrayPayload {
public:
    using value_type = int64_t;

    explicit ArrayInteger(Allocator&) noexcept;
    ~ArrayInteger() noexcept override
    {
    }

    static value_type default_value(bool)
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

    // Disable copying, this is not allowed.
    ArrayInteger& operator=(const ArrayInteger&) = delete;
    ArrayInteger(const ArrayInteger&) = delete;

    void create(Type type = type_Normal, bool context_flag = false);

    void add(int64_t value);
    void set(size_t ndx, int64_t value);
    void set_uint(size_t ndx, uint_fast64_t value) noexcept;
    int64_t get(size_t ndx) const noexcept;
    uint64_t get_uint(size_t ndx) const noexcept;
    static int64_t get(const char* header, size_t ndx) noexcept;
    bool compare(const ArrayInteger& a) const noexcept;
    void move(ArrayInteger& dst, size_t ndx)
    {
        Array::move(dst, ndx);
    }

    bool is_null(size_t) const
    {
        return false;
    }

    /// Add \a diff to the element at the specified index.
    void adjust(size_t ndx, int_fast64_t diff);

    /// Add \a diff to all the elements in the specified index range.
    void adjust(size_t begin, size_t end, int_fast64_t diff);

    /// Add signed \a diff to all elements that are greater than, or equal to \a
    /// limit.
    void adjust_ge(int_fast64_t limit, int_fast64_t diff);

    int64_t operator[](size_t ndx) const noexcept
    {
        return get(ndx);
    }
    int64_t front() const noexcept;
    int64_t back() const noexcept;

    size_t lower_bound(int64_t value) const noexcept;
    size_t upper_bound(int64_t value) const noexcept;

private:
    template <size_t w>
    bool minmax(size_t from, size_t to, uint64_t maxdiff, int64_t* min, int64_t* max) const;
};

class ArrayRef : public ArrayInteger {
public:
    using value_type = ref_type;

    explicit ArrayRef(Allocator& allocator) noexcept
        : ArrayInteger(allocator)
    {
    }

    void create(size_t sz = 0)
    {
        Array::create(type_HasRefs, false, sz, 0);
    }

    void add(ref_type value)
    {
        Array::add(from_ref(value));
    }

    void set(size_t ndx, ref_type value)
    {
        Array::set(ndx, from_ref(value));
    }

    void insert(size_t ndx, ref_type value)
    {
        Array::insert(ndx, from_ref(value));
    }

    ref_type get(size_t ndx) const noexcept
    {
        return to_ref(Array::get(ndx));
    }
    void verify() const;
};

class ArrayIntNull : public Array, public ArrayPayload {
public:
    using value_type = util::Optional<int64_t>;

    explicit ArrayIntNull(Allocator&) noexcept;
    ~ArrayIntNull() noexcept override;

    static value_type default_value(bool nullable)
    {
        return nullable ? util::none : util::Optional<int64_t>(0);
    }

    /// Construct an array of the specified type and size, and return just the
    /// reference to the underlying memory. All elements will be initialized to
    /// the specified value.
    static MemRef create_array(Type, bool context_flag, size_t size, Allocator&);
    void create(Type = type_Normal, bool context_flag = false);

    void init_from_ref(ref_type) noexcept override;
    void set_parent(ArrayParent* parent, size_t ndx_in_parent) noexcept override
    {
        Array::set_parent(parent, ndx_in_parent);
    }
    void init_from_mem(MemRef) noexcept;
    void init_from_parent() noexcept;

    size_t size() const noexcept;
    bool is_empty() const noexcept;

    void insert(size_t ndx, value_type value);
    void add(value_type value);
    void set(size_t ndx, value_type value);
    value_type get(size_t ndx) const noexcept;
    static value_type get(const char* header, size_t ndx) noexcept;
    void get_chunk(size_t ndx, value_type res[8]) const noexcept;
    void set_null(size_t ndx);
    bool is_null(size_t ndx) const noexcept;
    int64_t null_value() const noexcept;

    value_type operator[](size_t ndx) const noexcept;
    value_type front() const noexcept;
    value_type back() const noexcept;
    void erase(size_t ndx);
    void erase(size_t begin, size_t end);
    void move(ArrayIntNull& dst, size_t ndx);
    void clear();
    void set_all_to_zero();

    void move(size_t begin, size_t end, size_t dest_begin);

    size_t lower_bound(int64_t value) const noexcept;
    size_t upper_bound(int64_t value) const noexcept;

    int64_t sum(size_t start = 0, size_t end = npos) const;
    size_t count(int64_t value) const noexcept;
    bool maximum(int64_t& result, size_t start = 0, size_t end = npos, size_t* return_ndx = nullptr) const;
    bool minimum(int64_t& result, size_t start = 0, size_t end = npos, size_t* return_ndx = nullptr) const;

    bool find(int cond, Action action, value_type value, size_t start, size_t end, size_t baseindex,
              QueryState<int64_t>* state) const;

    template <class cond, Action action, size_t bitwidth, class Callback>
    bool find(value_type value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
              Callback callback) const;

    // This is the one installed into the m_finder slots.
    template <class cond, Action action, size_t bitwidth>
    bool find(int64_t value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state) const;

    template <class cond, Action action, class Callback>
    bool find(value_type value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
              Callback callback) const;

    // Optimized implementation for release mode
    template <class cond, Action action, size_t bitwidth, class Callback>
    bool find_optimized(value_type value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
                        Callback callback) const;

    // Called for each search result
    template <Action action, class Callback>
    bool find_action(size_t index, value_type value, QueryState<int64_t>* state, Callback callback) const;

    template <Action action, class Callback>
    bool find_action_pattern(size_t index, uint64_t pattern, QueryState<int64_t>* state, Callback callback) const;

    // Wrappers for backwards compatibility and for simple use without
    // setting up state initialization etc
    template <class cond>
    size_t find_first(value_type value, size_t start = 0, size_t end = npos) const;

    void find_all(IntegerColumn* result, value_type value, size_t col_offset = 0, size_t begin = 0,
                  size_t end = npos) const;


    size_t find_first(value_type value, size_t begin = 0, size_t end = npos) const;

protected:
    void avoid_null_collision(int64_t value);

private:
    template <bool find_max>
    bool minmax_helper(int64_t& result, size_t start = 0, size_t end = npos, size_t* return_ndx = nullptr) const;

    int_fast64_t choose_random_null(int64_t incoming) const;
    void replace_nulls_with(int64_t new_null);
    bool can_use_as_null(int64_t value) const;
};


// Implementation:

inline ArrayInteger::ArrayInteger(Allocator& allocator) noexcept
    : Array(allocator)
{
    m_is_inner_bptree_node = false;
}

inline void ArrayInteger::add(int64_t value)
{
    Array::add(value);
}

inline int64_t ArrayInteger::get(size_t ndx) const noexcept
{
    return Array::get(ndx);
}

inline int64_t ArrayInteger::get(const char* header, size_t ndx) noexcept
{
    return Array::get(header, ndx);
}

inline void ArrayInteger::set(size_t ndx, int64_t value)
{
    Array::set(ndx, value);
}

inline void ArrayInteger::set_uint(size_t ndx, uint_fast64_t value) noexcept
{
    // When a value of a signed type is converted to an unsigned type, the C++
    // standard guarantees that negative values are converted from the native
    // representation to 2's complement, but the effect of conversions in the
    // opposite direction is left unspecified by the
    // standard. `realm::util::from_twos_compl()` is used here to perform the
    // correct opposite unsigned-to-signed conversion, which reduces to a no-op
    // when 2's complement is the native representation of negative values.
    set(ndx, util::from_twos_compl<int_fast64_t>(value));
}

inline bool ArrayInteger::compare(const ArrayInteger& a) const noexcept
{
    if (a.size() != size())
        return false;

    for (size_t i = 0; i < size(); ++i) {
        if (get(i) != a.get(i))
            return false;
    }

    return true;
}

inline int64_t ArrayInteger::front() const noexcept
{
    return Array::front();
}

inline int64_t ArrayInteger::back() const noexcept
{
    return Array::back();
}

inline void ArrayInteger::adjust(size_t ndx, int_fast64_t diff)
{
    Array::adjust(ndx, diff);
}

inline void ArrayInteger::adjust(size_t begin, size_t end, int_fast64_t diff)
{
    Array::adjust(begin, end, diff);
}

inline void ArrayInteger::adjust_ge(int_fast64_t limit, int_fast64_t diff)
{
    Array::adjust_ge(limit, diff);
}

inline size_t ArrayInteger::lower_bound(int64_t value) const noexcept
{
    return lower_bound_int(value);
}

inline size_t ArrayInteger::upper_bound(int64_t value) const noexcept
{
    return upper_bound_int(value);
}


inline ArrayIntNull::ArrayIntNull(Allocator& allocator) noexcept
    : Array(allocator)
{
}

inline ArrayIntNull::~ArrayIntNull() noexcept
{
}

inline void ArrayIntNull::create(Type type, bool context_flag)
{
    MemRef r = create_array(type, context_flag, 0, m_alloc);
    init_from_mem(r);
}


inline size_t ArrayIntNull::size() const noexcept
{
    return Array::size() - 1;
}

inline bool ArrayIntNull::is_empty() const noexcept
{
    return size() == 0;
}

inline void ArrayIntNull::insert(size_t ndx, value_type value)
{
    if (value) {
        avoid_null_collision(*value);
        Array::insert(ndx + 1, *value);
    }
    else {
        Array::insert(ndx + 1, null_value());
    }
}

inline void ArrayIntNull::add(value_type value)
{
    if (value) {
        avoid_null_collision(*value);
        Array::add(*value);
    }
    else {
        Array::add(null_value());
    }
}

inline void ArrayIntNull::set(size_t ndx, value_type value)
{
    if (value) {
        avoid_null_collision(*value);
        Array::set(ndx + 1, *value);
    }
    else {
        Array::set(ndx + 1, null_value());
    }
}

inline void ArrayIntNull::set_null(size_t ndx)
{
    Array::set(ndx + 1, null_value());
}

inline ArrayIntNull::value_type ArrayIntNull::get(size_t ndx) const noexcept
{
    int64_t value = Array::get(ndx + 1);
    if (value == null_value()) {
        return util::none;
    }
    return util::some<int64_t>(value);
}

inline ArrayIntNull::value_type ArrayIntNull::get(const char* header, size_t ndx) noexcept
{
    int64_t null_value = Array::get(header, 0);
    int64_t value = Array::get(header, ndx + 1);
    if (value == null_value) {
        return util::none;
    }
    else {
        return util::some<int64_t>(value);
    }
}

inline bool ArrayIntNull::is_null(size_t ndx) const noexcept
{
    return !get(ndx);
}

inline int64_t ArrayIntNull::null_value() const noexcept
{
    return Array::get(0);
}

inline ArrayIntNull::value_type ArrayIntNull::operator[](size_t ndx) const noexcept
{
    return get(ndx);
}

inline ArrayIntNull::value_type ArrayIntNull::front() const noexcept
{
    return get(0);
}

inline ArrayIntNull::value_type ArrayIntNull::back() const noexcept
{
    return Array::back();
}

inline void ArrayIntNull::erase(size_t ndx)
{
    Array::erase(ndx + 1);
}

inline void ArrayIntNull::erase(size_t begin, size_t end)
{
    Array::erase(begin + 1, end + 1);
}

inline void ArrayIntNull::clear()
{
    Array::truncate(0);
    Array::add(0);
}

inline void ArrayIntNull::set_all_to_zero()
{
    // FIXME: Array::set_all_to_zero does something else
    for (size_t i = 0; i < size(); ++i) {
        set(i, 0);
    }
}

inline void ArrayIntNull::move(size_t begin, size_t end, size_t dest_begin)
{
    Array::move(begin + 1, end + 1, dest_begin + 1);
}

inline size_t ArrayIntNull::lower_bound(int64_t value) const noexcept
{
    // FIXME: Consider this behaviour with NULLs.
    // Array::lower_bound_int assumes an already sorted array, but
    // this array could be sorted with nulls first or last.
    return Array::lower_bound_int(value);
}

inline size_t ArrayIntNull::upper_bound(int64_t value) const noexcept
{
    // FIXME: see lower_bound
    return Array::upper_bound_int(value);
}

inline int64_t ArrayIntNull::sum(size_t start, size_t end) const
{
    // FIXME: Optimize
    int64_t sum_of_range = 0;
    if (end == npos)
        end = size();
    for (size_t i = start; i < end; ++i) {
        value_type x = get(i);
        if (x) {
            sum_of_range += *x;
        }
    }
    return sum_of_range;
}

inline size_t ArrayIntNull::count(int64_t value) const noexcept
{
    size_t count_of_value = Array::count(value);
    if (value == null_value()) {
        --count_of_value;
    }
    return count_of_value;
}

// FIXME: Optimize
template <bool find_max>
inline bool ArrayIntNull::minmax_helper(int64_t& result, size_t start, size_t end, size_t* return_ndx) const
{
    size_t best_index = 1;

    if (end == npos) {
        end = m_size;
    }

    ++start;

    REALM_ASSERT(start < m_size && end <= m_size && start < end);

    if (m_size == 1) {
        // empty array
        return false;
    }

    if (m_width == 0) {
        if (return_ndx)
            *return_ndx = best_index - 1;
        result = 0;
        return true;
    }

    int64_t m = Array::get(start);

    const int64_t null_val = null_value();
    for (; start < end; ++start) {
        const int64_t v = Array::get(start);
        if (find_max ? v > m : v < m) {
            if (v == null_val) {
                continue;
            }
            m = v;
            best_index = start;
        }
    }

    result = m;
    if (return_ndx) {
        *return_ndx = best_index - 1;
    }
    return true;
}

inline bool ArrayIntNull::maximum(int64_t& result, size_t start, size_t end, size_t* return_ndx) const
{
    return minmax_helper<true>(result, start, end, return_ndx);
}

inline bool ArrayIntNull::minimum(int64_t& result, size_t start, size_t end, size_t* return_ndx) const
{
    return minmax_helper<false>(result, start, end, return_ndx);
}

inline bool ArrayIntNull::find(int cond, Action action, value_type value, size_t start, size_t end, size_t baseindex,
                               QueryState<int64_t>* state) const
{
    if (value) {
        return Array::find(cond, action, *value, start, end, baseindex, state, true /*treat as nullable array*/,
                           false /*search parameter given in 'value' argument*/);
    }
    else {
        return Array::find(cond, action, 0 /* unused dummy*/, start, end, baseindex, state,
                           true /*treat as nullable array*/, true /*search for null, ignore value argument*/);
    }
}

template <class cond, Action action, size_t bitwidth, class Callback>
bool ArrayIntNull::find(value_type value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
                        Callback callback) const
{
    if (value) {
        return Array::find<cond, action>(*value, start, end, baseindex, state, std::forward<Callback>(callback),
                                         true /*treat as nullable array*/,
                                         false /*search parameter given in 'value' argument*/);
    }
    else {
        return Array::find<cond, action>(0 /*ignored*/, start, end, baseindex, state,
                                         std::forward<Callback>(callback), true /*treat as nullable array*/,
                                         true /*search for null, ignore value argument*/);
    }
}


template <class cond, Action action, size_t bitwidth>
bool ArrayIntNull::find(int64_t value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state) const
{
    return Array::find<cond, action>(value, start, end, baseindex, state, true /*treat as nullable array*/,
                                     false /*search parameter given in 'value' argument*/);
}


template <class cond, Action action, class Callback>
bool ArrayIntNull::find(value_type value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
                        Callback callback) const
{
    if (value) {
        return Array::find<cond, action>(*value, start, end, baseindex, state, std::forward<Callback>(callback),
                                         true /*treat as nullable array*/,
                                         false /*search parameter given in 'value' argument*/);
    }
    else {
        return Array::find<cond, action>(0 /*ignored*/, start, end, baseindex, state,
                                         std::forward<Callback>(callback), true /*treat as nullable array*/,
                                         true /*search for null, ignore value argument*/);
    }
}


template <Action action, class Callback>
bool ArrayIntNull::find_action(size_t index, value_type value, QueryState<int64_t>* state, Callback callback) const
{
    if (value) {
        return Array::find_action<action, Callback>(index, *value, state, callback, true /*treat as nullable array*/,
                                                    false /*search parameter given in 'value' argument*/);
    }
    else {
        return Array::find_action<action, Callback>(index, 0 /* ignored */, state, callback,
                                                    true /*treat as nullable array*/,
                                                    true /*search for null, ignore value argument*/);
    }
}


template <Action action, class Callback>
bool ArrayIntNull::find_action_pattern(size_t index, uint64_t pattern, QueryState<int64_t>* state,
                                       Callback callback) const
{
    return Array::find_action_pattern<action, Callback>(index, pattern, state, callback,
                                                        true /*treat as nullable array*/,
                                                        false /*search parameter given in 'value' argument*/);
}


template <class cond>
size_t ArrayIntNull::find_first(value_type value, size_t start, size_t end) const
{
    QueryState<int64_t> state(act_ReturnFirst, 1);
    if (value) {
        Array::find<cond, act_ReturnFirst>(*value, start, end, 0, &state, Array::CallbackDummy(),
                                           true /*treat as nullable array*/,
                                           false /*search parameter given in 'value' argument*/);
    }
    else {
        Array::find<cond, act_ReturnFirst>(0 /*ignored*/, start, end, 0, &state, Array::CallbackDummy(),
                                           true /*treat as nullable array*/,
                                           true /*search for null, ignore value argument*/);
    }

    if (state.m_match_count > 0)
        return to_size_t(state.m_state);
    else
        return not_found;
}

inline size_t ArrayIntNull::find_first(value_type value, size_t begin, size_t end) const
{
    return find_first<Equal>(value, begin, end);
}
}

#endif // REALM_ARRAY_INTEGER_HPP
