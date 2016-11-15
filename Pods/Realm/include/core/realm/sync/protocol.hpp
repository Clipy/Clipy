/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2016] Realm Inc
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
#ifndef REALM_SYNC_PROTOCOL_HPP
#define REALM_SYNC_PROTOCOL_HPP


// NOTE: The protocol specification is in `/doc/protocol.md`


namespace realm {
namespace sync {

// Protocol versions:
//
//   1 Initial version.
//
//   2 Introduces the UNBOUND message (sent from server to client in
//     response to a BIND message).
//
//   3 Introduces the ERROR message (sent from server to client before the
//     server closes a connection). Introduces MARK message from client to
//     server, and MARK response message from server to client as a way for the
//     client to wait for download to complete.
//
//   4 User token and signature are now passed as a single string (see
//     /doc/protocol.md for details). Also, `application_ident` parameter
//     removed from IDENT message.
//
//   5 IDENT message renamed to CLIENT, and ALLOC message (client->server)
//     renamed to IDENT. Also, <client info> parameter added to CLIENT
//     message. Also, the protocol has been changed to make the clients
//     acquisition of a server allocated file identifier pair be part of a
//     session from the servers point of view. File identifier and version
//     parameters moved from the BIND message to a new IDENT message sent by
//     client when it has obtained the file identifier pair. Both the new IDENT
//     message and the ALLOC message sent by the server are now properly
//     associated with a session.
//
//   6 Server session IDs have been added to the IDENT, DOWNLOAD, and PROGRESS
//     messages, and the "Divergent history" error code was added as an
//     indication that a server version / session ID pair does not match the
//     server's history.
//
//   7 FIXME: Who introduced version 7? Please describe what changed.
//
//   8 Error code (`bad_authentication`) moved from 200-range to 300-range
//     because it is now session specific. Other error codes were renumbered.
//
//   9 New format of the DOWNLOAD message to support progress reporting on the
//     client
//  10 Error codes reordered (now categorized as either connection or session
//     level errors).
//  11 Bugfixes in Link List and ChangeLinkTargets merge rules, that
//    make previous versions incompatible.
//  12 FIXME What was 12?
//  13 Bugfixes in Link List and ChangeLinkTargets merge rules, that
//     make previous versions incompatible.
//  14 Further bugfixes related to primary keys and link lists. Add support for
//     LinkListSwap.
//  15 Deleting an object with a primary key deletes all objects on other
//     with the same primary key.
constexpr int get_current_protocol_version() noexcept
{
    return 15;
}

// Reserve 0 for compatibility with std::error_condition.
//
// ATTENTION: Please remember to update is_session_level_error() and
// is_connection_level_error() definitions when adding/removing error codes.
//
enum class Error {
    invalid_error                =  99, // Server sent an invalid error code (ERROR)

    // Connection level and protocol errors
    connection_closed            = 100, // Connection closed (no error)
    other_error                  = 101, // Other connection level error
    unknown_message              = 102, // Unknown type of input message
    bad_syntax                   = 103, // Bad syntax in input message head
    limits_exceeded              = 104, // Limits exceeded in input message
    wrong_protocol_version       = 105, // Wrong protocol version (CLIENT)
    bad_session_ident            = 106, // Bad session identifier in input message
    reuse_of_session_ident       = 107, // Overlapping reuse of session identifier (BIND)
    bound_in_other_session       = 108, // Client file bound in other session (IDENT)
    bad_message_order            = 109, // Bad input message order

    // Session level errors
    session_closed               = 200, // Session closed (no error)
    other_session_error          = 201, // Other session level error
    token_expired                = 202, // Access token expired
    bad_authentication           = 203, // Bad user authentication (BIND, REFRESH)
    illegal_realm_path           = 204, // Illegal Realm path (BIND)
    no_such_realm                = 205, // No such Realm (BIND)
    permission_denied            = 206, // Permission denied (BIND, REFRESH)
    bad_server_file_ident        = 207, // Bad server file identifier (IDENT)
    bad_client_file_ident        = 208, // Bad client file identifier (IDENT)
    bad_server_version           = 209, // Bad server version (IDENT, UPLOAD)
    bad_client_version           = 210, // Bad client version (IDENT, UPLOAD)
    diverging_histories          = 211, // Diverging histories (IDENT)
    bad_changeset                = 212, // Bad changeset (UPLOAD)
    disabled_session             = 213, // Disabled session
};

inline constexpr bool is_session_level_error(Error error)
{
    return int(error) >= 200 && int(error) <= 213;
}

inline constexpr bool is_connection_level_error(Error error)
{
    return int(error) >= 100 && int(error) <= 109;
}

const char* get_error_message(Error error_code);

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_PROTOCOL_HPP
