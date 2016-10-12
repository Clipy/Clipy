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

#ifndef REALM_IMPL_SEQUENTIAL_GETTER_HPP
#define REALM_IMPL_SEQUENTIAL_GETTER_HPP

namespace realm {

class SequentialGetterBase {
public:
    virtual ~SequentialGetterBase() noexcept {}
};

template<class ColType>
class SequentialGetter : public SequentialGetterBase {
public:
    using T = typename ColType::value_type;
    using ArrayType = typename ColType::LeafType;

    SequentialGetter() {}

    SequentialGetter(const Table& table, size_t column_ndx)
    {
        if (column_ndx != not_found)
            m_column = static_cast<const ColType*>(&table.get_column_base(column_ndx));
        init(m_column);
    }

    SequentialGetter(const ColType* column)
    {
        init(column);
    }

    ~SequentialGetter() noexcept override {}

    void init(const ColType* column)
    {
        m_array_ptr.reset(); // Explicitly destroy the old one first, because we're reusing the memory.
        m_array_ptr.reset(new(&m_leaf_accessor_storage) ArrayType(column->get_alloc()));
        m_column = column;
        m_leaf_end = 0;
    }

    REALM_FORCEINLINE bool cache_next(size_t index)
    {
        // Return whether or not leaf array has changed (could be useful to know for caller)
        if (index >= m_leaf_end || index < m_leaf_start) {
            typename ColType::LeafInfo leaf { &m_leaf_ptr, m_array_ptr.get() };
            size_t ndx_in_leaf;
            m_column->get_leaf(index, ndx_in_leaf, leaf);
            m_leaf_start = index - ndx_in_leaf;
            const size_t leaf_size = m_leaf_ptr->size();
            m_leaf_end = m_leaf_start + leaf_size;
            return true;
        }
        return false;
    }


    REALM_FORCEINLINE T get_next(size_t index)
    {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4800)   // Disable the Microsoft warning about bool performance issue.
#endif

        cache_next(index);
        T av = m_leaf_ptr->get(index - m_leaf_start);
        return av;

#ifdef _MSC_VER
#pragma warning(pop)
#endif
    }

    size_t local_end(size_t global_end)
    {
        if (global_end > m_leaf_end)
            return m_leaf_end - m_leaf_start;
        else
            return global_end - m_leaf_start;
    }

    size_t m_leaf_start;
    size_t m_leaf_end;
    const ColType* m_column;

    const ArrayType* m_leaf_ptr = nullptr;
private:
    // Leaf cache for when the root of the column is not a leaf.
    // This dog and pony show is because Array has a reference to Allocator internally,
    // but we need to be able to transfer queries between contexts, so init() reinitializes
    // the leaf cache in the context of the current column.
    typename std::aligned_storage<sizeof(ArrayType), alignof(ArrayType)>::type m_leaf_accessor_storage;
    std::unique_ptr<ArrayType, PlacementDelete> m_array_ptr;
};

} // namespace realm

#endif // REALM_IMPL_SEQUENTIAL_GETTER_HPP
