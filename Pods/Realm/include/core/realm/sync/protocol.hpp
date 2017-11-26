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

#include <system_error>

#include <realm/util/buffer_stream.hpp>
#include <realm/util/compression.hpp>
#include <realm/util/logger.hpp>
#include <realm/util/memory_stream.hpp>
#include <realm/util/hex_dump.hpp>
#include <realm/util/optional.hpp>

#include <realm/sync/transform.hpp>
#include <realm/sync/history.hpp>

#include <realm/sync/client.hpp> // Get rid of this?


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

constexpr int get_current_protocol_version() noexcept
{
    return 22;
}

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
    disabled_session             = 213, // Disabled session
    partial_sync_disabled        = 214, // Partial sync disabled (BIND)
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
namespace protocol {

using OutputBuffer = util::ResettableExpandableBufferOutputStream;
using session_ident_type = uint_fast64_t;
using file_ident_type = uint_fast64_t;
using version_type = uint_fast64_t;
using timestamp_type  = uint_fast64_t;
using request_ident_type    = uint_fast64_t;


class ClientProtocol {
public:
    util::Logger& logger;

    enum class Error {
        unknown_message             = 101, // Unknown type of input message
        bad_syntax                  = 102, // Bad syntax in input message head
        limits_exceeded             = 103, // Limits exceeded in input message
        bad_changeset_header_syntax = 108, // Bad syntax in changeset header (DOWNLOAD)
        bad_changeset_size          = 109, // Bad changeset size in changeset header (DOWNLOAD)
        bad_server_version          = 111, // Bad server version in changeset header (DOWNLOAD)
        bad_error_code              = 114, ///< Bad error code (ERROR)
        bad_decompression           = 115, // Error in decompression (DOWNLOAD)
    };

    ClientProtocol(util::Logger& logger);


    /// Messages sent by the client.

    void make_client_message(OutputBuffer& out, const std::string& client_info);

    void make_bind_message(OutputBuffer& out, session_ident_type session_ident,
                           const std::string& server_path,
                           const std::string& signed_user_token,
                           bool need_file_ident_pair);

    void make_refresh_message(OutputBuffer& out, session_ident_type session_ident,
                              const std::string& signed_user_token);

    void make_ident_message(OutputBuffer& out, session_ident_type session_ident,
                            file_ident_type server_file_ident,
                            file_ident_type client_file_ident,
                            int_fast64_t client_file_ident_secret,
                            SyncProgress progress);

    class UploadMessageBuilder {
    public:
        util::Logger& logger;

        UploadMessageBuilder(util::Logger& logger,
                             OutputBuffer& body_buffer,
                             std::vector<char>& compression_buffer,
                             util::compression::CompressMemoryArena& compress_memory_arena);

        void add_changeset(version_type client_version, version_type server_version,
                           timestamp_type timestamp, BinaryData changeset);

        void make_upload_message(OutputBuffer& out, session_ident_type session_ident);

    private:
        size_t m_num_changesets = 0;
        OutputBuffer& m_body_buffer;
        std::vector<char>& m_compression_buffer;
        util::compression::CompressMemoryArena& m_compress_memory_arena;
    };

    UploadMessageBuilder make_upload_message_builder();

    void make_upload_message(OutputBuffer& out, session_ident_type session_ident,
                             version_type client_version, version_type server_version,
                             size_t changeset_size, timestamp_type timestamp,
                             const std::unique_ptr<char[]>& body_buffer);

    void make_unbind_message(OutputBuffer& out, session_ident_type session_ident);

    void make_mark_message(OutputBuffer& out, session_ident_type session_ident,
                           request_ident_type request_ident);

    void make_ping(OutputBuffer& out, uint_fast64_t timestamp, uint_fast64_t rtt);


    // Messages received by the client.

