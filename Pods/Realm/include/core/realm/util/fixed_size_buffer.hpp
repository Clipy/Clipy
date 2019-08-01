/*************************************************************************
 *
 * Copyright 2019 Realm Inc.
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

#ifndef REALM_UTIL_FIXED_SIZE_BUFFER_HPP
#define REALM_UTIL_FIXED_SIZE_BUFFER_HPP

#include <vector>
#include <cstddef>
#include <stdexcept>

namespace realm {
namespace util {

/// This is a buffer with a fixed size. You can only insert elements.
/// When the number of elements inserted matches the size of the buffer,
/// additional insertions will overwrite the oldest elements.
template <class T>
class FixedSizeBuffer {
public:
    class iterator;

    FixedSizeBuffer(size_t sz)
        : m_size(sz)
    {
        if (sz == 0)
            throw std::runtime_error("FixedSizeBuffer size cannot be 0");
        m_buffer.reserve(sz);
    }
    size_t size()
    {
        return m_buffer.size();
    }
    void insert(const T& val)
    {
        if (m_buffer.size() < m_size) {
            m_buffer.emplace_back(val);
        }
        else {
            m_buffer[m_oldest] = val;
            ++m_oldest;
            if (m_oldest == m_size)
                m_oldest = 0;
        }
    }
    T& at(size_t n)
    {
        auto idx = (n + m_oldest) % m_size;
        return m_buffer[idx];
    }
    T& operator[](size_t n)
    {
        return at(n);
    }
    iterator begin()
    {
        return iterator(*this, 0);
    }
    iterator end()
    {
        return iterator(*this, m_buffer.size());
    }

private:
    std::vector<T> m_buffer;
    size_t m_size;
    size_t m_oldest = 0;
};

template <class T>
class FixedSizeBuffer<T>::iterator {
public:
    typedef std::forward_iterator_tag iterator_category;
    typedef T value_type;
    typedef ptrdiff_t difference_type;
    typedef T* pointer;
    typedef T& reference;

    iterator(FixedSizeBuffer<T>& b, size_t ndx)
        : m_cb(b)
        , m_ndx(ndx)
    {
    }
    pointer operator->()
    {
        return &m_cb[m_ndx];
    }
    reference operator*()
    {
        return m_cb[m_ndx];
    }
    iterator& operator++()
    {
        ++m_ndx;
        return *this;
    }
    iterator operator++(int)
    {
        iterator tmp(*this);
        operator++();
        return tmp;
    }
    bool operator!=(const iterator& rhs)
    {
        return m_ndx != rhs.m_ndx;
    }
    bool operator==(const iterator& rhs)
    {
        return m_ndx == rhs.m_ndx;
    }

private:
    FixedSizeBuffer<T>& m_cb;
    size_t m_ndx;
};

} // namespace util
} // namespace realm


#endif /* REALM_UTIL_FIXED_SIZE_BUFFER_HPP */
