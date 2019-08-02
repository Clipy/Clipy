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

#ifndef REALM_UTIL_ALLOCATOR_HPP
#define REALM_UTIL_ALLOCATOR_HPP

#include <cstdlib>
#include <memory>
#include <realm/util/backtrace.hpp>

namespace realm {
namespace util {

/// Dynamic heap allocation interface.
///
/// Implementors may optionally implement a static method `get_default()`, which
/// should return a reference to an allocator instance. This allows
/// `STLAllocator` to be default-constructed.
///
/// NOTE: This base class is not related to the `realm::Allocator` interface,
/// which is used in the context of allocating memory inside a Realm file.
struct AllocatorBase {
    static constexpr std::size_t max_alignment = 16; // FIXME: This is arch-dependent

    /// Allocate \a size bytes at aligned at \a align.
    ///
    /// May throw `std::bad_alloc` if allocation fails. May **NOT** return
    /// an invalid pointer (such as `nullptr`).
    virtual void* allocate(std::size_t size, std::size_t align) = 0;

    /// Free the previously allocated block of memory. \a size is not required
    /// to be accurate, and is only provided for statistics and debugging
    /// purposes.
    ///
    /// \a ptr may be `nullptr`, in which case this shall be a noop.
    virtual void free(void* ptr, size_t size) noexcept = 0;
};

/// Implementation of AllocatorBase that uses `operator new`/`operator delete`.
///
/// Using this allocator with standard containers is zero-overhead: No
/// additional storage is required at any level.
struct DefaultAllocator final : AllocatorBase {
    /// Return a reference to a global singleton.
    ///
    /// This method is thread-safe.
    static DefaultAllocator& get_default() noexcept;

    /// Allocate memory (using `operator new`).
    ///
    /// \a align must not exceed `max_alignment` before C++17.
    ///
    /// This method is thread-safe.
    void* allocate(std::size_t size, std::size_t align) final;

    /// Free memory (using `operator delete`).
    ///
    /// If \a ptr equals `nullptr`, this is a no-op.
    ///
    /// This method is thread-safe.
    void free(void* ptr, std::size_t size) noexcept final;

private:
    static DefaultAllocator g_instance;
    DefaultAllocator()
    {
    }
};

template <class T, class Allocator = AllocatorBase>
struct STLDeleter;

namespace detail {
/// Base class for things that hold a reference to an allocator. The default
/// implementation carries a pointer to the allocator instance. Singleton
/// allocators (such as `DefaultAllocator`) may specialize this class such that
/// no extra storage is needed.
template <class Allocator>
struct GetAllocator {
    // Note: Some allocators may not define get_default(). This is OK, and
    // this constructor will not be instantiated (SFINAE).
    GetAllocator() noexcept
        : m_allocator(&Allocator::get_default())
    {
    }

    template <class A>
    GetAllocator(A& allocator) noexcept
        : m_allocator(&allocator)
    {
    }

    template <class A>
    GetAllocator& operator=(const GetAllocator<A>& other) noexcept
    {
        m_allocator = &other.get_allocator();
        return *this;
    }

    Allocator& get_allocator() const noexcept
    {
        return *m_allocator;
    }

    bool operator==(const GetAllocator& other) const noexcept
    {
        return m_allocator == other.m_allocator;
    }

    bool operator!=(const GetAllocator& other) const noexcept
    {
        return m_allocator != other.m_allocator;
    }

    Allocator* m_allocator;
};

/// Specialization for `DefaultAllocator` that has zero size, i.e. no extra
/// storage requirements compared with `std::allocator<T>`.
template <>
struct GetAllocator<DefaultAllocator> {
    GetAllocator() noexcept
    {
    }

    GetAllocator(DefaultAllocator&) noexcept
    {
    }

    DefaultAllocator& get_allocator() const noexcept
    {
        return DefaultAllocator::get_default();
    }

    bool operator==(const GetAllocator&) const noexcept
    {
        return true;
    }

    bool operator!=(const GetAllocator&) const noexcept
    {
        return false;
    }
};
} // namespace detail

/// STL-compatible static dispatch bridge to a dynamic implementation of
/// `AllocatorBase`. Wraps a pointer to an object that adheres to the
/// `AllocatorBase` interface. It is optional whether the `Allocator` class
/// template argument actually derives from `AllocatorBase`.
///
/// The intention is that users of this class can set `Allocator` to the
/// nearest-known base class of the expected allocator implementations, such
/// that appropriate devirtualization can take place.
template <class T, class Allocator = AllocatorBase>
struct STLAllocator : detail::GetAllocator<Allocator> {
    using value_type = T;
    using Deleter = STLDeleter<T, Allocator>;

    // These typedefs are optional, but GCC 4.9 requires them when using the
    // allocator together with std::map, std::basic_string, etc.
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;

    /// The default constructor is only availble when the static method
    /// `Allocator::get_default()` exists.
    STLAllocator() noexcept
    {
    }

    constexpr STLAllocator(Allocator& base) noexcept
        : detail::GetAllocator<Allocator>(base)
    {
    }
    template <class U, class A>
    constexpr STLAllocator(const STLAllocator<U, A>& other) noexcept
        : detail::GetAllocator<Allocator>(other.get_allocator())
    {
    }

    STLAllocator& operator=(const STLAllocator& other) noexcept = default;

    T* allocate(std::size_t n)
    {
        static_assert(alignof(T) <= Allocator::max_alignment, "Over-aligned allocation");
        void* ptr = this->get_allocator().allocate(sizeof(T) * n, alignof(T));
        return static_cast<T*>(ptr);
    }

    void deallocate(T* ptr, std::size_t n) noexcept
    {
        this->get_allocator().free(ptr, sizeof(T) * n);
    }

