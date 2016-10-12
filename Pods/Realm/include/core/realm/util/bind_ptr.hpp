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
#ifndef REALM_UTIL_BIND_PTR_HPP
#define REALM_UTIL_BIND_PTR_HPP

#include <algorithm>
#include <atomic>
#include <ostream>
#include <utility>

#include <realm/util/features.h>
#include <realm/util/assert.hpp>


namespace realm {
namespace util {

class bind_ptr_base {
public:
    struct adopt_tag {};
};


/// A generic intrusive smart pointer that binds itself explicitely to
/// the target object.
///
/// This class is agnostic towards what 'binding' means for the target
/// object, but a common use is 'reference counting'. See RefCountBase
/// for an example of that.
///
/// This smart pointer implementation assumes that the target object
/// destructor never throws.
template<class T> class bind_ptr: public bind_ptr_base {
public:
    constexpr bind_ptr() noexcept: m_ptr(nullptr) {}
    ~bind_ptr() noexcept { unbind(); }

    explicit bind_ptr(T* p) noexcept { bind(p); }
    template<class U> explicit bind_ptr(U* p) noexcept { bind(p); }

    bind_ptr(T* p, adopt_tag) noexcept { m_ptr = p; }
    template<class U> bind_ptr(U* p, adopt_tag) noexcept { m_ptr = p; }

    // Copy construct
    bind_ptr(const bind_ptr& p) noexcept { bind(p.m_ptr); }
    template<class U>
    bind_ptr(const bind_ptr<U>& p) noexcept { bind(p.m_ptr); }

    // Copy assign
    bind_ptr& operator=(const bind_ptr& p) noexcept { bind_ptr(p).swap(*this); return *this; }
    template<class U>
    bind_ptr& operator=(const bind_ptr<U>& p) noexcept { bind_ptr(p).swap(*this); return *this; }

    // Move construct
    bind_ptr(bind_ptr&& p) noexcept: m_ptr(p.release()) {}
    template<class U>
    bind_ptr(bind_ptr<U>&& p) noexcept: m_ptr(p.release()) {}

    // Move assign
    bind_ptr& operator=(bind_ptr&& p) noexcept { bind_ptr(std::move(p)).swap(*this); return *this; }
    template<class U>
    bind_ptr& operator=(bind_ptr<U>&& p) noexcept { bind_ptr(std::move(p)).swap(*this); return *this; }

    //@{
    // Comparison
    template<class U>
    bool operator==(const bind_ptr<U>&) const noexcept;

    template<class U>
    bool operator==(U*) const noexcept;

    template<class U>
    bool operator!=(const bind_ptr<U>&) const noexcept;

    template<class U>
    bool operator!=(U*) const noexcept;

    template<class U>
    bool operator<(const bind_ptr<U>&) const noexcept;

    template<class U>
    bool operator<(U*) const noexcept;

    template<class U>
    bool operator>(const bind_ptr<U>&) const noexcept;

    template<class U>
    bool operator>(U*) const noexcept;

    template<class U>
    bool operator<=(const bind_ptr<U>&) const noexcept;

    template<class U>
    bool operator<=(U*) const noexcept;

    template<class U>
    bool operator>=(const bind_ptr<U>&) const noexcept;

    template<class U>
    bool operator>=(U*) const noexcept;
    //@}

    // Dereference
    T& operator*() const noexcept { return *m_ptr; }
    T* operator->() const noexcept { return m_ptr; }

    explicit operator bool() const noexcept { return m_ptr != 0; }

    T* get() const noexcept { return m_ptr; }
    void reset() noexcept { bind_ptr().swap(*this); }
    void reset(T* p) noexcept { bind_ptr(p).swap(*this); }
    template<class U>
    void reset(U* p) noexcept { bind_ptr(p).swap(*this); }

    T* release() noexcept { T* const p = m_ptr; m_ptr = nullptr; return p; }

    void swap(bind_ptr& p) noexcept { std::swap(m_ptr, p.m_ptr); }
    friend void swap(bind_ptr& a, bind_ptr& b) noexcept { a.swap(b); }

protected:
    struct casting_move_tag {};
    template<class U>
    bind_ptr(bind_ptr<U>* p, casting_move_tag) noexcept:
        m_ptr(static_cast<T*>(p->release())) {}

private:
    T* m_ptr;

    void bind(T* p) noexcept { if (p) p->bind_ptr(); m_ptr = p; }
    void unbind() noexcept { if (m_ptr) m_ptr->unbind_ptr(); }

    template<class> friend class bind_ptr;
};


template<class C, class T, class U>
inline std::basic_ostream<C,T>& operator<<(std::basic_ostream<C,T>& out, const bind_ptr<U>& p)
{
    out << static_cast<const void*>(p.get());
    return out;
}


//@{
// Comparison
template<class T, class U>
bool operator==(T*, const bind_ptr<U>&) noexcept;
template<class T, class U>
bool operator!=(T*, const bind_ptr<U>&) noexcept;
template<class T, class U>
bool operator<(T*, const bind_ptr<U>&) noexcept;
template<class T, class U>
bool operator>(T*, const bind_ptr<U>&) noexcept;
template<class T, class U>
bool operator<=(T*, const bind_ptr<U>&) noexcept;
template<class T, class U>
bool operator>=(T*, const bind_ptr<U>&) noexcept;
//@}



/// Polymorphic convenience base class for reference counting objects.
///
/// Together with bind_ptr, this class delivers simple instrusive
/// reference counting.
///
/// \sa bind_ptr
class RefCountBase {
public:
    RefCountBase() noexcept: m_ref_count(0) {}
    virtual ~RefCountBase() noexcept { REALM_ASSERT(m_ref_count == 0); }

