/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2016] Realm Inc
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

#ifndef REALM_UTIL_CF_PTR_HPP
#define REALM_UTIL_CF_PTR_HPP

#include <realm/util/assert.hpp>

#if REALM_PLATFORM_APPLE

#include <CoreFoundation/CoreFoundation.h>

namespace realm {
namespace util {

template<class Ref>
class CFPtr {
public:
    explicit CFPtr(Ref ref = nullptr) noexcept:
        m_ref(ref)
    {
    }

    CFPtr(CFPtr&& rg) noexcept:
        m_ref(rg.m_ref)
    {
        rg.m_ref = nullptr;
    }

    ~CFPtr() noexcept
    {
        if (m_ref)
            CFRelease(m_ref);
    }

    CFPtr& operator=(CFPtr&& rg) noexcept
    {
        REALM_ASSERT(!m_ref || m_ref != rg.m_ref);
        if (m_ref)
            CFRelease(m_ref);
        m_ref = rg.m_ref;
        rg.m_ref = nullptr;
        return *this;
    }

    explicit operator bool() const noexcept
    {
        return bool(m_ref);
    }

    Ref get() const noexcept
    {
        return m_ref;
    }

    Ref release() noexcept
    {
        Ref ref = m_ref;
        m_ref = nullptr;
        return ref;
    }

    void reset(Ref ref = nullptr) noexcept
    {
        REALM_ASSERT(!m_ref || m_ref != ref);
        if (m_ref)
            CFRelease(m_ref);
        m_ref = ref;
    }

private:
    Ref m_ref;
};

template<class Ref>
CFPtr<Ref> adoptCF(Ref ptr) {
    return CFPtr<Ref>(ptr);
}

template<class Ref>
CFPtr<Ref> retainCF(Ref ptr) {
    CFRetain(ptr);
    return CFPtr<Ref>(ptr);
}


}
}


#endif // REALM_PLATFORM_APPLE

#endif // REALM_UTIL_CF_PTR_HPP
