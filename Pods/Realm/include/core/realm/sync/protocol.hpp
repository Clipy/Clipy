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

#include <cstdint>
#include <system_error>

#include <realm/replication.hpp>



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
//
//  10 Error codes reordered (now categorized as either connection or session
//     level errors).
//
//  11 Bugfixes in Link List and ChangeLinkTargets merge rules, that
//     make previous versions incompatible.
//
//  12 FIXME What was 12?
//
//  13 Bugfixes in Link List and ChangeLinkTargets merge rules, that
//     make previous versions incompatible.
//
//  14 Further bugfixes related to primary keys and link lists. Add support for
//     LinkListSwap.
//
//  15 Deleting an object with a primary key deletes all objects on other
//     with the same primary key.
//
//  16 Downloadable bytes added to DOWNLOAD message. It is used for download progress
//     by the client
//
//  17 Added PING and PONG messages. It is used for rtt monitoring and dead
//     connection detection by both the client and the server.
//
//  18 Enhanced the session_ident to accept values of size up to at least 63 bits.
//
//  19 New instruction log format with stable object IDs and arrays of
//     primitives (Generalized LinkList* commands to Container* commands)
//     Message format is identical to version 18.
//
//  20 Added support for log compaction in DOWNLOAD message.
//
//  21 Removed "class_" prefix in instructions referencing tables.
//
//  22 Fixed a bug in the merge rule of MOVE vs SWAP.
//
//  23 Introduced full support for session specific ERROR messages. Removed the
//     obsolete concept of a "server file identifier". Added support for relayed
//     subtier client file identifier allocation. For this purpose, the message
//     that was formerly known as ALLOC was renamed to IDENT, and a new ALLOC
//     message was added in both directions. Added the ability for an UPLOAD
//     message to carry a per-changeset origin client file identifier. Added
//     `<upload server version>` parameter to DOWNLOAD message. Added new error
//     codes 215 "Unsupported session-level feature" and 216 "Bad origin client
//     file identifier (UPLOAD)".
//
//  24 Support schema-breaking instructions. Official support for partial sync.

constexpr int get_current_protocol_version() noexcept
{
    return 24;
}


// These integer types are selected so that they accomodate the requirements of
// the protocol specification (`/doc/protocol.md`).
using file_ident_type    = std::uint_fast64_t;
using version_type       = Replication::version_type;
using salt_type          = std::int_fast64_t;
using timestamp_type     = std::uint_fast64_t;
using session_ident_type = std::uint_fast64_t;
using request_ident_type = std::uint_fast64_t;


constexpr file_ident_type get_max_file_ident()
{
    return 0x0'7FFF'FFFF'FFFF'FFFF;
}


struct SaltedFileIdent {
    file_ident_type ident;
    /// History divergence and identity spoofing protection.
    salt_type salt;
};

struct SaltedVersion {
    version_type version;
    /// History divergence protection.
    salt_type salt;
};


/// \brief A client's reference to a position in the server-side history.
///
/// A download cursor refers to a position in the server-side history. If
/// `server_version` is zero, the position is at the beginning of the history,
/// otherwise the position is after the entry whose changeset produced that
/// version. In general, positions are to be understood as places between two
/// adjacent history entries.
///
/// `last_integrated_client_version` is the version produced on the client by
/// the last changeset that was sent to the server and integrated into the
/// server-side Realm state at the time indicated by the history position
/// specified by `server_version`, or zero if no changesets from the client were
/// integrated by the server at that point in time.
struct DownloadCursor {
    version_type server_version;
    version_type last_integrated_client_version;
};


/// \brief The server's reference to a position in the client-side history.
///
/// An upload cursor refers to a position in the client-side history. If
/// `client_version` is zero, the position is at the beginning of the history,
/// otherwise the position is after the entry whose changeset produced that
/// version. In general, positions are to be understood as places between two
/// adjacent history entries.
///
/// `last_integrated_server_version` is the version produced on the server by
/// the last changeset that was sent to the client and integrated into the
/// client-side Realm state at the time indicated by the history position
/// specified by `client_version`, or zero if no changesets from the server were
/// integrated by the client at that point in time.
struct UploadCursor {
    version_type client_version;
    version_type last_integrated_server_version;
};


/// A client's record of the current point of progress of the synchronization
/// process. The client must store this persistently in the local Realm file.
struct SyncProgress {
    SaltedVersion     latest_server_version{0, 0};
    DownloadCursor    download{0, 0};
    UploadCursor      upload{0, 0};
    std::int_fast64_t downloadable_bytes = 0;
};



/// \brief Protocol errors discovered by the server, and reported to the client
/// by way of ERROR messages.
///
/// These errors will be reported to the client-side application via the error
/// handlers of the affected sessions.
///
/// ATTENTION: Please remember to update is_session_level_error() when
/// adding/removing error codes.
enum class ProtocolError {
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
    bad_decompression            = 110, // Error in decompression (UPLOAD)
    bad_changeset_header_syntax  = 111, // Bad syntax in a changeset header (UPLOAD)
    bad_changeset_size           = 112, // Bad size specified in changeset header (UPLOAD)
    bad_changesets               = 113, // Bad changesets (UPLOAD)

    // Session level errors
    session_closed               = 200, // Session closed (no error)
    other_session_error          = 201, // Other session level error
    token_expired                = 202, // Access token expired
    bad_authentication           = 203, // Bad user authentication (BIND, REFRESH)
    illegal_realm_path           = 204, // Illegal Realm path (BIND)
    no_such_realm                = 205, // No such Realm (BIND)
    permission_denied            = 206, // Permission denied (BIND, REFRESH)
    bad_server_file_ident        = 207, // Bad server file identifier (IDENT) (obsolete!)
    bad_client_file_ident        = 208, // Bad client file identifier (IDENT)
    bad_server_version           = 209, // Bad server version (IDENT, UPLOAD)
    bad_client_version           = 210, // Bad client version (IDENT, UPLOAD)
    diverging_histories          = 211, // Diverging histories (IDENT)
    bad_changeset                = 212, // Bad changeset (UPLOAD)
    superseded                   = 213, // Superseded by new session for same client-side file
    disabled_session             = 213, // Alias for `superseded` (deprecated)
    partial_sync_disabled        = 214, // Partial sync disabled (BIND)
    unsupported_session_feature  = 215, // Unsupported session-level feature
    bad_origin_file_ident        = 216, // Bad origin file identifier (UPLOAD)
};

inline constexpr bool is_session_level_error(ProtocolError error)
{
    return int(error) >= 200 && int(error) <= 299;
}

/// Returns null if the specified protocol error code is not defined by
/// ProtocolError.
const char* get_protocol_error_message(int error_code) noexcept;

const std::error_category& protocol_error_category() noexcept;

std::error_code make_error_code(ProtocolError) noexcept;

} // namespace sync
} // namespace realm

namespace std {

template<> struct is_error_code_enum<realm::sync::ProtocolError> {
    static const bool value = true;
};

} // namespace std

namespace realm {
namespace sync {

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_PROTOCOL_HPP
