/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2015] Realm Inc
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of Realm Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Realm Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Realm Incorporated.
 *
 **************************************************************************/
#ifndef REALM_ARRAY_BIG_BLOBS_HPP
#define REALM_ARRAY_BIG_BLOBS_HPP

#include <realm/array_blob.hpp>

namespace realm {


class ArrayBigBlobs: public Array {
public:
    typedef BinaryData value_type;

    explicit ArrayBigBlobs(Allocator&, bool nullable) noexcept;

    BinaryData get(size_t ndx) const noexcept;
    void set(size_t ndx, BinaryData value, bool add_zero_term = false);
    void add(BinaryData value, bool add_zero_term = false);
    void insert(size_t ndx, BinaryData value, bool add_zero_term = false);
    void erase(size_t ndx);
    void truncate(size_t new_size);
    void clear();
    void destroy();

    size_t count(BinaryData value, bool is_string = false, size_t begin = 0,
                      size_t end = npos) const noexcept;
    size_t find_first(BinaryData value, bool is_string = false, size_t begin = 0,
                           size_t end = npos) const noexcept;
    void find_all(IntegerColumn& result, BinaryData value, bool is_string = false,
                  size_t add_offset = 0,
                  size_t begin = 0, size_t end = npos);

    /// Get the specified element without the cost of constructing an
    /// array instance. If an array instance is already available, or
    /// you need to get multiple values, then this method will be
    /// slower.
    static BinaryData get(const char* header, size_t ndx, Allocator&) noexcept;

    ref_type bptree_leaf_insert(size_t ndx, BinaryData, bool add_zero_term,
                                TreeInsertBase& state);

    //@{
    /// Those that return a string, discard the terminating zero from
    /// the stored value. Those that accept a string argument, add a
    /// terminating zero before storing the value.
    StringData get_string(size_t ndx) const noexcept;
    void add_string(StringData value);
    void set_string(size_t ndx, StringData value);
    void insert_string(size_t ndx, StringData value);
    static StringData get_string(const char* header, size_t ndx, Allocator&, bool nullable) noexcept;
    ref_type bptree_leaf_insert_string(size_t ndx, StringData, TreeInsertBase& state);
    //@}

    /// Create a new empty big blobs array and attach this accessor to
    /// it. This does not modify the parent reference information of
    /// this accessor.
    ///
    /// Note that the caller assumes ownership of the allocated
    /// underlying node. It is not owned by the accessor.
    void create();

    /// Construct a copy of the specified slice of this big blobs
    /// array using the specified target allocator.
    MemRef slice(size_t offset, size_t slice_size, Allocator& target_alloc) const;

#ifdef REALM_DEBUG
    void verify() const;
    void to_dot(std::ostream&, bool is_strings, StringData title = StringData()) const;
#endif

private:
    bool m_nullable;
};



// Implementation:

inline ArrayBigBlobs::ArrayBigBlobs(Allocator& allocator, bool nullable) noexcept:
                                    Array(allocator), m_nullable(nullable)
{
}

inline BinaryData ArrayBigBlobs::get(size_t ndx) const noexcept
{
    ref_type ref = get_as_ref(ndx);
    if (ref == 0)
        return BinaryData(); // realm::null();

    const char* blob_header = get_alloc().translate(ref);
    const char* value = ArrayBlob::get(blob_header, 0);
    size_t blob_size = get_size_from_header(blob_header);
    return BinaryData(value, blob_size);
}

inline BinaryData ArrayBigBlobs::get(const char* header, size_t ndx,
                                     Allocator& alloc) noexcept
{
    ref_type blob_ref = to_ref(Array::get(header, ndx));
    if (blob_ref == 0)
        return BinaryData();

    const char* blob_header = alloc.translate(blob_ref);
    const char* blob_data = Array::get_data_from_header(blob_header);
    size_t blob_size = Array::get_size_from_header(blob_header);
    return BinaryData(blob_data, blob_size);
}

inline void ArrayBigBlobs::erase(size_t ndx)
{
    ref_type blob_ref = Array::get_as_ref(ndx);
    if (blob_ref != 0) { // nothing to destroy if null
        Array::destroy(blob_ref, get_alloc()); // Shallow
    }
    Array::erase(ndx);
}

inline void ArrayBigBlobs::truncate(size_t new_size)
{
    Array::truncate_and_destroy_children(new_size);
}

inline void ArrayBigBlobs::clear()
{
    Array::clear_and_destroy_children();
}

inline void ArrayBigBlobs::destroy()
{
    Array::destroy_deep();
}

inline StringData ArrayBigBlobs::get_string(size_t ndx) const noexcept
{
    BinaryData bin = get(ndx);
    if (bin.is_null())
        return realm::null();
    else
        return StringData(bin.data(), bin.size()-1); // Do not include terminating zero
}

inline void ArrayBigBlobs::set_string(size_t ndx, StringData value)
{
    REALM_ASSERT_DEBUG(!(!m_nullable && value.is_null()));
    BinaryData bin(value.data(), value.size());
    bool add_zero_term = true;
    set(ndx, bin, add_zero_term);
}

inline void ArrayBigBlobs::add_string(StringData value)
{
    REALM_ASSERT_DEBUG(!(!m_nullable && value.is_null()));
    BinaryData bin(value.data(), value.size());
    bool add_zero_term = true;
    add(bin, add_zero_term);
}

inline void ArrayBigBlobs::insert_string(size_t ndx, StringData value)
{
    REALM_ASSERT_DEBUG(!(!m_nullable && value.is_null()));
    BinaryData bin(value.data(), value.size());
    bool add_zero_term = true;
    insert(ndx, bin, add_zero_term);
}

inline StringData ArrayBigBlobs::get_string(const char* header, size_t ndx,
                                            Allocator& alloc, bool nullable) noexcept
{
    static_cast<void>(nullable);
    BinaryData bin = get(header, ndx, alloc);
    REALM_ASSERT_DEBUG(!(!nullable && bin.is_null()));
    if (bin.is_null())
        return realm::null();
    else
        return StringData(bin.data(), bin.size()-1); // Do not include terminating zero
}

inline ref_type ArrayBigBlobs::bptree_leaf_insert_string(size_t ndx, StringData value,
                                                         TreeInsertBase& state)
{
    REALM_ASSERT_DEBUG(!(!m_nullable && value.is_null()));
    BinaryData bin(value.data(), value.size());
    bool add_zero_term = true;
    return bptree_leaf_insert(ndx, bin, add_zero_term, state);
}

inline void ArrayBigBlobs::create()
{
    bool context_flag = true;
    Array::create(type_HasRefs, context_flag); // Throws
}

inline MemRef ArrayBigBlobs::slice(size_t offset, size_t slice_size,
                                   Allocator& target_alloc) const
{
    return slice_and_clone_children(offset, slice_size, target_alloc);
}


} // namespace realm

#endif // REALM_ARRAY_BIG_BLOBS_HPP
