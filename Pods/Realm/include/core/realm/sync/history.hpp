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

#include <cstdint>
#include <memory>
#include <chrono>
#include <string>

#include <realm/util/string_view.hpp>
#include <realm/impl/cont_transact_hist.hpp>
#include <realm/sync/instruction_replication.hpp>
#include <realm/sync/protocol.hpp>
#include <realm/sync/transform.hpp>
#include <realm/sync/object_id.hpp>
#include <realm/sync/instructions.hpp>

#ifndef REALM_SYNC_HISTORY_HPP
#define REALM_SYNC_HISTORY_HPP


namespace realm {
namespace _impl {

struct ObjectIDHistoryState;

} // namespace _impl
} // namespace realm


namespace realm {
namespace sync {

struct VersionInfo {
    /// Realm snapshot version.
    version_type realm_version = 0;

    /// The synchronization version corresponding to `realm_version`.
    ///
    /// In the context of the client-side history type `sync_version.version`
    /// will currently always be equal to `realm_version` and
    /// `sync_version.salt` will always be zero.
    SaltedVersion sync_version = {0, 0};
};


struct SerialTransactSubstitutions {
    struct Class {
        InternString name;
        std::size_t substitutions_end;
    };
    std::vector<Class> classes;
    std::vector<std::pair<ObjectID, ObjectID>> substitutions;
};


timestamp_type generate_changeset_timestamp() noexcept;

// FIXME: in C++17, switch to using std::timespec in place of last two
// arguments.
void map_changeset_timestamp(timestamp_type, std::time_t& seconds_since_epoch,
                             long& nanoseconds) noexcept;


/// Thrown if changeset cooking is not either consistently on or consistently
/// off during synchronization (ClientHistory::set_sync_progress() and
/// ClientHistory::integrate_server_changesets()).
class InconsistentUseOfCookedHistory;

/// Thrown if a bad server version is passed to
/// ClientHistory::get_cooked_status().
class BadCookedServerVersion;


class ClientHistoryBase :
        public InstructionReplication {
public:
    using SyncTransactCallback = void(VersionID old_version, VersionID new_version);

    /// Get the version of the latest snapshot of the associated Realm, as well
    /// as the client file identifier and the synchronization progress as they
    /// are stored in that snapshot.
    ///
    /// The returned current client version is the version produced by the last
    /// changeset in the history. The type of version returned here, is the one
    /// that identifies an entry in the sync history. Whether this is the same
    /// as the snapshot number of the Realm file depends on the history
    /// implementation.
    ///
    /// The returned client file identifier is the one that was last stored by
    /// set_client_file_ident(), or `SaltedFileIdent{0, 0}` if
    /// set_client_file_ident() has never been called.
    ///
    /// The returned SyncProgress is the one that was last stored by
    /// set_sync_progress(), or `SyncProgress{}` if set_sync_progress() has
    /// never been called.
    virtual void get_status(version_type& current_client_version,
                            SaltedFileIdent& client_file_ident,
                            SyncProgress& progress) const = 0;

    /// Stores the server assigned client file identifier in the associated
    /// Realm file, such that it is available via get_status() during future
    /// synchronization sessions. It is an error to set this identifier more
    /// than once per Realm file.
    ///
    /// \param client_file_ident The server assigned client-side file
    /// identifier. A client-side file identifier is a non-zero positive integer
    /// strictly less than 2**64. The server guarantees that all client-side
    /// file identifiers generated on behalf of a particular server Realm are
    /// unique with respect to each other. The server is free to generate
    /// identical identifiers for two client files if they are associated with
    /// different server Realms.
    ///
    /// \param fix_up_object_ids The object ids that depend on client file ident
    /// will be fixed in both state and history if this parameter is true. If
    /// it is known that there are no objects to fix, it can be set to false to
    /// achieve higher performance.
    ///
    /// The client is required to obtain the file identifier before engaging in
    /// synchronization proper, and it must store the identifier and use it to
    /// reestablish the connection between the client file and the server file
    /// when engaging in future synchronization sessions.
    virtual void set_client_file_ident(SaltedFileIdent client_file_ident,
                                       bool fix_up_object_ids) = 0;

    /// Stores the SyncProgress progress in the associated Realm file in a way
    /// that makes it available via get_status() during future synchronization
    /// sessions. Progress is reported by the server in the DOWNLOAD message.
    ///
    /// See struct SyncProgress for a description of \param progress.
    ///
    /// \throw InconsistentUseOfCookedHistory If a changeset cooker has been
    /// attached to this history object, and the Realm file does not have a
    /// cooked history, and a cooked history can no longer be added because some
    /// synchronization has already happened. Or if no changeset cooker has been
    /// attached, and the Realm file does have a cooked history.
    virtual void set_sync_progress(const SyncProgress& progress, VersionInfo&) = 0;

    struct UploadChangeset {
        timestamp_type origin_timestamp;
        file_ident_type origin_file_ident;
        UploadCursor progress;
        ChunkedBinaryData changeset;
        std::unique_ptr<char[]> buffer;
    };

    /// \brief Scan through the history for changesets to be uploaded.
    ///
    /// This function scans the history for changesets to be uploaded, i.e., for
    /// changesets that are not empty, and were not produced by integration of
    /// changesets recieved from the server. The scan begins at the position
    /// specified by the initial value of \a upload_progress.client_version, and
    /// ends no later than at the position specified by \a end_version.
    ///
    /// The implementation is allowed to end the scan before \a end_version,
    /// such as to limit the combined size of returned changesets. However, if
    /// the specified range contains any changesets that are supposed to be
    /// uploaded, this function must return at least one.
    ///
    /// Upon return, \a upload_progress will have been updated to point to the
    /// position from which the next scan should resume. This must be a position
    /// after the last returned changeset, and before any remaining changesets
    /// that are supposed to be uploaded, although never a position that
    /// succeeds \a end_version.
    ///
    /// The value passed as \a upload_progress by the caller, must either be one
    /// that was produced by an earlier invocation of
    /// find_uploadable_changesets(), one that was returned by get_status(), or
    /// one that was received by the client in a DOWNLOAD message from the
    /// server. When the value comes from a DOWNLOAD message, it is supposed to
    /// reflect a value of UploadChangeset::progress produced by an earlier
    /// invocation of find_uploadable_changesets().
    ///
    /// Found changesets are added to \a uploadable_changesets.
    ///
    /// \param locked_server_version will be set to the value that should be
    /// used as `<locked server version>` in a DOWNLOAD message.
    ///
    /// For changesets of local origin, UploadChangeset::origin_file_ident will
    /// be zero.
    virtual void find_uploadable_changesets(UploadCursor& upload_progress, version_type end_version,
                                            std::vector<UploadChangeset>& uploadable_changesets,
                                            version_type& locked_server_version) const = 0;

    using RemoteChangeset = Transformer::RemoteChangeset;

    // FIXME: Apparently, this feature is expected by object store, but why?
    // What is it ultimately used for? (@tgoyne)
    class SyncTransactReporter {
    public:
        virtual void report_sync_transact(VersionID old_version, VersionID new_version) = 0;
    protected:
        ~SyncTransactReporter() {}
    };

    enum class IntegrationError {
        bad_origin_file_ident,
        bad_changeset
    };

    /// \brief Integrate a sequence of changesets received from the server using
    /// a single Realm transaction.
    ///
    /// Each changeset will be transformed as if by a call to
    /// Transformer::transform_remote_changeset(), and then applied to the
    /// associated Realm.
    ///
    /// As a final step, each changeset will be added to the local history (list
    /// of applied changesets).
    ///
    /// This function checks whether the specified changesets specify valid
    /// remote origin file identifiers and whether the changesets contain valid
    /// sequences of instructions. The caller must already have ensured that the
    /// origin file identifiers are strictly positive and not equal to the file
    /// identifier assigned to this client by the server.
    ///
    /// If any of the changesets are invalid, this function returns false and
    /// sets `integration_error` to the appropriate value. If they are all
    /// deemed valid, this function updates \a version_info to reflect the new
    /// version produced by the transaction.
    ///
    /// \param progress is the SyncProgress received in the download message.
    /// Progress will be persisted along with the changesets.
    ///
    /// \param num_changesets The number of passed changesets. Must be non-zero.
    ///
    /// \param transact_reporter An optional callback which will be called with the
    /// version immediately processing the sync transaction and that of the sync
    /// transaction.
    ///
    /// \throw InconsistentUseOfCookedHistory If a changeset cooker has been
    /// attached to this history object, and the Realm file does not have a
    /// cooked history, and a cooked history can no longer be added because some
    /// synchronization has already happened. Or if no changeset cooker has been
    /// attached, and the Realm file does have a cooked history.
    virtual bool integrate_server_changesets(const SyncProgress& progress,
                                             const RemoteChangeset* changesets,
                                             std::size_t num_changesets, VersionInfo& new_version,
                                             IntegrationError& integration_error, util::Logger&,
                                             SyncTransactReporter* transact_reporter = nullptr,
                                             const SerialTransactSubstitutions* = nullptr) = 0;

protected:
    ClientHistoryBase(const std::string& realm_path);
};



class ClientHistory : public ClientHistoryBase {
public:
    class ChangesetCooker;
    class Config;

