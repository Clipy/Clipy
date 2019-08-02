/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2018] Realm Inc
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
#ifndef REALM_UTIL_ALLOCATION_METRICS_HPP
#define REALM_UTIL_ALLOCATION_METRICS_HPP

#include <atomic>
#include <vector>

#include <realm/util/allocator.hpp>

namespace realm {
namespace util {

/// Designate a name to be used in heap allocation metrics.
///
/// An instance can be used with `AllocationMetricsContext::get_metric()` to
/// obtain an instance of `MeteredAllocator` that counts
/// allocations/deallocations towards this name, within that context.
///
/// Instances of `AllocationMetricName` should be statically allocated. When an
/// instance has been initialized, it must not be destroyed until the program
/// terminates. This is to ensure that iterating over existing names is
/// thread-safe and lock-free.
///
/// Similarly, when an instance of `AllocationMetricsContext` has been
/// allocated, no further instances of AllocationMetricName must be
/// instantiated.
struct AllocationMetricName {
    explicit AllocationMetricName(const char* name) noexcept;

    /// Get the string name.
    ///
    /// This method is thread-safe.
    const char* name() const noexcept;

    /// Get the index of this metric. The index corresponds to an allocator
    /// inside the current instance of AllocatorMetricTenant.
    ///
    /// This method is thread-safe.
    size_t index() const noexcept;

    /// Get the next name. The names are returned in no particular order.
    ///
    /// This method is thread-safe.
    const AllocationMetricName* next() const noexcept;

    /// Get the first name in the internal list of names, for the purpose
    /// of iterating over all names in the program.
    ///
    /// This method is thread-safe.
    static const AllocationMetricName* get_top() noexcept;
    static const AllocationMetricName* find(const char* name) noexcept;
private:
    const char* m_name;
    size_t m_index; // Index into `AllocationMetricsContext::m_metrics`.

    // This is used to iterate over all existing components. Instances of
    // AllocationMetricName are expected to be statically allocated.
    const AllocationMetricName* m_next = nullptr;
};


/// A heap memory allocator that keeps track of how much was
/// allocated/deallocated throughout its lifetime.
///
/// Memory is allocated with `DefaultAllocator`.
///
/// All methods on instances of this class are thread-safe.
class MeteredAllocator final : public AllocatorBase {
public:
    MeteredAllocator() noexcept;

    static MeteredAllocator& unknown() noexcept;

    /// Return the currently allocated number of bytes.
    ///
    /// This method is thread-safe, but may temporarily return slightly
    /// inaccurate results if allocations/deallocations are happening while it
    /// is being called.
    std::size_t get_currently_allocated_bytes() const noexcept;

    /// Return the total number of bytes that have been allocated (including
    /// allocations that have since been freed).
    ///
    /// This method is thread-safe.
    std::size_t get_total_allocated_bytes() const noexcept;

    /// Return the total number of bytes that have been freed.
    ///
    /// This method is thread-safe.
    std::size_t get_total_deallocated_bytes() const noexcept;

    // AllocatorBase interface:

    /// Return a reference to an MeteredAllocator that belongs to the current
    /// AllocationMetricsContext (if any) and the current AllocationMetricNameScope
    /// (if any).
    ///
    /// The returned reference is valid for the duration of the lifetime of the
    /// instance of AllocationMetricsContext that is "current" at the time of
    /// calling this function, and namely it is valid beyond the lifetime of
    /// the current AllocationMetricNameScope.
    ///
    /// If there is no current AllocationMetricsContext, the global "unknown"
    /// tenant will be used.
    ///
    /// If no metric name is currently in scope (through the use of
    /// AllocationMetricNameScope), allocations and deallocations will be counted
    /// towards the default "unknown" metric.
    ///
    /// This method is thread-safe.
    static MeteredAllocator& get_default() noexcept;

    /// Allocate memory, accounting for the allocation in metrics.
    ///
    /// This method is thread-safe.
    void* allocate(size_t size, size_t align) override final;

    /// Free memory, accounting for the deallocation in metrics.
    ///
    /// This method is thread-safe.
    void free(void* ptr, size_t size) noexcept override final;

    /// Notify metrics that an allocation happened.
    ///
    /// This method is thread-safe.
    void did_allocate_bytes(std::size_t) noexcept;

    /// Notify metrics that a deallocation happened.
    ///
    /// This method is thread-safe.
    void did_free_bytes(std::size_t) noexcept;

private:
    std::atomic<std::size_t> m_allocated_bytes;
    // These members are spaced by 64 bytes to prevent false sharing
    // (inter-processor CPU locks when multiple processes are modifying them
    // concurrently).
    char dummy[56];
    std::atomic<std::size_t> m_deallocated_bytes;
    char dummy2[56]; // Prevent false sharing with the next element.
};

/// `AllocationMetricsContext` represents a runtime scope for metrics, such as
/// for instance a server running in a multi-tenant scenario, where each tenant
/// would have one context associated with it.
///
/// `AllocationMetricsContext` is not available on mobile, due to lack of
/// thread-local storage support on iOS.
struct AllocationMetricsContext {
public:
    AllocationMetricsContext();
    ~AllocationMetricsContext();

#if !REALM_MOBILE
    /// Get the thread-specific AllocationMetricsContext. If none has been set, a
    /// reference to a  globally-allocated "unknown" tenant will be returned.
    static AllocationMetricsContext& get_current() noexcept;
#endif

