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
#ifndef REALM_COLUMN_FWD_HPP
#define REALM_COLUMN_FWD_HPP

#include <cstdint>

namespace realm {

// Regular classes
class ColumnBase;
class StringColumn;
class StringEnumColumn;
class BinaryColumn;
class SubtableColumn;
class MixedColumn;
class LinkColumn;
class LinkListColumn;

// Templated classes
template<class T>
class Column;
template<class T>
class BasicColumn;

namespace util {
template <class> class Optional;
}

// Shortcuts, aka typedefs.
using IntegerColumn = Column<int64_t>;
using IntNullColumn = Column<util::Optional<int64_t>>;
using DoubleColumn = Column<double>;
using FloatColumn = Column<float>;

} // namespace realm

#endif // REALM_COLUMN_FWD_HPP
