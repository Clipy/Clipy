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
#ifndef REALM_SYNC_FEATURE_TOKEN_HPP
#define REALM_SYNC_FEATURE_TOKEN_HPP

#include <realm/string_data.hpp>
#include <realm/util/features.h>

namespace realm {
namespace sync {

#if !REALM_MOBILE
#define REALM_HAVE_FEATURE_TOKENS 1

void set_feature_token(StringData token);

bool is_feature_enabled(StringData feature_name);
#else
#define REALM_HAVE_FEATURE_TOKENS 0
#endif

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_FEATURE_TOKEN_HPP