    // parse_pong_received takes a (WebSocket) pong and parses it.
    // The result of the parsing is handled by an object of type Connection.
    // Typically, Connection would be the Connection class from client.cpp
    template <typename Connection>
    void parse_pong_received(Connection& connection, const char* data, size_t size)
    {
        util::MemoryInputStream in;
        in.set_buffer(data, data + size);
        in.unsetf(std::ios_base::skipws);

        uint_fast64_t timestamp;

        char newline;
        in >> timestamp >> newline;
        bool good_syntax = in && size_t(in.tellg()) == size && newline == '\n';
        if (!good_syntax)
            goto bad_syntax;

        connection.receive_pong(timestamp);
        return;

    bad_syntax:
        logger.error("Bad syntax in input message '%1'",
                     StringData(data, size));
        connection.handle_protocol_error(Error::bad_syntax); // Throws
        return;
    }

    // parse_message_received takes a (WebSocket) message and parses it.
    // The result of the parsing is handled by an object of type Connection.
    // Typically, Connection would be the Connection class from client.cpp
    template <typename Connection>
    void parse_message_received(Connection& connection, const char* data, size_t size)
    {
        util::MemoryInputStream in;
        in.set_buffer(data, data + size);
        in.unsetf(std::ios_base::skipws);
        size_t header_size = 0;
        std::string message_type;
        in >> message_type;

        if (message_type == "download") {
            session_ident_type session_ident;
            SyncProgress progress;
            int is_body_compressed;
            size_t uncompressed_body_size, compressed_body_size;
            char sp_1, sp_2, sp_3, sp_4, sp_5, sp_6, sp_7, sp_8, sp_9, sp_10, newline;

            in >> sp_1 >> session_ident >> sp_2 >> progress.scan_server_version >> sp_3 >>
                progress.scan_client_version >> sp_4 >> progress.latest_server_version >>
                sp_5 >> progress.latest_server_session_ident >> sp_6 >>
                progress.latest_client_version >> sp_7 >> progress.downloadable_bytes >>
                sp_8 >> is_body_compressed >> sp_9 >> uncompressed_body_size >> sp_10 >>
                compressed_body_size >> newline; // Throws

            bool good_syntax = in && sp_1 == ' ' && sp_2 == ' ' &&
                sp_3 == ' ' && sp_4 == ' ' && sp_5 == ' ' && sp_6 == ' ' &&
                sp_7 == ' ' && sp_8 == ' ' && sp_9 == ' ' && sp_10 == ' ' &&
                newline == '\n';
            if (!good_syntax)
                goto bad_syntax;
            header_size = size_t(in.tellg());
            if (uncompressed_body_size > s_max_body_size)
                goto limits_exceeded;

            size_t body_size = is_body_compressed ? compressed_body_size : uncompressed_body_size;
            if (header_size + body_size != size)
                goto bad_syntax;

            BinaryData body(data + header_size, body_size);
            BinaryData uncompressed_body;

            std::unique_ptr<char[]> uncompressed_body_buffer;
            // if is_body_compressed == true, we must decompress the received body.
            if (is_body_compressed) {
                uncompressed_body_buffer.reset(new char[uncompressed_body_size]);
                std::error_code ec = util::compression::decompress(body.data(),  compressed_body_size,
                                                                   uncompressed_body_buffer.get(),
                                                                   uncompressed_body_size);

                if (ec) {
                    logger.error("compression::inflate: %1", ec.message());
                    connection.handle_protocol_error(Error::bad_decompression);
                    return;
                }

                uncompressed_body = BinaryData(uncompressed_body_buffer.get(), uncompressed_body_size);
            }
            else {
                uncompressed_body = body;
            }

            logger.debug("Download message compression: is_body_compressed = %1, "
                         "compressed_body_size=%2, uncompressed_body_size=%3",
                         is_body_compressed, compressed_body_size, uncompressed_body_size);

            util::MemoryInputStream in;
            in.unsetf(std::ios_base::skipws);
            in.set_buffer(uncompressed_body.data(), uncompressed_body.data() + uncompressed_body_size);

            std::vector<Transformer::RemoteChangeset> received_changesets;

            // Loop through the body and find the changesets.
            size_t position = 0;
            while (position < uncompressed_body_size) {
                version_type server_version;
                version_type client_version;
                timestamp_type origin_timestamp;
                file_ident_type origin_client_file_ident;
                size_t original_changeset_size, changeset_size;
                char sp_1, sp_2, sp_3, sp_4, sp_5, sp_6;

                in >> server_version >> sp_1 >> client_version >> sp_2 >> origin_timestamp >>
                    sp_3 >> origin_client_file_ident >> sp_4 >> original_changeset_size >>
                    sp_5 >> changeset_size >> sp_6;

                bool good_syntax = in && sp_1 == ' ' && sp_2 == ' ' &&
                    sp_3 == ' ' && sp_4 == ' ' && sp_5 == ' ' && sp_6 == ' ';

                if (!good_syntax) {
                    logger.error("Bad changeset header syntax");
                    connection.handle_protocol_error(Error::bad_changeset_header_syntax);
                    return;
                }

                // Update position to the end of the change set
                position = size_t(in.tellg()) + changeset_size;

                if (position > uncompressed_body_size) {
                    logger.error("Bad changeset size");
                    connection.handle_protocol_error(Error::bad_changeset_size);
                    return;
                }

                if (server_version == 0) {
                    // The received changeset can never have version 0.
                    logger.error("Bad server version");
                    connection.handle_protocol_error(Error::bad_server_version);
                    return;
                }

                BinaryData changeset_data(uncompressed_body.data() + size_t(in.tellg()), changeset_size);
                in.seekg(position);

                if (logger.would_log(util::Logger::Level::trace)) {
                    logger.trace("Received: DOWNLOAD CHANGESET(server_version=%1, client_version=%2, "
                                  "origin_timestamp=%3, origin_client_file_ident=%4, original_changeset_size=%5, changeset_size=%6)",
                                  server_version, client_version, origin_timestamp,
                                  origin_client_file_ident, original_changeset_size, changeset_size); // Throws
                    logger.trace("Changeset: %1", util::hex_dump(changeset_data.data(), changeset_size)); // Throws
                }

                Transformer::RemoteChangeset changeset_2(server_version, client_version,
                                                         changeset_data, origin_timestamp,
                                                         origin_client_file_ident);
                changeset_2.original_changeset_size = original_changeset_size;
                received_changesets.push_back(changeset_2); // Throws
            }

            connection.receive_download_message(session_ident, progress, received_changesets); // Throws
            return;
        }
        if (message_type == "unbound") {
            session_ident_type session_ident;
            char sp_1, newline;
            in >> sp_1 >> session_ident >> newline; // Throws
            bool good_syntax = in && size_t(in.tellg()) == size && sp_1 == ' ' &&
                newline == '\n';
            if (!good_syntax)
                goto bad_syntax;
            header_size = size_t(in.tellg());

            connection.receive_unbound_message(session_ident); // Throws
            return;
        }
        if (message_type == "error") {
            int error_code;
            size_t message_size;
            bool try_again;
            session_ident_type session_ident;
            char sp_1, sp_2, sp_3, sp_4, newline;
            in >> sp_1 >> error_code >> sp_2 >> message_size >> sp_3 >> try_again >> sp_4 >>
                session_ident >> newline; // Throws
            bool good_syntax = in && sp_1 == ' ' && sp_2 == ' ' && sp_3 == ' ' &&
                sp_4 == ' ' && newline == '\n';
            if (!good_syntax)
                goto bad_syntax;
            header_size = size_t(in.tellg());
            if (header_size + message_size != size)
                goto bad_syntax;

            bool unknown_error = !get_protocol_error_message(error_code);
            if (unknown_error) {
                logger.error("Bad error code"); // Throws
                connection.handle_protocol_error(Error::bad_error_code);
                return;
            }

            std::string message{data + header_size, message_size}; // Throws (copy)

            connection.receive_error_message(error_code, message_size, try_again, session_ident, message); // Throws
            return;
        }
        if (message_type == "mark") {
            session_ident_type session_ident;
            request_ident_type request_ident;
            char sp_1, sp_2, newline;
            in >> sp_1 >> session_ident >> sp_2 >> request_ident >> newline; // Throws
            bool good_syntax = in && size_t(in.tellg()) == size && sp_1 == ' ' &&
                sp_2 == ' ' && newline == '\n';
            if (!good_syntax)
                goto bad_syntax;
            header_size = size_t(in.tellg());

            connection.receive_mark_message(session_ident, request_ident); // Throws
            return;
        }
        if (message_type == "alloc") {
            session_ident_type session_ident;
            file_ident_type server_file_ident, client_file_ident;
            int_fast64_t client_file_ident_secret;
            char sp_1, sp_2, sp_3, sp_4, newline;
            in >> sp_1 >> session_ident >> sp_2 >> server_file_ident >> sp_3 >>
                client_file_ident >> sp_4 >> client_file_ident_secret >> newline; // Throws
            bool good_syntax = in && size_t(in.tellg()) == size && sp_1 == ' ' &&
                sp_2 == ' ' && sp_3 == ' ' && sp_4 == ' ' && newline == '\n';
            if (!good_syntax)
                goto bad_syntax;
            header_size = size_t(in.tellg());

            connection.receive_alloc_message(session_ident,server_file_ident, client_file_ident,
                                             client_file_ident_secret); // Throws
            return;
        }

        logger.error("Unknown input message type '%1'",
                     StringData(data, size));
        connection.handle_protocol_error(Error::unknown_message);
        return;
    bad_syntax:
        logger.error("Bad syntax in input message '%1'",
                     StringData(data, size));
        connection.handle_protocol_error(Error::bad_syntax);
        return;
    limits_exceeded:
        logger.error("Limits exceeded in input message '%1'",
                     StringData(data, header_size));
        connection.handle_protocol_error(Error::limits_exceeded);
        return;
    }

private:
    static constexpr size_t s_max_body_size = std::numeric_limits<size_t>::max();

