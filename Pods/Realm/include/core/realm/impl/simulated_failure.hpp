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

#ifndef REALM_IMPL_SIMULATED_FAILURE_HPP
#define REALM_IMPL_SIMULATED_FAILURE_HPP

#include <cstdint>
#include <system_error>

#include <realm/util/features.h>

#ifdef REALM_DEBUG
#define REALM_ENABLE_SIMULATED_FAILURE
#endif

namespace realm {
namespace _impl {

class SimulatedFailure : public std::system_error {
public:
    enum FailureType {
        generic,
        slab_alloc__reset_free_space_tracking,
        slab_alloc__remap,
        shared_group__grow_reader_mapping,
        sync_client__read_head,
        sync_server__read_head,
        _num_failure_types
    };

    class OneShotPrimeGuard;
    class RandomPrimeGuard;

    /// Prime the specified failure type on the calling thread for triggering
    /// once.
    static void prime_one_shot(FailureType);

    /// Prime the specified failure type on the calling thread for triggering
    /// randomly \a n out of \a m times.
    static void prime_random(FailureType, int n, int m, uint_fast64_t seed = 0);

    /// Unprime the specified failure type on the calling thread.
    static void unprime(FailureType) noexcept;

    /// Returns true according to the mode of priming of the specified failure
    /// type on the calling thread, but only if REALM_ENABLE_SIMULATED_FAILURE
    /// was defined during compilation. If REALM_ENABLE_SIMULATED_FAILURE was
    /// not defined, this function always return false.
    static bool check_trigger(FailureType) noexcept;

    /// The specified error code is set to `make_error_code(failure_type)` if
    /// check_trigger() returns true. Otherwise it is set to
    /// `std::error_code()`. Returns a copy of the updated error code.
    static std::error_code trigger(FailureType failure_type, std::error_code&) noexcept;

    /// Throws SimulatedFailure if check_trigger() returns true. The exception
    /// will be constructed with an error code equal to
    /// `make_error_code(failure_type)`.
    static void trigger(FailureType failure_type);

    /// Returns true when, and only when REALM_ENABLE_SIMULATED_FAILURE was
    /// defined during compilation.
    static constexpr bool is_enabled();

    SimulatedFailure(std::error_code);

private:
#ifdef REALM_ENABLE_SIMULATED_FAILURE
    static void do_prime_one_shot(FailureType);
    static void do_prime_random(FailureType, int n, int m, uint_fast64_t seed);
    static void do_unprime(FailureType) noexcept;
    static bool do_check_trigger(FailureType) noexcept;
#endif
};

std::error_code make_error_code(SimulatedFailure::FailureType) noexcept;


class SimulatedFailure::OneShotPrimeGuard {
public:
    OneShotPrimeGuard(FailureType);
    ~OneShotPrimeGuard() noexcept;

private:
    const FailureType m_type;
};


class SimulatedFailure::RandomPrimeGuard {
public:
    RandomPrimeGuard(FailureType, int n, int m, uint_fast64_t seed = 0);
    ~RandomPrimeGuard() noexcept;

private:
    const FailureType m_type;
};


// Implementation

inline void SimulatedFailure::prime_one_shot(FailureType failure_type)
{
#ifdef REALM_ENABLE_SIMULATED_FAILURE
    do_prime_one_shot(failure_type);
#else
    static_cast<void>(failure_type);
#endif
}

inline void SimulatedFailure::prime_random(FailureType failure_type, int n, int m, uint_fast64_t seed)
{
#ifdef REALM_ENABLE_SIMULATED_FAILURE
    do_prime_random(failure_type, n, m, seed);
#else
    static_cast<void>(failure_type);
    static_cast<void>(n);
    static_cast<void>(m);
    static_cast<void>(seed);
#endif
}

inline void SimulatedFailure::unprime(FailureType failure_type) noexcept
{
#ifdef REALM_ENABLE_SIMULATED_FAILURE
    do_unprime(failure_type);
#else
    static_cast<void>(failure_type);
#endif
}

inline bool SimulatedFailure::check_trigger(FailureType failure_type) noexcept
{
#ifdef REALM_ENABLE_SIMULATED_FAILURE
    return do_check_trigger(failure_type);
#else
    static_cast<void>(failure_type);
    return false;
#endif
}

inline std::error_code SimulatedFailure::trigger(FailureType failure_type, std::error_code& ec) noexcept
{
    if (check_trigger(failure_type)) {
        ec = make_error_code(failure_type);
    }
    else {
        ec = std::error_code();
    }
    return ec;
}

inline void SimulatedFailure::trigger(FailureType failure_type)
{
    if (check_trigger(failure_type))
        throw SimulatedFailure(make_error_code(failure_type));
}

inline constexpr bool SimulatedFailure::is_enabled()
{
#ifdef REALM_ENABLE_SIMULATED_FAILURE
    return true;
#else
    return false;
#endif
}

inline SimulatedFailure::SimulatedFailure(std::error_code ec)
    : std::system_error(ec)
{
}

inline SimulatedFailure::OneShotPrimeGuard::OneShotPrimeGuard(FailureType failure_type)
    : m_type(failure_type)
{
    prime_one_shot(m_type);
}

inline SimulatedFailure::OneShotPrimeGuard::~OneShotPrimeGuard() noexcept
{
    unprime(m_type);
}

inline SimulatedFailure::RandomPrimeGuard::RandomPrimeGuard(FailureType failure_type, int n, int m,
                                                            uint_fast64_t seed)
    : m_type(failure_type)
{
    prime_random(m_type, n, m, seed);
}

inline SimulatedFailure::RandomPrimeGuard::~RandomPrimeGuard() noexcept
{
    unprime(m_type);
}

} // namespace _impl
} // namespace realm

#endif // REALM_IMPL_SIMULATED_FAILURE_HPP
