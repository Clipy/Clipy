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
#ifndef REALM_UTIL_VALUE_RESET_GUARD_HPP
#define REALM_UTIL_VALUE_RESET_GUARD_HPP

#include <utility>

namespace realm {
namespace util {

template<class T, class U> class ValueResetGuard {
public:
    ValueResetGuard(T& var, U val);
    ValueResetGuard(ValueResetGuard&&);
    ~ValueResetGuard();

private:
    T* m_var;
    U m_val;
};


/// Set \a var to `T{}` when the returned object is destroyed.
template<class T> ValueResetGuard<T, T> make_value_reset_guard(T& var);

/// Set \a var to \a val when the returned object is destroyed.
template<class T, class U> ValueResetGuard<T, U> make_value_reset_guard(T& var, U val);

/// Set \a var to \a val_1 immediately, and then to \a val_2 when the returned
/// object is destroyed.
template<class T, class U> ValueResetGuard<T, U> make_temp_assign(T& var, U val_1, U val_2 = {});



// Implementation

template<class T, class U> inline ValueResetGuard<T, U>::ValueResetGuard(T& var, U val) :
    m_var{&var},
    m_val{std::move(val)}
{
}

template<class T, class U> inline ValueResetGuard<T, U>::ValueResetGuard(ValueResetGuard&& other) :
    m_var{other.m_var},
    m_val{std::move(other.m_val)}
{
    other.m_var = nullptr;
}

template<class T, class U> inline ValueResetGuard<T, U>::~ValueResetGuard()
{
    if (m_var)
        *m_var = std::move(m_val);
}

template<class T> inline ValueResetGuard<T, T> make_value_reset_guard(T& var)
{
    return ValueResetGuard<T, T>(var, T{});
}

template<class T, class U> inline ValueResetGuard<T, U> make_value_reset_guard(T& var, U val)
{
    return ValueResetGuard<T, U>(var, std::move(val));
}

template<class T, class U> inline ValueResetGuard<T, U> make_temp_assign(T& var, U val_1, U val_2)
{
    var = std::move(val_1);
    return make_value_reset_guard(var, std::move(val_2));
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_VALUE_RESET_GUARD_HPP
