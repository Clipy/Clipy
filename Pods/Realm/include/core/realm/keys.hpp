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

#ifndef REALM_KEYS_HPP
#define REALM_KEYS_HPP

#include <realm/util/to_string.hpp>
#include <realm/column_type.hpp>
#include <ostream>
#include <vector>

namespace realm {

struct TableKey {
    static constexpr uint32_t null_value = uint32_t(-1) >> 1; // free top bit

    constexpr TableKey() noexcept
        : value(null_value)
    {
    }
    explicit TableKey(uint32_t val) noexcept
        : value(val)
    {
    }
    TableKey& operator=(uint32_t val) noexcept
    {
        value = val;
        return *this;
    }
    bool operator==(const TableKey& rhs) const noexcept
    {
        return value == rhs.value;
    }
    bool operator!=(const TableKey& rhs) const noexcept
    {
        return value != rhs.value;
    }
    bool operator<(const TableKey& rhs) const noexcept
    {
        return value < rhs.value;
    }
    explicit operator bool() const noexcept
    {
        return value != null_value;
    }
    uint32_t value;
};


inline std::ostream& operator<<(std::ostream& os, TableKey tk)
{
    os << "TableKey(" << tk.value << ")";
    return os;
}

namespace util {

inline std::string to_string(TableKey tk)
{
    return to_string(tk.value);
}
}

class TableVersions : public std::vector<std::pair<TableKey, uint64_t>> {
public:
    TableVersions()
    {
    }
    TableVersions(TableKey key, uint64_t version)
    {
        emplace_back(key, version);
    }
    bool operator==(const TableVersions& other) const;
};

struct ColKey {
    static constexpr int64_t null_value = int64_t(uint64_t(-1) >> 1); // free top bit

    struct Idx {
        unsigned val;
    };

    constexpr ColKey() noexcept
        : value(null_value)
    {
    }
    constexpr explicit ColKey(int64_t val) noexcept
        : value(val)
    {
    }
    explicit ColKey(Idx index, ColumnType type, ColumnAttrMask attrs, unsigned tag) noexcept
        : ColKey((index.val & 0xFFFFUL) | ((type & 0x3FUL) << 16) | ((attrs.m_value & 0xFFUL) << 22) |
                 ((tag & 0xFFFFFFFFUL) << 30))
    {
    }
    ColKey& operator=(int64_t val) noexcept
    {
        value = val;
        return *this;
    }
    bool operator==(const ColKey& rhs) const noexcept
    {
        return value == rhs.value;
    }
    bool operator!=(const ColKey& rhs) const noexcept
    {
        return value != rhs.value;
    }
    bool operator<(const ColKey& rhs) const noexcept
    {
        return value < rhs.value;
    }
    bool operator>(const ColKey& rhs) const noexcept
    {
        return value > rhs.value;
    }
    explicit operator bool() const noexcept
    {
        return value != null_value;
    }
    Idx get_index() const noexcept
    {
        return Idx{static_cast<unsigned>(value) & 0xFFFFU};
    }
    ColumnType get_type() const noexcept
    {
        return ColumnType((static_cast<unsigned>(value) >> 16) & 0x3F);
    }
    ColumnAttrMask get_attrs() const noexcept
    {
        return ColumnAttrMask((static_cast<unsigned>(value) >> 22) & 0xFF);
    }
    unsigned get_tag() const noexcept
    {
        return (value >> 30) & 0xFFFFFFFFUL;
    }
    int64_t value;
};

static_assert(ColKey::null_value == 0x7fffffffffffffff, "Fix this");

inline std::ostream& operator<<(std::ostream& os, ColKey ck)
{
    os << "ColKey(" << ck.value << ")";
    return os;
}

struct ObjKey {
    constexpr ObjKey() noexcept
        : value(-1)
    {
    }
    explicit constexpr ObjKey(int64_t val) noexcept
        : value(val)
    {
    }
    ObjKey& operator=(int64_t val) noexcept
    {
        value = val;
        return *this;
    }
    bool operator==(const ObjKey& rhs) const noexcept
    {
        return value == rhs.value;
    }
    bool operator!=(const ObjKey& rhs) const noexcept
    {
        return value != rhs.value;
    }
    bool operator<(const ObjKey& rhs) const noexcept
    {
        return value < rhs.value;
    }
    bool operator<=(const ObjKey& rhs) const noexcept
    {
        return value <= rhs.value;
    }
    bool operator>(const ObjKey& rhs) const noexcept
    {
        return value > rhs.value;
    }
    bool operator>=(const ObjKey& rhs) const noexcept
    {
        return value >= rhs.value;
    }
    explicit operator bool() const noexcept
    {
        return value != -1;
    }
    int64_t value;

private:
    // operator bool will enable casting to integer. Prevent this.
    operator int64_t() const = delete;
};

class ObjKeys : public std::vector<ObjKey> {
public:
    ObjKeys(const std::vector<int64_t>& init)
    {
        reserve(init.size());
        for (auto i : init) {
            emplace_back(i);
        }
    }
    ObjKeys()
    {
    }
};


inline std::ostream& operator<<(std::ostream& ostr, ObjKey key)
{
    ostr << "ObjKey(" << key.value << ")";
    return ostr;
}

constexpr ObjKey null_key;

namespace util {

inline std::string to_string(ColKey ck)
{
    return to_string(ck.value);
}

} // namespace util

} // namespace realm


namespace std {

template <>
struct hash<realm::ObjKey> {
    size_t operator()(realm::ObjKey key) const
    {
        return std::hash<uint64_t>{}(key.value);
    }
};

} // namespace std


#endif
