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

/*
Searching: The main finding function is:
    template <class cond, Action action, size_t bitwidth, class Callback>
    void find(int64_t value, size_t start, size_t end, size_t baseindex, QueryState *state, Callback callback) const

    cond:       One of Equal, NotEqual, Greater, etc. classes
    Action:     One of act_ReturnFirst, act_FindAll, act_Max, act_CallbackIdx, etc, constants
    Callback:   Optional function to call for each search result. Will be called if action == act_CallbackIdx

    find() will call find_action_pattern() or find_action() that again calls match() for each search result which
    optionally calls callback():

        find() -> find_action() -------> bool match() -> bool callback()
             |                            ^
             +-> find_action_pattern()----+

    If callback() returns false, find() will exit, otherwise it will keep searching remaining items in array.
*/

#ifndef REALM_ARRAY_HPP
#define REALM_ARRAY_HPP

#include <realm/node.hpp>

#include <cmath>
#include <cstdlib> // size_t
#include <algorithm>
#include <utility>
#include <vector>
#include <ostream>

#include <cstdint> // unint8_t etc

#include <realm/util/assert.hpp>
#include <realm/util/file_mapper.hpp>
#include <realm/utilities.hpp>
#include <realm/alloc.hpp>
#include <realm/string_data.hpp>
#include <realm/query_conditions.hpp>
#include <realm/column_fwd.hpp>
#include <realm/array_direct.hpp>
#include <realm/array_unsigned.hpp>

/*
    MMX: mmintrin.h
    SSE: xmmintrin.h
    SSE2: emmintrin.h
    SSE3: pmmintrin.h
    SSSE3: tmmintrin.h
    SSE4A: ammintrin.h
    SSE4.1: smmintrin.h
    SSE4.2: nmmintrin.h
*/
#ifdef REALM_COMPILER_SSE
#include <emmintrin.h>             // SSE2
#include <realm/realm_nmmintrin.h> // SSE42
#endif

namespace realm {

template <class T>
inline T no0(T v)
{
    return v == 0 ? 1 : v;
}

// Pre-definitions
struct ObjKey;
class Array;
class GroupWriter;
namespace _impl {
class ArrayWriterBase;
}

template <class T>
class BPlusTree;

using KeyColumn = BPlusTree<ObjKey>;


struct MemStats {
    size_t allocated = 0;
    size_t used = 0;
    size_t array_count = 0;
};

#ifdef REALM_DEBUG
template <class C, class T>
std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>& out, MemStats stats);
#endif


// Stores a value obtained from Array::get(). It is a ref if the least
// significant bit is clear, otherwise it is a tagged integer. A tagged interger
// is obtained from a logical integer value by left shifting by one bit position
// (multiplying by two), and then setting the least significant bit to
// one. Clearly, this means that the maximum value that can be stored as a
// tagged integer is 2**63 - 1.
class RefOrTagged {
public:
    bool is_ref() const noexcept;
    bool is_tagged() const noexcept;
    ref_type get_as_ref() const noexcept;
    uint_fast64_t get_as_int() const noexcept;

    static RefOrTagged make_ref(ref_type) noexcept;
    static RefOrTagged make_tagged(uint_fast64_t) noexcept;

private:
    int_fast64_t m_value;
    RefOrTagged(int_fast64_t) noexcept;
    friend class Array;
};


struct TreeInsertBase {
    size_t m_split_offset;
    size_t m_split_size;
};

class Array : public Node, public ArrayParent {
public:
    //    void state_init(int action, QueryState *state);
    //    bool match(int action, size_t index, int64_t value, QueryState *state);

    /// Create an array accessor in the unattached state.
    explicit Array(Allocator& allocator) noexcept
        : Node(allocator)
    {
    }

    ~Array() noexcept override
    {
    }

    /// Create a new integer array of the specified type and size, and filled
    /// with the specified value, and attach this accessor to it. This does not
    /// modify the parent reference information of this accessor.
    ///
    /// Note that the caller assumes ownership of the allocated underlying
    /// node. It is not owned by the accessor.
    void create(Type, bool context_flag = false, size_t size = 0, int_fast64_t value = 0);

    /// Reinitialize this array accessor to point to the specified new
    /// underlying memory. This does not modify the parent reference information
    /// of this accessor.
    void init_from_ref(ref_type ref) noexcept
    {
        REALM_ASSERT_DEBUG(ref);
        char* header = m_alloc.translate(ref);
        init_from_mem(MemRef(header, ref, m_alloc));
    }

    /// Same as init_from_ref(ref_type) but avoid the mapping of 'ref' to memory
    /// pointer.
    void init_from_mem(MemRef) noexcept;

    /// Same as `init_from_ref(get_ref_from_parent())`.
    void init_from_parent() noexcept
    {
        ref_type ref = get_ref_from_parent();
        init_from_ref(ref);
    }

    /// Called in the context of Group::commit() to ensure that attached
    /// accessors stay valid across a commit. Please note that this works only
    /// for non-transactional commits. Accessors obtained during a transaction
    /// are always detached when the transaction ends.
    ///
    /// Returns true if, and only if the array has changed. If the array has not
    /// changed, then its children are guaranteed to also not have changed.
    bool update_from_parent(size_t old_baseline) noexcept;

    /// Change the type of an already attached array node.
    ///
    /// The effect of calling this function on an unattached accessor is
    /// undefined.
    void set_type(Type);

    /// Construct a complete copy of this array (including its subarrays) using
    /// the specified target allocator and return just the reference to the
    /// underlying memory.
    MemRef clone_deep(Allocator& target_alloc) const;

    /// Construct an empty integer array of the specified type, and return just
    /// the reference to the underlying memory.
    static MemRef create_empty_array(Type, bool context_flag, Allocator&);

    /// Construct an integer array of the specified type and size, and return
    /// just the reference to the underlying memory. All elements will be
    /// initialized to the specified value.
    static MemRef create_array(Type, bool context_flag, size_t size, int_fast64_t value, Allocator&);

    Type get_type() const noexcept;


    static void add_to_column(IntegerColumn* column, int64_t value);
    static void add_to_column(KeyColumn* column, int64_t value);

    void insert(size_t ndx, int_fast64_t value);
    void add(int_fast64_t value);

    // Used from ArrayBlob
    size_t blob_size() const noexcept;
    ref_type blob_replace(size_t begin, size_t end, const char* data, size_t data_size, bool add_zero_term);

    /// This function is guaranteed to not throw if the current width is
    /// sufficient for the specified value (e.g. if you have called
    /// ensure_minimum_width(value)) and get_alloc().is_read_only(get_ref())
    /// returns false (noexcept:array-set). Note that for a value of zero, the
    /// first criterion is trivially satisfied.
    void set(size_t ndx, int64_t value);

    void set_as_ref(size_t ndx, ref_type ref);

    template <size_t w>
    void set(size_t ndx, int64_t value);

    int64_t get(size_t ndx) const noexcept;

    template <size_t w>
    int64_t get(size_t ndx) const noexcept;

    void get_chunk(size_t ndx, int64_t res[8]) const noexcept;

    template <size_t w>
    void get_chunk(size_t ndx, int64_t res[8]) const noexcept;

    ref_type get_as_ref(size_t ndx) const noexcept;

    RefOrTagged get_as_ref_or_tagged(size_t ndx) const noexcept;
    void set(size_t ndx, RefOrTagged);
    void add(RefOrTagged);
    void ensure_minimum_width(RefOrTagged);

    int64_t front() const noexcept;
    int64_t back() const noexcept;

    /// Remove the element at the specified index, and move elements at higher
    /// indexes to the next lower index.
    ///
    /// This function does **not** destroy removed subarrays. That is, if the
    /// erased element is a 'ref' pointing to a subarray, then that subarray
    /// will not be destroyed automatically.
    ///
    /// This function guarantees that no exceptions will be thrown if
    /// get_alloc().is_read_only(get_ref()) would return false before the
    /// call. This is automatically guaranteed if the array is used in a
    /// non-transactional context, or if the array has already been successfully
    /// modified within the current write transaction.
    void erase(size_t ndx);

    /// Same as erase(size_t), but remove all elements in the specified
    /// range.
    ///
    /// Please note that this function does **not** destroy removed subarrays.
    ///
    /// This function guarantees that no exceptions will be thrown if
    /// get_alloc().is_read_only(get_ref()) would return false before the call.
    void erase(size_t begin, size_t end);

    /// Reduce the size of this array to the specified number of elements. It is
    /// an error to specify a size that is greater than the current size of this
    /// array. The effect of doing so is undefined. This is just a shorthand for
    /// calling the ranged erase() function with appropriate arguments.
    ///
    /// Please note that this function does **not** destroy removed
    /// subarrays. See clear_and_destroy_children() for an alternative.
    ///
    /// This function guarantees that no exceptions will be thrown if
    /// get_alloc().is_read_only(get_ref()) would return false before the call.
    void truncate(size_t new_size);

    /// Reduce the size of this array to the specified number of elements. It is
    /// an error to specify a size that is greater than the current size of this
    /// array. The effect of doing so is undefined. Subarrays will be destroyed
    /// recursively, as if by a call to `destroy_deep(subarray_ref, alloc)`.
    ///
    /// This function is guaranteed not to throw if
    /// get_alloc().is_read_only(get_ref()) returns false.
    void truncate_and_destroy_children(size_t new_size);

    /// Remove every element from this array. This is just a shorthand for
    /// calling truncate(0).
    ///
    /// Please note that this function does **not** destroy removed
    /// subarrays. See clear_and_destroy_children() for an alternative.
    ///
    /// This function guarantees that no exceptions will be thrown if
    /// get_alloc().is_read_only(get_ref()) would return false before the call.
    void clear();

    /// Remove every element in this array. Subarrays will be destroyed
    /// recursively, as if by a call to `destroy_deep(subarray_ref,
    /// alloc)`. This is just a shorthand for calling
    /// truncate_and_destroy_children(0).
    ///
    /// This function guarantees that no exceptions will be thrown if
    /// get_alloc().is_read_only(get_ref()) would return false before the call.
    void clear_and_destroy_children();

    /// If neccessary, expand the representation so that it can store the
    /// specified value.
    void ensure_minimum_width(int_fast64_t value);

    /// This one may change the represenation of the array, so be carefull if
    /// you call it after ensure_minimum_width().
    void set_all_to_zero();

    /// Add \a diff to the element at the specified index.
    void adjust(size_t ndx, int_fast64_t diff);

    /// Add \a diff to all the elements in the specified index range.
    void adjust(size_t begin, size_t end, int_fast64_t diff);

    /// Add signed \a diff to all elements that are greater than, or equal to \a
    /// limit.
    void adjust_ge(int_fast64_t limit, int_fast64_t diff);

    //@{
    /// This is similar in spirit to std::move() from `<algorithm>`.
    /// \a dest_begin must not be in the range [`begin`,`end`)
    ///
    /// This function is guaranteed to not throw if
    /// `get_alloc().is_read_only(get_ref())` returns false.
    void move(size_t begin, size_t end, size_t dest_begin);
    //@}

    // Move elements from ndx and above to another array
    void move(Array& dst, size_t ndx);