    /// Get the persisted upload/download progress in bytes.
    virtual void get_upload_download_bytes(std::uint_fast64_t& downloaded_bytes,
                                           std::uint_fast64_t& downloadable_bytes,
                                           std::uint_fast64_t& uploaded_bytes,
                                           std::uint_fast64_t& uploadable_bytes,
                                           std::uint_fast64_t& snapshot_version) = 0;

    /// See set_cooked_progress().
    struct CookedProgress {
        std::int_fast64_t changeset_index = 0;
        std::int_fast64_t intrachangeset_progress = 0;
    };

    /// Get information about the current state of the cooked history including
    /// the point of progress of its consumption.
    ///
    /// \param server_version The server version associated with the last cooked
    /// changeset that should be skipped. See `/doc/cooked_history.md` for an
    /// explanation of the rationale behind this. Specifying zero means that no
    /// changesets should be skipped. It is an error to specify a nonzero server
    /// version that is not the server version associated with any of of the
    /// cooked changesets, or to specify a nonzero server version that precedes
    /// the one, that is associated with the last cooked changeset that was
    /// marked as consumed. Doing so, will cause BadCookedServerVersion to be
    /// thrown.
    ///
    /// \param num_changesets Set to the total number of produced cooked
    /// changesets over the lifetime of the Realm file to which this history
    /// accessor object is attached. This is the number of previously consumed
    /// changesets plus the number of unconsumed changesets remaining in the
    /// Realm file.
    ///
    /// \param progress The point of progress of the consumption of the cooked
    /// history. Initially, and until explicitly modified by
    /// set_cooked_progress(), both `CookedProgress::changeset_index` and
    /// `CookedProgress::intrachangeset_progress` are zero. If a nonzero value
    /// was passed for \a server_version, \a progress will be transparently
    /// adjusted to account for the skipped changesets. See also \a
    /// num_skipped_changesets. If one or more changesets are skipped,
    /// `CookedProgress::intrachangeset_progress` will be set to zero.
    ///
    /// \param num_skipped_changesets The number of skipped changesets. See also
    /// \a server_version.
    ///
    /// \throw BadCookedServerVersion See \a server_version.
    virtual void get_cooked_status(version_type server_version, std::int_fast64_t& num_changesets,
                                   CookedProgress& progress,
                                   std::int_fast64_t& num_skipped_changesets) const = 0;

