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

#ifndef REALM_TABLE_REF_HPP
#define REALM_TABLE_REF_HPP

#include <cstddef>
#include <algorithm>

#include <realm/util/bind_ptr.hpp>

namespace realm {


class Table;


/// A reference-counting "smart pointer" for referring to table
/// accessors.
///
/// The purpose of this smart pointer is to keep the referenced table
/// accessor alive for as long as anybody is referring to it, however,
/// for stack allocated table accessors, the lifetime is necessarily
/// determined by scope (see below).
///
/// Please take note of the distinction between a "table" and a "table
/// accessor" here. A table accessor is an instance of `Table`,
/// and it may, or may not be attached to an
/// actual table at any specific point in time, but this state of
/// attachment of the accessor has nothing to do with the function of
/// the smart pointer. Also, in the rest of the documentation of this
/// class, whenever you see `Table::%foo`, you are supposed to read it
/// as, `Table::%foo`.
///
///
/// Table accessors are either created directly by an application via
/// a call to one of the public table constructors, or they are
/// created internally by the Realm library, such as when the
/// application calls Group::get_table(), Table::get_subtable(), or
/// Table::create().
///
/// Applications can safely assume that all table accessors, created
/// internally by the Realm library, have a lifetime that is managed
/// by reference counting. This means that the application can prolong
/// the lifetime of *such* table accessors indefinitely by holding on
/// to at least one smart pointer, but note that the guarantee of the
/// continued existence of the accessor, does not imply that the
/// accessor remains attached to the underlying table (see
/// Table::is_attached() for details). Accessors whose lifetime are
/// controlled by reference counting are destroyed exactly when the
/// reference count drops to zero.
///
/// When an application creates a new table accessor by a direct call
/// to one of the public constructors, the lifetime of that table
/// accessor is *not*, and cannot be managed by reference
/// counting. This is true regardless of the way the accessor is
/// created (i.e., regardless of whether it is an automatic variable
/// on the stack, or created on the heap using `new`). However, for
/// convenience, but with one important caveat, it is still possible
/// to use smart pointers to refer to such accessors. The caveat is
/// that no smart pointers are allowed to refer to the accessor at the
/// point in time when its destructor is called. It is entirely the
/// responsibility of the application to ensure that this requirement
/// is met. Failing to do so, will result in undefined
/// behavior. Finally, please note that an application is always free
/// to use Table::create() as an alternative to creating free-standing
/// top-level tables on the stack, and that this is indeed neccessary
/// when fully reference counted lifetimes are required.
///
/// So, at any time, and for any table accessor, an application can
/// call Table::get_table_ref() to obtain a smart pointer that refers
/// to that table, however, while that is always possible and safe, it
/// is not always possible to extend the lifetime of an accessor by
/// holding on to a smart pointer. The question of whether that is
/// possible, depends directly on the way the accessor was created.
///
///
/// Apart from keeping track of the number of references, these smart
/// pointers behaves almost exactly like regular pointers. In
/// particular, it is possible to dereference a TableRef and get a
/// `Table&` out of it, however, if you are not careful, this can
/// easily lead to dangling references:
///
/// \code{.cpp}
///
///   Table& sub_1 = *(table.get_subtable(0,0));
///   sub_1.add_empty_row(); // Oops, sub_1 may be dangling!
///
/// \endcode
///
/// Whether `sub_1` is actually dangling in the example above will
/// depend on whether other references to the same subtable accessor
/// already exist, but it is never wise to rely in this. Here is a
/// safe and proper alternative:
///
/// \code{.cpp}
///
///   TableRef sub_2 = table.get_subtable(0,0);
///   sub_2.add_empty_row(); // Safe!
///
///   void do_something(Table&);
///   do_something(*(table.get_subtable(0,0))); // Also safe!
///
/// \endcode
///
///
/// \sa Table
/// \sa TableRef
template <class T>
class BasicTableRef : util::bind_ptr<T> {
public:
    constexpr BasicTableRef() noexcept
    {
    }
    ~BasicTableRef() noexcept
    {
    }

    // Copy construct
    BasicTableRef(const BasicTableRef& r) noexcept
        : util::bind_ptr<T>(r)
    {
    }
    template <class U>
    BasicTableRef(const BasicTableRef<U>& r) noexcept
        : util::bind_ptr<T>(r)
    {
    }

