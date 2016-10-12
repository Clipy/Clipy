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


#pragma once
#ifndef REALM_UTIL_PRIORITY_QUEUE_HPP
#define REALM_UTIL_PRIORITY_QUEUE_HPP

#include <vector>
#include <functional>
#include <algorithm>

namespace realm {
namespace util {


/// PriorityQueue corresponds exactly to `std::priority_queue`, but has the extra feature
/// of allowing iteration and erasure of elements in the queue.
///
/// PriorityQueue only allows const access to its elements, because non-const access
/// would open up the risk of changing the ordering of the elements.
///
/// Note: As opposed to `std::priority_queue`, this does not store elements in a heap
/// internally. Instead, elements are stored in sorted order. Users of this class are
/// allowed to operate on this assumption.
template<class T, class Container = std::vector<T>,
    class Compare = std::less<typename Container::value_type>>
class PriorityQueue : private Compare {
public:
    using container_type  = Container;
    using value_type      = typename Container::value_type;
    using size_type       = typename Container::size_type;
    using reference       = typename Container::reference;
    using const_reference = typename Container::const_reference;
    using const_reverse_iterator = typename Container::const_reverse_iterator;
    using const_iterator         = typename Container::const_iterator;

    //{@
    /// Construct a PriorityQueue, optionally providing a comparator object.
    PriorityQueue(const Compare& compare, const Container& cont);

    explicit PriorityQueue(const Compare& compare = Compare{}, Container&& cont = Container{});

    template<class InputIt>
    PriorityQueue(InputIt first, InputIt last, const Compare& compare, const Container& cont);

    template<class InputIt>
    PriorityQueue(InputIt first, InputIt last, const Compare& compare = Compare{},
                  Container&& cont = Container{});
    //@}
    // Skipping Allocator-specific template constructors.

    PriorityQueue(const PriorityQueue&) = default;
    PriorityQueue(PriorityQueue&&) = default;
    PriorityQueue& operator=(const PriorityQueue&) = default;
    PriorityQueue& operator=(PriorityQueue&&) = default;

    bool empty() const;
    size_type size() const;

    //{@
    /// Push an element to the priority queue.
    ///
    /// If insertion to the underlying `Container` invalidates
    /// iterators and references, any iterators and references into this
    /// priority queue are also invalidated. By default, this is the case.
    void push(const T& value);
    void push(T&& value);
    //@}

    /// Pop the largest element from the priority queue.
    ///
    /// If `pop_back` on the underlying `Container` invalidates
    /// iterators and references, any iterators and reference into this
    /// priority queue are also invalidated. By default, this is *NOT* the case.
    ///
    /// Calling `pop()` on an empty priority queue is undefined.
    void pop();

    /// Return a reference to the largest element of the priority queue.
    ///
    /// Calling `top()` on an empty priority queue is undefined.
    const_reference top() const;

    /// Pop the top of the queue and return it by moving it out of the queue.
    ///
    /// Note: This method does not exist in `std::priority_queue`.
    ///
    /// Calling `pop_top()` on an empty priorty queue is undefined.
    value_type pop_top();

    // FIXME: emplace() deliberately omitted for simplicity.

    /// Swap the contents of this priority queue with the contents of \a other.
    void swap(PriorityQueue& other);

    // Not in std::priority_queue:

    /// Return an iterator to the beginning of the queue (smallest element first).
    const_iterator begin() const;

    /// Return an iterator to the end of the queue (largest element last);
    const_iterator end() const;

    /// Return a reverse iterator into the priority queue (largest element first).
    const_reverse_iterator rbegin() const;

    /// Return a reverse iterator representing the end of the priority queue (smallest element last).
    const_reverse_iterator rend() const;

    /// Erase element pointed to by \a it.
    ///
    /// Note: This function differs from `std::priority_queue` by returning the erased
    /// element using move semantics.
    ///
    /// Calling `erase()` with a beyond-end iterator (such as what is returned by `end()`)
    /// is undefined.
    value_type erase(const_iterator it);

    /// Remove all elements from the priority queue.
    void clear();

    /// Calls `reserve()` on the underlying `Container`.
    void reserve(size_type);

private:
    Container m_queue;