    // Permanent buffer to use for building messages.
    OutputBuffer m_output_buffer;

    // Permanent buffers to use for internal purposes such as compression.
    std::vector<char> m_buffer;

    util::compression::CompressMemoryArena m_compress_memory_arena;
};


class ServerProtocol {
public:
    util::Logger& logger;

    enum class Error {
        unknown_message             = 101, // Unknown type of input message
        bad_syntax                  = 102, // Bad syntax in input message head
        limits_exceeded             = 103, // Limits exceeded in input message
        bad_decompression           = 104, // Error in decompression (UPLOAD)
        bad_changeset_header_syntax = 105, // Bad syntax in changeset header (UPLOAD)
        bad_changeset_size          = 106, // Changeset size doesn't fit in message (UPLOAD)
    };

    ServerProtocol(util::Logger& logger);

    // Messages sent by the server to the client

    void make_alloc_message(OutputBuffer& out, session_ident_type session_ident,
                            file_ident_type server_file_ident,
                            file_ident_type client_file_ident,
                            std::int_fast64_t client_file_ident_secret);

    void make_unbound_message(OutputBuffer& out, session_ident_type session_ident);


    struct ChangesetInfo {
        version_type server_version;
        version_type client_version;
        HistoryEntry entry;
        size_t original_size;
    };

