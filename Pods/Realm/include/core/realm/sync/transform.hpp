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

#ifndef REALM_SYNC_TRANSFORM_HPP
#define REALM_SYNC_TRANSFORM_HPP

#include <stddef.h>

#include <realm/util/buffer.hpp>
#include <realm/impl/continuous_transactions_history.hpp>
#include <realm/group_shared.hpp>

namespace realm {
namespace sync {

/// Represents an entry in the history of changes in a sync-enabled Realm
/// file. Server and client use different history formats, but this class is
/// used both on the server and the client side. Each history entry corresponds
/// to a version of the Realm state. For server and client-side histories, these
/// versions are referred to as *server versions* and *client versions*
/// respectively. These versions may, or may not correspond to Realm snapshot
/// numbers (on the server-side they currently do not).
///
/// FIXME: Move this class into a separate header
/// (`<realm/sync/history_entry.hpp>`).
class HistoryEntry {
public:
    using timestamp_type  = uint_fast64_t;
    using file_ident_type = uint_fast64_t;
    using version_type    = _impl::History::version_type;

    /// The time of origination of the changes referenced by this history entry,
    /// meassured as the number of milliseconds since 2015-01-01T00:00:00Z, not
    /// including leap seconds. For changes of local origin, this is the local
    /// time at the point when the local transaction was committed. For changes
    /// of remote origin, it is the remote time of origin at the client
    /// identified by `origin_client_file_ident`.
    timestamp_type origin_timestamp;

    /// For changes of local origin, this is the identifier of the local
    /// file. On the client side, the special value, zero, is used as a stand-in
    /// for the actual file identifier. This is necessary because changes may
    /// occur on the client-side before it obtains the the actual identifier
    /// from the server. Depending on context, the special value, zero, will, or
    /// will not have been replaced by the actual local file identifier.
    ///
    /// For changes of remote origin, this is the identifier of the file in the
    /// context of which this change originated. This may be a client, or a
    /// server-side file. For example, when a change "travels" from client file
    /// A with identifier 2, through the server, to client file B with
    /// identifier 3, then `origin_client_file_ident` will be 2 on the server
    /// and in client file A. On the other hand, if the change originates on the
    /// server, and the server-side file identifier is 1, then
    /// `origin_client_file_ident` will be 1 in both client files.
    ///
    /// FIXME: Rename this member to `origin_file_ident`. It is no longer
    /// necessarily a client-side file.
    file_ident_type origin_client_file_ident;

    /// For changes of local origin on the client side, this is the last server
    /// version integrated locally prior to this history entry. In other words,
    /// it is a copy of `remote_version` of the last preceding history entry
    /// that carries changes of remote origin, or zero if there is no such
    /// preceding history entry.
    ///
    /// For changes of local origin on the server-side, this is always zero.
    ///
    /// For changes of remote origin, this is the version produced within the
    /// remote-side Realm file by the change that gave rise to this history
    /// entry. The remote-side Realm file is not always the same Realm file from
    /// which the change originated. On the client side, the remote side is
    /// always the server side, and `remote_version` is always a server version
    /// (since clients do not speak directly to each other). On the server side,
    /// the remote side is always a client side, and `remote_version` is always
    /// a client version.
    version_type remote_version;

    /// Referenced memory is not owned by this class.
    BinaryData changeset;
};


/// The interface between the sync history and the operational transformer
/// (Transformer).
class TransformHistory {
public:
    using timestamp_type  = HistoryEntry::timestamp_type;
    using file_ident_type = HistoryEntry::file_ident_type;
    using version_type    = HistoryEntry::version_type;