    const Compare& compare() const;
    Compare& compare();
};


/// Implementation

template<class T, class Container, class Compare>
PriorityQueue<T, Container, Compare>::PriorityQueue(const Compare& compare, const Container& cont):
    Compare(compare), m_queue(cont)
{
}

template<class T, class Container, class Compare>
PriorityQueue<T, Container, Compare>::PriorityQueue(const Compare& compare, Container&& cont):
    Compare(compare), m_queue(std::move(cont))
{
}

template<class T, class Container, class Compare>
template<class InputIt>
PriorityQueue<T, Container, Compare>::PriorityQueue(InputIt first, InputIt last,
                                                    const Compare& compare, const Container& cont):
    Compare(compare), m_queue(cont)
{
    for (auto it = first; it != last; ++it) {
        push(*it);
    }
}

template<class T, class Container, class Compare>
template<class InputIt>
PriorityQueue<T, Container, Compare>::PriorityQueue(InputIt first, InputIt last,
                                                    const Compare& compare, Container&& cont):
    Compare(compare), m_queue(std::move(cont))
{
    for (auto it = first; it != last; ++it) {
        push(*it);
    }
}

template<class T, class Container, class Compare>
typename PriorityQueue<T, Container, Compare>::size_type
PriorityQueue<T, Container, Compare>::size() const
{
    return m_queue.size();
}

template<class T, class Container, class Compare>
bool PriorityQueue<T, Container, Compare>::empty() const
{
    return m_queue.empty();
}

template<class T, class Container, class Compare>
void PriorityQueue<T, Container, Compare>::push(const T& element)
{
    auto it = std::lower_bound(m_queue.begin(), m_queue.end(), element, compare());
    m_queue.insert(it, element);
}

template<class T, class Container, class Compare>
void PriorityQueue<T, Container, Compare>::push(T&& element)
{
    auto it = std::lower_bound(m_queue.begin(), m_queue.end(), element, compare());
    m_queue.insert(it, std::move(element));
}

template<class T, class Container, class Compare>
void PriorityQueue<T, Container, Compare>::pop()
{
    m_queue.pop_back();
}

template<class T, class Container, class Compare>
typename PriorityQueue<T, Container, Compare>::const_reference
PriorityQueue<T, Container, Compare>::top() const
{
    return m_queue.back();
}

template<class T, class Container, class Compare>
typename PriorityQueue<T, Container, Compare>::value_type
PriorityQueue<T, Container, Compare>::pop_top()
{
    value_type value = std::move(m_queue.back());
    m_queue.pop_back();
    return value;
}

template<class T, class Container, class Compare>
Compare& PriorityQueue<T, Container, Compare>::compare()
{
    return *this;
}

template<class T, class Container, class Compare>
const Compare& PriorityQueue<T, Container, Compare>::compare() const
{
    return *this;
}

template<class T, class Container, class Compare>
typename PriorityQueue<T, Container, Compare>::const_iterator
PriorityQueue<T, Container, Compare>::begin() const
{
    return m_queue.begin();
}

template<class T, class Container, class Compare>
typename PriorityQueue<T, Container, Compare>::const_iterator
PriorityQueue<T, Container, Compare>::end() const
{
    return m_queue.end();
}

template<class T, class Container, class Compare>
typename PriorityQueue<T, Container, Compare>::const_reverse_iterator
PriorityQueue<T, Container, Compare>::rbegin() const
{
    return m_queue.rbegin();
}

template<class T, class Container, class Compare>
typename PriorityQueue<T, Container, Compare>::const_reverse_iterator
PriorityQueue<T, Container, Compare>::rend() const
{
    return m_queue.rend();
}

template<class T, class Container, class Compare>
typename PriorityQueue<T, Container, Compare>::value_type
PriorityQueue<T, Container, Compare>::erase(const_iterator it)
{
    // Convert to non-const iterator:
    auto non_const_iterator = m_queue.begin() + (it - m_queue.begin());
    value_type value = std::move(*non_const_iterator);
    m_queue.erase(non_const_iterator);
    return value;
}

template<class T, class Container, class Compare>
void PriorityQueue<T, Container, Compare>::clear()
{
    m_queue.clear();
}

template<class T, class Container, class Compare>
void PriorityQueue<T, Container, Compare>::reserve(size_type sz)
{
    m_queue.reserve(sz);
}

template<class T, class Container, class Compare>
void PriorityQueue<T, Container, Compare>::swap(PriorityQueue& other)
{
    using std::swap;
    swap(m_queue, other.m_queue);
    swap(compare(), other.compare());
}


}
}

#endif // REALM_UTIL_PRIORITY_QUEUE_HPP

