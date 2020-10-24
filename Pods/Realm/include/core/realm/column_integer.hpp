/*************************************************************************
 *
 * Copyright 2018 Realm Inc.
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

#ifndef REALM_COLUMN_INTEGER_HPP
#define REALM_COLUMN_INTEGER_HPP

#include <realm/bplustree.hpp>
#include <realm/array_integer.hpp>
#include <realm/array_key.hpp>

namespace realm {

class IntegerColumn;

class IntegerColumnIterator {
public:
    typedef std::random_access_iterator_tag iterator_category;
    typedef int64_t value_type;
    typedef ptrdiff_t difference_type;
    typedef const value_type* pointer;
    typedef const value_type& reference;

    IntegerColumnIterator(const IntegerColumn* tree, size_t pos)
        : m_tree(tree)
        , m_pos(pos)
    {
    }

    size_t get_position() const
    {
        return m_pos;
    }

    int64_t operator->() const;
    int64_t operator*() const;
    int64_t operator[](size_t ndx) const;

    IntegerColumnIterator& operator++()
    {
        m_pos++;
        return *this;
    }

    IntegerColumnIterator operator++(int)
    {
        IntegerColumnIterator tmp(*this);
        operator++();
        return tmp;
    }

    IntegerColumnIterator& operator--()
    {
        m_pos--;
        return *this;
    }

    IntegerColumnIterator& operator+=(ptrdiff_t adj)
    {
        m_pos += adj;
        return *this;
    }

    IntegerColumnIterator& operator-=(ptrdiff_t adj)
    {
        m_pos -= adj;
        return *this;
    }

    IntegerColumnIterator operator+(ptrdiff_t adj)
    {
        return {m_tree, m_pos + adj};
    }

    IntegerColumnIterator operator-(ptrdiff_t adj)
    {
        return {m_tree, m_pos - adj};
    }

    IntegerColumnIterator operator--(int)
    {
        IntegerColumnIterator tmp(*this);
        operator--();
        return tmp;
    }

    ptrdiff_t operator-(const IntegerColumnIterator& rhs)
    {
        return m_pos - rhs.m_pos;
    }

    bool operator!=(const IntegerColumnIterator& rhs) const
    {
        return m_pos != rhs.m_pos;
    }

    bool operator==(const IntegerColumnIterator& rhs) const
    {
        return m_pos == rhs.m_pos;
    }

    bool operator>(const IntegerColumnIterator& rhs) const
    {
        return m_pos > rhs.m_pos;
    }

    bool operator<(const IntegerColumnIterator& rhs) const
    {
        return m_pos < rhs.m_pos;
    }

    bool operator>=(const IntegerColumnIterator& rhs) const
    {
        return m_pos >= rhs.m_pos;
    }

    bool operator<=(const IntegerColumnIterator& rhs) const
    {
        return m_pos <= rhs.m_pos;
    }

private:
    const IntegerColumn* m_tree;
    size_t m_pos;
};

class IntegerColumn : public BPlusTree<int64_t> {
public:
    using const_iterator = IntegerColumnIterator;

    IntegerColumn(Allocator& alloc, ref_type ref = 0)
        : BPlusTree(alloc)
    {
        if (ref != 0)
            init_from_ref(ref);
    }

    int64_t back()
    {
        return get(size() - 1);
    }
    IntegerColumnIterator cbegin() const
    {
        return IntegerColumnIterator(this, 0);
    }
    IntegerColumnIterator cend() const
    {
        return IntegerColumnIterator(this, size());
    }
};

inline int64_t IntegerColumnIterator::operator->() const
{
    return m_tree->get(m_pos);
}

inline int64_t IntegerColumnIterator::operator*() const
{
    return m_tree->get(m_pos);
}

inline int64_t IntegerColumnIterator::operator[](size_t ndx) const
{
    return m_tree->get(m_pos + ndx);
}

inline std::ostream& operator<<(std::ostream& out, const IntegerColumnIterator& it)
{
    out << "IntegerColumnIterator at index: " << it.get_position();
    return out;
}
}

#endif /* REALM_COLUMN_INTEGER_HPP */
