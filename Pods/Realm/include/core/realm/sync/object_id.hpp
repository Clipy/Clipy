/*************************************************************************
 *
 * Copyright 2017 Realm Inc.
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

#ifndef REALM_SYNC_OBJECT_ID_HPP
#define REALM_SYNC_OBJECT_ID_HPP

#include <functional> // std::hash
#include <string>
#include <iosfwd> // operator<<

#include <stdint.h>

#include <realm/util/optional.hpp>
#include <realm/string_data.hpp>
#include <realm/data_type.hpp>

// Only set this to one when testing the code paths that exercise object ID
// hash collisions. It artificially limits the "optimistic" local ID to use
// only the lower 15 bits of the ID rather than the lower 63 bits, making it
// feasible to generate collisions within reasonable time.
#define REALM_EXERCISE_OBJECT_ID_COLLISION 0

namespace realm {

class Group;

namespace sync {

/// ObjectIDs are globally unique, and up to 128 bits wide. They are represented
/// as two 64-bit integers, each of which may frequently be small, for best
/// on-wire compressibility.
struct ObjectID {
    constexpr ObjectID(uint64_t hi, uint64_t lo);

    // FIXME: Remove "empty" ObjectIDs, wrap in Optional instead.
    constexpr ObjectID(realm::util::None = realm::util::none);
    constexpr ObjectID(const ObjectID&) noexcept = default;
    ObjectID& operator=(const ObjectID&) noexcept = default;

    constexpr uint64_t lo() const { return m_lo; }
    constexpr uint64_t hi() const { return m_hi; }

    std::string to_string() const;

    constexpr bool operator<(const ObjectID& other) const;
    constexpr bool operator==(const ObjectID& other) const;
    constexpr bool operator!=(const ObjectID& other) const;

private:
    uint64_t m_lo;
    uint64_t m_hi;
};

/// Implementors of this interface should define a way to map from 128-bit
/// on-write ObjectIDs to local 64-bit object IDs.
///
/// The three object ID types are:
/// a. Object IDs for objects in tables without primary keys.
/// b. Object IDs for objects in tables with integer primary keys.
/// c. Object IDs for objects in tables with other primary key types.
///
/// For integer primary keys (b), the Object ID is just the integer value.
///
/// For objects without primary keys (a), a "squeezed" tuple of the
/// client_file_ident and a peer-local sequence number is used as the local
/// Object ID. The on-write Object ID is the "unsqueezed" format. The methods on
/// this interface ending in "_squeezed" aid in the creation and conversion of
/// these IDs.
///
/// For objects with other types of primary keys (c), the ObjectID
/// is a 128-bit hash of the primary key value. However, the local object ID
/// must be a 64-bit integer, because that is the maximum size integer that
/// Realm is able to store. The solution is to optimistically use the lower 63
/// bits of the on-wire Object ID, and use a local ID with the upper 64th bit
/// set when there is a collision in the lower 63 bits between two different
/// hash values.
class ObjectIDProvider {
public:
    using LocalObjectID = int_fast64_t;

    /// Calculate optimistic local ID that may collide with others. It is up to
    /// the caller to ensure that collisions are detected and that
    /// allocate_local_id_after_collision() is called to obtain a non-colliding
    /// ID.
    static LocalObjectID get_optimistic_local_id_hashed(ObjectID global_id);

    /// Find the local 64-bit object ID for the provided global 128-bit ID.
    virtual LocalObjectID global_to_local_object_id_hashed(size_t table_ndx, ObjectID global_id) const = 0;

    /// After a local ID collision has been detected, this function may be
    /// called to obtain a non-colliding local ID in such a way that subsequence
    /// calls to global_to_local_object_id() will return the correct local ID
    /// for both \a incoming_id and \a colliding_id.
    virtual LocalObjectID allocate_local_id_after_hash_collision(size_t table_ndx,
                                                                 ObjectID incoming_id,
                                                                 ObjectID colliding_id,
                                                                 LocalObjectID colliding_local_id) = 0;
    static LocalObjectID make_tagged_local_id_after_hash_collision(uint64_t sequence_number);
    virtual void free_local_id_after_hash_collision(size_t table_ndx, ObjectID object_id) = 0;

    /// Some Object IDs are generated as a tuple of the client_file_ident and a
    /// local sequence number. This function takes the next number in the
    /// sequence for the given table and returns an appropriate globally unique
    /// ObjectID.
    virtual ObjectID allocate_object_id_squeezed(size_t table_ndx) = 0;
    static LocalObjectID global_to_local_object_id_squeezed(ObjectID);
    static ObjectID local_to_global_object_id_squeezed(LocalObjectID);

    virtual int_fast64_t get_client_file_ident() const = 0;
};

/// Implementation:


constexpr ObjectID::ObjectID(uint64_t hi, uint64_t lo): m_lo(lo), m_hi(hi)
{
}

constexpr ObjectID::ObjectID(realm::util::None): m_lo(-1), m_hi(-1)
{
}

constexpr bool ObjectID::operator<(const ObjectID& other) const
{
    return (m_hi == other.m_hi) ? (m_lo < other.m_lo) : (m_hi < other.m_hi);
}

constexpr bool ObjectID::operator==(const ObjectID& other) const
{
    return m_hi == other.m_hi && m_lo == other.m_lo;
}

constexpr bool ObjectID::operator!=(const ObjectID& other) const
{
    return !(*this == other);
}

std::ostream& operator<<(std::ostream&, const realm::sync::ObjectID&);

inline ObjectIDProvider::LocalObjectID
ObjectIDProvider::get_optimistic_local_id_hashed(ObjectID global_id)
{
#if REALM_EXERCISE_OBJECT_ID_COLLISION
    const uint64_t optimistic_mask = 0xff;
#else
    const uint64_t optimistic_mask = 0x7fffffffffffffff;
#endif
    static_assert(optimistic_mask < 0x8000000000000000, "optimistic Object ID mask must leave the 64th bit zero");
    return global_id.lo() & optimistic_mask;
}

inline ObjectIDProvider::LocalObjectID
ObjectIDProvider::make_tagged_local_id_after_hash_collision(uint64_t sequence_number)
{
    REALM_ASSERT(sequence_number < 0x8000000000000000);
    return 0x8000000000000000 | sequence_number;
}

inline ObjectIDProvider::LocalObjectID
ObjectIDProvider::global_to_local_object_id_squeezed(ObjectID object_id)
{
    REALM_ASSERT(object_id.hi() <= std::numeric_limits<uint32_t>::max());
    REALM_ASSERT(object_id.lo() <= std::numeric_limits<uint32_t>::max());

    uint64_t a =  object_id.lo() & 0xff;
    uint64_t b = (object_id.hi() & 0xff) << 8;
    uint64_t c = (object_id.lo() & 0xffffff00) << 8;
    uint64_t d = (object_id.hi() & 0xffffff00) << 32;
    union {
        uint64_t u;
        int64_t  s;
    } bitcast;
    bitcast.u = a | b | c | d;
    return bitcast.s;
}

inline ObjectID
ObjectIDProvider::local_to_global_object_id_squeezed(LocalObjectID squeezed)
{
    union {
        uint64_t u;
        int64_t  s;
    } bitcast;
    bitcast.s = squeezed;

    uint64_t u = bitcast.u;

    uint64_t lo = (u & 0xff) | ((u & 0xffffff0000) >> 8);
    uint64_t hi = ((u & 0xff00) >> 8) | ((u & 0xffffff0000000000) >> 32);
    return ObjectID{hi, lo};
}

} // namespace sync
} // namespace realm

namespace std {

template <>
struct hash<realm::sync::ObjectID> {
    size_t operator()(realm::sync::ObjectID oid) const
    {
        return std::hash<uint64_t>{}(oid.lo()) ^ std::hash<uint64_t>{}(oid.hi());
    }
};

} // namespace std

#endif // REALM_SYNC_OBJECT_ID_HPP