    operator Allocator&() const
    {
        return this->get_allocator();
    }

    template <class U>
    struct rebind {
        using other = STLAllocator<U, Allocator>;
    };

    // construct() and destroy() are optional, but are required by some
    // containers under GCC 4.9 (verified for at least std::list).
    template <class... Args>
    void construct(T* ptr, Args&&... args)
    {
        ::new (ptr) T(std::forward<Args>(args)...);
    }

    template <class U>
    void destroy(U* ptr)
    {
        ptr->~U();
    }

private:
    template <class U, class A>
    friend struct STLAllocator;
};

template <class T, class Allocator>
struct STLDeleter : detail::GetAllocator<Allocator> {
    // The reason for this member is to accurately pass `size` to `free()` when
    // deallocating. `sizeof(T)` may not be good enough, because the pointer may
    // have been cast to a relative type of different size.
    size_t m_size;

    explicit STLDeleter(Allocator& allocator) noexcept
        : STLDeleter(0, allocator)
    {
    }
    explicit STLDeleter(size_t size, Allocator& allocator) noexcept
        : detail::GetAllocator<Allocator>(allocator)
        , m_size(size)
    {
    }

    template <class U, class A>
    STLDeleter(const STLDeleter<U, A>& other) noexcept
        : detail::GetAllocator<Allocator>(other.get_allocator())
        , m_size(other.m_size)
    {
    }

    void operator()(T* ptr)
    {
        ptr->~T();
        this->get_allocator().free(ptr, m_size);
    }
};

template <class T, class Allocator>
struct STLDeleter<T[], Allocator> : detail::GetAllocator<Allocator> {
    // Note: Array-allocated pointers cannot be upcast to base classes, because
    // of array slicing.
    size_t m_count;
    explicit STLDeleter(Allocator& allocator) noexcept
        : STLDeleter(0, allocator)
    {
    }
    explicit STLDeleter(size_t count, Allocator& allocator) noexcept
        : detail::GetAllocator<Allocator>(allocator)
        , m_count(count)
    {
    }

    template <class A>
    STLDeleter(const STLDeleter<T[], A>& other) noexcept
        : detail::GetAllocator<Allocator>(other.get_allocator())
        , m_count(other.m_count)
    {
    }

    template <class A>
    STLDeleter& operator=(const STLDeleter<T[], A>& other) noexcept
    {
        static_cast<detail::GetAllocator<Allocator>&>(*this) =
            static_cast<const detail::GetAllocator<A>&>(other);
        m_count = other.m_count;
        return *this;
    }

    void operator()(T* ptr)
    {
        for (size_t i = 0; i < m_count; ++i) {
            ptr[i].~T();
        }
        this->get_allocator().free(ptr, m_count * sizeof(T));
    }
};

/// make_unique with custom allocator (non-array version)
template <class T, class Allocator = DefaultAllocator, class... Args>
auto make_unique(Allocator& allocator, Args&&... args)
    -> std::enable_if_t<!std::is_array<T>::value, std::unique_ptr<T, STLDeleter<T, Allocator>>>
{
    void* memory = allocator.allocate(sizeof(T), alignof(T)); // Throws
    T* ptr;
    try {
        ptr = new (memory) T(std::forward<Args>(args)...); // Throws
    }
    catch (...) {
        allocator.free(memory, sizeof(T));
        throw;
    }
    std::unique_ptr<T, STLDeleter<T, Allocator>> result{ptr, STLDeleter<T, Allocator>{sizeof(T), allocator}};
    return result;
}

/// make_unique with custom allocator supporting `get_default()`
/// (non-array-version)
template <class T, class Allocator = DefaultAllocator, class... Args>
auto make_unique(Args&&... args)
    -> std::enable_if_t<!std::is_array<T>::value, std::unique_ptr<T, STLDeleter<T, Allocator>>>
{
    return make_unique<T, Allocator>(Allocator::get_default(), std::forward<Args>(args)...);
}

/// make_unique with custom allocator (array version)
template <class Tv, class Allocator>
auto make_unique(Allocator& allocator, size_t count)
    -> std::enable_if_t<std::is_array<Tv>::value, std::unique_ptr<Tv, STLDeleter<Tv, Allocator>>>
{
    using T = std::remove_extent_t<Tv>;
    void* memory = allocator.allocate(sizeof(T) * count, alignof(T)); // Throws
    T* ptr = reinterpret_cast<T*>(memory);
    size_t constructed = 0;
    try {
        // FIXME: Can't use array placement new, because MSVC has a buggy
        // implementation of it. :-(
        while (constructed < count) {
            new (&ptr[constructed]) T; // Throws
            ++constructed;
        }
    }
    catch (...) {
        for (size_t i = 0; i < constructed; ++i) {
            ptr[i].~T();
        }
        allocator.free(memory, sizeof(T) * count);
        throw;
    }
    std::unique_ptr<T[], STLDeleter<T[], Allocator>> result{ptr, STLDeleter<T[], Allocator>{count, allocator}};
    return result;
}

/// make_unique with custom allocator supporting `get_default()` (array version)
template <class Tv, class Allocator = DefaultAllocator>
auto make_unique(size_t count)
    -> std::enable_if_t<std::is_array<Tv>::value, std::unique_ptr<Tv, STLDeleter<Tv, Allocator>>>
{
    return make_unique<Tv, Allocator>(Allocator::get_default(), count);
}


// Implementation:

inline DefaultAllocator& DefaultAllocator::get_default() noexcept
{
    return g_instance;
}

inline void* DefaultAllocator::allocate(std::size_t size, std::size_t)
{
    return new char[size];
}

inline void DefaultAllocator::free(void* ptr, std::size_t) noexcept
{
    delete[] static_cast<char*>(ptr);
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_ALLOCATOR_HPP