    /// Fetch the cooked changeset at the specified index.
    ///
    /// Cooked changesets are made available in the order they are produced by
    /// the changeset cooker (ChangesetCooker).
    ///
    /// Behaviour is undefined if the specified index is less than the index
    /// (CookedProgress::changeset_index) returned by get_cooked_progress(), or
    /// if it is greater than, or equal to the total number of cooked changesets
    /// (as returned by get_num_cooked_changesets()).
    ///
    /// The callee must append the bytes of the located cooked changeset to the
    /// specified buffer, which does not have to be empty initially.
    ///
    /// \param server_version Will be set to the version produced on the server
    /// by an earlier form of the retreived changeset. If the cooked changeset
    /// was produced (as output of cooker) before migration of the client-side
    /// history compartment to schema version 2, then \a server_version will be
    /// set to zero instead, because the real value is unkown. Zero is not a
    /// possible value in any other case.
    virtual void get_cooked_changeset(std::int_fast64_t index,
                                      util::AppendBuffer<char>&,
                                      version_type& server_version) const = 0;

    /// Persistently stores the point of progress of the consumer of cooked
    /// changesets.
    ///
    /// The changeset index (CookedProgress::changeset_index) is the index (as
    /// passed to get_cooked_changeset()) of the first unconsumed cooked
    /// changset. Changesets at lower indexes will no longer be available.
    ///
    /// The intrachangeset progress field
    /// (CookedProgress::intrachangeset_progress) will be faithfully persisted,
    /// but will otherwise be treated as an opaque object by the history
    /// internals.
    ///
    /// As well as allowing for later retrieval, the specification of the point
    /// of progress of the consumer of cooked changesets also has the effect of
    /// trimming obsolete cooked changesets from the Realm file (i.e., removal
    /// of all changesets at indexes lower than
    /// CookedProgress::intrachangeset_progress).  Indeed, if this function is
    /// never called, but cooked changesets are continually being produced, then
    /// the Realm file will grow without bounds.
    ///
    /// It is an error if the specified index (CookedProgress::changeset_index)
    /// is lower than the index returned by get_cooked_progress(), and if it is
    /// higher that the value returned by get_num_cooked_changesets().
    ///
    /// \return The snapshot number produced by the transaction performed
    /// internally in set_cooked_progress(). This is also the client-side sync
    /// version, and it should be passed to
    /// sync::Session::nonsync_transact_notify() if a synchronization session is
    /// in progress for the same file while set_cooked_progress() is
    /// called. Doing so, ensures that the server will be notified about the
    /// released server versions as soon as possible.
    ///
    /// \throw InconsistentUseOfCookedHistory If this file does not have a
    /// cooked history and one can no longer be added because changesets of
    /// remote origin has already been integrated.
    virtual version_type set_cooked_progress(CookedProgress) = 0;

