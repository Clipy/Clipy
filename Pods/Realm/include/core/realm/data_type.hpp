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

#ifndef REALM_DATA_TYPE_HPP
#define REALM_DATA_TYPE_HPP

namespace realm {

// Note: Value assignments must be kept in sync with <realm/column_type.h>
// Note: Value assignments must be kept in sync with <realm/c/data_type.h>
// Note: Value assignments must be kept in sync with <realm/objc/type.h>
// Note: Value assignments must be kept in sync with "com/realm/ColumnType.java"
// Note: Any change to this enum is a file-format breaking change.
enum DataType {
    type_Int = 0,
    type_Bool = 1,
    type_Float = 9,
    type_Double = 10,
    type_String = 2,
    type_Binary = 4,
    type_OldDateTime = 7,
    type_Timestamp = 8,
    type_Table = 5,
    type_Mixed = 6,
    type_Link = 12,
    type_LinkList = 13
};

/// See Descriptor::set_link_type().
enum LinkType {
    link_Strong,
    link_Weak,
};

} // namespace realm

#endif // REALM_DATA_TYPE_HPP
