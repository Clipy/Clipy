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

#ifndef REALM_COLUMN_TYPE_HPP
#define REALM_COLUMN_TYPE_HPP

namespace realm {


// Note: Enumeration value assignments must be kept in sync with
// <realm/data_type.hpp>.
enum ColumnType {
    // Column types
    col_type_Int = 0,
    col_type_Bool = 1,
    col_type_String = 2,
    col_type_StringEnum = 3, // double refs
    col_type_Binary = 4,
    col_type_Table = 5,
    col_type_Mixed = 6,
    col_type_OldDateTime = 7,
    col_type_Timestamp = 8,
    col_type_Float = 9,
    col_type_Double = 10,
    col_type_Reserved4 = 11, // Decimal
    col_type_Link = 12,
    col_type_LinkList = 13,
    col_type_BackLink = 14
};


// Column attributes can be combined using bitwise or.
enum ColumnAttr {
    col_attr_None = 0,
    col_attr_Indexed = 1,

    /// Specifies that this column forms a unique constraint. It requires
    /// `col_attr_Indexed`.
    col_attr_Unique = 2,

    /// Reserved for future use.
    col_attr_Reserved = 4,

    /// Specifies that the links of this column are strong, not weak. Applies
    /// only to link columns (`type_Link` and `type_LinkList`).
    col_attr_StrongLinks = 8,

    /// Specifies that elements in the column can be null.
    col_attr_Nullable = 16
};


} // namespace realm

#endif // REALM_COLUMN_TYPE_HPP
