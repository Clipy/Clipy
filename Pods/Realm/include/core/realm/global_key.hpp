/*************************************************************************
 *
 * Copyright 2019 Realm Inc.
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

#ifndef REALM_GLOBAL_KEY_HPP
#define REALM_GLOBAL_KEY_HPP

#include <realm/keys.hpp>
#include <realm/util/optional.hpp>
#include <limits>
#include <cstdint>

namespace realm {

class StringData;
class Mixed;

/// GlobalKeys are globally unique for a given class (table), and up to 128 bits
/// wide. They are represented as two 64-bit integers, each of which may
/// frequently be small, for best on-wire compressibility.
///
/// We define a way to map from 128-bit on-write GlobalKeys to local 64-bit ObjKeys.
///
/// The three object ID types are:
/// a. Global Keyss for objects in tables without primary keys.
/// b. Global Keys for objects in tables with integer primary keys.
/// c. Global Keys for objects in tables with other primary key types.
///
/// For objects without primary keys (a), a "squeezed" tuple of the
/// client_file_ident and a peer-local sequence number is used as the local
/// ObjKey. The on-write Object ID is the "unsqueezed" format.
///
/// For integer primary keys (b), the GlobalKey just the integer value as the low
/// part.
///
/// For objects with other types of primary keys (c), the GlobalKey is a 128-bit
/// hash of the primary key value. However, the local object ID must be a 63-bit
/// integer, because that is the maximum size integer that can be used in an ObjKey.
/// The solution is to optimistically use the lower 62 bits of the on-wire GlobalKey.
/// If this results in a ObjKey which is already in use, a new local ObjKey is
/// generated with the 63th bit set and using a locally generated sequence number for
/// the lower bits. The mapping between GlobalKey and ObjKey is stored in the Table
/// structure.

struct GlobalKey {
    constexpr GlobalKey(uint64_t h, uint64_t l)
        : m_lo(l)
        , m_hi(h)
    {
    }
    static GlobalKey from_string(StringData);

    constexpr GlobalKey(realm::util::None = realm::util::none)
        : m_lo(-1)
        , m_hi(-1)
    {
    }

    // Construct an ObjectId from either a string or an integer
    GlobalKey(Mixed pk);

    // Construct an object id from the local squeezed ObjKey
    GlobalKey(ObjKey squeezed, uint64_t sync_file_id)
    {
        uint64_t u = uint64_t(squeezed.value);

        m_lo = (u & 0xff) | ((u & 0xffffff0000) >> 8);
        m_hi = ((u & 0xff00) >> 8) | ((u & 0xffffff0000000000) >> 32);
        if (m_hi == 0)
            m_hi = sync_file_id;
    }

    constexpr GlobalKey(const GlobalKey&) noexcept = default;
    GlobalKey& operator=(const GlobalKey&) noexcept = default;

    constexpr uint64_t lo() const
    {
        return m_lo;
    }
    constexpr uint64_t hi() const
    {
        return m_hi;
    }

    std::string to_string() const;

    constexpr bool operator<(const GlobalKey& other) const
    {
        return (m_hi == other.m_hi) ? (m_lo < other.m_lo) : (m_hi < other.m_hi);
    }
    constexpr bool operator==(const GlobalKey& other) const
    {
        return m_hi == other.m_hi && m_lo == other.m_lo;
    }
    constexpr bool operator!=(const GlobalKey& other) const
    {
        return !(*this == other);
    }

    explicit constexpr operator bool() const noexcept
    {
        return (*this != GlobalKey{});
    }

    // Generate a local ObjKey from the GlobalKey. If the object is created
    // in this realm (sync_file_id == hi) then 0 is used for hi. In this
    // way we achieves that objects created before first contact with the
    // server does not need to change key.
    ObjKey get_local_key(uint64_t sync_file_id)
    {
        REALM_ASSERT(m_hi <= 0x3fffffff);
        REALM_ASSERT(lo() <= std::numeric_limits<uint32_t>::max());

        auto high = m_hi;
        if (high == sync_file_id)
            high = 0;
        uint64_t a = m_lo & 0xff;
        uint64_t b = (high & 0xff) << 8;
        uint64_t c = (m_lo & 0xffffff00) << 8;
        uint64_t d = (high & 0x3fffff00) << 32;

        return ObjKey(int64_t(a | b | c | d));
    }

private:
    uint64_t m_lo;
    uint64_t m_hi;
};

std::ostream& operator<<(std::ostream&, const GlobalKey&);
std::istream& operator>>(std::istream&, GlobalKey&);

} // namespace realm

namespace std {

template <>
struct hash<realm::GlobalKey> {
    size_t operator()(realm::GlobalKey oid) const
    {
        return std::hash<uint64_t>{}(oid.lo()) ^ std::hash<uint64_t>{}(oid.hi());
    }
};

} // namespace std

#endif /* REALM_OBJECT_ID_HPP */