    /// \brief Get the number of cooked changesets so far produced for this
    /// Realm.
    ///
    /// This is the same thing as is returned via \a num_changesets by
    /// get_cooked_status().
    std::int_fast64_t get_num_cooked_changesets() const noexcept;

    /// \brief Returns the persisted progress that was last stored by
    /// set_cooked_progress().
    ///
    /// This is the same thing as is returned via \a progress by
    /// get_cooked_status() when invoked with a server version of zero.
    CookedProgress get_cooked_progress() const noexcept;

    /// Same as get_cooked_changeset(std::int_fast64_t,
    /// util::AppendBuffer<char>&, version_type&) but does not retreived the
    /// server version.
    void get_cooked_changeset(std::int_fast64_t index, util::AppendBuffer<char>&) const;

    /// Return an upload cursor as it would be when the uploading process
    /// reaches the snapshot to which the current transaction is bound.
    ///
    /// **CAUTION:** Must be called only while a transaction (read or write) is
    /// in progress via the SharedGroup object associated with this history
    /// object.
    virtual UploadCursor get_upload_anchor_of_current_transact() const = 0;

    /// Return the synchronization changeset of the current transaction as it
    /// would be if that transaction was committed at this time.
    ///
    /// The returned memory reference may be invalidated by subsequent
    /// operations on the Realm state.
    ///
    /// **CAUTION:** Must be called only while a write transaction is in
    /// progress via the SharedGroup object associated with this history object.
    virtual util::StringView get_sync_changeset_of_current_transact() const noexcept = 0;

protected:
    ClientHistory(const std::string& realm_path);
};


/// \brief Abstract interface for changeset cookers.
///
/// Note, it is completely up to the application to decide what a cooked
/// changeset is. History objects (instances of ClientHistory) are required to
/// treat cooked changesets as opaque entities. For an example of a concrete
/// changeset cooker, see TrivialChangesetCooker which defines the cooked
/// changesets to be identical copies of the raw changesets.
class ClientHistory::ChangesetCooker {
public:
    virtual ~ChangesetCooker() {}

    /// \brief An opportunity to produce a cooked changeset.
    ///
    /// When the implementation chooses to produce a cooked changeset, it must
    /// write the cooked changeset to the specified buffer, and return
    /// true. When the implementation chooses not to produce a cooked changeset,
    /// it must return false. The implementation is allowed to write to the
    /// buffer, and return false, and in that case, the written data will be
    /// ignored.
    ///
    /// \param prior_state The state of the local Realm on which the specified
    /// raw changeset is based.
    ///
    /// \param changeset, changeset_size The raw changeset.
    ///
    /// \param buffer The buffer to which the cooked changeset must be written.
    ///
    /// \return True if a cooked changeset was produced. Otherwise false.
    virtual bool cook_changeset(const Group& prior_state,
                                const char* changeset, std::size_t changeset_size,
                                util::AppendBuffer<char>& buffer) = 0;
};


class ClientHistory::Config {
public:
    Config() {}

    /// Must be set to true if, and only if the created history object
    /// represents (is owned by) the sync agent of the specified Realm file. At
    /// most one such instance is allowed to participate in a Realm file access
    /// session at any point in time. Ordinarily the sync agent is encapsulated
    /// by the sync::Client class, and the history instance representing the
    /// agent is created transparently by sync::Client (one history instance per
    /// sync::Session object).
    bool owner_is_sync_agent = false;