    void make_download_message(int protocol_version, OutputBuffer& out, session_ident_type session_ident,
                               version_type scan_server_version,
                               version_type scan_client_version,
                               version_type latest_server_version,
                               int_fast64_t latest_server_session_ident,
                               version_type latest_client_version,
                               uint_fast64_t downloadable_bytes,
                               std::size_t num_changesets, BinaryData body);

    void make_error_message(OutputBuffer& out, ProtocolError error_code,
                            const char* message, size_t message_size,
                            bool try_again, session_ident_type session_ident);

    void make_mark_message(OutputBuffer& out, session_ident_type session_ident,
                           request_ident_type request_ident);

    void make_pong(OutputBuffer& out, uint_fast64_t timestamp);

    // Messages received by the server.

    // parse_ping_received takes a (WebSocket) ping and parses it.
    // The result of the parsing is handled by an object of type Connection.
    // Typically, Connection would be the Connection class from server.cpp
    template <typename Connection>
    void parse_ping_received(Connection& connection, const char* data, size_t size)
    {
        util::MemoryInputStream in;
        in.set_buffer(data, data + size);
        in.unsetf(std::ios_base::skipws);

        int_fast64_t timestamp, rtt;

        char sp_1, newline;
        in >> timestamp >> sp_1 >> rtt >> newline;
        bool good_syntax = in && size_t(in.tellg()) == size && sp_1 == ' ' &&
            newline == '\n';
        if (!good_syntax)
            goto bad_syntax;

        connection.receive_ping(timestamp, rtt);
        return;

    bad_syntax:
        logger.error("Bad syntax in PING message '%1'",
                     StringData(data, size));
        connection.handle_protocol_error(Error::bad_syntax);
        return;
    }

