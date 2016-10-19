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

#ifndef REALM_UTIL_RANDOM_HPP
#define REALM_UTIL_RANDOM_HPP

#include <stddef.h>
#include <limits>
#include <array>
#include <random>
#include <algorithm>
#include <functional>

namespace realm {
namespace util {

/// Perform a nondeterministc seeding of the specified pseudo random number
/// generator.
///
/// \tparam Engine A type that satisfies UniformRandomBitGenerator as defined by
/// the C++ standard.
///
/// \tparam state_size The number of words of type Engine::result_type that make
/// up the engine state.
///
/// Thread-safe.
///
/// FIXME: Move this to core repo, as it is generally useful.
template<class Engine, size_t state_size = Engine::state_size>
void seed_prng_nondeterministically(Engine&);




// Implementation

} // namespace util

namespace _impl {

void get_extra_seed_entropy(unsigned int& extra_entropy_1, unsigned int& extra_entropy_2,
                            unsigned int& extra_entropy_3);

} // namespace _impl

namespace util {

template<class Engine, size_t state_size> void seed_prng_nondeterministically(Engine& engine)
{
    // This implementation was informed and inspired by
    // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0205r0.html.
    //
    // The number of bits of entropy needed is `state_size *
    // std::numeric_limits<typename Engine::result_type>::digits` (assuming that
    // the engine uses all available bits in each word).
    //
    // Each invocation of `std::random_device::operator()` gives us
    // `std::numeric_limits<unsigned int>::digits` bits (assuming maximum
    // entropy). Note that `std::random_device::result_type` must be `unsigned
    // int`, `std::random_device::min()` must return zero, and
    // `std::random_device::max()` must return `std::numeric_limits<unsigned
    // int>::max()`.
    //
    // Ideally, we could have used `std::random_device::entropy()` as the actual
    // number of bits of entropy produced per invocation of
    // `std::random_device::operator()`, however, it is incorrectly implemented
    // on many platform. Also, it is supposed to return zero when
    // `std::random_device` is just a PRNG, but that would leave us with no way
    // to continue.
    //
    // When the actual entropy from `std::random_device` is less than maximum,
    // the seeding will be less than optimal. For example, if the actual entropy
    // is only half of the maximum, then the seeding will only produce half the
    // entrpy that it ought to, but that will generally still be a good seeding.
    //
    // For the (assumed) rare cases where `std::random_device` is a PRGN that is
    // not nondeterministically seeded, we include a bit of extra entropy taken
    // from such places as the current time and the ID of the executing process
    // (when available).

    constexpr long seed_bits_needed = state_size *
        long(std::numeric_limits<typename Engine::result_type>::digits);
    constexpr int seed_bits_per_device_invocation =
        std::numeric_limits<unsigned int>::digits;
    constexpr size_t seed_words_needed =
        size_t((seed_bits_needed + (seed_bits_per_device_invocation - 1)) /
               seed_bits_per_device_invocation); // Rounding up
    constexpr int num_extra = 3;
    std::array<std::random_device::result_type, seed_words_needed+num_extra> seed_values;
    std::random_device rnddev;
    std::generate(seed_values.begin(), seed_values.end()-num_extra, std::ref(rnddev));

    unsigned int extra_entropy[3];
    _impl::get_extra_seed_entropy(extra_entropy[0], extra_entropy[1], extra_entropy[2]);
    static_assert(num_extra == sizeof extra_entropy / sizeof extra_entropy[0], "Mismatch");
    std::copy(extra_entropy, extra_entropy+num_extra, seed_values.end()-num_extra);

    std::seed_seq seed_seq(seed_values.begin(), seed_values.end());
    engine.seed(seed_seq);
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_RANDOM_HPP
