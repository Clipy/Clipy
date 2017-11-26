/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2017] Realm Inc
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

#ifndef REALM_SYNC_CHANGESET_PARSER_HPP
#define REALM_SYNC_CHANGESET_PARSER_HPP

#include <realm/sync/changeset.hpp>
#include <realm/impl/input_stream.hpp>

namespace realm {
namespace sync {

struct ChangesetParser {
    void parse(_impl::NoCopyInputStream&, InstructionHandler&);
private:
    struct State;
};

void parse_changeset(_impl::NoCopyInputStream&, Changeset& out_log);
void parse_changeset(_impl::InputStream&, Changeset& out_log);


} // namespace sync
} // namespace realm

#endif // REALM_SYNC_CHANGESET_PARSER_HPP