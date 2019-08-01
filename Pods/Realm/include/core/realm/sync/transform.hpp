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
#include <realm/impl/cont_transact_hist.hpp>
#include <realm/group_shared.hpp>
#include <realm/chunked_binary.hpp>
#include <realm/sync/protocol.hpp>

namespace realm {
namespace sync {

struct Changeset;

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
    /// The time of origination of the changes referenced by this history entry,
    /// meassured as the number of milliseconds since 2015-01-01T00:00:00Z, not
    /// including leap seconds. For changes of local origin, this is the local
    /// time at the point when the local transaction was committed. For changes
    /// of remote origin, it is the remote time of origin at the peer (client or
    /// server) identified by `origin_file_ident`.
    timestamp_type origin_timestamp;

    /// The identifier of the file in the context of which the initial
    /// untransformed changeset originated, or zero if the changeset originated
    /// on the local peer (client or server).
    ///
    /// For example, when a changeset "travels" from a file with identifier 2 on
    /// client A, through a file with identifier 1 on the server, to a file with
    /// identifier 3 on client B, then `origin_file_ident` will be 0 on client
    /// A, 2 on the server, and 2 on client B. On the other hand, if the server
    /// was the originator of the changeset, then `origin_file_ident` would be
    /// zero on the server, and 1 on both clients.
    file_ident_type origin_file_ident;

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
    ChunkedBinaryData changeset;
};



/// The interface between the sync history and the operational transformer
/// (Transformer) for the purpose of transforming changesets received from a
/// particular *remote peer*.
class TransformHistory {
public:
    /// Get the first history entry where the produced version is greater than
    /// `begin_version` and less than or equal to `end_version`, and whose
    /// changeset is neither empty, nor produced by integration of a changeset
    /// received from the remote peer associated with this history.
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
    /// `begin_version` is equal to `end_version`, it is the empty range. If
    /// `begin_version` is zero, it means that everything preceeding
    /// `end_version` is to be considered, which is again the empty range if
    /// `end_version` is also zero. Zero is a special value in that no changeset
    /// produces that version. It is an error if `end_version` precedes
    /// `begin_version`, or if `end_version` is zero and `begin_version` is not.
    ///
    /// \return The version produced by the changeset of the located history
    /// entry, or zero if no history entry exists matching the specified and
    /// implied criteria.
    virtual version_type find_history_entry(version_type begin_version, version_type end_version,
                                            HistoryEntry& entry) const noexcept = 0;

    /// Get the specified reciprocal changeset. The targeted history entry is
    /// the one whose untransformed changeset produced the specified version.
    virtual ChunkedBinaryData get_reciprocal_transform(version_type version) const = 0;

    /// Replace the specified reciprocally transformed changeset. The targeted
    /// history entry is the one whose untransformed changeset produced the
    /// specified version.
    ///
    /// \param encoded_changeset The new reciprocally transformed changeset.
    virtual void set_reciprocal_transform(version_type version, BinaryData encoded_changeset) = 0;

    virtual ~TransformHistory() noexcept {}
};



class TransformError; // Exception

class Transformer {
public:
    class RemoteChangeset;
    class Reporter;

    /// Produce operationally transformed versions of the specified changesets,
    /// which are assumed to be received from a particular remote peer, P,
    /// represented by the specified transform history. Note that P is not
    /// necessarily the peer on which the changes originated.
    ///
    /// Operational transformation is carried out between the specified
    /// changesets and all causally unrelated changesets in the local history. A
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
    /// next incoming changeset from P.
    ///
    /// In general, if A and B are two causally unrelated (alternative)
    /// changesets based on the same version V, then the operational
    /// transformation between A and B produces changesets A' and B' such that
    /// both of the concatenated changesets A + B' and B + A' produce the same
    /// final state when applied to V. Operational transformation is meaningful
    /// only when carried out between alternative changesets based on the same
    /// version.
    ///
    /// \param local_file_ident The identifier of the local Realm file. The
    /// transformer uses this as the actual origin file identifier for
    /// changesets where HistoryEntry::origin_file_ident is zero, i.e., when the
    /// changeset is of local origin. The specified identifier must never be
    /// zero.
    ///
    /// \return The size of the transformed version of the specified
    /// changesets. Upon return, the transformed changesets are concatenated
    /// and placed in \a output_buffer.
    ///
    /// \throw TransformError Thrown if operational transformation fails due to
    /// a problem with the specified changeset.
    ///
    /// FIXME: Consider using std::error_code instead of throwing
    /// TransformError.
    virtual void transform_remote_changesets(TransformHistory&,
                                             file_ident_type local_file_ident,
                                             version_type current_local_version,
                                             Changeset* changesets,
                                             std::size_t num_changesets,
                                             Reporter* = nullptr,
                                             util::Logger* = nullptr) = 0;

    virtual ~Transformer() noexcept {}
};

std::unique_ptr<Transformer> make_transformer();



class Transformer::RemoteChangeset {
public:
    /// The version produced on the remote peer by this changeset.
    ///
    /// On the server, the remote peer is the client from which the changeset
    /// originated, and `remote_version` is the client version produced by the
    /// changeset on that client.
    ///
    /// On a client, the remote peer is the server, and `remote_version` is the
    /// server version produced by this changeset on the server. Since the
    /// server is never the originator of changes, this changeset must in turn
    /// have been produced on the server by integration of a changeset uploaded
    /// by some other client.
    version_type remote_version = 0;

    /// The last local version that has been integrated into `remote_version`.
    ///
    /// A local version, L, has been integrated into a remote version, R, when,
    /// and only when L is the latest local version such that all preceeding
    /// changesets in the local history have been integrated by the remote peer
    /// prior to R.
    ///
    /// On the server, this is the last server version integrated into the
    /// client version `remote_version`. On a client, it is the last client
    /// version integrated into the server version `remote_version`.
    version_type last_integrated_local_version = 0;

    /// The changeset itself.
    ChunkedBinaryData data;

    /// Same meaning as `HistoryEntry::origin_timestamp`.
    timestamp_type origin_timestamp = 0;

    /// Same meaning as `HistoryEntry::origin_file_ident`.
    file_ident_type origin_file_ident = 0;

    /// If the changeset was compacted during download, the size of the original
    /// changeset.
    std::size_t original_changeset_size = 0;

    RemoteChangeset() {}
    RemoteChangeset(version_type rv, version_type lv, ChunkedBinaryData d, timestamp_type ot,
                    file_ident_type fi);
};



class Transformer::Reporter {
public:
    virtual void on_changesets_merged(long num_merges) = 0;
};



void parse_remote_changeset(const Transformer::RemoteChangeset&, Changeset&);




// Implementation

class TransformError: public std::runtime_error {
public:
    TransformError(const std::string& message):
        std::runtime_error(message)
    {
    }
};

inline Transformer::RemoteChangeset::RemoteChangeset(version_type rv, version_type lv,
                                                     ChunkedBinaryData d, timestamp_type ot,
                                                     file_ident_type fi):
    remote_version(rv),
    last_integrated_local_version(lv),
    data(d),
    origin_timestamp(ot),
    origin_file_ident(fi)
{
}

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_TRANSFORM_HPP