    //@{
    /// Find the lower/upper bound of the specified value in a sequence of
    /// integers which must already be sorted ascendingly.
    ///
    /// For an integer value '`v`', lower_bound_int(v) returns the index '`l`'
    /// of the first element such that `get(l) &ge; v`, and upper_bound_int(v)
    /// returns the index '`u`' of the first element such that `get(u) &gt;
    /// v`. In both cases, if no such element is found, the returned value is
    /// the number of elements in the array.
    ///
    ///     3 3 3 4 4 4 5 6 7 9 9 9
    ///     ^     ^     ^     ^     ^
    ///     |     |     |     |     |
    ///     |     |     |     |      -- Lower and upper bound of 15
    ///     |     |     |     |
    ///     |     |     |      -- Lower and upper bound of 8
    ///     |     |     |
    ///     |     |      -- Upper bound of 4
    ///     |     |
    ///     |      -- Lower bound of 4
    ///     |
    ///      -- Lower and upper bound of 1
    ///
    /// These functions are similar to std::lower_bound() and
    /// std::upper_bound().
    ///
    /// We currently use binary search. See for example
    /// http://www.tbray.org/ongoing/When/200x/2003/03/22/Binary.
    ///
    /// FIXME: It may be worth considering if overall efficiency can be improved
    /// by doing a linear search for short sequences.
    size_t lower_bound_int(int64_t value) const noexcept;
    size_t upper_bound_int(int64_t value) const noexcept;
    //@}

    /// \brief Search the \c Array for a value greater or equal than \a target,
    /// starting the search at the \a start index.
    ///
    /// The \c Array must be sorted in ascending order.
    ///
    /// Behaviour is undefined if:
    /// - sorting conditions are not respected;
    /// - \a start is greater than the number of elements in this \c Array
    ///
    /// \param target the smallest value to search for
    /// \param start the offset at which to start searching in the array
    /// \return the index of the value if found, or realm::not_found otherwise
    size_t find_gte(const int64_t target, size_t start, size_t end = size_t(-1)) const;

    int64_t sum(size_t start = 0, size_t end = size_t(-1)) const;
    size_t count(int64_t value) const noexcept;

    bool maximum(int64_t& result, size_t start = 0, size_t end = size_t(-1), size_t* return_ndx = nullptr) const;

    bool minimum(int64_t& result, size_t start = 0, size_t end = size_t(-1), size_t* return_ndx = nullptr) const;

    /// This information is guaranteed to be cached in the array accessor.
    bool is_inner_bptree_node() const noexcept;

    /// Returns true if type is either type_HasRefs or type_InnerColumnNode.
    ///
    /// This information is guaranteed to be cached in the array accessor.
    bool has_refs() const noexcept;
    void set_has_refs(bool) noexcept;

    /// This information is guaranteed to be cached in the array accessor.
    ///
    /// Columns and indexes can use the context bit to differentiate leaf types.
    bool get_context_flag() const noexcept;
    void set_context_flag(bool) noexcept;

    /// Recursively destroy children (as if calling
    /// clear_and_destroy_children()), then put this accessor into the detached
    /// state (as if calling detach()), then free the allocated memory. If this
    /// accessor is already in the detached state, this function has no effect
    /// (idempotency).
    void destroy_deep() noexcept;

    /// Shorthand for `destroy_deep(MemRef(ref, alloc), alloc)`.
    static void destroy_deep(ref_type ref, Allocator& alloc) noexcept;

    /// Destroy the specified array node and all of its children, recursively.
    ///
    /// This is done by freeing the specified array node after calling
    /// destroy_deep() for every contained 'ref' element.
    static void destroy_deep(MemRef, Allocator&) noexcept;

    // Clone deep
    static MemRef clone(MemRef, Allocator& from_alloc, Allocator& target_alloc);

    // Serialization

    /// Returns the ref (position in the target stream) of the written copy of
    /// this array, or the ref of the original array if \a only_if_modified is
    /// true, and this array is unmodified (Alloc::is_read_only()).
    ///
    /// The number of bytes that will be written by a non-recursive invocation
    /// of this function is exactly the number returned by get_byte_size().
    ///
    /// \param out The destination stream (writer).
    ///
    /// \param deep If true, recursively write out subarrays, but still subject
    /// to \a only_if_modified.
    ///
    /// \param only_if_modified Set to `false` to always write, or to `true` to
    /// only write the array if it has been modified.
    ref_type write(_impl::ArrayWriterBase& out, bool deep, bool only_if_modified) const;

    /// Same as non-static write() with `deep` set to true. This is for the
    /// cases where you do not already have an array accessor available.
    static ref_type write(ref_type, Allocator&, _impl::ArrayWriterBase&, bool only_if_modified);

    // Main finding function - used for find_first, find_all, sum, max, min, etc.
    bool find(int cond, Action action, int64_t value, size_t start, size_t end, size_t baseindex,
              QueryState<int64_t>* state, bool nullable_array = false, bool find_null = false) const;

    // Templated find function to avoid conversion to and from integer represenation of condition
    template <class cond>
    bool find(Action action, int64_t value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
              bool nullable_array = false, bool find_null = false) const
    {
        if (action == act_ReturnFirst) {
            REALM_TEMPEX3(return find, cond, act_ReturnFirst, m_width,
                                 (value, start, end, baseindex, state, CallbackDummy(), nullable_array, find_null))
        }
        else if (action == act_Sum) {
            REALM_TEMPEX3(return find, cond, act_Sum, m_width,
                                 (value, start, end, baseindex, state, CallbackDummy(), nullable_array, find_null))
        }
        else if (action == act_Min) {
            REALM_TEMPEX3(return find, cond, act_Min, m_width,
                                 (value, start, end, baseindex, state, CallbackDummy(), nullable_array, find_null))
        }
        else if (action == act_Max) {
            REALM_TEMPEX3(return find, cond, act_Max, m_width,
                                 (value, start, end, baseindex, state, CallbackDummy(), nullable_array, find_null))
        }
        else if (action == act_Count) {
            REALM_TEMPEX3(return find, cond, act_Count, m_width,
                                 (value, start, end, baseindex, state, CallbackDummy(), nullable_array, find_null))
        }
        else if (action == act_FindAll) {
            REALM_TEMPEX3(return find, cond, act_FindAll, m_width,
                                 (value, start, end, baseindex, state, CallbackDummy(), nullable_array, find_null))
        }
        else if (action == act_CallbackIdx) {
            REALM_TEMPEX3(return find, cond, act_CallbackIdx, m_width,
                                 (value, start, end, baseindex, state, CallbackDummy(), nullable_array, find_null))
        }
        REALM_ASSERT_DEBUG(false);
        return false;
    }


    /*
    bool find(int cond, Action action, null, size_t start, size_t end, size_t baseindex,
              QueryState<int64_t>* state) const;
    */

    template <class cond, Action action, size_t bitwidth, class Callback>
    bool find(int64_t value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
              Callback callback, bool nullable_array = false, bool find_null = false) const;

    // This is the one installed into the m_vtable->finder slots.
    template <class cond, Action action, size_t bitwidth>
    bool find(int64_t value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state) const;

    template <class cond, Action action, class Callback>
    bool find(int64_t value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
              Callback callback, bool nullable_array = false, bool find_null = false) const;

    /*
    template <class cond, Action action, class Callback>
    bool find(null, size_t start, size_t end, size_t baseindex,
              QueryState<int64_t>* state, Callback callback) const;
    */

    // Optimized implementation for release mode
    template <class cond, Action action, size_t bitwidth, class Callback>
    bool find_optimized(int64_t value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
                        Callback callback, bool nullable_array = false, bool find_null = false) const;

    // Called for each search result
    template <Action action, class Callback>
    bool find_action(size_t index, util::Optional<int64_t> value, QueryState<int64_t>* state,
                     Callback callback) const;

    template <Action action, class Callback>
    bool find_action_pattern(size_t index, uint64_t pattern, QueryState<int64_t>* state, Callback callback) const;

    // Wrappers for backwards compatibility and for simple use without
    // setting up state initialization etc
    template <class cond>
    size_t find_first(int64_t value, size_t start = 0, size_t end = size_t(-1)) const;

    void find_all(IntegerColumn* result, int64_t value, size_t col_offset = 0, size_t begin = 0,
                  size_t end = size_t(-1)) const;

    size_t find_first(int64_t value, size_t begin = 0, size_t end = size_t(-1)) const;

    // Non-SSE find for the four functions Equal/NotEqual/Less/Greater
    template <class cond, Action action, size_t bitwidth, class Callback>
    bool compare(int64_t value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
                 Callback callback) const;

    // Non-SSE find for Equal/NotEqual
    template <bool eq, Action action, size_t width, class Callback>
    inline bool compare_equality(int64_t value, size_t start, size_t end, size_t baseindex,
                                 QueryState<int64_t>* state, Callback callback) const;

    // Non-SSE find for Less/Greater
    template <bool gt, Action action, size_t bitwidth, class Callback>
    bool compare_relation(int64_t value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
                          Callback callback) const;

    template <class cond, Action action, size_t foreign_width, class Callback, size_t width>
    bool compare_leafs_4(const Array* foreign, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
                         Callback callback) const;

    template <class cond, Action action, class Callback, size_t bitwidth, size_t foreign_bitwidth>
    bool compare_leafs(const Array* foreign, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
                       Callback callback) const;

    template <class cond, Action action, class Callback>
    bool compare_leafs(const Array* foreign, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
                       Callback callback) const;

    template <class cond, Action action, size_t width, class Callback>
    bool compare_leafs(const Array* foreign, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
                       Callback callback) const;

// SSE find for the four functions Equal/NotEqual/Less/Greater
#ifdef REALM_COMPILER_SSE
    template <class cond, Action action, size_t width, class Callback>
    bool find_sse(int64_t value, __m128i* data, size_t items, QueryState<int64_t>* state, size_t baseindex,
                  Callback callback) const;

    template <class cond, Action action, size_t width, class Callback>
    REALM_FORCEINLINE bool find_sse_intern(__m128i* action_data, __m128i* data, size_t items,
                                           QueryState<int64_t>* state, size_t baseindex, Callback callback) const;

#endif

    template <size_t width>
    inline bool test_zero(uint64_t value) const; // Tests value for 0-elements

    template <bool eq, size_t width>
    size_t find_zero(uint64_t v) const; // Finds position of 0/non-zero element

    template <size_t width, bool zero>
    uint64_t cascade(uint64_t a) const; // Sets lowermost bits of zero or non-zero elements

    template <bool gt, size_t width>
    int64_t
    find_gtlt_magic(int64_t v) const; // Compute magic constant needed for searching for value 'v' using bit hacks

    template <size_t width>
    inline int64_t lower_bits() const; // Return chunk with lower bit set in each element

    size_t first_set_bit(unsigned int v) const;
    size_t first_set_bit64(int64_t v) const;

    template <size_t w>
    int64_t get_universal(const char* const data, const size_t ndx) const;

    // Find value greater/less in 64-bit chunk - only works for positive values
    template <bool gt, Action action, size_t width, class Callback>
    bool find_gtlt_fast(uint64_t chunk, uint64_t magic, QueryState<int64_t>* state, size_t baseindex,
                        Callback callback) const;

    // Find value greater/less in 64-bit chunk - no constraints
    template <bool gt, Action action, size_t width, class Callback>
    bool find_gtlt(int64_t v, uint64_t chunk, QueryState<int64_t>* state, size_t baseindex, Callback callback) const;