    /// If a changeset cooker is specified, then the created history object will
    /// allow for a cooked changeset to be produced for each changeset of remote
    /// origin; that is, for each changeset that is integrated during the
    /// execution of ClientHistory::integrate_remote_changesets(). If no
    /// changeset cooker is specified, then no cooked changesets will be
    /// produced on behalf of the created history object.
    ///
    /// ClientHistory::integrate_remote_changesets() will pass each incoming
    /// changeset to the cooker after operational transformation; that is, when
    /// the chageset is ready to be applied to the local Realm state.
    std::shared_ptr<ChangesetCooker> changeset_cooker;
};

/// \brief Create a "sync history" implementation of the realm::Replication
/// interface.
///
/// The intended role for such an object is as a plugin for new
/// realm::SharedGroup objects.
std::unique_ptr<ClientHistory> make_client_history(const std::string& realm_path,
                                                   ClientHistory::Config = {});



// Implementation

inline timestamp_type generate_changeset_timestamp() noexcept
{
    namespace chrono = std::chrono;
    // Unfortunately, C++11 does not specify what the epoch is for
    // `chrono::system_clock` (or for any other clock). It is believed, however,
    // that there is a de-facto standard, that the Epoch for
    // `chrono::system_clock` is the Unix epoch, i.e., 1970-01-01T00:00:00Z. See
    // http://stackoverflow.com/a/29800557/1698548. Additionally, it is assumed
    // that leap seconds are not included in the value returned by
    // time_since_epoch(), i.e., that it conforms to POSIX time. This is known
    // to be true on Linux.
    //
    // FIXME: Investigate under which conditions OS X agrees with POSIX about
    // not including leap seconds in the value returned by time_since_epoch().
    //
    // FIXME: Investigate whether Microsoft Windows agrees with POSIX about
    // about not including leap seconds in the value returned by
    // time_since_epoch().
    auto time_since_epoch = chrono::system_clock::now().time_since_epoch();
    std::uint_fast64_t millis_since_epoch =
        chrono::duration_cast<chrono::milliseconds>(time_since_epoch).count();
    // `offset_in_millis` is the number of milliseconds between
    // 1970-01-01T00:00:00Z and 2015-01-01T00:00:00Z not counting leap seconds.
    std::uint_fast64_t offset_in_millis = 1420070400000ULL;
    return timestamp_type(millis_since_epoch - offset_in_millis);
}

inline void map_changeset_timestamp(timestamp_type timestamp, std::time_t& seconds_since_epoch,
                                    long& nanoseconds) noexcept
{
    std::uint_fast64_t offset_in_millis = 1420070400000ULL;
    std::uint_fast64_t millis_since_epoch = std::uint_fast64_t(offset_in_millis + timestamp);
    seconds_since_epoch = std::time_t(millis_since_epoch / 1000);
    nanoseconds = long(millis_since_epoch % 1000 * 1000000L);
}

class InconsistentUseOfCookedHistory : public std::exception {
public:
    InconsistentUseOfCookedHistory(const char* message) noexcept :
        m_message{message}
    {
    }
    const char* what() const noexcept override final
    {
        return m_message;
    }
private:
    const char* m_message;
};

class BadCookedServerVersion : public std::exception {
public:
    BadCookedServerVersion(const char* message) noexcept :
        m_message{message}
    {
    }
    const char* what() const noexcept override final
    {
        return m_message;
    }
private:
    const char* m_message;
};

inline ClientHistoryBase::ClientHistoryBase(const std::string& realm_path) :
    InstructionReplication{realm_path} // Throws
{
}

inline ClientHistory::ClientHistory(const std::string& realm_path) :
    ClientHistoryBase{realm_path} // Throws
{
}

inline std::int_fast64_t ClientHistory::get_num_cooked_changesets() const noexcept
{
    version_type server_version = 0; // Skip nothing
    std::int_fast64_t num_changesets = 0;
    ClientHistory::CookedProgress progress;
    std::int_fast64_t num_skipped_changesets = 0;
    get_cooked_status(server_version, num_changesets, progress, num_skipped_changesets);
    REALM_ASSERT(progress.changeset_index <= num_changesets);
    REALM_ASSERT(num_skipped_changesets == 0);
    return num_changesets;
}

inline auto ClientHistory::get_cooked_progress() const noexcept -> CookedProgress
{
    version_type server_version = 0; // Skip nothing
    std::int_fast64_t num_changesets = 0;
    ClientHistory::CookedProgress progress;
    std::int_fast64_t num_skipped_changesets = 0;
    get_cooked_status(server_version, num_changesets, progress, num_skipped_changesets);
    REALM_ASSERT(progress.changeset_index <= num_changesets);
    REALM_ASSERT(num_skipped_changesets == 0);
    return progress;
}

inline void ClientHistory::get_cooked_changeset(std::int_fast64_t index,
                                                util::AppendBuffer<char>& buffer) const
{
    version_type server_version; // Dummy
    get_cooked_changeset(index, buffer, server_version); // Throws
}

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_HISTORY_HPP
