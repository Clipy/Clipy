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

template<class T> class ValueResetGuard {
public:
    ValueResetGuard(T& var, T val);
    ValueResetGuard(ValueResetGuard&&);
    ~ValueResetGuard();

private:
    T* m_var;
    T m_val;
};


/// Set \a var to \a val when the returned object is destroyed.
template<class T> inline ValueResetGuard<T> make_value_reset_guard(T& var, T val = {});

/// Set \a var to \a val_1 immediately, and then to \a val_2 when the returned
/// object is destroyed.
template<class T> inline ValueResetGuard<T> make_temp_assign(T& var, T val_1, T val_2 = {});



// Implementation

template<class T> inline ValueResetGuard<T>::ValueResetGuard(T& var, T val) :
    m_var{&var},
    m_val{std::move(val)}
{
}

template<class T> inline ValueResetGuard<T>::ValueResetGuard(ValueResetGuard&& other) :
    m_var{other.m_var},
    m_val{std::move(other.m_val)}
{
    other.m_var = nullptr;
}

template<class T> inline ValueResetGuard<T>::~ValueResetGuard()
{
    if (m_var)
        *m_var = std::move(m_val);
}

template<class T> inline ValueResetGuard<T> make_value_reset_guard(T& var, T val)
{
    return ValueResetGuard<T>(var, std::move(val));
}

template<class T> inline ValueResetGuard<T> make_temp_assign(T& var, T val_1, T val_2)
{
    var = std::move(val_1);
    return make_value_reset_guard(var, std::move(val_2));
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_VALUE_RESET_GUARD_HPP
