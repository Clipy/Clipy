/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2012] Realm Inc
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

#ifndef REALM_IMPL_TABLE_PATH_HPP
#define REALM_IMPL_TABLE_PATH_HPP

#include <stddef.h>
#include <iterator>
#include <vector>

namespace realm {
namespace _impl {

class TablePath {
public:
    TablePath()
    {
    }

    TablePath(const size_t* begin, size_t len) : m_coords(begin, begin + len)
    {
    }

    TablePath(size_t group_level_ndx, size_t num_pairs, const size_t* pairs)
    {
        m_coords.reserve(num_pairs * 2 + 1);
        m_coords.push_back(group_level_ndx);
        std::copy(pairs, pairs + num_pairs * 2, std::back_inserter(m_coords));
    }

    bool operator==(const TablePath& other) const
    {
        if (m_coords.size() != other.m_coords.size()) {
            return false;
        }
        for (size_t i = 0; i < m_coords.size(); ++i) {
            if (m_coords[i] != other.m_coords[i]) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const TablePath& other) const
    {
        return !((*this) == other);
    }

    void push(size_t coord)
    {
        m_coords.push_back(coord);
    }

    size_t size() const
    {
        return m_coords.size();
    }

    void clear()
    {
        m_coords.clear();
    }

    // FIXME: Should be private, but is accessed directly from sync.cpp.
    std::vector<size_t> m_coords;
};

} // namespace _impl
} // namespace realm

#endif // REALM_IMPL_TABLE_PATH_HPP