    /// Get the statically-allocated "unknown" tenant.
    static AllocationMetricsContext& get_unknown();

    MeteredAllocator& get_metric(const AllocationMetricName& name) noexcept;
private:
    std::unique_ptr<MeteredAllocator[]> m_metrics;

    // In debug builds, this is incremented/decremented by
    // `AllocationMetricsContextScope`, and checked in the destructor, to avoid
    // dangling references.
    std::atomic<std::size_t> m_refcount;
    friend class AllocationMetricsContextScope;
};

/// Open a scope where metered memory allocations are counted towards the given
/// name.
///
/// Creating an instance of this class causes calls to
/// `MeteredAllocator::get_default()` from the current thread to return a
/// reference to an allocator that accounts for allocations/deallocations
/// under the named metric specified as the constructor argument.
///
/// When such an instance is destroyed, the previous scope will come back
/// in effect (if one exists; if none exists, the "unknown" metric will be
/// used).
///
/// It is usually an error to create instances of this class with non-scope
/// lifetime, for example on the heap. For that reason, `operator new` is
/// disabled as a precaution.
///
/// If no `AllocationMetricsContext` is current (by instantiation of
/// `AllocationMetricsContextScope`), metrics recorded in the scope introduced
/// by this instance will count towards the "unknown" context, accessible by
/// calling `AllocationMetricsContext::get_unknown()`.
class AllocationMetricNameScope final {
public:
    /// Establish a scope under which all allocations will be tracked as
    /// belonging to \a name.
    explicit AllocationMetricNameScope(const AllocationMetricName& name) noexcept;
    ~AllocationMetricNameScope();
    AllocationMetricNameScope(AllocationMetricNameScope&&) = delete;
    AllocationMetricNameScope& operator=(AllocationMetricNameScope&&) = delete;

    void* operator new(std::size_t) = delete;
private:
    const AllocationMetricName& m_name;
    const AllocationMetricName* m_previous = nullptr;
};

/// Open a scope using the given context for allocation metrics.
///
/// Creating an instance of this class causes calls to
/// `AllocationMetricsContext::get_current()` to return the provided
/// instance. This function is called when by `MeteredAllocator::get_default()`
/// to return an instance that belongs to the given context.
///
/// When the instance is destroyed, the previous context will become active, or
/// the "unknown" context if there was none.
///
/// It is usually an error to create instances of this class with non-scope
/// lifetime, for example on the heap. For that reason, `operator new` is
/// disabled as a precaution.
class AllocationMetricsContextScope final {
public:
    explicit AllocationMetricsContextScope(AllocationMetricsContext& context) noexcept;
    ~AllocationMetricsContextScope();
    AllocationMetricsContextScope(AllocationMetricsContextScope&&) = delete;
    AllocationMetricsContextScope& operator=(AllocationMetricsContextScope&&) = delete;

    void* operator new(std::size_t) = delete;

private:
    AllocationMetricsContext& m_context;
    AllocationMetricsContext& m_previous;
};


/// Convenience STL-compatible allocator that counts allocations as part of the
/// current AllocationMetricNameScope.
template <class T>
using MeteredSTLAllocator = STLAllocator<T, MeteredAllocator>;


// Implementation:

inline const char* AllocationMetricName::name() const noexcept
{
    return m_name;
}

inline size_t AllocationMetricName::index() const noexcept
{
    return m_index;
}

inline const AllocationMetricName* AllocationMetricName::next() const noexcept
{
    return m_next;
}

inline std::size_t MeteredAllocator::get_currently_allocated_bytes() const noexcept
{
    return get_total_allocated_bytes() - get_total_deallocated_bytes();
}

inline std::size_t MeteredAllocator::get_total_allocated_bytes() const noexcept
{
    return m_allocated_bytes.load(std::memory_order_relaxed);
}

inline std::size_t MeteredAllocator::get_total_deallocated_bytes() const noexcept
{
    return m_deallocated_bytes.load(std::memory_order_relaxed);
}

inline void* MeteredAllocator::allocate(size_t size, size_t align)
{
    void* ptr = DefaultAllocator::get_default().allocate(size, align);
    did_allocate_bytes(size);
    return ptr;
}

inline void MeteredAllocator::free(void* ptr, size_t size) noexcept
{
    DefaultAllocator::get_default().free(ptr, size);
    did_free_bytes(size);
}

inline void MeteredAllocator::did_allocate_bytes(std::size_t size) noexcept
{
#if !REALM_MOBILE
    m_allocated_bytes.fetch_add(size, std::memory_order_relaxed);
#else
    static_cast<void>(size);
#endif
}

inline void MeteredAllocator::did_free_bytes(std::size_t size) noexcept
{
#if !REALM_MOBILE
    m_deallocated_bytes.fetch_add(size, std::memory_order_relaxed);
#else
    static_cast<void>(size);
#endif
}
} // namespace util
} // namespace realm

#endif // REALM_UTIL_ALLOCATION_METRICS_HPP
