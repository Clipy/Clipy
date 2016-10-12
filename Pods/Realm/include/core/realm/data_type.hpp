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
#ifndef REALM_DATA_TYPE_HPP
#define REALM_DATA_TYPE_HPP

namespace realm {

// Note: Value assignments must be kept in sync with <realm/column_type.h>
// Note: Value assignments must be kept in sync with <realm/c/data_type.h>
// Note: Value assignments must be kept in sync with <realm/objc/type.h>
// Note: Value assignments must be kept in sync with "com/realm/ColumnType.java"
enum DataType {
    type_Int         =  0,
    type_Bool        =  1,
    type_Float       =  9,
    type_Double      = 10,
    type_String      =  2,
    type_Binary      =  4,
    type_OldDateTime =  7,
    type_Timestamp   =  8,
    type_Table       =  5,
    type_Mixed       =  6,
    type_Link        = 12,
    type_LinkList    = 13
};

/// See Descriptor::set_link_type().
enum LinkType {
    link_Strong,
    link_Weak
};

} // namespace realm

#endif // REALM_DATA_TYPE_HPP
