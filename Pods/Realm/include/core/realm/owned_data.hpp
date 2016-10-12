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

    /// If \a data_to_copy is 'null', \a data_size must be zero.
    OwnedData(const char* data_to_copy, size_t data_size) : m_size(data_size)
    {
        REALM_ASSERT_DEBUG(data_to_copy || data_size == 0);
        if (data_to_copy) {
            m_data = std::unique_ptr<char[]>(new char[data_size]);
            memcpy(m_data.get(), data_to_copy, data_size);
        }
    }

    /// If \a unique_data is 'null', \a data_size must be zero.
    OwnedData(std::unique_ptr<char[]> unique_data, size_t data_size) noexcept :
        m_data(std::move(unique_data)), m_size(data_size)
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