    /// Get the first history entry where the produced version is greater than
    /// `begin_version` and less than or equal to `end_version`, and whose
    /// changeset is neither empty, nor produced by integration of a changeset
    /// received from the specified remote peer.
    ///
    /// If \a buffer is non-null, memory will be allocated and transferred to
    /// \a buffer. The changeset will be copied into the newly allocated memory.
    ///
    /// If \a buffer is null, the changeset is not copied out of the Realm,
    /// and entry.changeset.data() does not point to the changeset.
    /// The changeset in the Realm could be chunked, hence it is not possible
    /// to point to it with BinaryData. entry.changeset.size() always gives the
    /// size of the changeset.
    ///
    /// \param begin_version, end_version The range of versions to consider. If
    /// `begin_version` is equal to `end_version`, this is the empty range. If
    /// `begin_version` is zero, it means that everything preceeding
    /// `end_version` is to be considered, which is again the empty range if
    /// `end_version` is also zero. Zero is is special value in that no
    /// changeset produces that version. It is an error if `end_version`
    /// preceeds `begin_version`, or if `end_version` is zero and
    /// `begin_version` is not.
    ///
    /// \param not_from_remote_client_file_ident Skip entries whose changeset is
    /// produced by integration of changesets received from this remote
    /// peer. Zero if the remote peer is the server, otherwise the peer
    /// identifier of a client.
    ///
    /// \param only_nonempty Skip entries with empty changesets.
    ///
    /// \return The version produced by the changeset of the located history
    /// entry, or zero if no history entry exists matching the specified
    /// criteria.
    virtual version_type find_history_entry(version_type begin_version, version_type end_version,
                                            file_ident_type not_from_remote_client_file_ident,
                                            bool only_nonempty, HistoryEntry& entry,
                                            util::Optional<std::unique_ptr<char[]>&> buffer) const noexcept = 0;

    /// Copy a contiguous sequence of bytes from the specified reciprocally
    /// transformed changeset into the specified buffer. The targeted history
    /// entry is the one whose untransformed changeset produced the specified
    /// version. Copying starts at the specified offset within the transform,
    /// and will continue until the end of the transform or the end of the
    /// buffer, whichever comes first. The first copied byte is always placed in
    /// `buffer[0]`. The number of copied bytes is returned.
    ///
    /// \param remote_client_file_ident Zero if the remote peer is the server,
    /// otherwise the peer identifier of a client.
    virtual size_t read_reciprocal_transform(version_type version,
                                             file_ident_type remote_client_file_ident,
                                             size_t offset, char* buffer, size_t size) const = 0;

    /// Replace a contiguous chunk of bytes within the specified reciprocally
    /// transformed changeset. The targeted history entry is the one whose
    /// untransformed changeset produced the specified version. If the new chunk
    /// has a different size than the on it replaces, subsequent bytes (those
    /// beyond the end of the replaced chunk) are shifted to lower or higher
    /// offsets accordingly. If `replaced_size` is equal to `size_t(-1)`, the
    /// replaced chunk extends from `offset` to the end of the transform. Let
    /// `replaced_size_2` be the actual size of the replaced chunk, then the
    /// total number of bytes in the transform will increase by `size -
    /// replaced_size_2`. It is an error if `replaced_size` is not `size_t(-1)`
    /// and `offset + replaced_size` is greater than the size of the transform.
    ///
    /// \param remote_client_file_ident See read_reciprocal_transform().
    ///
    /// \param offset The index of the first replaced byte relative to the
    /// beginning of the transform.
    ///
    /// \param replaced_size The number of bytes in the replaced chunk.
    ///
    /// \param data The buffer holding the replacing chunk.
    ///
    /// \param size The number of bytes in the replacing chunk, which is also
    /// the number of bytes that will be read from the specified buffer.
    virtual void write_reciprocal_transform(version_type version,
                                            file_ident_type remote_client_file_ident,
                                            size_t offset, size_t replaced_size,
                                            const char* data, size_t size) = 0;

    virtual ~TransformHistory() noexcept {}
};


class TransformError; // Exception


class Transformer {
public:
    using timestamp_type  = HistoryEntry::timestamp_type;
    using file_ident_type = HistoryEntry::file_ident_type;
    using version_type    = HistoryEntry::version_type;

