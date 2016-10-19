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
    ///
    /// All clients that will be, or are already participating in
    /// synchronization must guarantee that their local history is causally
    /// consistent. The convergence guarantee offered by the merge system relies
    /// strongly on this.
    ///
    /// FIXME: In its current form, the merge algorithm seems to achieve
    /// convergence even without causal consistency. Figure out whether we still
    /// want to require it, and if so, why.
    ///
    /// **Definition:** The local history is *causally consistent* if, and only
    /// if every entry, referring to changes of local origin, has an effective
    /// timestamp, which is greater than, or equal to the effective timestamp of
    /// all preceding entries in the local history.
    ///
    /// **Definition:** The *effective timestamp* of a history entry is the pair
    /// `(origin_timestamp, origin_client_file_ident)` endowed with the standard
    /// lexicographic order. Note that this implies that it is impossible for
    /// two entries to have equal effective timestamps if they originate from
    /// different clients.
    timestamp_type origin_timestamp;

    /// For changes of local origin, `origin_client_file_ident` is always
    /// zero. For changes of remote origin, this history entry was produced by
    /// the integration of a changeset received from a remote peer P. In some
    /// cases, that changeset may itself have been produced by the integration
    /// on P of a changeset received from another remote peer. In any case, as
    /// long as these changes are of remote origin, `origin_client_file_ident`
    /// identifies the peer on which they originated, which may or may not be P.
    ///
    /// More concretely, on the server-side, the remote peer is a client, and
    /// and the changes always originate from that client, so
    /// `origin_client_file_ident` always refer to that client. Conversely, on
    /// the client-side, the remote peer is the server, and the server received
    /// the original changeset from a client, so `origin_client_file_ident`
    /// refers to that client.
    ///
    /// Note that *peer* is used colloquially here to refer to a particular
    /// synchronization participant. In reality, a synchronization participant
    /// is either a server-side file, or a particular client-side file
    /// associated with that server-side file. To make things even more
    /// confusing, a single client application may contain multiple client-side
    /// files associated with the same server-side file. In the same vein,
    /// *client* should be understood as client-side file, and *remote peer* as
    /// any other file from the set of associated files, even other such files
    /// contained within the same client application, if there are any.
    file_ident_type origin_client_file_ident;

    /// For changes of local origin, `remote_version` is the version produced on
    /// the remote peer by the last changeset integrated locally prior to the
    /// production of the changeset referenced by this history entry, or zero if
    /// no remote changeset was integrated yet. This only makes sense when there
    /// is a unique remote peer, and since that is not the case on the server,
    /// the server cannot be the originator of any changes.
    ///
    /// For changes of remote origin, this history entry was produced by the
    /// integration of a changeset directly received from a remote peer P, and
    /// `remote_version` is then the version produced on P by that
    /// changeset. Note that such changes may have originated from a different
    /// peer (not P), but `remote_version` will still be the version produced on
    /// P.
    ///
    /// More concretely, on the server-side, the remote peer is a client, and
    /// the changes always originate from that client, and `remote_version` is
    /// the `<client version>` specified in an UPLOAD message of the
    /// client-server communication protocol. Conversely, for changes of remote
    /// origin on the client-side, the remote peer is the server, and
    /// `remote_version` is the <server version> specified in a received
    /// DOWNLOAD message.
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

    /// Get the first history entry whose changeset produced a version that
    /// succeeds `begin_version` and, and does not succeed `end_version`, and
    /// whose changeset was not produced by integration of a changeset received
    /// from the specified remote peer.
    ///
    /// The ownership of the memory referenced by `entry.changeset` is **not**
    /// passed to the caller upon return. The callee retains ownership.
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
                                            bool only_nonempty,
                                            HistoryEntry& entry) const noexcept = 0;

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

protected:
    static bool register_local_time(timestamp_type local_timestamp,
                                    timestamp_type& timestamp_threshold) noexcept;

    static bool register_remote_time(timestamp_type remote_timestamp,
                                     timestamp_type& timestamp_threshold) noexcept;
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

inline bool TransformHistory::register_local_time(timestamp_type local_timestamp,
                                                  timestamp_type& timestamp_threshold) noexcept
{
    // Needed to ensure causal consistency. This also guards against
    // nonmonotonic local time.
    if (timestamp_threshold < local_timestamp) {
        timestamp_threshold = local_timestamp;
        return true;
    }
    return false;
}

inline bool TransformHistory::register_remote_time(timestamp_type remote_timestamp,
                                                   timestamp_type& timestamp_threshold) noexcept
{
    // To ensure causal consistency, we need to know the latest remote (or
    // local) timestamp seen so far. Adding one to the incoming remote
    // timestamp, before using it to bump the `timestamp_threshold`, is a
    // simple way of ensuring not only proper ordering among timestamps, but
    // also among 'effective timestamps' (which is required), regardless of the
    // values of the assiciated client file identifiers.
    if (timestamp_threshold < remote_timestamp + 1) {
        timestamp_threshold = remote_timestamp + 1;
        return true;
    }
    return false;
}

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
