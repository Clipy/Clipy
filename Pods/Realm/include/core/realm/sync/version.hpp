/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2013] Realm Inc
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
#ifndef REALM_SYNC_VERSION_HPP
#define REALM_SYNC_VERSION_HPP

#include <realm/util/features.h>

#define REALM_SYNC_VER_MAJOR 1
#define REALM_SYNC_VER_MINOR 10
#define REALM_SYNC_VER_PATCH 8
#define REALM_SYNC_PRODUCT_NAME "realm-sync"

#define REALM_SYNC_VER_STRING REALM_QUOTE(REALM_SYNC_VER_MAJOR) "." \
    REALM_QUOTE(REALM_SYNC_VER_MINOR) "." REALM_QUOTE(REALM_SYNC_VER_PATCH)
#define REALM_SYNC_VER_CHUNK "[" REALM_SYNC_PRODUCT_NAME "-" REALM_SYNC_VER_STRING "]"

#endif // REALM_SYNC_VERSION_HPP