    // Copy assign
    BasicTableRef& operator=(const BasicTableRef&) noexcept;
    template <class U>
    BasicTableRef& operator=(const BasicTableRef<U>&) noexcept;

    // Move construct
    BasicTableRef(BasicTableRef&& r) noexcept
        : util::bind_ptr<T>(std::move(r))
    {
    }
    template <class U>
    BasicTableRef(BasicTableRef<U>&& r) noexcept
        : util::bind_ptr<T>(std::move(r))
    {
    }

    // Move assign
    BasicTableRef& operator=(BasicTableRef&&) noexcept;
    template <class U>
    BasicTableRef& operator=(BasicTableRef<U>&&) noexcept;

    //@{
    /// Comparison
    template <class U>
    bool operator==(const BasicTableRef<U>&) const noexcept;

    template <class U>
    bool operator==(U*) const noexcept;

    template <class U>
    bool operator!=(const BasicTableRef<U>&) const noexcept;

    template <class U>
    bool operator!=(U*) const noexcept;

    template <class U>
    bool operator<(const BasicTableRef<U>&) const noexcept;

    template <class U>
    bool operator<(U*) const noexcept;

    template <class U>
    bool operator>(const BasicTableRef<U>&) const noexcept;

    template <class U>
    bool operator>(U*) const noexcept;

    template <class U>
    bool operator<=(const BasicTableRef<U>&) const noexcept;

    template <class U>
    bool operator<=(U*) const noexcept;

    template <class U>
    bool operator>=(const BasicTableRef<U>&) const noexcept;

    template <class U>
    bool operator>=(U*) const noexcept;
//@}

// Dereference
#ifdef __clang__
    // Clang has a bug that causes it to effectively ignore the 'using' declaration.
    T& operator*() const noexcept
    {
        return util::bind_ptr<T>::operator*();
    }
#else
    using util::bind_ptr<T>::operator*;
#endif
    using util::bind_ptr<T>::operator->;

    using util::bind_ptr<T>::operator bool;

    T* get() const noexcept
    {
        return util::bind_ptr<T>::get();
    }
    void reset() noexcept
    {
        util::bind_ptr<T>::reset();
    }
    void reset(T* t) noexcept
    {
        util::bind_ptr<T>::reset(t);
    }

    void swap(BasicTableRef& r) noexcept
    {
        this->util::bind_ptr<T>::swap(r);
    }
    friend void swap(BasicTableRef& a, BasicTableRef& b) noexcept
    {
        a.swap(b);
    }

    template <class U>
    friend BasicTableRef<U> unchecked_cast(BasicTableRef<Table>) noexcept;

    template <class U>
    friend BasicTableRef<const U> unchecked_cast(BasicTableRef<const Table>) noexcept;

private:
    template <class>
    struct GetRowAccType {
        typedef void type;
    };

    typedef typename GetRowAccType<T>::type RowAccessor;

public:
    /// Same as 'table[i]' where 'table' is the referenced table.
    RowAccessor operator[](size_t i) const noexcept
    {
        return (*this->get())[i];
    }

    explicit BasicTableRef(T* t) noexcept
        : util::bind_ptr<T>(t)
    {
    }

private:
    friend class SubtableColumnBase;
    friend class Table;
    friend class Group;

    template <class>
    friend class BasicTableRef;

    typedef typename util::bind_ptr<T>::casting_move_tag casting_move_tag;
    template <class U>
    BasicTableRef(BasicTableRef<U>* r, casting_move_tag) noexcept
        : util::bind_ptr<T>(r, casting_move_tag())
    {
    }
};


typedef BasicTableRef<Table> TableRef;
typedef BasicTableRef<const Table> ConstTableRef;


template <class C, class T, class U>
inline std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>& out, const BasicTableRef<U>& p)
{
    out << static_cast<const void*>(&*p);
    return out;
}

template <class T>
inline BasicTableRef<T> unchecked_cast(TableRef t) noexcept
{
    return BasicTableRef<T>(&t, typename BasicTableRef<T>::casting_move_tag());
}