    /// Get the specified element without the cost of constructing an
    /// array instance. If an array instance is already available, or
    /// you need to get multiple values, then this method will be
    /// slower.
    static int_fast64_t get(const char* header, size_t ndx) noexcept;

    /// Like get(const char*, size_t) but gets two consecutive
    /// elements.
    static std::pair<int64_t, int64_t> get_two(const char* header, size_t ndx) noexcept;

    static void get_three(const char* data, size_t ndx, ref_type& v0, ref_type& v1, ref_type& v2) noexcept;

    static RefOrTagged get_as_ref_or_tagged(const char* header, size_t ndx) noexcept
    {
        return get(header, ndx);
    }

    /// Get the number of bytes currently in use by this array. This
    /// includes the array header, but it does not include allocated
    /// bytes corresponding to excess capacity. The result is
    /// guaranteed to be a multiple of 8 (i.e., 64-bit aligned).
    ///
    /// This number is exactly the number of bytes that will be
    /// written by a non-recursive invocation of write().
    size_t get_byte_size() const noexcept;

    /// Get the maximum number of bytes that can be written by a
    /// non-recursive invocation of write() on an array with the
    /// specified number of elements, that is, the maximum value that
    /// can be returned by get_byte_size().
    static size_t get_max_byte_size(size_t num_elems) noexcept;

    /// FIXME: Belongs in IntegerArray
    static size_t calc_aligned_byte_size(size_t size, int width);

    class MemUsageHandler {
    public:
        virtual void handle(ref_type ref, size_t allocated, size_t used) = 0;
    };

    void report_memory_usage(MemUsageHandler&) const;

    void stats(MemStats& stats_dest) const noexcept;

    void verify() const;

#ifdef REALM_DEBUG
    void print() const;
    typedef size_t (*LeafVerifier)(MemRef, Allocator&);
    void verify_bptree(LeafVerifier) const;
    typedef void (*LeafDumper)(MemRef, Allocator&, std::ostream&, int level);
    void dump_bptree_structure(std::ostream&, int level, LeafDumper) const;
    void to_dot(std::ostream&, StringData title = StringData()) const;
    class ToDotHandler {
    public:
        virtual void to_dot(MemRef leaf_mem, ArrayParent*, size_t ndx_in_parent, std::ostream&) = 0;
        ~ToDotHandler()
        {
        }
    };
    void bptree_to_dot(std::ostream&, ToDotHandler&) const;
    void to_dot_parent_edge(std::ostream&) const;
#endif

    Array& operator=(const Array&) = delete; // not allowed
    Array(const Array&) = delete; // not allowed
protected:
    typedef bool (*CallbackDummy)(int64_t);

protected:
    // This returns the minimum value ("lower bound") of the representable values
    // for the given bit width. Valid widths are 0, 1, 2, 4, 8, 16, 32, and 64.
    template <size_t width>
    static int_fast64_t lbound_for_width() noexcept;

    static int_fast64_t lbound_for_width(size_t width) noexcept;

    // This returns the maximum value ("inclusive upper bound") of the representable values
    // for the given bit width. Valid widths are 0, 1, 2, 4, 8, 16, 32, and 64.
    template <size_t width>
    static int_fast64_t ubound_for_width() noexcept;

    static int_fast64_t ubound_for_width(size_t width) noexcept;

    template <size_t width>
    void set_width() noexcept;
    void set_width(size_t) noexcept;

private:
    void do_ensure_minimum_width(int_fast64_t);

    template <size_t w>
    int64_t sum(size_t start, size_t end) const;

    template <bool max, size_t w>
    bool minmax(int64_t& result, size_t start, size_t end, size_t* return_ndx) const;

    template <size_t w>
    size_t find_gte(const int64_t target, size_t start, size_t end) const;

    template <size_t w>
    size_t adjust_ge(size_t start, size_t end, int_fast64_t limit, int_fast64_t diff);

protected:
    /// It is an error to specify a non-zero value unless the width
    /// type is wtype_Bits. It is also an error to specify a non-zero
    /// size if the width type is wtype_Ignore.
    static MemRef create(Type, bool context_flag, WidthType, size_t size, int_fast64_t value, Allocator&);

    // Overriding method in ArrayParent
    void update_child_ref(size_t, ref_type) override;

    // Overriding method in ArrayParent
    ref_type get_child_ref(size_t) const noexcept override;

    void destroy_children(size_t offset = 0) noexcept;

    std::pair<ref_type, size_t> get_to_dot_parent(size_t ndx_in_parent) const override;

protected:
    // Getters and Setters for adaptive-packed arrays
    typedef int64_t (Array::*Getter)(size_t) const; // Note: getters must not throw
    typedef void (Array::*Setter)(size_t, int64_t);
    typedef bool (Array::*Finder)(int64_t, size_t, size_t, size_t, QueryState<int64_t>*) const;
    typedef void (Array::*ChunkGetter)(size_t, int64_t res[8]) const; // Note: getters must not throw

    struct VTable {
        Getter getter;
        ChunkGetter chunk_getter;
        Setter setter;
        Finder finder[cond_VTABLE_FINDER_COUNT]; // one for each active function pointer
    };
    template <size_t w>
    struct VTableForWidth;

protected:
    /// Takes a 64-bit value and returns the minimum number of bits needed
    /// to fit the value. For alignment this is rounded up to nearest
    /// log2. Posssible results {0, 1, 2, 4, 8, 16, 32, 64}
    static size_t bit_width(int64_t value);

    void report_memory_usage_2(MemUsageHandler&) const;

private:
    Getter m_getter = nullptr; // cached to avoid indirection
    const VTable* m_vtable = nullptr;

protected:
    int64_t m_lbound; // min number that can be stored with current m_width
    int64_t m_ubound; // max number that can be stored with current m_width

    bool m_is_inner_bptree_node; // This array is an inner node of B+-tree.
    bool m_has_refs;             // Elements whose first bit is zero are refs to subarrays.
    bool m_context_flag;         // Meaning depends on context.

private:
    ref_type do_write_shallow(_impl::ArrayWriterBase&) const;
    ref_type do_write_deep(_impl::ArrayWriterBase&, bool only_if_modified) const;

    friend class Allocator;
    friend class SlabAlloc;
    friend class GroupWriter;
};

class ClusterKeyArray : public ArrayUnsigned {
public:
    using ArrayUnsigned::ArrayUnsigned;

    uint64_t get(size_t ndx) const
    {
        return (m_data != nullptr) ? ArrayUnsigned::get(ndx) : uint64_t(ndx);
    }
};

// Implementation:
template <>
class QueryState<int64_t> : public QueryStateBase {
public:
    int64_t m_state = 0;

    template <Action action>
    bool uses_val()
    {
        if (action == act_Max || action == act_Min || action == act_Sum)
            return true;
        else
            return false;
    }

    QueryState(Action action, size_t limit = -1)
        : QueryState(action, int64_t(0), limit)
    {
    }

    QueryState(Action action, KeyColumn* akku, size_t limit = -1)
        : QueryState(action, reinterpret_cast<int64_t>(akku), limit)
    {
    }
    QueryState(Action action, IntegerColumn* akku, size_t limit = -1)
        : QueryState(action, reinterpret_cast<int64_t>(akku), limit)
    {
    }

    template <Action action, bool pattern>
    inline bool match(size_t index, uint64_t indexpattern, int64_t value)
    {
        if (pattern) {
            if (action == act_Count) {
                // If we are close to 'limit' argument in query, we cannot count-up a complete chunk. Count up single
                // elements instead
                if (m_match_count + 64 >= m_limit)
                    return false;

                m_state += fast_popcount64(indexpattern);
                m_match_count = size_t(m_state);
                return true;
            }
            // Other aggregates cannot (yet) use bit pattern for anything. Make Array-finder call with pattern = false
            // instead
            return false;
        }

        ++m_match_count;

        if (action == act_Max) {
            if (value > m_state) {
                m_state = value;
                m_minmax_index = m_key_values ? m_key_values->get(index) + m_key_offset : index;
            }
        }
        else if (action == act_Min) {
            if (value < m_state) {
                m_state = value;
                m_minmax_index = m_key_values ? m_key_values->get(index) + m_key_offset : index;
            }
        }
        else if (action == act_Sum)
            m_state += value;
        else if (action == act_Count) {
            m_state++;
            m_match_count = size_t(m_state);
        }
        else if (action == act_FindAll) {
            if (m_key_values) {
                int64_t key_value = m_key_values->get(index) + m_key_offset;
                Array::add_to_column(reinterpret_cast<KeyColumn*>(m_state), key_value);
            }
            else {
                Array::add_to_column(reinterpret_cast<IntegerColumn*>(m_state), index);
            }
        }
        else if (action == act_ReturnFirst) {
            m_state = index;
            return false;
        }
        else {
            REALM_ASSERT_DEBUG(false);
        }
        return (m_limit > m_match_count);
    }

    template <Action action, bool pattern>
    inline bool match(size_t index, uint64_t indexpattern, util::Optional<int64_t> value)
    {
        // FIXME: This is a temporary hack for nullable integers.
        if (value) {
            return match<action, pattern>(index, indexpattern, *value);
        }

        // If value is null, the only sensible actions are count, find_all, and return first.
        // Max, min, and sum should all have no effect.
        if (action == act_Count) {
            m_state++;
            m_match_count = size_t(m_state);
        }
        else if (action == act_FindAll) {
            if (m_key_values) {
                int64_t key_value = m_key_values->get(index) + m_key_offset;
                Array::add_to_column(reinterpret_cast<KeyColumn*>(m_state), key_value);
            }
            else {
                Array::add_to_column(reinterpret_cast<IntegerColumn*>(m_state), index);
            }
        }
        else if (action == act_ReturnFirst) {
            m_match_count++;
            m_state = index;
            return false;
        }
        return m_limit > m_match_count;
    }

private:
    QueryState(Action action, int64_t akku, size_t limit)
        : QueryStateBase(limit)
    {
        if (action == act_Max)
            m_state = std::numeric_limits<int64_t>::min();
        else if (action == act_Min)
            m_state = std::numeric_limits<int64_t>::max();
        else if (action == act_ReturnFirst)
            m_state = not_found;
        else if (action == act_Sum)
            m_state = 0;
        else if (action == act_Count)
            m_state = 0;
        else if (action == act_FindAll)
            m_state = akku;
        else if (action == act_CallbackIdx) {
        }
        else {
            REALM_ASSERT_DEBUG(false);
        }
    }
};

// Used only for Basic-types: currently float and double
template <class R>
class QueryState : public QueryStateBase {
public:
    R m_state;

    template <Action action>
    bool uses_val()
    {
        return (action == act_Max || action == act_Min || action == act_Sum || action == act_Count);
    }

    QueryState(Action action, Array* = nullptr, size_t limit = -1)
        : QueryStateBase(limit)
    {
        REALM_ASSERT((std::is_same<R, float>::value || std::is_same<R, double>::value));
        if (action == act_Max)
            m_state = -std::numeric_limits<R>::infinity();
        else if (action == act_Min)
            m_state = std::numeric_limits<R>::infinity();
        else if (action == act_Sum)
            m_state = 0.0;
        else if (action == act_Count)
            m_state = 0.0;
        else {
            REALM_ASSERT_DEBUG(false);
        }
    }

