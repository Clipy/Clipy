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

#include <realm/sync/history.hpp>

#ifndef REALM_SYNC_CHANGESET_COOKER_HPP
#define REALM_SYNC_CHANGESET_COOKER_HPP

namespace realm {
namespace sync {

/// Copy raw changesets unmodified.
class TrivialChangesetCooker: public ClientReplication::ChangesetCooker {
public:
    bool cook_changeset(const Group&, const char* changeset,
                        std::size_t changeset_size,
                        util::AppendBuffer<char>&) override;
};

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_CHANGESET_COOKER_HPP