    struct RemoteChangeset {
        /// The version produced on the remote peer by this changeset.
        ///
        /// On the server, the remote peer is the client from which the
        /// changeset originated, and `remote_version` is the client version
        /// produced by the changeset on that client.
        ///
        /// On a client, the remote peer is the server, and `remote_version` is
        /// the server version produced by this changeset on the server. Since
        /// the server is never the originator of changes, this changeset must
        /// in turn have been produced on the server by integration of a
        /// changeset uploaded by some other client.
        version_type remote_version;

        /// The last local version that has been integrated into
        /// `remote_version`.
        ///
        /// A local version, L, has been integrated into a remote version, R,
        /// when, and only when L is the latest local version such that all
        /// preceeding changesets in the local history have been integrated by
        /// the remote peer prior to R.
        ///
        /// On the server, this is the last server version integrated into the
        /// client version `remote_version`. On a client, it is the last client
        /// version integrated into the server version `remote_version`.
        version_type last_integrated_local_version;

        /// The changeset itself.
        BinaryData data;

        /// Same meaning as `HistoryEntry::origin_timestamp`.
        timestamp_type origin_timestamp;

        /// Same meaning as `HistoryEntry::origin_client_file_ident`.
        file_ident_type origin_client_file_ident;

        RemoteChangeset(version_type rv, version_type lv, BinaryData d, timestamp_type ot,
                        file_ident_type fi);
    };

    /// Produce an operationally transformed version of the specified changeset,
    /// which is assumed to be of remote origin, and received from remote peer
    /// P. Note that P is not necessarily the peer from which the changes
    /// originated.
    ///
    /// Operational transformation is carried out between the specified
    /// changeset and all causally unrelated changesets in the local history. A
    /// changeset in the local history is causally unrelated if, and only if it
    /// occurs after the local changeset that produced
    /// `remote_changeset.last_integrated_local_version` and is not a produced
    /// by integration of a changeset received from P. This assumes that
    /// `remote_changeset.last_integrated_local_version` is set to the local
    /// version produced by the last local changeset, that was integrated by P
    /// before P produced the specified changeset.
    ///
    /// The operational transformation is reciprocal (two-way), so it also
    /// transforms the causally unrelated local changesets. This process does
    /// not modify the history itself (the changesets available through
    /// TransformHistory::get_history_entry()), instead the reciprocally
    /// transformed changesets are stored separately, and individually for each
    /// remote peer, such that they can participate in transformation of the
    /// next incoming changeset from P. Note that the peer identifier of P can
    /// be derived from `origin_client_file_ident` and information about whether
    /// the local peer is a server or a client.
    ///
    /// In general, if A and B are two causally unrelated (alternative)
    /// changesets based on the same version V, then the operational
    /// transformation between A and B produces changesets A' and B' such that
    /// both of the concatenated changesets A + B' and B + A' produce the same
    /// final state when applied to V. Operational transformation is meaningful
    /// only when carried out between alternative changesets based on the same
    /// version.
    ///
    /// \return The size of the transformed version of the specified
    /// changeset. Upon return, the changeset itself is stored in the specified
    /// output buffer.
    ///
    /// \throw TransformError Thrown if operational transformation fails due to
    /// a problem with the specified changeset.
    virtual size_t transform_remote_changeset(TransformHistory&,
                                              version_type current_local_version,
                                              RemoteChangeset changeset,
                                              util::Buffer<char>& output_buffer) = 0;

    virtual ~Transformer() noexcept {}
};


/// \param local_client_file_ident The server assigned local client file
/// identifier. This must be zero on the server-side, and only on the
/// server-side.
std::unique_ptr<Transformer> make_transformer(Transformer::file_ident_type local_client_file_ident);




// Implementation

class TransformError: public std::runtime_error {
public:
    TransformError(const std::string& message):
        std::runtime_error(message)
    {
    }
};

inline Transformer::RemoteChangeset::RemoteChangeset(version_type rv, version_type lv,
                                                     BinaryData d, timestamp_type ot,
                                                     file_ident_type fi):
    remote_version(rv),
    last_integrated_local_version(lv),
    data(d),
    origin_timestamp(ot),
    origin_client_file_ident(fi)
{
}

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_TRANSFORM_HPP
