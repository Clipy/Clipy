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

#include <memory>
#include <string>

#include <realm/impl/continuous_transactions_history.hpp>
#include <realm/sync/transform.hpp>

#ifndef REALM_SYNC_HISTORY_HPP
#define REALM_SYNC_HISTORY_HPP

namespace realm {
namespace sync {

/// SyncProgress is the progress sent by the server in
/// the download message. The server scans through its
/// history in connection with every download message.
/// scan_server_version is the server_version of the changeset
/// at which the server ended the scan. scan_client_version
/// is the client_version for this client that was last integrated before
/// scan_server_version.
/// latest_server_version is the end of the server history, and
/// latest_server_session_ident is the server_session_ident
/// corresponding to latest_sever_version.
/// latest_client_version is the corresponding client_version.
/// In other words, latest_client_version is the very latest version
/// of a changeset originating from this client.
///
/// The client persists the entire progress. It is not very important to
/// persist latest_server_version, but for consistency the entire
/// progress is persisted.
struct SyncProgress {
    using version_type = HistoryEntry::version_type;

    version_type scan_server_version = 0;
    version_type scan_client_version = 0;
    version_type latest_server_version = 0;
    int_fast64_t latest_server_session_ident = 0;
    version_type latest_client_version = 0;
};


class SyncHistory:
        public TrivialReplication {
public:
    using version_type    = TrivialReplication::version_type;
    using file_ident_type = HistoryEntry::file_ident_type;
    using SyncTransactCallback = void(VersionID old_version, VersionID new_version);

    SyncHistory(const std::string& realm_path);

    /// Get the version of the latest snapshot of the associated Realm, as well
    /// as the file identifier pair and the synchronization progress pair as
    /// they are stored in that snapshot.
    ///
    /// The returned current client version is the version of the latest
    /// snapshot of the associated SharedGroup object, and is guaranteed to be
    /// zero for the initial empty Realm state.
    ///
    /// The returned file identifier pair (server, client) is the one that was
    /// last stored by set_file_ident_pair(). If no identifier pair has been
    /// stored yet, \a client_file_ident is set to zero.
    ///
    /// The returned SyncProgress is the one that was last stored by
    /// set_sync_progress(), or {} if set_sync_progress() has never been called
    /// for the associated Realm file.
    virtual void get_status(version_type& current_client_version,
                            file_ident_type& server_file_ident,
                            file_ident_type& client_file_ident,
                            int_fast64_t& client_file_ident_secret,
                            SyncProgress& progress) = 0;

    /// Stores the server assigned file identifier pair (server, client) in the
    /// associated Realm file, such that it is available via get_status() during
    /// future synchronization sessions. It is an error to set this identifier
    /// pair more than once per Realm file.
    ///
    /// \param server_file_ident The server assigned server-side file
    /// identifier. This can be any non-zero integer strictly less than 2**64.
    /// The server is supposed to choose a cryptic value that cannot easily be
    /// guessed by clients (intentionally or not), and its only puspose is to
    /// provide a higher level of fidelity during subsequent identification of
    /// the server Realm. The server does not have to guarantee that this
    /// identifier is unique, but in almost all cases it will be. Since the
    /// client will also always specify the path name when referring to a server
    /// file, the lack of a uniqueness guarantee is effectively not a problem.
    ///
    /// \param client_file_ident The server assigned client-side file
    /// identifier. A client-side file identifier is a non-zero positive integer
    /// strictly less than 2**64. The server guarantees that all client-side
    /// file identifiers generated on behalf of a particular server Realm are
    /// unique with respect to each other. The server is free to generate
    /// identical identifiers for two client files if they are associated with
    /// different server Realms.
    ///
    /// The client is required to obtain the file identifiers before engaging in
    /// synchronization proper, and it must store the identifiers and use
    /// them to reestablish the connection between the client file and the server
    /// file when engaging in future synchronization sessions.
    virtual void set_file_ident_pair(file_ident_type server_file_ident,
                                     file_ident_type client_file_ident,
                                     int_fast64_t client_file_ident_secret) = 0;

    /// Stores the SyncProgress progress in the
    /// associated Realm file in a way that makes it available via get_status()
    /// during future synchronization sessions. Progress is reported by the
    /// server in the DOWNLOAD message.
    ///
    /// See struct SyncProgress for a description of \param progress.
    ///
    /// `progress.scan_client_version` has an effect on the process by which
    /// old history entries are discarded.
    ///
    /// `progress.scan_client_version` The version produced on this client by the last
    /// changeset, that was sent to, and integrated by the server at the time
    /// `progress.scan_server_version was produced, or zero if
    /// `progress.scan_server_version` is zero.
    ///
    /// Since all changesets produced after `progres.scan_client_version` are potentially
    /// needed during operational transformation of the next changeset received
    /// from the server, the implementation of this class must promise to retain
    /// all history entries produced after `progress.scan_client_version`. That is, a history
    /// entry with a changeset, that produces version V, is guaranteed to be
    /// retained as long as V is strictly greater than `progress.scan_client_version`.
    ///
    /// It is an error to specify a client version that is less than the
    /// currently stored version, since there is no way to get discarded history
    /// back.
    virtual void set_sync_progress(SyncProgress progress) = 0;

    /// Get the first history entry whose changeset produced a version that
    /// succeeds `begin_version` and, and does not succeed `end_version`, whose
    /// changeset was not produced by integration of a changeset received from
    /// the server, and whose changeset was not empty.
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
    /// \param buffer Owner of memory referenced by entry.changeset upon return.
    ///
    /// \return The version produced by the changeset of the located history
    /// entry, or zero if no history entry exists matching the specified
    /// criteria.
    virtual version_type find_history_entry_for_upload(version_type begin_version,
                                                       version_type end_version,
                                                       HistoryEntry& entry,
                                                       std::unique_ptr<char[]>& buffer) const = 0;

    using RemoteChangeset = Transformer::RemoteChangeset;

    /// \brief Integrate a sequence of remote changesets using a single Realm
    /// transaction.
    ///
    /// Each changeset will be transformed as if by a call to
    /// Transformer::transform_remote_changeset(), and then applied to the
    /// associated Realm.
    ///
    /// As a final step, each changeset will be added to the local history (list
    /// of applied changesets).
    ///
    /// \param progress is the SyncProgress received in the download message.
    /// Progress will be persisted along with the changesets.
    ///
    /// \param num_changesets The number of passed changesets. Must be non-zero.
    ///
    /// \param callback An optional callback which will be called with the
    /// version immediately processing the sync transaction and that of the sync
    /// transaction.
    ///
    /// \return The new local version produced by the application of the
    /// transformed changeset.
    virtual version_type integrate_remote_changesets(SyncProgress progress,
                                                     const RemoteChangeset* changesets,
                                                     size_t num_changesets,
                                                     util::Logger* replay_logger,
                                                     std::function<SyncTransactCallback>& callback) = 0;
};


std::unique_ptr<SyncHistory> make_sync_history(const std::string& realm_path);



// Implementation

inline SyncHistory::SyncHistory(const std::string& realm_path):
    TrivialReplication(realm_path)
{
}

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_SYNC_HPP
