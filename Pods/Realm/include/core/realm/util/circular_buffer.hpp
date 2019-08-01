/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2016] Realm Inc
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

#ifndef REALM_UTIL_CIRCULAR_BUFFER_HPP
#define REALM_UTIL_CIRCULAR_BUFFER_HPP

#include <cstddef>
#include <type_traits>
#include <limits>
#include <memory>
#include <iterator>
#include <algorithm>
#include <utility>
#include <stdexcept>
#include <initializer_list>

#include <realm/util/safe_int_ops.hpp>
#include <realm/util/backtrace.hpp>

namespace realm {
namespace util {

/// \brief A container backed by a "circular buffer".
///
/// This container is similar to std::deque in that it offers efficient element
/// insertion and removal at both ends. Insertion at either end occurs in
/// amortized constant time. Removal at either end occurs in constant time.
///
/// As opposed to std::deque, this container allows for reservation of buffer
/// space, such that value insertion can be guaranteed to not reallocate buffer
/// memory, and to not throw.
///
/// More specifically, a single insert operation, that inserts zero or more
/// values at either end, is guaranteed to not reallocate buffer memory if the
/// prior capacity (capacity()) is greater than, or equal to the prior size
/// (size()) plus the number of inserted values. Further more, such an operation
/// is guaranteed to not throw if the capacity is sufficient, and the relevant
/// constructor of the value type does not throw, and, in the case of inserting
/// a range of values specified as a pair of iterators, if no exception is
/// thrown while operating on those iterators.
///
/// This container uses a single contiguous chunk of memory as backing storage,
/// but it allows for the logical sequence of values to wrap around from the
/// end, to the beginning of that chunk. Because the logical sequence of values
/// can have a storage-wise discontinuity of this kind, this container does not
/// meet the requirements of `ContiguousContainer` (as defined by C++17).
///
/// When the first element is removed (pop_front()), iterators pointing to the
/// removed element will be invalidated. All other iterators, including "end
/// iterators" (end()), will remain valid.
///
/// When the last element is removed (pop_back()), iterators pointing to the
/// removed element will become "end iterators" (end()), and "end iterators"
/// will be invalidated. All other iterators will remain valid.
///
/// When an element is inserted at the front (push_front()), and the prior
/// capacity (capacity()) is strictly greater than the prior size (size()), all
/// iterators remain valid.
///
/// When an element is inserted at the back (push_back()), and the prior
/// capacity (capacity()) is strictly greater than the prior size (size()), "end
/// iterators" (end()) become iterators to the inserted element, and all other
/// iterators remain valid.
///
/// Operations pop_front(), pop_back(), and clear(), are guaranteed to leave the
/// capacity unchanged.
///
/// Iterators are of the "random access" kind (std::random_access_iterator_tag).
template<class T> class CircularBuffer {
private:
    template<class> class Iter;

    template<class I> using RequireIter =
        std::enable_if_t<std::is_convertible<typename std::iterator_traits<I>::iterator_category,
                                             std::input_iterator_tag>::value>;

public:
    static_assert(std::is_nothrow_destructible<T>::value, "");

    using value_type      = T;
    using size_type       = std::size_t;
    using reference       = value_type&;
    using const_reference = const value_type&;
    using iterator        = Iter<value_type>;
    using const_iterator  = Iter<const value_type>;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    CircularBuffer() noexcept;
    CircularBuffer(const CircularBuffer&);
    CircularBuffer(CircularBuffer&&) noexcept;
    CircularBuffer(std::initializer_list<T>);
    explicit CircularBuffer(size_type size);
    CircularBuffer(size_type size, const T& value);
    template<class I, class = RequireIter<I>> CircularBuffer(I begin, I end);
    ~CircularBuffer() noexcept;

    CircularBuffer& operator=(const CircularBuffer&);
    CircularBuffer& operator=(CircularBuffer&&) noexcept;
    CircularBuffer& operator=(std::initializer_list<T>);

    void assign(std::initializer_list<T>);
    void assign(size_type size, const T& value);
    template<class I, class = RequireIter<I>> void assign(I begin, I end);

    // Element access

    reference at(size_type);
    const_reference at(size_type) const;

    reference operator[](size_type) noexcept;
    const_reference operator[](size_type) const noexcept;

    reference front() noexcept;
    const_reference front() const noexcept;

    reference back() noexcept;
    const_reference back() const noexcept;

    // Iterators

    iterator begin() noexcept;
    const_iterator begin() const noexcept;
    const_iterator cbegin() const noexcept;

    iterator end() noexcept;
    const_iterator end() const noexcept;
    const_iterator cend() const noexcept;

    reverse_iterator rbegin() noexcept;
    const_reverse_iterator rbegin() const noexcept;
    const_reverse_iterator crbegin() const noexcept;

    reverse_iterator rend() noexcept;
    const_reverse_iterator rend() const noexcept;
    const_reverse_iterator crend() const noexcept;

    // Size / capacity

    bool empty() const noexcept;
    size_type size() const noexcept;
    size_type capacity() const noexcept;

    void reserve(size_type capacity);
    void shrink_to_fit();

    // Modifiers

    reference push_front(const T&);
    reference push_back(const T&);

    reference push_front(T&&);
    reference push_back(T&&);

    template<class... Args> reference emplace_front(Args&&...);
    template<class... Args> reference emplace_back(Args&&...);

    void pop_front() noexcept;
    void pop_back() noexcept;

    // FIXME: emplace(const_iterator i, ...) -> j = unwrap(i.m_index); if (j >= (m_size+1)/2) insert_near_back(j, ...); else insert_near_front(j, ...);                            

    void clear() noexcept;
    void resize(size_type size);
    void resize(size_type size, const T& value);

    void swap(CircularBuffer&) noexcept;

    // Comparison

    template<class U> bool operator==(const CircularBuffer<U>&) const
        noexcept(noexcept(std::declval<T>() == std::declval<U>()));
    template<class U> bool operator!=(const CircularBuffer<U>&) const
        noexcept(noexcept(std::declval<T>() == std::declval<U>()));
    template<class U> bool operator<(const CircularBuffer<U>&) const
        noexcept(noexcept(std::declval<T>() < std::declval<U>()));
    template<class U> bool operator>(const CircularBuffer<U>&) const
        noexcept(noexcept(std::declval<T>() < std::declval<U>()));
    template<class U> bool operator<=(const CircularBuffer<U>&) const
        noexcept(noexcept(std::declval<T>() < std::declval<U>()));
    template<class U> bool operator>=(const CircularBuffer<U>&) const
        noexcept(noexcept(std::declval<T>() < std::declval<U>()));

private:
    using Strut = typename std::aligned_storage<sizeof(T), alignof(T)>::type;
    std::unique_ptr<Strut[]> m_memory_owner;

    // Index of first element in allocated memory chunk.
    size_type m_begin = 0;

    // The number of elements within the allocated memory chunk, that are
    // currently in use, i.e., the logical size of the circular buffer.
    size_type m_size = 0;

    // Number of elements of type T that will fit into the currently allocated
    // memory chunk.
    //
    // Except when m_size is zero, m_allocated_size must be strictly greater
    // than m_size. This is required to ensure that the iterators returned by
    // begin() and end() are equal only when the buffer is empty.
    //
    // INVARIANT: m_size == 0 ? m_allocated_size == 0 : m_size < m_allocated_size
    size_type m_allocated_size = 0;

    T* get_memory_ptr() noexcept;

    // Assumption: index < m_allocated_size
    size_type circular_inc(size_type index) noexcept;
    size_type circular_dec(size_type index) noexcept;
    size_type wrap(size_type index) noexcept;
    size_type unwrap(size_type index) noexcept;

    template<class I> void copy(I begin, I end);
    template<class I> void copy(I begin, I end, std::input_iterator_tag);
    template<class I> void copy(I begin, I end, std::forward_iterator_tag);

    void destroy(size_type offset = 0) noexcept;

    void realloc(size_type new_allocated_size);
};


template<class T> void swap(CircularBuffer<T>&, CircularBuffer<T>&) noexcept;




// Implementation

template<class T> template<class U> class CircularBuffer<T>::Iter :
        public std::iterator<std::random_access_iterator_tag, U> {
public:
    using difference_type = std::ptrdiff_t;

    Iter() noexcept
    {
    }

    template<class V> Iter(const Iter<V>& i) noexcept
    {
        operator=(i);
    }

    template<class V> Iter& operator=(const Iter<V>& i) noexcept
    {
        // Check constness convertability
        static_assert(std::is_convertible<V*,U*>::value, "");
        m_buffer = i.m_buffer;
        m_index = i.m_index;
        return *this;
    }

    U& operator*() const noexcept
    {
        T* memory = m_buffer->get_memory_ptr();
        return memory[m_index];
    }

    U* operator->() const noexcept
    {
        return &operator*();
    }

    U& operator[](difference_type i) const noexcept
    {
        Iter j = *this;
        j += i;
        return *j;
    }

    Iter& operator++() noexcept
    {
        m_index = m_buffer->circular_inc(m_index);
        return *this;
    }

    Iter& operator--() noexcept
    {
        m_index = m_buffer->circular_dec(m_index);
        return *this;
    }

    Iter operator++(int) noexcept
    {
        size_type i = m_index;
        operator++();
        return Iter{m_buffer, i};
    }

    Iter operator--(int) noexcept
    {
        size_type i = m_index;
        operator--();
        return Iter{m_buffer, i};
    }

    Iter& operator+=(difference_type value) noexcept
    {
        // Care is needed to avoid unspecified arithmetic behaviour here. We can
        // assume, however, that if `i` is the unwrapped (logical) index of the
        // element pointed to by this iterator, then the mathematical value of i
        // + value is representable in `size_type` (otherwise the resulting
        // iterator would escape the boundaries of the buffer). We can therefore
        // safely perform the addition in the unsigned domain of unwrapped
        // element indexes, and rely on two's complement representation for
        // negative values.
        size_type i = m_buffer->unwrap(m_index);
        i += size_type(value);
        m_index = m_buffer->wrap(i);
        return *this;
    }

    Iter& operator-=(difference_type value) noexcept
    {
        // Care is needed to avoid unspecified arithmetic behaviour here. See
        // the comment in the implementation of operator+=().
        size_type i = m_buffer->unwrap(m_index);
        i -= size_type(value);
        m_index = m_buffer->wrap(i);
        return *this;
    }

    Iter operator+(difference_type value) const noexcept
    {
        Iter i = *this;
        i += value;
        return i;
    }

    Iter operator-(difference_type value) const noexcept
    {
        Iter i = *this;
        i -= value;
        return i;
    }

    friend Iter operator+(difference_type value, const Iter& i) noexcept
    {
        Iter j = i;
        j += value;
        return j;
    }

    template<class V> difference_type operator-(const Iter<V>& i) const noexcept
    {
        REALM_ASSERT(m_buffer == i.m_buffer);
        size_type i_1 = m_buffer->unwrap(m_index);
        size_type i_2 = i.m_buffer->unwrap(i.m_index);
        return from_twos_compl<difference_type>(size_type(i_1 - i_2));
    }

    template<class V> bool operator==(const Iter<V>& i) const noexcept
    {
        REALM_ASSERT(m_buffer == i.m_buffer);
        return (m_index == i.m_index);
    }

    template<class V> bool operator!=(const Iter<V>& i) const noexcept
    {
        return !operator==(i);
    }

    template<class V> bool operator<(const Iter<V>& i) const noexcept
    {
        REALM_ASSERT(m_buffer == i.m_buffer);
        size_type i_1 = m_buffer->unwrap(m_index);
        size_type i_2 = i.m_buffer->unwrap(i.m_index);
        return (i_1 < i_2);
    }

    template<class V> bool operator>(const Iter<V>& i) const noexcept
    {
        return (i < *this);
    }

    template<class V> bool operator<=(const Iter<V>& i) const noexcept
    {
        return !operator>(i);
    }

    template<class V> bool operator>=(const Iter<V>& i) const noexcept
    {
        return !operator<(i);
    }

private:
    CircularBuffer* m_buffer = nullptr;

    // Index of iterator position from beginning of allocated memory, i.e., from
    // beginning of m_buffer->get_memory_ptr().
    size_type m_index = 0;

    Iter(CircularBuffer* buffer, size_type index) noexcept :
        m_buffer{buffer},
        m_index{index}
    {
    }

    friend class CircularBuffer<T>;
    template<class> friend class Iter;
};

template<class T> inline CircularBuffer<T>::CircularBuffer() noexcept
{
}

template<class T> inline CircularBuffer<T>::CircularBuffer(const CircularBuffer& buffer)
{
    try {
        copy(buffer.begin(), buffer.end()); // Throws
    }
    catch (...) {
        // If an exception was thrown above, the destructor will not be called,
        // so we need to manually destroy the copies that were already made.
        destroy();
        throw;
    }
}

template<class T> inline CircularBuffer<T>::CircularBuffer(CircularBuffer&& buffer) noexcept :
    m_memory_owner{std::move(buffer.m_memory_owner)},
    m_begin{buffer.m_begin},
    m_size{buffer.m_size},
    m_allocated_size{buffer.m_allocated_size}
{
    buffer.m_begin          = 0;
    buffer.m_size           = 0;
    buffer.m_allocated_size = 0;
}

template<class T> inline CircularBuffer<T>::CircularBuffer(std::initializer_list<T> list)
{
    try {
        copy(list.begin(), list.end()); // Throws
    }
    catch (...) {
        // If an exception was thrown above, the destructor will not be called,
        // so we need to manually destroy the copies that were already made.
        destroy();
        throw;
    }
}

template<class T> inline CircularBuffer<T>::CircularBuffer(size_type count)
{
    try {
        resize(count); // Throws
    }
    catch (...) {
        // If an exception was thrown above, the destructor will not be called,
        // so we need to manually destroy the instances that were already
        // created.
        destroy();
        throw;
    }
}

template<class T> inline CircularBuffer<T>::CircularBuffer(size_type count, const T& value)
{
    try {
        resize(count, value); // Throws
    }
    catch (...) {
        // If an exception was thrown above, the destructor will not be called,
        // so we need to manually destroy the copies that were already made.
        destroy();
        throw;
    }
}

template<class T> template<class I, class> inline CircularBuffer<T>::CircularBuffer(I begin, I end)
{
    try {
        copy(begin, end); // Throws
    }
    catch (...) {
        // If an exception was thrown above, the destructor will not be called,
        // so we need to manually destroy the copies that were already made.
        destroy();
        throw;
    }
}

template<class T> inline CircularBuffer<T>::~CircularBuffer() noexcept
{
    destroy();
}

template<class T>
inline auto CircularBuffer<T>::operator=(const CircularBuffer& buffer) -> CircularBuffer&
{
    clear();
    copy(buffer.begin(), buffer.end()); // Throws
    return *this;
}

template<class T>
inline auto CircularBuffer<T>::operator=(CircularBuffer&& buffer) noexcept -> CircularBuffer&
{
    destroy();
    m_memory_owner   = std::move(buffer.m_memory_owner);
    m_begin          = buffer.m_begin;
    m_size           = buffer.m_size;
    m_allocated_size = buffer.m_allocated_size;
    buffer.m_begin          = 0;
    buffer.m_size           = 0;
    buffer.m_allocated_size = 0;
    return *this;
}

template<class T>
inline auto CircularBuffer<T>::operator=(std::initializer_list<T> list) -> CircularBuffer&
{
    clear();
    copy(list.begin(), list.end()); // Throws
    return *this;
}

template<class T> inline void CircularBuffer<T>::assign(std::initializer_list<T> list)
{
    clear();
    copy(list.begin(), list.end()); // Throws
}

template<class T> inline void CircularBuffer<T>::assign(size_type count, const T& value)
{
    clear();
    resize(count, value); // Throws
}

template<class T> template<class I, class> inline void CircularBuffer<T>::assign(I begin, I end)
{
    clear();
    copy(begin, end); // Throws
}

template<class T> inline auto CircularBuffer<T>::at(size_type i) -> reference
{
    if (REALM_LIKELY(i < m_size))
        return operator[](i);
    throw util::out_of_range{"Index"};
}

template<class T> inline auto CircularBuffer<T>::at(size_type i) const -> const_reference
{
    return const_cast<CircularBuffer*>(this)->at(i); // Throws
}

template<class T>
inline auto CircularBuffer<T>::operator[](size_type i) noexcept -> reference
{
    REALM_ASSERT(i < m_size);
    T* memory = get_memory_ptr();
    size_type j = wrap(i);
    return memory[j];
}

template<class T>
inline auto CircularBuffer<T>::operator[](size_type i) const noexcept -> const_reference
{
    return const_cast<CircularBuffer*>(this)->operator[](i);
}

template<class T> inline auto CircularBuffer<T>::front() noexcept -> reference
{
    return operator[](0);
}

template<class T> inline auto CircularBuffer<T>::front() const noexcept -> const_reference
{
    return operator[](0);
}

template<class T> inline auto CircularBuffer<T>::back() noexcept -> reference
{
    return operator[](m_size-1);
}

template<class T>
inline auto CircularBuffer<T>::back() const noexcept -> const_reference
{
    return operator[](m_size-1);
}

template<class T> inline auto CircularBuffer<T>::begin() noexcept -> iterator
{
    return iterator{this, m_begin};
}

template<class T> inline auto CircularBuffer<T>::begin() const noexcept -> const_iterator
{
    return const_cast<CircularBuffer*>(this)->begin();
}

template<class T> inline auto CircularBuffer<T>::cbegin() const noexcept -> const_iterator
{
    return begin();
}

template<class T> inline auto CircularBuffer<T>::end() noexcept -> iterator
{
    size_type i = wrap(m_size);
    return iterator{this, i};
}

template<class T> inline auto CircularBuffer<T>::end() const noexcept -> const_iterator
{
    return const_cast<CircularBuffer*>(this)->end();
}

template<class T> inline auto CircularBuffer<T>::cend() const noexcept -> const_iterator
{
    return end();
}

template<class T> inline auto CircularBuffer<T>::rbegin() noexcept -> reverse_iterator
{
    return std::reverse_iterator<iterator>(end());
}

template<class T> inline auto CircularBuffer<T>::rbegin() const noexcept -> const_reverse_iterator
{
    return const_cast<CircularBuffer*>(this)->rbegin();
}

template<class T> inline auto CircularBuffer<T>::crbegin() const noexcept -> const_reverse_iterator
{
    return rbegin();
}

template<class T> inline auto CircularBuffer<T>::rend() noexcept -> reverse_iterator
{
    return std::reverse_iterator<iterator>(begin());
}

template<class T> inline auto CircularBuffer<T>::rend() const noexcept -> const_reverse_iterator
{
    return const_cast<CircularBuffer*>(this)->rend();
}

template<class T> inline auto CircularBuffer<T>::crend() const noexcept -> const_reverse_iterator
{
    return rend();
}

template<class T> inline bool CircularBuffer<T>::empty() const noexcept
{
    return (m_size == 0);
}

template<class T> inline auto CircularBuffer<T>::size() const noexcept -> size_type
{
    return m_size;
}

template<class T> void CircularBuffer<T>::reserve(size_type capacity)
{
    if (capacity == 0)
        return;

    // An extra element of capacity is needed such that the end iterator can
    // always point one beyond the last element without becomeing equal to an
    // iterator to the first element.
    size_type min_allocated_size = capacity;
    if (REALM_UNLIKELY(int_add_with_overflow_detect(min_allocated_size, 1)))
        throw util::overflow_error{"Capacity"};

    if (min_allocated_size <= m_allocated_size)
        return;

    size_type new_allocated_size = m_allocated_size;
    if (REALM_UNLIKELY(int_multiply_with_overflow_detect(new_allocated_size, 2)))
        new_allocated_size = std::numeric_limits<size_type>::max();
    if (new_allocated_size < min_allocated_size)
        new_allocated_size = min_allocated_size;
    realloc(new_allocated_size); // Throws
}

template<class T> inline void CircularBuffer<T>::shrink_to_fit()
{
    if (m_size > 0) {
        // An extra element of capacity is needed such that the end iterator can
        // always point one beyond the last element without becomeing equal to
        // an iterator to the first element.
        size_type new_allocated_size = m_size + 1;
        if (new_allocated_size < m_allocated_size)
            realloc(new_allocated_size); // Throws
    }
    else {
        m_memory_owner.reset();
        m_begin = 0;
        m_allocated_size = 0;
    }
}

template<class T> inline auto CircularBuffer<T>::capacity() const noexcept -> size_type
{
    return (m_allocated_size > 0 ? m_allocated_size - 1 : 0);
}

template<class T> inline auto CircularBuffer<T>::push_front(const T& value) -> reference
{
    return emplace_front(value); // Throws
}

template<class T> inline auto CircularBuffer<T>::push_back(const T& value) -> reference
{
    return emplace_back(value); // Throws
}

template<class T> inline auto CircularBuffer<T>::push_front(T&& value) -> reference
{
    return emplace_front(value); // Throws
}

template<class T> inline auto CircularBuffer<T>::push_back(T&& value) -> reference
{
    return emplace_back(value); // Throws
}

template<class T>
template<class... Args> inline auto CircularBuffer<T>::emplace_front(Args&&... args) -> reference
{
    size_type new_size = m_size + 1;
    reserve(new_size); // Throws
    REALM_ASSERT(m_allocated_size > 0);
    T* memory = get_memory_ptr();
    size_type i = circular_dec(m_begin);
    new (&memory[i]) T(std::forward<Args>(args)...); // Throws
    m_begin = i;
    m_size = new_size;
    return memory[i];
}

template<class T>
template<class... Args> inline auto CircularBuffer<T>::emplace_back(Args&&... args) -> reference
{
    size_type new_size = m_size + 1;
    reserve(new_size); // Throws
    REALM_ASSERT(m_allocated_size > 0);
    T* memory = get_memory_ptr();
    size_type i = wrap(m_size);
    new (&memory[i]) T(std::forward<Args>(args)...); // Throws
    m_size = new_size;
    return memory[i];
}

template<class T> inline void CircularBuffer<T>::pop_front() noexcept
{
    REALM_ASSERT(m_size > 0);
    T* memory = get_memory_ptr();
    memory[m_begin].~T();
    m_begin = circular_inc(m_begin);
    --m_size;
}

template<class T> inline void CircularBuffer<T>::pop_back() noexcept
{
    REALM_ASSERT(m_size > 0);
    T* memory = get_memory_ptr();
    size_type new_size = m_size - 1;
    size_type i = wrap(new_size);
    memory[i].~T();
    m_size = new_size;
}

template<class T> inline void CircularBuffer<T>::clear() noexcept
{
    destroy();
    m_begin = 0;
    m_size = 0;
}

template<class T> inline void CircularBuffer<T>::resize(size_type size)
{
    if (size <= m_size) {
        size_type offset = size;
        destroy(offset);
        m_size = size;
        return;
    }
    reserve(size); // Throws
    T* memory = get_memory_ptr();
    size_type i = wrap(m_size);
    do {
        new (&memory[i]) T(); // Throws
        i = circular_inc(i);
        ++m_size;
    }
    while (m_size < size);
}

template<class T> inline void CircularBuffer<T>::resize(size_type size, const T& value)
{
    if (size <= m_size) {
        size_type offset = size;
        destroy(offset);
        m_size = size;
        return;
    }
    reserve(size); // Throws
    T* memory = get_memory_ptr();
    size_type i = wrap(m_size);
    do {
        new (&memory[i]) T(value); // Throws
        i = circular_inc(i);
        ++m_size;
    }
    while (m_size < size);
}

template<class T> inline void CircularBuffer<T>::swap(CircularBuffer& buffer) noexcept
{
    std::swap(m_memory_owner,   buffer.m_memory_owner);
    std::swap(m_begin,          buffer.m_begin);
    std::swap(m_size,           buffer.m_size);
    std::swap(m_allocated_size, buffer.m_allocated_size);
}

template<class T> template<class U>
inline bool CircularBuffer<T>::operator==(const CircularBuffer<U>& buffer) const
    noexcept(noexcept(std::declval<T>() == std::declval<U>()))
{
    return std::equal(begin(), end(), buffer.begin(), buffer.end()); // Throws
}

template<class T> template<class U>
inline bool CircularBuffer<T>::operator!=(const CircularBuffer<U>& buffer) const
    noexcept(noexcept(std::declval<T>() == std::declval<U>()))
{
    return !operator==(buffer); // Throws
}

template<class T> template<class U>
inline bool CircularBuffer<T>::operator<(const CircularBuffer<U>& buffer) const
    noexcept(noexcept(std::declval<T>() < std::declval<U>()))
{
    return std::lexicographical_compare(begin(), end(), buffer.begin(), buffer.end()); // Throws
}

template<class T> template<class U>
inline bool CircularBuffer<T>::operator>(const CircularBuffer<U>& buffer) const
    noexcept(noexcept(std::declval<T>() < std::declval<U>()))
{
    return (buffer < *this); // Throws
}

template<class T> template<class U>
inline bool CircularBuffer<T>::operator<=(const CircularBuffer<U>& buffer) const
    noexcept(noexcept(std::declval<T>() < std::declval<U>()))
{
    return !operator>(buffer); // Throws
}

template<class T> template<class U>
inline bool CircularBuffer<T>::operator>=(const CircularBuffer<U>& buffer) const
    noexcept(noexcept(std::declval<T>() < std::declval<U>()))
{
    return !operator<(buffer); // Throws
}

template<class T> inline T* CircularBuffer<T>::get_memory_ptr() noexcept
{
    return static_cast<T*>(static_cast<void*>(m_memory_owner.get()));
}

template<class T>
inline auto CircularBuffer<T>::circular_inc(size_type index) noexcept -> size_type
{
    size_type index_2 = index + 1;
    if (REALM_LIKELY(index_2 < m_allocated_size))
        return index_2;
    return 0;
}

template<class T>
inline auto CircularBuffer<T>::circular_dec(size_type index) noexcept -> size_type
{
    if (REALM_LIKELY(index > 0))
        return index - 1;
    return m_allocated_size - 1;
}

template<class T>
inline auto CircularBuffer<T>::wrap(size_type index) noexcept -> size_type
{
    size_type top = m_allocated_size - m_begin;
    if (index < top)
        return m_begin + index;
    return index - top;
}

template<class T>
inline auto CircularBuffer<T>::unwrap(size_type index) noexcept -> size_type
{
    if (index >= m_begin)
        return index - m_begin;
    return m_allocated_size - (m_begin - index);
}

template<class T> template<class I> inline void CircularBuffer<T>::copy(I begin, I end)
{
    using iterator_category = typename std::iterator_traits<I>::iterator_category;
    copy(begin, end, iterator_category{}); // Throws
}

template<class T> template<class I>
inline void CircularBuffer<T>::copy(I begin, I end, std::input_iterator_tag)
{
    for (I j = begin; j != end; ++j)
        push_back(*j); // Throws
}

template<class T> template<class I>
inline void CircularBuffer<T>::copy(I begin, I end, std::forward_iterator_tag)
{
    REALM_ASSERT(m_begin == 0);
    REALM_ASSERT(m_size == 0);
    size_type size = std::distance(begin, end);
    reserve(size); // Throws
    T* memory = get_memory_ptr();
    for (I i = begin; i != end; ++i) {
        new (&memory[m_size]) T(*i); // Throws
        ++m_size;
    }
}

template<class T> inline void CircularBuffer<T>::destroy(size_type offset) noexcept
{
    T* memory = get_memory_ptr();
    size_type j = m_begin;
    for (size_type i = offset; i < m_size; ++i) {
        memory[j].~T();
        j = circular_inc(j);
    }
}

template<class T> void CircularBuffer<T>::realloc(size_type new_allocated_size)
{
    REALM_ASSERT(new_allocated_size > 1);
    REALM_ASSERT(new_allocated_size > m_size);

    // Allocate new buffer
    std::unique_ptr<Strut[]> new_memory_owner =
        std::make_unique<Strut[]>(new_allocated_size); // Throws
    T* memory = get_memory_ptr();

    // Move or copy elements to new buffer
    {
        T* new_memory = static_cast<T*>(static_cast<void*>(new_memory_owner.get()));
        size_type i = 0;
        try {
            size_type j = m_begin;
            while (i < m_size) {
                new (&new_memory[i]) T(std::move_if_noexcept(memory[j])); // Throws
                ++i;
                j = circular_inc(j);
            }
        }
        catch (...) {
            // If an exception was thrown above, we know that elements were
            // copied, and not moved (assuming that T is copy constructable if
            // it is not nothrow move constructible), so we need to back out by
            // destroying the copies that were already made.
            for (size_type j = 0; j < i; ++j)
                new_memory[j].~T();
            throw;
        }
    }

    // Destroy old elements
    {
        size_type j = m_begin;
        for (size_type i = 0; i < m_size; ++i) {
            memory[j].~T();
            j = circular_inc(j);
        }
    }

    m_memory_owner = std::move(new_memory_owner);
    m_begin = 0;
    m_allocated_size = new_allocated_size;
}

template<class T> inline void swap(CircularBuffer<T>& a, CircularBuffer<T>& b) noexcept
{
    a.swap(b);
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_CIRCULAR_BUFFER_HPP

