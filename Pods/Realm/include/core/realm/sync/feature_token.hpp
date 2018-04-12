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

#include <realm/util/features.h>

#if !REALM_MOBILE && !defined(REALM_EXCLUDE_FEATURE_TOKENS)
#define REALM_HAVE_FEATURE_TOKENS 1
#else
#define REALM_HAVE_FEATURE_TOKENS 0
#endif

#if REALM_HAVE_FEATURE_TOKENS

#include <memory>

#include <realm/string_data.hpp>

namespace realm {
namespace sync {

class FeatureGate {
public:

    // The constructor takes a JWT token as argument.
    // The constructor throws a std::runtime_error if
    // the token is invalid. An invalid token is a token
    // that has bad syntax, is not signed by Realm, or is 
    // expired.
    FeatureGate(StringData token);

    // Constructs a feature gate without any features.
    FeatureGate();
    ~FeatureGate();

    FeatureGate(FeatureGate&&);
    FeatureGate& operator=(FeatureGate&&);

    bool has_feature(StringData feature_name);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};


} // namespace sync
} // namespace realm

#endif // REALM_HAVE_FEATURE_TOKENS
#endif // REALM_SYNC_FEATURE_TOKEN_HPP