    template <Action action, bool pattern, typename resulttype>
    inline bool match(size_t index, uint64_t /*indexpattern*/, resulttype value)
    {
        if (pattern)
            return false;

        static_assert(action == act_Sum || action == act_Max || action == act_Min || action == act_Count,
                      "Search action not supported");

        if (action == act_Count) {
            ++m_match_count;
        }
        else if (!null::is_null_float(value)) {
            ++m_match_count;
            if (action == act_Max) {
                if (value > m_state) {
                    m_state = value;
                    if (m_key_values) {
                        m_minmax_index = m_key_values->get(index) + m_key_offset;
                    }
                    else {
                        m_minmax_index = int64_t(index);
                    }
                }
            }
            else if (action == act_Min) {
                if (value < m_state) {
                    m_state = value;
                    if (m_key_values) {
                        m_minmax_index = m_key_values->get(index) + m_key_offset;
                    }
                    else {
                        m_minmax_index = int64_t(index);
                    }
                }
            }
            else if (action == act_Sum)
                m_state += value;
            else {
                REALM_ASSERT_DEBUG(false);
            }
        }

        return (m_limit > m_match_count);
    }
};

inline bool RefOrTagged::is_ref() const noexcept
{
    return (m_value & 1) == 0;
}

inline bool RefOrTagged::is_tagged() const noexcept
{
    return !is_ref();
}

inline ref_type RefOrTagged::get_as_ref() const noexcept
{
    // to_ref() is defined in <alloc.hpp>
    return to_ref(m_value);
}

inline uint_fast64_t RefOrTagged::get_as_int() const noexcept
{
    // The bitwise AND is there in case uint_fast64_t is wider than 64 bits.
    return (uint_fast64_t(m_value) & 0xFFFFFFFFFFFFFFFFULL) >> 1;
}

inline RefOrTagged RefOrTagged::make_ref(ref_type ref) noexcept
{
    // from_ref() is defined in <alloc.hpp>
    int_fast64_t value = from_ref(ref);
    return RefOrTagged(value);
}

inline RefOrTagged RefOrTagged::make_tagged(uint_fast64_t i) noexcept
{
    REALM_ASSERT(i < (1ULL << 63));
    int_fast64_t value = util::from_twos_compl<int_fast64_t>((i << 1) | 1);
    return RefOrTagged(value);
}

inline RefOrTagged::RefOrTagged(int_fast64_t value) noexcept
    : m_value(value)
{
}

inline void Array::create(Type type, bool context_flag, size_t length, int_fast64_t value)
{
    MemRef mem = create_array(type, context_flag, length, value, m_alloc); // Throws
    init_from_mem(mem);
}


inline Array::Type Array::get_type() const noexcept
{
    if (m_is_inner_bptree_node) {
        REALM_ASSERT_DEBUG(m_has_refs);
        return type_InnerBptreeNode;
    }
    if (m_has_refs)
        return type_HasRefs;
    return type_Normal;
}


inline void Array::get_chunk(size_t ndx, int64_t res[8]) const noexcept
{
    REALM_ASSERT_DEBUG(ndx < m_size);
    (this->*(m_vtable->chunk_getter))(ndx, res);
}


inline int64_t Array::get(size_t ndx) const noexcept
{
    REALM_ASSERT_DEBUG(is_attached());
    REALM_ASSERT_DEBUG(ndx < m_size);
    return (this->*m_getter)(ndx);

    // Two ideas that are not efficient but may be worth looking into again:
    /*
        // Assume correct width is found early in REALM_TEMPEX, which is the case for B tree offsets that
        // are probably either 2^16 long. Turns out to be 25% faster if found immediately, but 50-300% slower
        // if found later
        REALM_TEMPEX(return get, (ndx));
    */
    /*
        // Slightly slower in both of the if-cases. Also needs an matchcount m_size check too, to avoid
        // reading beyond array.
        if (m_width >= 8 && m_size > ndx + 7)
            return get<64>(ndx >> m_shift) & m_widthmask;
        else
            return (this->*(m_vtable->getter))(ndx);
    */
}

inline int64_t Array::front() const noexcept
{
    return get(0);
}

inline int64_t Array::back() const noexcept
{
    return get(m_size - 1);
}

inline ref_type Array::get_as_ref(size_t ndx) const noexcept
{
    REALM_ASSERT_DEBUG(is_attached());
    REALM_ASSERT_DEBUG(m_has_refs);
    int64_t v = get(ndx);
    return to_ref(v);
}

inline RefOrTagged Array::get_as_ref_or_tagged(size_t ndx) const noexcept
{
    REALM_ASSERT(has_refs());
    return RefOrTagged(get(ndx));
}

inline void Array::set(size_t ndx, RefOrTagged ref_or_tagged)
{
    REALM_ASSERT(has_refs());
    set(ndx, ref_or_tagged.m_value); // Throws
}

inline void Array::add(RefOrTagged ref_or_tagged)
{
    REALM_ASSERT(has_refs());
    add(ref_or_tagged.m_value); // Throws
}

inline void Array::ensure_minimum_width(RefOrTagged ref_or_tagged)
{
    REALM_ASSERT(has_refs());
    ensure_minimum_width(ref_or_tagged.m_value); // Throws
}

inline bool Array::is_inner_bptree_node() const noexcept
{
    return m_is_inner_bptree_node;
}

inline bool Array::has_refs() const noexcept
{
    return m_has_refs;
}

inline void Array::set_has_refs(bool value) noexcept
{
    if (m_has_refs != value) {
        REALM_ASSERT(!is_read_only());
        m_has_refs = value;
        set_hasrefs_in_header(value, get_header());
    }
}

inline bool Array::get_context_flag() const noexcept
{
    return m_context_flag;
}

inline void Array::set_context_flag(bool value) noexcept
{
    if (m_context_flag != value) {
        REALM_ASSERT(!is_read_only());
        m_context_flag = value;
        set_context_flag_in_header(value, get_header());
    }
}

inline void Array::destroy_deep() noexcept
{
    if (!is_attached())
        return;

    if (m_has_refs)
        destroy_children();

    char* header = get_header_from_data(m_data);
    m_alloc.free_(m_ref, header);
    m_data = nullptr;
}

inline ref_type Array::write(_impl::ArrayWriterBase& out, bool deep, bool only_if_modified) const
{
    REALM_ASSERT(is_attached());

    if (only_if_modified && m_alloc.is_read_only(m_ref))
        return m_ref;

    if (!deep || !m_has_refs)
        return do_write_shallow(out); // Throws

    return do_write_deep(out, only_if_modified); // Throws
}

inline ref_type Array::write(ref_type ref, Allocator& alloc, _impl::ArrayWriterBase& out, bool only_if_modified)
{
    if (only_if_modified && alloc.is_read_only(ref))
        return ref;

    Array array(alloc);
    array.init_from_ref(ref);

    if (!array.m_has_refs)
        return array.do_write_shallow(out); // Throws

    return array.do_write_deep(out, only_if_modified); // Throws
}

inline void Array::add(int_fast64_t value)
{
    insert(m_size, value);
}

inline void Array::erase(size_t ndx)
{
    // This can throw, but only if array is currently in read-only
    // memory.
    move(ndx + 1, size(), ndx);

    // Update size (also in header)
    --m_size;
    set_header_size(m_size);
}


inline void Array::erase(size_t begin, size_t end)
{
    if (begin != end) {
        // This can throw, but only if array is currently in read-only memory.
        move(end, size(), begin); // Throws

        // Update size (also in header)
        m_size -= end - begin;
        set_header_size(m_size);
    }
}

inline void Array::clear()
{
    truncate(0); // Throws
}

inline void Array::clear_and_destroy_children()
{
    truncate_and_destroy_children(0);
}

inline void Array::destroy_deep(ref_type ref, Allocator& alloc) noexcept
{
    destroy_deep(MemRef(ref, alloc), alloc);
}

inline void Array::destroy_deep(MemRef mem, Allocator& alloc) noexcept
{
    if (!get_hasrefs_from_header(mem.get_addr())) {
        alloc.free_(mem);
        return;
    }
    Array array(alloc);
    array.init_from_mem(mem);
    array.destroy_deep();
}


inline void Array::adjust(size_t ndx, int_fast64_t diff)
{
    REALM_ASSERT_3(ndx, <=, m_size);
    if (diff != 0) {
        // FIXME: Should be optimized
        int_fast64_t v = get(ndx);
        set(ndx, int64_t(v + diff)); // Throws
    }
}

inline void Array::adjust(size_t begin, size_t end, int_fast64_t diff)
{
    if (diff != 0) {
        // FIXME: Should be optimized
        for (size_t i = begin; i != end; ++i)
            adjust(i, diff); // Throws
    }
}


//-------------------------------------------------


inline size_t Array::get_byte_size() const noexcept
{
    const char* header = get_header_from_data(m_data);
    WidthType wtype = Node::get_wtype_from_header(header);
    size_t num_bytes = NodeHeader::calc_byte_size(wtype, m_size, m_width);

    REALM_ASSERT_7(m_alloc.is_read_only(m_ref), ==, true, ||, num_bytes, <=, get_capacity_from_header(header));

    return num_bytes;
}


//-------------------------------------------------

inline MemRef Array::clone_deep(Allocator& target_alloc) const
{
    char* header = get_header_from_data(m_data);
    return clone(MemRef(header, m_ref, m_alloc), m_alloc, target_alloc); // Throws
}

inline MemRef Array::create_empty_array(Type type, bool context_flag, Allocator& alloc)
{
    size_t size = 0;
    int_fast64_t value = 0;
    return create_array(type, context_flag, size, value, alloc); // Throws
}

inline MemRef Array::create_array(Type type, bool context_flag, size_t size, int_fast64_t value, Allocator& alloc)
{
    return create(type, context_flag, wtype_Bits, size, value, alloc); // Throws
}

inline size_t Array::get_max_byte_size(size_t num_elems) noexcept
{
    int max_bytes_per_elem = 8;
    return header_size + num_elems * max_bytes_per_elem;
}


inline void Array::update_child_ref(size_t child_ndx, ref_type new_ref)
{
    set(child_ndx, new_ref);
}

inline ref_type Array::get_child_ref(size_t child_ndx) const noexcept
{
    return get_as_ref(child_ndx);
}

inline void Array::ensure_minimum_width(int_fast64_t value)
{
    if (value >= m_lbound && value <= m_ubound)
        return;
    do_ensure_minimum_width(value);
}


//*************************************************************************************
// Finding code                                                                       *
//*************************************************************************************

template <size_t w>
int64_t Array::get(size_t ndx) const noexcept
{
    return get_universal<w>(m_data, ndx);
}

template <size_t w>
int64_t Array::get_universal(const char* data, size_t ndx) const
{
    if (w == 0) {
        return 0;
    }
    else if (w == 1) {
        size_t offset = ndx >> 3;
        return (data[offset] >> (ndx & 7)) & 0x01;
    }
    else if (w == 2) {
        size_t offset = ndx >> 2;
        return (data[offset] >> ((ndx & 3) << 1)) & 0x03;
    }
    else if (w == 4) {
        size_t offset = ndx >> 1;
        return (data[offset] >> ((ndx & 1) << 2)) & 0x0F;
    }
    else if (w == 8) {
        return *reinterpret_cast<const signed char*>(data + ndx);
    }
    else if (w == 16) {
        size_t offset = ndx * 2;
        return *reinterpret_cast<const int16_t*>(data + offset);
    }
    else if (w == 32) {
        size_t offset = ndx * 4;
        return *reinterpret_cast<const int32_t*>(data + offset);
    }
    else if (w == 64) {
        size_t offset = ndx * 8;
        return *reinterpret_cast<const int64_t*>(data + offset);
    }
    else {
        REALM_ASSERT_DEBUG(false);
        return int64_t(-1);
    }
}