    RefCountBase(const RefCountBase&) = delete;
    RefCountBase(RefCountBase&&) = delete;

    void operator=(const RefCountBase&) = delete;
    void operator=(RefCountBase&&) = delete;

protected:
    void bind_ptr() const noexcept { ++m_ref_count; }
    void unbind_ptr() const noexcept { if (--m_ref_count == 0) delete this; }

private:
    mutable unsigned long m_ref_count;

    template<class> friend class bind_ptr;
};


/// Same as RefCountBase, but this one makes the copying of, and the
/// destruction of counted references thread-safe.
///
/// \sa RefCountBase
/// \sa bind_ptr
class AtomicRefCountBase {
public:
    AtomicRefCountBase() noexcept: m_ref_count(0) {}
    virtual ~AtomicRefCountBase() noexcept { REALM_ASSERT(m_ref_count == 0); }

    AtomicRefCountBase(const AtomicRefCountBase&) = delete;
    AtomicRefCountBase(AtomicRefCountBase&&) = delete;

    void operator=(const AtomicRefCountBase&) = delete;
    void operator=(AtomicRefCountBase&&) = delete;

protected:
    // FIXME: Operators ++ and -- as used below use
    // std::memory_order_seq_cst. I'm not sure whether this is the
    // choice that leads to maximum efficiency, but at least it is
    // safe.
    void bind_ptr() const noexcept { ++m_ref_count; }
    void unbind_ptr() const noexcept { if (--m_ref_count == 0) delete this; }

private:
    mutable std::atomic<unsigned long> m_ref_count;

    template<class> friend class bind_ptr;
};





// Implementation:

template<class T>
template<class U>
bool bind_ptr<T>::operator==(const bind_ptr<U>& p) const noexcept
{
    return m_ptr == p.m_ptr;
}

template<class T>
template<class U>
bool bind_ptr<T>::operator==(U* p) const noexcept
{
    return m_ptr == p;
}

template<class T>
template<class U>
bool bind_ptr<T>::operator!=(const bind_ptr<U>& p) const noexcept
{
    return m_ptr != p.m_ptr;
}

template<class T>
template<class U>
bool bind_ptr<T>::operator!=(U* p) const noexcept
{
    return m_ptr != p;
}

template<class T>
template<class U>
bool bind_ptr<T>::operator<(const bind_ptr<U>& p) const noexcept
{
    return m_ptr < p.m_ptr;
}

template<class T>
template<class U>
bool bind_ptr<T>::operator<(U* p) const noexcept
{
    return m_ptr < p;
}

template<class T>
template<class U>
bool bind_ptr<T>::operator>(const bind_ptr<U>& p) const noexcept
{
    return m_ptr > p.m_ptr;
}

template<class T>
template<class U>
bool bind_ptr<T>::operator>(U* p) const noexcept
{
    return m_ptr > p;
}

template<class T>
template<class U>
bool bind_ptr<T>::operator<=(const bind_ptr<U>& p) const noexcept
{
    return m_ptr <= p.m_ptr;
}

template<class T>
template<class U>
bool bind_ptr<T>::operator<=(U* p) const noexcept
{
    return m_ptr <= p;
}

template<class T>
template<class U>
bool bind_ptr<T>::operator>=(const bind_ptr<U>& p) const noexcept
{
    return m_ptr >= p.m_ptr;
}

template<class T>
template<class U>
bool bind_ptr<T>::operator>=(U* p) const noexcept
{
    return m_ptr >= p;
}

template<class T, class U>
bool operator==(T* a, const bind_ptr<U>& b) noexcept
{
    return b == a;
}

template<class T, class U>
bool operator!=(T* a, const bind_ptr<U>& b) noexcept
{
    return b != a;
}

template<class T, class U>
bool operator<(T* a, const bind_ptr<U>& b) noexcept
{
    return b > a;
}

template<class T, class U>
bool operator>(T* a, const bind_ptr<U>& b) noexcept
{
    return b < a;
}

template<class T, class U>
bool operator<=(T* a, const bind_ptr<U>& b) noexcept
{
    return b >= a;
}

template<class T, class U>
bool operator>=(T* a, const bind_ptr<U>& b) noexcept
{
    return b <= a;
}


} // namespace util
} // namespace realm

#endif // REALM_UTIL_BIND_PTR_HPP