template <class T>
inline BasicTableRef<const T> unchecked_cast(ConstTableRef t) noexcept
{
    return BasicTableRef<const T>(&t, typename BasicTableRef<T>::casting_move_tag());
}


//@{
/// Comparison
template <class T, class U>
bool operator==(T*, const BasicTableRef<U>&) noexcept;
template <class T, class U>
bool operator!=(T*, const BasicTableRef<U>&) noexcept;
template <class T, class U>
bool operator<(T*, const BasicTableRef<U>&) noexcept;
template <class T, class U>
bool operator>(T*, const BasicTableRef<U>&) noexcept;
template <class T, class U>
bool operator<=(T*, const BasicTableRef<U>&) noexcept;
template <class T, class U>
bool operator>=(T*, const BasicTableRef<U>&) noexcept;
//@}


// Implementation:

template <class T>
inline BasicTableRef<T>& BasicTableRef<T>::operator=(const BasicTableRef& r) noexcept
{
    this->util::bind_ptr<T>::operator=(r);
    return *this;
}

template <class T>
template <class U>
inline BasicTableRef<T>& BasicTableRef<T>::operator=(const BasicTableRef<U>& r) noexcept
{
    this->util::bind_ptr<T>::operator=(r);
    return *this;
}

template <class T>
inline BasicTableRef<T>& BasicTableRef<T>::operator=(BasicTableRef&& r) noexcept
{
    this->util::bind_ptr<T>::operator=(std::move(r));
    return *this;
}

template <class T>
template <class U>
inline BasicTableRef<T>& BasicTableRef<T>::operator=(BasicTableRef<U>&& r) noexcept
{
    this->util::bind_ptr<T>::operator=(std::move(r));
    return *this;
}

template <class T>
template <class U>
bool BasicTableRef<T>::operator==(const BasicTableRef<U>& p) const noexcept
{
    return get() == p.get();
}

template <class T>
template <class U>
bool BasicTableRef<T>::operator==(U* p) const noexcept
{
    return get() == p;
}

template <class T>
template <class U>
bool BasicTableRef<T>::operator!=(const BasicTableRef<U>& p) const noexcept
{
    return get() != p.get();
}

template <class T>
template <class U>
bool BasicTableRef<T>::operator!=(U* p) const noexcept
{
    return get() != p;
}

template <class T>
template <class U>
bool BasicTableRef<T>::operator<(const BasicTableRef<U>& p) const noexcept
{
    return get() < p.get();
}

template <class T>
template <class U>
bool BasicTableRef<T>::operator<(U* p) const noexcept
{
    return get() < p;
}

template <class T>
template <class U>
bool BasicTableRef<T>::operator>(const BasicTableRef<U>& p) const noexcept
{
    return get() > p.get();
}

template <class T>
template <class U>
bool BasicTableRef<T>::operator>(U* p) const noexcept
{
    return get() > p;
}

template <class T>
template <class U>
bool BasicTableRef<T>::operator<=(const BasicTableRef<U>& p) const noexcept
{
    return get() <= p.get();
}

template <class T>
template <class U>
bool BasicTableRef<T>::operator<=(U* p) const noexcept
{
    return get() <= p;
}

template <class T>
template <class U>
bool BasicTableRef<T>::operator>=(const BasicTableRef<U>& p) const noexcept
{
    return get() >= p.get();
}

template <class T>
template <class U>
bool BasicTableRef<T>::operator>=(U* p) const noexcept
{
    return get() >= p;
}

template <class T, class U>
bool operator==(T* a, const BasicTableRef<U>& b) noexcept
{
    return b == a;
}

template <class T, class U>
bool operator!=(T* a, const BasicTableRef<U>& b) noexcept
{
    return b != a;
}

template <class T, class U>
bool operator<(T* a, const BasicTableRef<U>& b) noexcept
{
    return b > a;
}

template <class T, class U>
bool operator>(T* a, const BasicTableRef<U>& b) noexcept
{
    return b < a;
}

template <class T, class U>
bool operator<=(T* a, const BasicTableRef<U>& b) noexcept
{
    return b >= a;
}

template <class T, class U>
bool operator>=(T* a, const BasicTableRef<U>& b) noexcept
{
    return b <= a;
}


} // namespace realm

#endif // REALM_TABLE_REF_HPP