/*
find() (calls find_optimized()) will call match() for each search result.

If pattern == true:
    'indexpattern' contains a 64-bit chunk of elements, each of 'width' bits in size where each element indicates a
    match if its lower bit is set, otherwise it indicates a non-match. 'index' tells the database row index of the
    first element. You must return true if you chose to 'consume' the chunk or false if not. If not, then Array-finder
    will afterwards call match() successive times with pattern == false.

If pattern == false:
    'index' tells the row index of a single match and 'value' tells its value. Return false to make Array-finder break
    its search or return true to let it continue until 'end' or 'limit'.

Array-finder decides itself if - and when - it wants to pass you an indexpattern. It depends on array bit width, match
frequency, and whether the arithemetic and computations for the given search criteria makes it feasible to construct
such a pattern.
*/

// These wrapper functions only exist to enable a possibility to make the compiler see that 'value' and/or 'index' are
// unused, such that caller's computation of these values will not be made. Only works if find_action() and
// find_action_pattern() rewritten as macros. Note: This problem has been fixed in next upcoming array.hpp version
template <Action action, class Callback>
bool Array::find_action(size_t index, util::Optional<int64_t> value, QueryState<int64_t>* state,
                        Callback callback) const
{
    if (action == act_CallbackIdx)
        return callback(index);
    else
        return state->match<action, false>(index, 0, value);
}
template <Action action, class Callback>
bool Array::find_action_pattern(size_t index, uint64_t pattern, QueryState<int64_t>* state, Callback callback) const
{
    static_cast<void>(callback);
    if (action == act_CallbackIdx) {
        // Possible future optimization: call callback(index) like in above find_action(), in a loop for each bit set
        // in 'pattern'
        return false;
    }
    return state->match<action, true>(index, pattern, 0);
}


template <size_t width, bool zero>
uint64_t Array::cascade(uint64_t a) const
{
    // Takes a chunk of values as argument and sets the least significant bit for each
    // element which is zero or non-zero, depending on the template parameter.
    // Example for zero=true:
    // width == 4 and a = 0x5fd07a107610f610
    // will return:       0x0001000100010001

    // static values needed for fast population count
    const uint64_t m1 = 0x5555555555555555ULL;

    if (width == 1) {
        return zero ? ~a : a;
    }
    else if (width == 2) {
        // Masks to avoid spillover between segments in cascades
        const uint64_t c1 = ~0ULL / 0x3 * 0x1;

        a |= (a >> 1) & c1; // cascade ones in non-zeroed segments
        a &= m1;            // isolate single bit in each segment
        if (zero)
            a ^= m1; // reverse isolated bits if checking for zeroed segments

        return a;
    }
    else if (width == 4) {
        const uint64_t m = ~0ULL / 0xF * 0x1;

        // Masks to avoid spillover between segments in cascades
        const uint64_t c1 = ~0ULL / 0xF * 0x7;
        const uint64_t c2 = ~0ULL / 0xF * 0x3;

        a |= (a >> 1) & c1; // cascade ones in non-zeroed segments
        a |= (a >> 2) & c2;
        a &= m; // isolate single bit in each segment
        if (zero)
            a ^= m; // reverse isolated bits if checking for zeroed segments

        return a;
    }
    else if (width == 8) {
        const uint64_t m = ~0ULL / 0xFF * 0x1;

        // Masks to avoid spillover between segments in cascades
        const uint64_t c1 = ~0ULL / 0xFF * 0x7F;
        const uint64_t c2 = ~0ULL / 0xFF * 0x3F;
        const uint64_t c3 = ~0ULL / 0xFF * 0x0F;

        a |= (a >> 1) & c1; // cascade ones in non-zeroed segments
        a |= (a >> 2) & c2;
        a |= (a >> 4) & c3;
        a &= m; // isolate single bit in each segment
        if (zero)
            a ^= m; // reverse isolated bits if checking for zeroed segments

        return a;
    }
    else if (width == 16) {
        const uint64_t m = ~0ULL / 0xFFFF * 0x1;

        // Masks to avoid spillover between segments in cascades
        const uint64_t c1 = ~0ULL / 0xFFFF * 0x7FFF;
        const uint64_t c2 = ~0ULL / 0xFFFF * 0x3FFF;
        const uint64_t c3 = ~0ULL / 0xFFFF * 0x0FFF;
        const uint64_t c4 = ~0ULL / 0xFFFF * 0x00FF;

        a |= (a >> 1) & c1; // cascade ones in non-zeroed segments
        a |= (a >> 2) & c2;
        a |= (a >> 4) & c3;
        a |= (a >> 8) & c4;
        a &= m; // isolate single bit in each segment
        if (zero)
            a ^= m; // reverse isolated bits if checking for zeroed segments

        return a;
    }

    else if (width == 32) {
        const uint64_t m = ~0ULL / 0xFFFFFFFF * 0x1;

        // Masks to avoid spillover between segments in cascades
        const uint64_t c1 = ~0ULL / 0xFFFFFFFF * 0x7FFFFFFF;
        const uint64_t c2 = ~0ULL / 0xFFFFFFFF * 0x3FFFFFFF;
        const uint64_t c3 = ~0ULL / 0xFFFFFFFF * 0x0FFFFFFF;
        const uint64_t c4 = ~0ULL / 0xFFFFFFFF * 0x00FFFFFF;
        const uint64_t c5 = ~0ULL / 0xFFFFFFFF * 0x0000FFFF;

        a |= (a >> 1) & c1; // cascade ones in non-zeroed segments
        a |= (a >> 2) & c2;
        a |= (a >> 4) & c3;
        a |= (a >> 8) & c4;
        a |= (a >> 16) & c5;
        a &= m; // isolate single bit in each segment
        if (zero)
            a ^= m; // reverse isolated bits if checking for zeroed segments

        return a;
    }
    else if (width == 64) {
        return (a == 0) == zero;
    }
    else {
        REALM_ASSERT_DEBUG(false);
        return uint64_t(-1);
    }
}

// This is the main finding function for Array. Other finding functions are just wrappers around this one.
// Search for 'value' using condition cond (Equal, NotEqual, Less, etc) and call find_action() or
// find_action_pattern() for each match. Break and return if find_action() returns false or 'end' is reached.

// If nullable_array is set, then find_optimized() will treat the array is being nullable, i.e. it will skip the
// first entry and compare correctly against null, etc.
//
// If find_null is set, it means that we search for a null. In that case, `value` is ignored. If find_null is set,
// then nullable_array must be set too.
template <class cond, Action action, size_t bitwidth, class Callback>
bool Array::find_optimized(int64_t value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
                           Callback callback, bool nullable_array, bool find_null) const
{
    REALM_ASSERT(!(find_null && !nullable_array));
    REALM_ASSERT_DEBUG(start <= m_size && (end <= m_size || end == size_t(-1)) && start <= end);

    size_t start2 = start;
    cond c;

    if (end == npos)
        end = nullable_array ? size() - 1 : size();

    if (nullable_array) {
        if (std::is_same<cond, Equal>::value) {
            // In case of Equal it is safe to use the optimized logic. We just have to fetch the null value
            // if this is what we are looking for. And we have to adjust the indexes to compensate for the
            // null value at position 0.
            if (find_null) {
                value = get(0);
            }
            else {
                // If the value to search for is equal to the null value, the value cannot be in the array
                if (value == get(0)) {
                    return true;
                }
            }
            start2++;
            end++;
            baseindex--;
        }
        else {
            // We were called by find() of a nullable array. So skip first entry, take nulls in count, etc, etc. Fixme:
            // Huge speed optimizations are possible here! This is a very simple generic method.
            auto null_value = get(0);
            for (; start2 < end; start2++) {
                int64_t v = get<bitwidth>(start2 + 1);
                bool value_is_null = (v == null_value);
                if (c(v, value, value_is_null, find_null)) {
                    util::Optional<int64_t> v2(value_is_null ? util::none : util::make_optional(v));
                    if (!find_action<action, Callback>(start2 + baseindex, v2, state, callback))
                        return false; // tell caller to stop aggregating/search
                }
            }
            return true; // tell caller to continue aggregating/search (on next array leafs)
        }
    }


    // Test first few items with no initial time overhead
    if (start2 > 0) {
        if (m_size > start2 && c(get<bitwidth>(start2), value) && start2 < end) {
            if (!find_action<action, Callback>(start2 + baseindex, get<bitwidth>(start2), state, callback))
                return false;
        }

        ++start2;

        if (m_size > start2 && c(get<bitwidth>(start2), value) && start2 < end) {
            if (!find_action<action, Callback>(start2 + baseindex, get<bitwidth>(start2), state, callback))
                return false;
        }

        ++start2;

        if (m_size > start2 && c(get<bitwidth>(start2), value) && start2 < end) {
            if (!find_action<action, Callback>(start2 + baseindex, get<bitwidth>(start2), state, callback))
                return false;
        }

        ++start2;

        if (m_size > start2 && c(get<bitwidth>(start2), value) && start2 < end) {
            if (!find_action<action, Callback>(start2 + baseindex, get<bitwidth>(start2), state, callback))
                return false;
        }

        ++start2;
    }

    if (!(m_size > start2 && start2 < end))
        return true;

    if (end == size_t(-1))
        end = m_size;

    // Return immediately if no items in array can match (such as if cond == Greater && value == 100 &&
    // m_ubound == 15)
    if (!c.can_match(value, m_lbound, m_ubound))
        return true;

    // optimization if all items are guaranteed to match (such as cond == NotEqual && value == 100 && m_ubound == 15)
    if (c.will_match(value, m_lbound, m_ubound)) {
        size_t end2;

        if (action == act_CallbackIdx)
            end2 = end;
        else {
            REALM_ASSERT_DEBUG(state->m_match_count < state->m_limit);
            size_t process = state->m_limit - state->m_match_count;
            end2 = end - start2 > process ? start2 + process : end;
        }
        if (action == act_Sum || action == act_Max || action == act_Min) {
            int64_t res;
            size_t res_ndx = 0;
            if (action == act_Sum)
                res = Array::sum(start2, end2);
            if (action == act_Max)
                Array::maximum(res, start2, end2, &res_ndx);
            if (action == act_Min)
                Array::minimum(res, start2, end2, &res_ndx);

            find_action<action, Callback>(res_ndx + baseindex, res, state, callback);
            // find_action will increment match count by 1, so we need to `-1` from the number of elements that
            // we performed the fast Array methods on.
            state->m_match_count += end2 - start2 - 1;
        }
        else if (action == act_Count) {
            state->m_state += end2 - start2;
        }
        else {
            for (; start2 < end2; start2++)
                if (!find_action<action, Callback>(start2 + baseindex, get<bitwidth>(start2), state, callback))
                    return false;
        }
        return true;
    }

    // finder cannot handle this bitwidth
    REALM_ASSERT_3(m_width, !=, 0);

#if defined(REALM_COMPILER_SSE)
    // Only use SSE if payload is at least one SSE chunk (128 bits) in size. Also note taht SSE doesn't support
    // Less-than comparison for 64-bit values.
    if ((!(std::is_same<cond, Less>::value && m_width == 64)) && end - start2 >= sizeof(__m128i) && m_width >= 8 &&
        (sseavx<42>() || (sseavx<30>() && std::is_same<cond, Equal>::value && m_width < 64))) {

        // find_sse() must start2 at 16-byte boundary, so search area before that using compare_equality()
        __m128i* const a = reinterpret_cast<__m128i*>(round_up(m_data + start2 * bitwidth / 8, sizeof(__m128i)));
        __m128i* const b = reinterpret_cast<__m128i*>(round_down(m_data + end * bitwidth / 8, sizeof(__m128i)));

        if (!compare<cond, action, bitwidth, Callback>(
                value, start2, (reinterpret_cast<char*>(a) - m_data) * 8 / no0(bitwidth), baseindex, state, callback))
            return false;

        // Search aligned area with SSE
        if (b > a) {
            if (sseavx<42>()) {
                if (!find_sse<cond, action, bitwidth, Callback>(
                        value, a, b - a, state,
                        baseindex + ((reinterpret_cast<char*>(a) - m_data) * 8 / no0(bitwidth)), callback))
                    return false;
            }
            else if (sseavx<30>()) {

                if (!find_sse<Equal, action, bitwidth, Callback>(
                        value, a, b - a, state,
                        baseindex + ((reinterpret_cast<char*>(a) - m_data) * 8 / no0(bitwidth)), callback))
                    return false;
            }
        }

        // Search remainder with compare_equality()
        if (!compare<cond, action, bitwidth, Callback>(
                value, (reinterpret_cast<char*>(b) - m_data) * 8 / no0(bitwidth), end, baseindex, state, callback))
            return false;

        return true;
    }
    else {
        return compare<cond, action, bitwidth, Callback>(value, start2, end, baseindex, state, callback);
    }
#else
    return compare<cond, action, bitwidth, Callback>(value, start2, end, baseindex, state, callback);
#endif
}

