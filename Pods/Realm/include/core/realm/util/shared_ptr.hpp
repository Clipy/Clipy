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

#ifndef REALM_SHARED_PTR_HPP
#define REALM_SHARED_PTR_HPP

#include <cstdlib> // size_t

namespace realm {
namespace util {

template <class T>
class SharedPtr {
public:
    SharedPtr(T* p)
    {
        init(p);
    }

    SharedPtr()
    {
        init(0);
    }

    ~SharedPtr()
    {
        decref();
    }

    SharedPtr(const SharedPtr<T>& o)
        : m_ptr(o.m_ptr)
        , m_count(o.m_count)
    {
        incref();
    }

    SharedPtr<T>& operator=(const SharedPtr<T>& o)
    {
        // if (m_ptr == o.m_ptr)
        if (this == &o)
            return *this;
        decref();
        m_ptr = o.m_ptr;
        m_count = o.m_count;
        incref();
        return *this;
    }

    T* operator->() const
    {
        return m_ptr;
    }

    T& operator*() const
    {
        return *m_ptr;
    }

    T* get() const
    {
        return m_ptr;
    }

    bool operator==(const SharedPtr<T>& o) const
    {
        return m_ptr == o.m_ptr;
    }

    bool operator!=(const SharedPtr<T>& o) const
    {
        return m_ptr != o.m_ptr;
    }

    bool operator<(const SharedPtr<T>& o) const
    {
        return m_ptr < o.m_ptr;
    }

    size_t ref_count() const
    {
        return *m_count;
    }

private:
    void init(T* p)
    {
        m_ptr = p;
        try {
            m_count = new size_t(1);
        }
        catch (...) {
            delete p;
            throw;
        }
    }

    void decref()
    {
        if (--(*m_count) == 0) {
            delete m_ptr;
            delete m_count;
        }
    }

    void incref()
    {
        ++(*m_count);
    }

    T* m_ptr;
    size_t* m_count;
};
}
}

#endif
