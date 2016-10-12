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
#ifndef REALM_VERSION_HPP
#define REALM_VERSION_HPP

#include <string>

#include <realm/util/features.h>

#define REALM_VER_MAJOR 1
#define REALM_VER_MINOR 3
#define REALM_VER_PATCH 1
#define REALM_PRODUCT_NAME "realm-core"

#define REALM_VER_STRING REALM_QUOTE(REALM_VER_MAJOR) "." REALM_QUOTE(REALM_VER_MINOR) "." REALM_QUOTE(REALM_VER_PATCH)
#define REALM_VER_CHUNK "[" REALM_PRODUCT_NAME "-" REALM_VER_STRING "]"

namespace realm {

enum Feature {
    feature_Debug,
    feature_Replication
};

class Version {
public:
    static int get_major() { return REALM_VER_MAJOR; }
    static int get_minor() { return REALM_VER_MINOR; }
    static int get_patch() { return REALM_VER_PATCH; }
    static std::string get_version();
    static bool is_at_least(int major, int minor, int patch);
    static bool has_feature(Feature feature);
};


} // namespace realm

#endif // REALM_VERSION_HPP