template <size_t width>
inline int64_t Array::lower_bits() const
{
    if (width == 1)
        return 0xFFFFFFFFFFFFFFFFULL;
    else if (width == 2)
        return 0x5555555555555555ULL;
    else if (width == 4)
        return 0x1111111111111111ULL;
    else if (width == 8)
        return 0x0101010101010101ULL;
    else if (width == 16)
        return 0x0001000100010001ULL;
    else if (width == 32)
        return 0x0000000100000001ULL;
    else if (width == 64)
        return 0x0000000000000001ULL;
    else {
        REALM_ASSERT_DEBUG(false);
        return int64_t(-1);
    }
}

// Tests if any chunk in 'value' is 0
template <size_t width>
inline bool Array::test_zero(uint64_t value) const
{
    uint64_t hasZeroByte;
    uint64_t lower = lower_bits<width>();
    uint64_t upper = lower_bits<width>() * 1ULL << (width == 0 ? 0 : (width - 1ULL));
    hasZeroByte = (value - lower) & ~value & upper;
    return hasZeroByte != 0;
}

// Finds first zero (if eq == true) or non-zero (if eq == false) element in v and returns its position.
// IMPORTANT: This function assumes that at least 1 item matches (test this with test_zero() or other means first)!
template <bool eq, size_t width>
size_t Array::find_zero(uint64_t v) const
{
    size_t start = 0;
    uint64_t hasZeroByte;
    // Warning free way of computing (1ULL << width) - 1
    uint64_t mask = (width == 64 ? ~0ULL : ((1ULL << (width == 64 ? 0 : width)) - 1ULL));

    if (eq == (((v >> (width * start)) & mask) == 0)) {
        return 0;
    }

    // Bisection optimization, speeds up small bitwidths with high match frequency. More partions than 2 do NOT pay
    // off because the work done by test_zero() is wasted for the cases where the value exists in first half, but
    // useful if it exists in last half. Sweet spot turns out to be the widths and partitions below.
    if (width <= 8) {
        hasZeroByte = test_zero<width>(v | 0xffffffff00000000ULL);
        if (eq ? !hasZeroByte : (v & 0x00000000ffffffffULL) == 0) {
            // 00?? -> increasing
            start += 64 / no0(width) / 2;
            if (width <= 4) {
                hasZeroByte = test_zero<width>(v | 0xffff000000000000ULL);
                if (eq ? !hasZeroByte : (v & 0x0000ffffffffffffULL) == 0) {
                    // 000?
                    start += 64 / no0(width) / 4;
                }
            }
        }
        else {
            if (width <= 4) {
                // ??00
                hasZeroByte = test_zero<width>(v | 0xffffffffffff0000ULL);
                if (eq ? !hasZeroByte : (v & 0x000000000000ffffULL) == 0) {
                    // 0?00
                    start += 64 / no0(width) / 4;
                }
            }
        }
    }

    while (eq == (((v >> (width * start)) & mask) != 0)) {
        // You must only call find_zero() if you are sure that at least 1 item matches
        REALM_ASSERT_3(start, <=, 8 * sizeof(v));
        start++;
    }

    return start;
}

// Generate a magic constant used for later bithacks
template <bool gt, size_t width>
int64_t Array::find_gtlt_magic(int64_t v) const
{
    uint64_t mask1 = (width == 64 ? ~0ULL : ((1ULL << (width == 64 ? 0 : width)) -
                                             1ULL)); // Warning free way of computing (1ULL << width) - 1
    uint64_t mask2 = mask1 >> 1;
    uint64_t magic = gt ? (~0ULL / no0(mask1) * (mask2 - v)) : (~0ULL / no0(mask1) * v);
    return magic;
}

template <bool gt, Action action, size_t width, class Callback>
bool Array::find_gtlt_fast(uint64_t chunk, uint64_t magic, QueryState<int64_t>* state, size_t baseindex,
                           Callback callback) const
{
    // Tests if a a chunk of values contains values that are greater (if gt == true) or less (if gt == false) than v.
    // Fast, but limited to work when all values in the chunk are positive.

    uint64_t mask1 = (width == 64 ? ~0ULL : ((1ULL << (width == 64 ? 0 : width)) -
                                             1ULL)); // Warning free way of computing (1ULL << width) - 1
    uint64_t mask2 = mask1 >> 1;
    uint64_t m = gt ? (((chunk + magic) | chunk) & ~0ULL / no0(mask1) * (mask2 + 1))
                    : ((chunk - magic) & ~chunk & ~0ULL / no0(mask1) * (mask2 + 1));
    size_t p = 0;
    while (m) {
        if (find_action_pattern<action, Callback>(baseindex, m >> (no0(width) - 1), state, callback))
            break; // consumed, so do not call find_action()

        size_t t = first_set_bit64(m) / no0(width);
        p += t;
        if (!find_action<action, Callback>(p + baseindex, (chunk >> (p * width)) & mask1, state, callback))
            return false;

        if ((t + 1) * width == 64)
            m = 0;
        else
            m >>= (t + 1) * width;
        p++;
    }

    return true;
}

