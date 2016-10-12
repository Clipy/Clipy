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
#ifndef REALM_OWNED_DATA_HPP
#define REALM_OWNED_DATA_HPP

#include <cstring>
#include <memory>

namespace realm {

/// A chunk of owned data.
class OwnedData {
public:
    /// Construct a null reference.
    OwnedData() noexcept {}

    /// If \a data is 'null', \a size must be zero.
    OwnedData(const char* data, size_t size) : m_size(size)
    {
        REALM_ASSERT_DEBUG(data || size == 0);
        if (data) {
            m_data = std::unique_ptr<char[]>(new char[size]);
            memcpy(m_data.get(), data, size);
        }
    }

    /// If \a data is 'null', \a size must be zero.
    OwnedData(std::unique_ptr<char[]> data, size_t size) noexcept :
        m_data(std::move(data)), m_size(size)
    {
        REALM_ASSERT_DEBUG(m_data || m_size == 0);
    }

    OwnedData(const OwnedData& other) : OwnedData(other.m_data.get(), other.m_size) { }
    OwnedData& operator=(const OwnedData& other);

    OwnedData(OwnedData&&) = default;
    OwnedData& operator=(OwnedData&&) = default;

    const char* data() const { return m_data.get(); }
    size_t size() const { return m_size; }

private:
    std::unique_ptr<char[]> m_data;
    size_t m_size = 0;
};

inline OwnedData& OwnedData::operator=(const OwnedData& other)
{
    if (this != &other) {
        if (other.m_data) {
            m_data = std::unique_ptr<char[]>(new char[other.m_size]);
            memcpy(m_data.get(), other.m_data.get(), other.m_size);
        } else {
            m_data = nullptr;
        }
        m_size = other.m_size;
    }
    return *this;
}

} // namespace realm

#endif // REALM_OWNED_DATA_HPP