    // UploadChangeset is used to store received changesets in
    // the UPLOAD message.
    struct UploadChangeset {
        version_type client_version;
        version_type server_version;
        timestamp_type timestamp;
        BinaryData changeset;
    };

    // parse_message_received takes a (WebSocket) message and parses it.
    // The result of the parsing is handled by an object of type Connection.
    // Typically, Connection would be the Connection class from server.cpp
    template <typename Connection>
    void parse_message_received(Connection& connection, const char* data, size_t size)
    {
        util::MemoryInputStream in;
        in.set_buffer(data, data + size);
        in.unsetf(std::ios_base::skipws);
        size_t header_size = 0;
        std::string message_type;
        in >> message_type;

        if (message_type == "upload") {
            session_ident_type session_ident;
            int is_body_compressed;
            size_t uncompressed_body_size, compressed_body_size;
            char sp_1, sp_2, sp_3, sp_4, newline;
            in >> sp_1 >> session_ident >> sp_2 >> is_body_compressed >> sp_3 >>
                uncompressed_body_size >> sp_4 >> compressed_body_size >>
                newline;
            bool good_syntax = in && sp_1 == ' ' && sp_2 == ' ' && sp_3 == ' ' &&
                sp_4 == ' ' && newline == '\n';

            if (!good_syntax)
                goto bad_syntax;
            header_size = size_t(in.tellg());
            if (uncompressed_body_size > s_max_body_size)
                goto limits_exceeded;

            size_t body_size = is_body_compressed ? compressed_body_size : uncompressed_body_size;
            if (header_size + body_size != size)
                goto bad_syntax;

            BinaryData body(data + header_size, body_size);

            BinaryData uncompressed_body;
            std::unique_ptr<char[]> uncompressed_body_buffer;
            // if is_body_compressed == true, we must decompress the received body.
            if (is_body_compressed) {
                uncompressed_body_buffer.reset(new char[uncompressed_body_size]);
                std::error_code ec = util::compression::decompress(body.data(),  compressed_body_size,
                                                                   uncompressed_body_buffer.get(),
                                                                   uncompressed_body_size);

                if (ec) {
                    logger.error("compression::inflate: %1", ec.message());
                    connection.handle_protocol_error(Error::bad_decompression);
                    return;
                }

                uncompressed_body = BinaryData(uncompressed_body_buffer.get(), uncompressed_body_size);
            }
            else {
                uncompressed_body = body;
            }

            logger.debug("Upload message compression: is_body_compressed = %1, "
                         "compressed_body_size=%2, uncompressed_body_size=%3",
                         is_body_compressed, compressed_body_size, uncompressed_body_size);

            util::MemoryInputStream in;
            in.unsetf(std::ios_base::skipws);
            in.set_buffer(uncompressed_body.data(), uncompressed_body.data() + uncompressed_body_size);

            std::vector<UploadChangeset> upload_changesets;

            // Loop through the body and find the changesets.
            size_t position = 0;
            while (position < uncompressed_body_size) {
                version_type client_version;
                version_type server_version;
                timestamp_type timestamp;
                size_t changeset_size;
                char sp_1, sp_2, sp_3, sp_4;

                in >> client_version >> sp_1 >> server_version >> sp_2 >> timestamp >>
                    sp_3 >> changeset_size >> sp_4;

                bool good_syntax = in && sp_1 == ' ' && sp_2 == ' ' &&
                    sp_3 == ' ' && sp_4 == ' ';

                if (!good_syntax) {
                    logger.error("Bad changeset header syntax");
                    connection.handle_protocol_error(Error::bad_changeset_header_syntax);
                    return;
                }

                // Update position to the end of the change set
                position = size_t(in.tellg()) + changeset_size;

                if (position > uncompressed_body_size) {
                    logger.error("Bad changeset size");
                    connection.handle_protocol_error(Error::bad_changeset_size);
                    return;
                }

                BinaryData changeset_data(uncompressed_body.data() +
                                          size_t(in.tellg()), changeset_size);
                in.seekg(position);

                if (logger.would_log(util::Logger::Level::trace)) {
                    logger.trace(
                        "Received: UPLOAD CHANGESET(client_version=%1, "
                        "server_version=%2, timestamp=%3, changeset_size=%4)",
                        client_version, server_version, timestamp,
                        changeset_size); // Throws
                    logger.trace("Changeset: %1",
                                 util::hex_dump(changeset_data.data(),
                                                changeset_size)); // Throws
                }

                UploadChangeset upload_changeset {
                    client_version,
                    server_version,
                    timestamp,
                    changeset_data
                };

                upload_changesets.push_back(upload_changeset); // Throws
            }

            connection.receive_upload_message(session_ident,upload_changesets); // Throws
            return;
        }
        if (message_type == "mark") {
            session_ident_type session_ident;
            request_ident_type request_ident;
            char sp_1, sp_2, newline;
            in >> sp_1 >> session_ident >> sp_2 >> request_ident >> newline;
            bool good_syntax = in && size_t(in.tellg()) == size &&
                sp_1 == ' ' && sp_2 == ' ' && newline == '\n';
            if (!good_syntax)
                goto bad_syntax;
            header_size = size;

            connection.receive_mark_message(session_ident, request_ident); // Throws
            return;
        }
        if (message_type == "bind") {
            session_ident_type session_ident;
            size_t path_size;
            size_t signed_user_token_size;
            bool need_file_ident_pair;
            char sp_1, sp_2, sp_3, sp_4, newline;
            in >> sp_1 >> session_ident >> sp_2 >> path_size >> sp_3 >>
                signed_user_token_size >> sp_4 >> need_file_ident_pair >>
                newline;
            bool good_syntax = in && sp_1 == ' ' && sp_2 == ' ' &&
                sp_3 == ' ' && sp_4 == ' ' && newline == '\n';
            if (!good_syntax)
                goto bad_syntax;
            header_size = size_t(in.tellg());
            if (path_size == 0)
                goto bad_syntax;
            if (path_size > s_max_path_size)
                goto limits_exceeded;
            if (signed_user_token_size > s_max_signed_user_token_size)
                goto limits_exceeded;
            if (header_size + path_size + signed_user_token_size != size)
                goto bad_syntax;

            std::string path {data + header_size, path_size}; // Throws
            std::string signed_user_token {data + header_size + path_size,
                signed_user_token_size}; // Throws

            connection.receive_bind_message(session_ident, std::move(path),
                                            std::move(signed_user_token),
                                            need_file_ident_pair); // Throws
            return;
        }
        if (message_type == "refresh") {
            session_ident_type session_ident;
            size_t signed_user_token_size;
            char sp_1, sp_2, newline;
            in >> sp_1 >> session_ident >> sp_2 >> signed_user_token_size >>
                newline;
            bool good_syntax = in && sp_1 == ' ' && sp_2 == ' ' && newline == '\n';
            if (!good_syntax)
                goto bad_syntax;
            header_size = size_t(in.tellg());
            if (signed_user_token_size > s_max_signed_user_token_size)
                goto limits_exceeded;
            if (header_size + signed_user_token_size != size)
                goto bad_syntax;

            std::string signed_user_token {data + header_size, signed_user_token_size};

            connection.receive_refresh_message(session_ident, std::move(signed_user_token)); // Throws
            return;
        }
        if (message_type == "ident") {
            session_ident_type session_ident;
            file_ident_type server_file_ident, client_file_ident;
            int_fast64_t client_file_ident_secret;
            version_type scan_server_version, scan_client_version, latest_server_version;
            int_fast64_t latest_server_session_ident;
            char sp_1, sp_2, sp_3, sp_4, sp_5, sp_6, sp_7, sp_8, newline;
            in >> sp_1 >> session_ident >> sp_2 >> server_file_ident >> sp_3 >>
                client_file_ident >> sp_4 >> client_file_ident_secret >> sp_5 >>
                scan_server_version >> sp_6 >> scan_client_version >> sp_7 >>
                latest_server_version >> sp_8 >> latest_server_session_ident >>
                newline;
            bool good_syntax = in && size_t(in.tellg()) == size && sp_1 == ' ' &&
                sp_2 == ' ' && sp_3 == ' ' && sp_4 == ' ' && sp_5 == ' ' &&
                sp_6 == ' ' && sp_7 == ' ' && sp_8 == ' ' && newline == '\n';
            if (!good_syntax)
                goto bad_syntax;
            header_size = size;

            connection.receive_ident_message(session_ident, server_file_ident, client_file_ident,
                                             client_file_ident_secret, scan_server_version,
                                             scan_client_version, latest_server_version,
                                             latest_server_session_ident); // Throws
            return;
        }
        if (message_type == "unbind") {
            session_ident_type session_ident;
            char sp_1, newline;
            in >> sp_1 >> session_ident >> newline;
            bool good_syntax = in && size_t(in.tellg()) == size &&
                sp_1 == ' ' && newline == '\n';
            if (!good_syntax)
                goto bad_syntax;
            header_size = size;

            connection.receive_unbind_message(session_ident); // Throws
            return;
        }
        if (message_type == "client") {
            int_fast64_t protocol_version;
            char sp_1, sp_2, newline;
            size_t client_info_size;
            in >> sp_1 >> protocol_version >> sp_2 >> client_info_size >> newline;
            bool good_syntax = in && sp_1 == ' ' && sp_2 == ' ' && newline == '\n';
            if (!good_syntax)
                goto bad_syntax;
            header_size = size_t(in.tellg());
            bool limits_exceeded = (client_info_size > s_max_client_info_size);
            if (limits_exceeded)
                goto limits_exceeded;
            if (header_size + client_info_size != size)
                goto bad_syntax;

            std::string client_info {data + header_size, client_info_size}; // Throws

            connection.receive_client_message(protocol_version, std::move(client_info)); // Throws
            return;
        }

        // unknown message
        if (size < 256)
            logger.error("Unknown input message type '%1'", StringData(data, size)); // Throws
        else
            logger.error("Unknown input message type '%1'.......", StringData(data, 256)); // Throws

        connection.handle_protocol_error(Error::unknown_message);
        return;

    bad_syntax:
        logger.error("Bad syntax in input message '%1'",
                     StringData(data, size));
        connection.handle_protocol_error(Error::bad_syntax); // Throws
        return;
    limits_exceeded:
        logger.error("Limits exceeded in input message '%1'",
                     StringData(data, header_size));
        connection.handle_protocol_error(Error::limits_exceeded); // Throws
        return;
    }

    void insert_single_changeset_download_message(OutputBuffer& out, const ChangesetInfo& changeset_info);

private:
    static constexpr size_t s_max_head_size              =  256;
    static constexpr size_t s_max_signed_user_token_size = 2048;
    static constexpr size_t s_max_client_info_size       = 1024;
    static constexpr size_t s_max_path_size              = 1024;
    static constexpr size_t s_max_body_size = std::numeric_limits<size_t>::max();

    util::compression::CompressMemoryArena m_compress_memory_arena;

    // Permanent buffer to use for internal purposes such as compression.
    std::vector<char> m_buffer;

    // Outputbuffer to use for internal purposes such as creating the
    // download body.
    OutputBuffer m_output_buffer;
};

// make_authorization_header() makes the value of the Authorization header used in the
// sync Websocket handshake.
std::string make_authorization_header(const std::string& signed_user_token);

// parse_authorization_header() parses the value of the Authorization header and returns
// the signed_user_token. None is returned in case of syntax error.
util::Optional<StringData> parse_authorization_header(const std::string& authorization_header);

} // namespace protocol
} // namespace sync
} // namespace realm

#endif // REALM_SYNC_PROTOCOL_HPP