// clang-format off
template <bool gt, Action action, size_t width, class Callback>
bool Array::find_gtlt(int64_t v, uint64_t chunk, QueryState<int64_t>* state, size_t baseindex, Callback callback) const
{
    // Find items in 'chunk' that are greater (if gt == true) or smaller (if gt == false) than 'v'. Fixme, __forceinline can make it crash in vS2010 - find out why
    if (width == 1) {
        for (size_t t = 0; t < 64; t++) {
            if (gt ? static_cast<int64_t>(chunk & 0x1) > v : static_cast<int64_t>(chunk & 0x1) < v) {if (!find_action<action, Callback>( t + baseindex, static_cast<int64_t>(chunk & 0x1), state, callback)) return false;}
            chunk >>= 1;
        }
    }
    else if (width == 2) {
        // Alot (50% +) faster than loop/compiler-unrolled loop
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 0 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 1 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 2 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 3 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 4 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 5 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 6 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 7 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;

        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 8 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 9 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 10 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 11 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 12 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 13 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 14 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 15 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;

        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 16 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 17 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 18 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 19 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 20 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 21 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 22 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 23 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;

        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 24 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 25 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 26 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 27 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 28 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 29 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 30 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
        if (gt ? static_cast<int64_t>(chunk & 0x3) > v : static_cast<int64_t>(chunk & 0x3) < v) {if (!find_action<action, Callback>( 31 + baseindex, static_cast<int64_t>(chunk & 0x3), state, callback)) return false;}
        chunk >>= 2;
    }
    else if (width == 4) {
        if (gt ? static_cast<int64_t>(chunk & 0xf) > v : static_cast<int64_t>(chunk & 0xf) < v) {if (!find_action<action, Callback>( 0 + baseindex, static_cast<int64_t>(chunk & 0xf), state, callback)) return false;}
        chunk >>= 4;
        if (gt ? static_cast<int64_t>(chunk & 0xf) > v : static_cast<int64_t>(chunk & 0xf) < v) {if (!find_action<action, Callback>( 1 + baseindex, static_cast<int64_t>(chunk & 0xf), state, callback)) return false;}
        chunk >>= 4;
        if (gt ? static_cast<int64_t>(chunk & 0xf) > v : static_cast<int64_t>(chunk & 0xf) < v) {if (!find_action<action, Callback>( 2 + baseindex, static_cast<int64_t>(chunk & 0xf), state, callback)) return false;}
        chunk >>= 4;
        if (gt ? static_cast<int64_t>(chunk & 0xf) > v : static_cast<int64_t>(chunk & 0xf) < v) {if (!find_action<action, Callback>( 3 + baseindex, static_cast<int64_t>(chunk & 0xf), state, callback)) return false;}
        chunk >>= 4;
        if (gt ? static_cast<int64_t>(chunk & 0xf) > v : static_cast<int64_t>(chunk & 0xf) < v) {if (!find_action<action, Callback>( 4 + baseindex, static_cast<int64_t>(chunk & 0xf), state, callback)) return false;}
        chunk >>= 4;
        if (gt ? static_cast<int64_t>(chunk & 0xf) > v : static_cast<int64_t>(chunk & 0xf) < v) {if (!find_action<action, Callback>( 5 + baseindex, static_cast<int64_t>(chunk & 0xf), state, callback)) return false;}
        chunk >>= 4;
        if (gt ? static_cast<int64_t>(chunk & 0xf) > v : static_cast<int64_t>(chunk & 0xf) < v) {if (!find_action<action, Callback>( 6 + baseindex, static_cast<int64_t>(chunk & 0xf), state, callback)) return false;}
        chunk >>= 4;
        if (gt ? static_cast<int64_t>(chunk & 0xf) > v : static_cast<int64_t>(chunk & 0xf) < v) {if (!find_action<action, Callback>( 7 + baseindex, static_cast<int64_t>(chunk & 0xf), state, callback)) return false;}
        chunk >>= 4;

        if (gt ? static_cast<int64_t>(chunk & 0xf) > v : static_cast<int64_t>(chunk & 0xf) < v) {if (!find_action<action, Callback>( 8 + baseindex, static_cast<int64_t>(chunk & 0xf), state, callback)) return false;}
        chunk >>= 4;
        if (gt ? static_cast<int64_t>(chunk & 0xf) > v : static_cast<int64_t>(chunk & 0xf) < v) {if (!find_action<action, Callback>( 9 + baseindex, static_cast<int64_t>(chunk & 0xf), state, callback)) return false;}
        chunk >>= 4;
        if (gt ? static_cast<int64_t>(chunk & 0xf) > v : static_cast<int64_t>(chunk & 0xf) < v) {if (!find_action<action, Callback>( 10 + baseindex, static_cast<int64_t>(chunk & 0xf), state, callback)) return false;}
        chunk >>= 4;
        if (gt ? static_cast<int64_t>(chunk & 0xf) > v : static_cast<int64_t>(chunk & 0xf) < v) {if (!find_action<action, Callback>( 11 + baseindex, static_cast<int64_t>(chunk & 0xf), state, callback)) return false;}
        chunk >>= 4;
        if (gt ? static_cast<int64_t>(chunk & 0xf) > v : static_cast<int64_t>(chunk & 0xf) < v) {if (!find_action<action, Callback>( 12 + baseindex, static_cast<int64_t>(chunk & 0xf), state, callback)) return false;}
        chunk >>= 4;
        if (gt ? static_cast<int64_t>(chunk & 0xf) > v : static_cast<int64_t>(chunk & 0xf) < v) {if (!find_action<action, Callback>( 13 + baseindex, static_cast<int64_t>(chunk & 0xf), state, callback)) return false;}
        chunk >>= 4;
        if (gt ? static_cast<int64_t>(chunk & 0xf) > v : static_cast<int64_t>(chunk & 0xf) < v) {if (!find_action<action, Callback>( 14 + baseindex, static_cast<int64_t>(chunk & 0xf), state, callback)) return false;}
        chunk >>= 4;
        if (gt ? static_cast<int64_t>(chunk & 0xf) > v : static_cast<int64_t>(chunk & 0xf) < v) {if (!find_action<action, Callback>( 15 + baseindex, static_cast<int64_t>(chunk & 0xf), state, callback)) return false;}
        chunk >>= 4;
    }
    else if (width == 8) {
        if (gt ? static_cast<int8_t>(chunk) > v : static_cast<int8_t>(chunk) < v) {if (!find_action<action, Callback>( 0 + baseindex, static_cast<int8_t>(chunk), state, callback)) return false;}
        chunk >>= 8;
        if (gt ? static_cast<int8_t>(chunk) > v : static_cast<int8_t>(chunk) < v) {if (!find_action<action, Callback>( 1 + baseindex, static_cast<int8_t>(chunk), state, callback)) return false;}
        chunk >>= 8;
        if (gt ? static_cast<int8_t>(chunk) > v : static_cast<int8_t>(chunk) < v) {if (!find_action<action, Callback>( 2 + baseindex, static_cast<int8_t>(chunk), state, callback)) return false;}
        chunk >>= 8;
        if (gt ? static_cast<int8_t>(chunk) > v : static_cast<int8_t>(chunk) < v) {if (!find_action<action, Callback>( 3 + baseindex, static_cast<int8_t>(chunk), state, callback)) return false;}
        chunk >>= 8;
        if (gt ? static_cast<int8_t>(chunk) > v : static_cast<int8_t>(chunk) < v) {if (!find_action<action, Callback>( 4 + baseindex, static_cast<int8_t>(chunk), state, callback)) return false;}
        chunk >>= 8;
        if (gt ? static_cast<int8_t>(chunk) > v : static_cast<int8_t>(chunk) < v) {if (!find_action<action, Callback>( 5 + baseindex, static_cast<int8_t>(chunk), state, callback)) return false;}
        chunk >>= 8;
        if (gt ? static_cast<int8_t>(chunk) > v : static_cast<int8_t>(chunk) < v) {if (!find_action<action, Callback>( 6 + baseindex, static_cast<int8_t>(chunk), state, callback)) return false;}
        chunk >>= 8;
        if (gt ? static_cast<int8_t>(chunk) > v : static_cast<int8_t>(chunk) < v) {if (!find_action<action, Callback>( 7 + baseindex, static_cast<int8_t>(chunk), state, callback)) return false;}
        chunk >>= 8;
    }
    else if (width == 16) {

        if (gt ? static_cast<short int>(chunk >> 0 * 16) > v : static_cast<short int>(chunk >> 0 * 16) < v) {if (!find_action<action, Callback>( 0 + baseindex, static_cast<short int>(chunk >> 0 * 16), state, callback)) return false;};
        if (gt ? static_cast<short int>(chunk >> 1 * 16) > v : static_cast<short int>(chunk >> 1 * 16) < v) {if (!find_action<action, Callback>( 1 + baseindex, static_cast<short int>(chunk >> 1 * 16), state, callback)) return false;};
        if (gt ? static_cast<short int>(chunk >> 2 * 16) > v : static_cast<short int>(chunk >> 2 * 16) < v) {if (!find_action<action, Callback>( 2 + baseindex, static_cast<short int>(chunk >> 2 * 16), state, callback)) return false;};
        if (gt ? static_cast<short int>(chunk >> 3 * 16) > v : static_cast<short int>(chunk >> 3 * 16) < v) {if (!find_action<action, Callback>( 3 + baseindex, static_cast<short int>(chunk >> 3 * 16), state, callback)) return false;};
    }
    else if (width == 32) {
        if (gt ? static_cast<int>(chunk) > v : static_cast<int>(chunk) < v) {if (!find_action<action, Callback>( 0 + baseindex, static_cast<int>(chunk), state, callback)) return false;}
        chunk >>= 32;
        if (gt ? static_cast<int>(chunk) > v : static_cast<int>(chunk) < v) {if (!find_action<action, Callback>( 1 + baseindex, static_cast<int>(chunk), state, callback)) return false;}
        chunk >>= 32;
    }
    else if (width == 64) {
        if (gt ? static_cast<int64_t>(v) > v : static_cast<int64_t>(v) < v) {if (!find_action<action, Callback>( 0 + baseindex, static_cast<int64_t>(v), state, callback)) return false;};
    }

    return true;
}
// clang-format on

/// Find items in this Array that are equal (eq == true) or different (eq = false) from 'value'
template <bool eq, Action action, size_t width, class Callback>
inline bool Array::compare_equality(int64_t value, size_t start, size_t end, size_t baseindex,
                                    QueryState<int64_t>* state, Callback callback) const
{
    REALM_ASSERT_DEBUG(start <= m_size && (end <= m_size || end == size_t(-1)) && start <= end);

    size_t ee = round_up(start, 64 / no0(width));
    ee = ee > end ? end : ee;
    for (; start < ee; ++start)
        if (eq ? (get<width>(start) == value) : (get<width>(start) != value)) {
            if (!find_action<action, Callback>(start + baseindex, get<width>(start), state, callback))
                return false;
        }

    if (start >= end)
        return true;

    if (width != 32 && width != 64) {
        const int64_t* p = reinterpret_cast<const int64_t*>(m_data + (start * width / 8));
        const int64_t* const e = reinterpret_cast<int64_t*>(m_data + (end * width / 8)) - 1;
        const uint64_t mask = (width == 64 ? ~0ULL : ((1ULL << (width == 64 ? 0 : width)) -
                                                      1ULL)); // Warning free way of computing (1ULL << width) - 1
        const uint64_t valuemask =
            ~0ULL / no0(mask) * (value & mask); // the "== ? :" is to avoid division by 0 compiler error

        while (p < e) {
            uint64_t chunk = *p;
            uint64_t v2 = chunk ^ valuemask;
            start = (p - reinterpret_cast<int64_t*>(m_data)) * 8 * 8 / no0(width);
            size_t a = 0;

            while (eq ? test_zero<width>(v2) : v2) {

                if (find_action_pattern<action, Callback>(start + baseindex, cascade<width, eq>(v2), state, callback))
                    break; // consumed

                size_t t = find_zero<eq, width>(v2);
                a += t;

                if (a >= 64 / no0(width))
                    break;

                if (!find_action<action, Callback>(a + start + baseindex, get<width>(start + a), state, callback))
                    return false;
                v2 >>= (t + 1) * width;
                a += 1;
            }

            ++p;
        }

        // Loop ended because we are near end or end of array. No need to optimize search in remainder in this case
        // because end of array means that
        // lots of search work has taken place prior to ending here. So time spent searching remainder is relatively
        // tiny
        start = (p - reinterpret_cast<int64_t*>(m_data)) * 8 * 8 / no0(width);
    }

    while (start < end) {
        if (eq ? get<width>(start) == value : get<width>(start) != value) {
            if (!find_action<action, Callback>(start + baseindex, get<width>(start), state, callback))
                return false;
        }
        ++start;
    }

    return true;
}

// There exists a couple of find() functions that take more or less template arguments. Always call the one that
// takes as most as possible to get best performance.

// This is the one installed into the m_vtable->finder slots.
template <class cond, Action action, size_t bitwidth>
bool Array::find(int64_t value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state) const
{
    return find<cond, action, bitwidth>(value, start, end, baseindex, state, CallbackDummy());
}

template <class cond, Action action, class Callback>
bool Array::find(int64_t value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
                 Callback callback, bool nullable_array, bool find_null) const
{
    REALM_TEMPEX4(return find, cond, action, m_width, Callback,
                         (value, start, end, baseindex, state, callback, nullable_array, find_null));
}

template <class cond, Action action, size_t bitwidth, class Callback>
bool Array::find(int64_t value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
                 Callback callback, bool nullable_array, bool find_null) const
{
    return find_optimized<cond, action, bitwidth, Callback>(value, start, end, baseindex, state, callback,
                                                            nullable_array, find_null);
}

#ifdef REALM_COMPILER_SSE
// 'items' is the number of 16-byte SSE chunks. Returns index of packed element relative to first integer of first
// chunk
template <class cond, Action action, size_t width, class Callback>
bool Array::find_sse(int64_t value, __m128i* data, size_t items, QueryState<int64_t>* state, size_t baseindex,
                     Callback callback) const
{
    __m128i search = {0};

    if (width == 8)
        search = _mm_set1_epi8(static_cast<char>(value));
    else if (width == 16)
        search = _mm_set1_epi16(static_cast<short int>(value));
    else if (width == 32)
        search = _mm_set1_epi32(static_cast<int>(value));
    else if (width == 64) {
        if (std::is_same<cond, Less>::value)
            REALM_ASSERT(false);
        else
            search = _mm_set_epi64x(value, value);
    }

    return find_sse_intern<cond, action, width, Callback>(data, &search, items, state, baseindex, callback);
}

// Compares packed action_data with packed data (equal, less, etc) and performs aggregate action (max, min, sum,
// find_all, etc) on value inside action_data for first match, if any
template <class cond, Action action, size_t width, class Callback>
REALM_FORCEINLINE bool Array::find_sse_intern(__m128i* action_data, __m128i* data, size_t items,
                                              QueryState<int64_t>* state, size_t baseindex, Callback callback) const
{
    size_t i = 0;
    __m128i compare_result = {0};
    unsigned int resmask;

    // Search loop. Unrolling it has been tested to NOT increase performance (apparently mem bound)
    for (i = 0; i < items; ++i) {
        // equal / not-equal
        if (std::is_same<cond, Equal>::value || std::is_same<cond, NotEqual>::value) {
            if (width == 8)
                compare_result = _mm_cmpeq_epi8(action_data[i], *data);
            if (width == 16)
                compare_result = _mm_cmpeq_epi16(action_data[i], *data);
            if (width == 32)
                compare_result = _mm_cmpeq_epi32(action_data[i], *data);
            if (width == 64) {
                compare_result = _mm_cmpeq_epi64(action_data[i], *data); // SSE 4.2 only
            }
        }

        // greater
        else if (std::is_same<cond, Greater>::value) {
            if (width == 8)
                compare_result = _mm_cmpgt_epi8(action_data[i], *data);
            if (width == 16)
                compare_result = _mm_cmpgt_epi16(action_data[i], *data);
            if (width == 32)
                compare_result = _mm_cmpgt_epi32(action_data[i], *data);
            if (width == 64)
                compare_result = _mm_cmpgt_epi64(action_data[i], *data);
        }
        // less
        else if (std::is_same<cond, Less>::value) {
            if (width == 8)
                compare_result = _mm_cmplt_epi8(action_data[i], *data);
            else if (width == 16)
                compare_result = _mm_cmplt_epi16(action_data[i], *data);
            else if (width == 32)
                compare_result = _mm_cmplt_epi32(action_data[i], *data);
            else
                REALM_ASSERT(false);
        }

        resmask = _mm_movemask_epi8(compare_result);

        if (std::is_same<cond, NotEqual>::value)
            resmask = ~resmask & 0x0000ffff;

        size_t s = i * sizeof(__m128i) * 8 / no0(width);

        while (resmask != 0) {

            uint64_t upper = lower_bits<width / 8>() << (no0(width / 8) - 1);
            uint64_t pattern =
                resmask &
                upper; // fixme, bits at wrong offsets. Only OK because we only use them in 'count' aggregate
            if (find_action_pattern<action, Callback>(s + baseindex, pattern, state, callback))
                break;

            size_t idx = first_set_bit(resmask) * 8 / no0(width);
            s += idx;
            if (!find_action<action, Callback>(
                    s + baseindex, get_universal<width>(reinterpret_cast<char*>(action_data), s), state, callback))
                return false;
            resmask >>= (idx + 1) * no0(width) / 8;
            ++s;
        }
    }

    return true;
}
#endif // REALM_COMPILER_SSE

template <class cond, Action action, class Callback>
bool Array::compare_leafs(const Array* foreign, size_t start, size_t end, size_t baseindex,
                          QueryState<int64_t>* state, Callback callback) const
{
    cond c;
    REALM_ASSERT_3(start, <=, end);
    if (start == end)
        return true;


    int64_t v;

    // We can compare first element without checking for out-of-range
    v = get(start);
    if (c(v, foreign->get(start))) {
        if (!find_action<action, Callback>(start + baseindex, v, state, callback))
            return false;
    }

    start++;

    if (start + 3 < end) {
        v = get(start);
        if (c(v, foreign->get(start)))
            if (!find_action<action, Callback>(start + baseindex, v, state, callback))
                return false;

        v = get(start + 1);
        if (c(v, foreign->get(start + 1)))
            if (!find_action<action, Callback>(start + 1 + baseindex, v, state, callback))
                return false;

        v = get(start + 2);
        if (c(v, foreign->get(start + 2)))
            if (!find_action<action, Callback>(start + 2 + baseindex, v, state, callback))
                return false;

        start += 3;
    }
    else if (start == end) {
        return true;
    }

    bool r;
    REALM_TEMPEX4(r = compare_leafs, cond, action, m_width, Callback,
                  (foreign, start, end, baseindex, state, callback))
    return r;
}


template <class cond, Action action, size_t width, class Callback>
bool Array::compare_leafs(const Array* foreign, size_t start, size_t end, size_t baseindex,
                          QueryState<int64_t>* state, Callback callback) const
{
    size_t fw = foreign->m_width;
    bool r;
    REALM_TEMPEX5(r = compare_leafs_4, cond, action, width, Callback, fw,
                  (foreign, start, end, baseindex, state, callback))
    return r;
}


template <class cond, Action action, size_t width, class Callback, size_t foreign_width>
bool Array::compare_leafs_4(const Array* foreign, size_t start, size_t end, size_t baseindex,
                            QueryState<int64_t>* state, Callback callback) const
{
    cond c;
    char* foreign_m_data = foreign->m_data;

    if (width == 0 && foreign_width == 0) {
        if (c(0, 0)) {
            while (start < end) {
                if (!find_action<action, Callback>(start + baseindex, 0, state, callback))
                    return false;
                start++;
            }
        }
        else {
            return true;
        }
    }


#if defined(REALM_COMPILER_SSE)
    if (sseavx<42>() && width == foreign_width && (width == 8 || width == 16 || width == 32)) {
        // We can only use SSE if both bitwidths are equal and above 8 bits and all values are signed
        // and the two arrays are aligned the same way
        if ((reinterpret_cast<size_t>(m_data) & 0xf) == (reinterpret_cast<size_t>(foreign_m_data) & 0xf)) {
            while (start < end && (((reinterpret_cast<size_t>(m_data) & 0xf) * 8 + start * width) % (128) != 0)) {
                int64_t v = get_universal<width>(m_data, start);
                int64_t fv = get_universal<foreign_width>(foreign_m_data, start);
                if (c(v, fv)) {
                    if (!find_action<action, Callback>(start + baseindex, v, state, callback))
                        return false;
                }
                start++;
            }
            if (start == end)
                return true;


            size_t sse_items = (end - start) * width / 128;
            size_t sse_end = start + sse_items * 128 / no0(width);

            while (start < sse_end) {
                __m128i* a = reinterpret_cast<__m128i*>(m_data + start * width / 8);
                __m128i* b = reinterpret_cast<__m128i*>(foreign_m_data + start * width / 8);

                bool continue_search =
                    find_sse_intern<cond, action, width, Callback>(a, b, 1, state, baseindex + start, callback);

                if (!continue_search)
                    return false;

                start += 128 / no0(width);
            }
        }
    }
#endif

    while (start < end) {
        int64_t v = get_universal<width>(m_data, start);
        int64_t fv = get_universal<foreign_width>(foreign_m_data, start);

        if (c(v, fv)) {
            if (!find_action<action, Callback>(start + baseindex, v, state, callback))
                return false;
        }

        start++;
    }

    return true;
}


template <class cond, Action action, size_t bitwidth, class Callback>
bool Array::compare(int64_t value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
                    Callback callback) const
{
    bool ret = false;

    if (std::is_same<cond, Equal>::value)
        ret = compare_equality<true, action, bitwidth, Callback>(value, start, end, baseindex, state, callback);
    else if (std::is_same<cond, NotEqual>::value)
        ret = compare_equality<false, action, bitwidth, Callback>(value, start, end, baseindex, state, callback);
    else if (std::is_same<cond, Greater>::value)
        ret = compare_relation<true, action, bitwidth, Callback>(value, start, end, baseindex, state, callback);
    else if (std::is_same<cond, Less>::value)
        ret = compare_relation<false, action, bitwidth, Callback>(value, start, end, baseindex, state, callback);
    else
        REALM_ASSERT_DEBUG(false);

    return ret;
}

template <bool gt, Action action, size_t bitwidth, class Callback>
bool Array::compare_relation(int64_t value, size_t start, size_t end, size_t baseindex, QueryState<int64_t>* state,
                             Callback callback) const
{
    REALM_ASSERT(start <= m_size && (end <= m_size || end == size_t(-1)) && start <= end);
    uint64_t mask = (bitwidth == 64 ? ~0ULL : ((1ULL << (bitwidth == 64 ? 0 : bitwidth)) -
                                               1ULL)); // Warning free way of computing (1ULL << width) - 1

    size_t ee = round_up(start, 64 / no0(bitwidth));
    ee = ee > end ? end : ee;
    for (; start < ee; start++) {
        if (gt ? (get<bitwidth>(start) > value) : (get<bitwidth>(start) < value)) {
            if (!find_action<action, Callback>(start + baseindex, get<bitwidth>(start), state, callback))
                return false;
        }
    }

    if (start >= end)
        return true; // none found, continue (return true) regardless what find_action() would have returned on match

    const int64_t* p = reinterpret_cast<const int64_t*>(m_data + (start * bitwidth / 8));
    const int64_t* const e = reinterpret_cast<int64_t*>(m_data + (end * bitwidth / 8)) - 1;

    // Matches are rare enough to setup fast linear search for remaining items. We use
    // bit hacks from http://graphics.stanford.edu/~seander/bithacks.html#HasLessInWord

    if (bitwidth == 1 || bitwidth == 2 || bitwidth == 4 || bitwidth == 8 || bitwidth == 16) {
        uint64_t magic = find_gtlt_magic<gt, bitwidth>(value);

        // Bit hacks only work if searched item has its most significant bit clear for 'greater than' or
        // 'item <= 1 << bitwidth' for 'less than'
        if (value != int64_t((magic & mask)) && value >= 0 && bitwidth >= 2 &&
            value <= static_cast<int64_t>((mask >> 1) - (gt ? 1 : 0))) {
            // 15 ms
            while (p < e) {
                uint64_t upper = lower_bits<bitwidth>() << (no0(bitwidth) - 1);

                const int64_t v = *p;
                size_t idx;

                // Bit hacks only works if all items in chunk have their most significant bit clear. Test this:
                upper = upper & v;

                if (!upper) {
                    idx = find_gtlt_fast<gt, action, bitwidth, Callback>(
                        v, magic, state, (p - reinterpret_cast<int64_t*>(m_data)) * 8 * 8 / no0(bitwidth) + baseindex,
                        callback);
                }
                else
                    idx = find_gtlt<gt, action, bitwidth, Callback>(
                        value, v, state, (p - reinterpret_cast<int64_t*>(m_data)) * 8 * 8 / no0(bitwidth) + baseindex,
                        callback);

                if (!idx)
                    return false;
                ++p;
            }
        }
        else {
            // 24 ms
            while (p < e) {
                int64_t v = *p;
                if (!find_gtlt<gt, action, bitwidth, Callback>(
                        value, v, state, (p - reinterpret_cast<int64_t*>(m_data)) * 8 * 8 / no0(bitwidth) + baseindex,
                        callback))
                    return false;
                ++p;
            }
        }
        start = (p - reinterpret_cast<int64_t*>(m_data)) * 8 * 8 / no0(bitwidth);
    }

    // matchcount logic in SIMD no longer pays off for 32/64 bit ints because we have just 4/2 elements

    // Test unaligned end and/or values of width > 16 manually
    while (start < end) {
        if (gt ? get<bitwidth>(start) > value : get<bitwidth>(start) < value) {
            if (!find_action<action, Callback>(start + baseindex, get<bitwidth>(start), state, callback))
                return false;
        }
        ++start;
    }
    return true;
}

template <class cond>
size_t Array::find_first(int64_t value, size_t start, size_t end) const
{
    REALM_ASSERT(start <= m_size && (end <= m_size || end == size_t(-1)) && start <= end);
    // todo, would be nice to avoid this in order to speed up find_first loops
    QueryState<int64_t> state(act_ReturnFirst, 1);
    Finder finder = m_vtable->finder[cond::condition];
    (this->*finder)(value, start, end, 0, &state);

    return static_cast<size_t>(state.m_state);
}

//*************************************************************************************
// Finding code ends                                                                  *
//*************************************************************************************


} // namespace realm

#endif // REALM_ARRAY_HPP
