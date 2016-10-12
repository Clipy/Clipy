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
#ifndef REALM_REPLICATION_HPP
#define REALM_REPLICATION_HPP

#include <algorithm>
#include <limits>
#include <memory>
#include <exception>
#include <string>

#include <realm/util/assert.hpp>
#include <realm/util/tuple.hpp>
#include <realm/util/safe_int_ops.hpp>
#include <realm/util/buffer.hpp>
#include <realm/util/string_buffer.hpp>
#include <realm/util/logger.hpp>
#include <realm/impl/continuous_transactions_history.hpp>
#include <realm/impl/transact_log.hpp>

namespace realm {

// FIXME: Be careful about the possibility of one modification function being called by another where both do transaction logging.

// FIXME: The current table/subtable selection scheme assumes that a TableRef of a subtable is not accessed after any modification of one of its ancestor tables.

// FIXME: Checking on same Table* requires that ~Table checks and nullifies on match. Another option would be to store m_selected_table as a TableRef. Yet another option would be to assign unique identifiers to each Table instance via Allocator. Yet another option would be to explicitely invalidate subtables recursively when parent is modified.

/// Replication is enabled by passing an instance of an implementation of this
/// class to the SharedGroup constructor.
class Replication:
        public _impl::TransactLogConvenientEncoder,
        protected _impl::TransactLogStream {
public:
    // Be sure to keep this type aligned with what is actually used in
    // SharedGroup.
    using version_type = _impl::History::version_type;
    using InputStream = _impl::NoCopyInputStream;
    class TransactLogApplier;
    class Interrupted; // Exception
    class SimpleIndexTranslator;

    virtual std::string get_database_path() = 0;

    /// Called during construction of the associated SharedGroup object.
    ///
    /// \param shared_group The assocoated SharedGroup object.
    virtual void initialize(SharedGroup& shared_group) = 0;

    /// Called by the associated SharedGroup object when a session is
    /// initiated. A *session* is a sequence of of temporally overlapping
    /// accesses to a specific Realm file, where each access consists of a
    /// SharedGroup object through which the Realm file is open. Session
    /// initiation occurs during the first opening of the Realm file within such
    /// a session.
    ///
    /// Session initiation fails if this function throws.
    ///
    /// \param version The current version of the associated Realm. Out-of-Realm
    /// history implementation can use this to trim off history entries that
    /// were successfully added to the history, but for which the corresponding
    /// subsequent commits on the Realm file failed.
    ///
    /// The default implementation does nothing.
    virtual void initiate_session(version_type version) = 0;

    /// Called by the associated SharedGroup object when a session is
    /// terminated. See initiate_session() for the definition of a
    /// session. Session termination occurs upon closing the Realm through the
    /// last SharedGroup object within the session.
    ///
    /// The default implementation does nothing.
    virtual void terminate_session() noexcept = 0;

    /// Called by the associated SharedGroup to close any open files
    /// or release similar system resources.
    ///
    /// This is a special purpose function that solves a problem that is
    /// specific to the implementation available through <commit_log.hpp>. At
    /// least for now, it is not to be considered a genuine part of the
    /// Replication interface. The default implementation does nothing and other
    /// implementations should not override this function.
    virtual void commit_log_close() noexcept {}

    //@{

    /// From the point of view of the Replication class, a transaction is
    /// initiated when, and only when the associated SharedGroup object calls
    /// initiate_transact() and the call is successful. The associated
    /// SharedGroup object must terminate every initiated transaction either by
    /// calling finalize_commit() or by calling abort_transact(). It may only
    /// call finalize_commit(), however, after calling prepare_commit(), and
    /// only when prepare_commit() succeeds. If prepare_commit() fails (i.e.,
    /// throws) abort_transact() must still be called.
    ///
    /// The associated SharedGroup object is supposed to terminate a transaction
    /// as soon as possible, and is required to terminate it before attempting
    /// to initiate a new one.
    ///
    /// initiate_transact() is called by the associated SharedGroup object as
    /// part of the initiation of a transaction, and at a time where the caller
    /// has acquired exclusive write access to the local Realm. The Replication
    /// implementation is allowed to perform "precursor transactions" on the
    /// local Realm at this time. During the initiated transaction, the
    /// associated SharedGroup object must inform the Replication object of all
    /// modifying operations by calling set_value() and friends.
    ///
    /// FIXME: There is currently no way for implementations to perform
    /// precursor transactions, since a regular transaction would cause a dead
    /// lock when it tries to acquire a write lock. Consider giving access to
    /// special non-locking precursor transactions via an extra argument to this
    /// function.
    ///
    /// prepare_commit() serves as the first phase of a two-phase commit. This
    /// function is called by the associated SharedGroup object immediately
    /// before the commit operation on the local Realm. The associated
    /// SharedGroup object will then, as the second phase, either call
    /// finalize_commit() or abort_transact() depending on whether the commit
    /// operation succeeded or not. The Replication implementation is allowed to
    /// modify the Realm via the associated SharedGroup object at this time
    /// (important to in-Realm histories).
    ///
    /// initiate_transact() and prepare_commit() are allowed to block the
    /// calling thread if, for example, they need to communicate over the
    /// network. If a calling thread is blocked in one of these functions, it
    /// must be possible to interrupt the blocking operation by having another
    /// thread call interrupt(). The contract is as follows: When interrupt() is
    /// called, then any execution of initiate_transact() or prepare_commit(),
    /// initiated before the interruption, must complete without blocking, or
    /// the execution must be aborted by throwing an Interrupted exception. If
    /// initiate_transact() or prepare_commit() throws Interrupted, it counts as
    /// a failed operation.
    ///
    /// finalize_commit() is called by the associated SharedGroup object
    /// immediately after a successful commit operation on the local Realm. This
    /// happens at a time where modification of the Realm is no longer possible
    /// via the associated SharedGroup object. In the case of in-Realm
    /// histories, the changes are automatically finalized as part of the commit
    /// operation performed by the caller prior to the invocation of
    /// finalize_commit(), so in that case, finalize_commit() might not need to
    /// do anything.
    ///
    /// abort_transact() is called by the associated SharedGroup object to
    /// terminate a transaction without committing. That is, any transaction
    /// that is not terminated by finalize_commit() is terminated by
    /// abort_transact(). This could be due to an explicit rollback, or due to a
    /// failed commit attempt.
    ///
    /// Note that finalize_commit() and abort_transact() are not allowed to
    /// throw.
    ///
    /// \param current_version The version of the snapshot that the current
    /// transaction is based on.
    ///
    /// \param history_updated Pass true only when the history has already been
    /// updated to reflect the currently bound snapshot, such as when
    /// _impl::History::update_early_from_top_ref() was called during the
    /// transition from a read transaction to the current write transaction.
    ///
    /// \return prepare_commit() returns the version of the new snapshot
    /// produced by the transaction.
    ///
    /// \throw Interrupted Thrown by initiate_transact() and prepare_commit() if
    /// a blocking operation was interrupted.

    void initiate_transact(version_type current_version, bool history_updated);
    version_type prepare_commit(version_type current_version);
    void finalize_commit() noexcept;
    void abort_transact() noexcept;

    //@}


    /// Interrupt any blocking call to a function in this class. This function
    /// may be called asyncronously from any thread, but it may not be called
    /// from a system signal handler.
    ///
    /// Some of the public function members of this class may block, but only
    /// when it it is explicitely stated in the documention for those functions.
    ///
    /// FIXME: Currently we do not state blocking behaviour for all the
    /// functions that can block.
    ///
    /// After any function has returned with an interruption indication, the
    /// only functions that may safely be called are abort_transact() and the
    /// destructor. If a client, after having received an interruption
    /// indication, calls abort_transact() and then clear_interrupt(), it may
    /// resume normal operation through this Replication object.
    void interrupt() noexcept;

    /// May be called by a client to reset this Replication object after an
    /// interrupted transaction. It is not an error to call this function in a
    /// situation where no interruption has occured.
    void clear_interrupt() noexcept;

    /// Apply a changeset to the specified group.
    ///
    /// \param logger If specified, and the library was compiled in debug mode,
    /// then a line describing each individual operation is writted to the
    /// specified logger.
    ///
    /// \throw BadTransactLog If the changeset could not be successfully parsed,
    /// or ended prematurely.
    static void apply_changeset(InputStream& changeset, Group&, util::Logger* logger = nullptr);

    enum HistoryType {
        /// No history available. No support for either continuous transactions
        /// or inter-client synchronization.
        hist_None = 0,

        /// Out-of-Realm history supporting continuous transactions.
        hist_OutOfRealm = 1,

        /// In-Realm history supporting continuous transactions
        /// (_impl::InRealmHistory).
        hist_InRealm = 2,

        /// In-Realm history supporting continuous transactions and inter-client
        /// synchronization (_impl::SyncHistory).
        hist_Sync = 3
    };

    /// Returns the type of history maintained by this Replication
    /// implementation, or \ref hist_None if no history is maintained by it.
    ///
    /// This type is used to ensure that all session participants agree on
    /// history type, and that the Realm file contains a compatible type of
    /// history, at the beginning of a new session.
    ///
    /// As a special case, if there is no top array (Group::m_top) at the
    /// beginning of a new session, then all history types (as returned by
    /// get_history_type()) are allowed during that session. Note that this is
    /// only possible if there was no preceding session, or if no transaction
    /// was sucessfully comitted during any of the preceding sessions. As soon
    /// as a transaction is successfully committed, the Realm contains at least
    /// a top array, and from that point on, the history type is generally
    /// fixed, although still subject to certain allowed changes (as mentioned
    /// below).
    ///
    /// For the sake of backwards compatibility with older Realm files that does
    /// not store any history type, the following rule shall apply:
    ///
    ///   - If the top array of a Realm file (Group::m_top) does not contain a
    ///     history type, because it is too short, it shall be understood as
    ///     implicitely storing the type \ref hist_None.
    ///
    /// Note: In what follows, the meaning of *preceding session* is: The last
    /// preceding session that modified the Realm by sucessfully committing a
    /// new snapshot.
    ///
    /// Older Realm files do not store any history type, even when they were
    /// last used with a history of type \ref hist_OutOfRealm. Howewver, since
    /// such histories (\ref hist_OutOfRealm) are placed outside the Realm file,
    /// and are transient (recreated at the beginning of each new session), a
    /// new session is not obliged to use the same type of history (\ref
    /// hist_OutOfRealm). For this reason, and to achieve further backwards
    /// compatibility, the following rules are adopted:
    ///
    ///   - At the beginning of a new session, if there is no stored history
    ///     type (no top array), or if the stored history type is \ref
    ///     hist_None, assume that the history type used during the preceding
    ///     session was \ref hist_None or \ref hist_OutOfRealm, or that there
    ///     was no preceding session. In all other cases, assume that the stored
    ///     history type is the type used during the preceding session.
    ///
    ///   - When storing the history type, store \ref hist_None if the history
    ///     type used in the current session is \ref hist_None or \ref
    ///     hist_OutOfRealm. In all other cases, store the actual history type
    ///     used.
    ///
    /// It shall be allowed to switch to a \ref hist_InRealm history if the
    /// stored history type is either \ref hist_None or \ref
    /// hist_OutOfRealm. Fortunately, this can be done simply by adding a
    /// history to the Realm file (of type \ref hist_InRealm), and that is
    /// possible because a \ref hist_InRealm history is independent of any
    /// history used in a previous session (as long as it was session-confined),
    /// or whether any history was used at all. Conversely, if a \ref
    /// hist_OutOfRealm history was used in the previous session, then the
    /// contents of that history becomes obsolete at the end of the previous
    /// session.
    ///
    /// On the other hand, as soon as a history of type \ref hist_InRealm is
    /// added to a Realm file, that history type is binding for all subsequent
    /// sessions. In theory, this constraint is not necessary, and a later
    /// switch to \ref hist_None or \ref hist_OutOfRealm would be possible
    /// because of the fact that the contents of the history becomes obsolete at
    /// the end of the session, however, because the \ref hist_InRealm history
    /// remains in the Realm file, there are practical complications, and for
    /// that reason, such switching shall not be supported.
    ///
    /// The \ref hist_Sync history type can only be used if the stored history
    /// type is also \ref hist_Sync, or when there is no top array
    /// yet. Additionally, when the stored history type is \ref hist_Sync, then
    /// all subsequent sesssions must have the same type. These restrictions
    /// apply because such a history needs to be maintained persistently across
    /// sessions. That is, the contents of such a history is not obsolete at the
    /// end of the session, and is in general needed during subsequent sessions.
    ///
    /// In general, if there is no stored history type (no top array) at the
    /// beginning of a new session, or if the stored type disagrees with what is
    /// returned by get_history_type() (which is possible due to particular
    /// allowed changes of history type), the actual history type (as returned
    /// by get_history_type()) used during that session, must be stored in the
    /// Realm during the first successfully committed transaction of that
    /// session, if any are sucessfully committed. But note that there is still
    /// no need to expand the top array to store the history type \ref
    /// hist_None, due to the rule mentioned above.
    ///
    /// Due to the rules listed above, a new history type only actually needs to
    /// be stored when the history type of the session (get_history_type()) is
    /// neither \ref hist_None nor \ref hist_OutOfRealm, and only when that
    /// differs from the stored history type, or if there is no top array at the
    /// beginning of the session.
    ///
    /// Summary of session-to-session history type change constraints:
    ///
    /// If there is no top array at the beginning of a new session, then all
    /// history types (as returned by get_history_type()) are possible during
    /// that session. Otherwise there must have been a preceding session (at
    /// least one that adds the top array), and the following rules then apply:
    ///
    /// <pre>
    ///
    ///                      Type stored in
    ///   Type used during   Realm file at
    ///   preceding          beginning of     Possible history types (as returned by
    ///   session            new session      get_history_type()) during new session
    ///   ----------------------------------------------------------------------------
    ///   hist_None          hist_None        hist_None, hist_OutOfRealm, hist_InRealm
    ///   hist_OutOfRealm    hist_None        hist_None, hist_OutOfRealm, hist_InRealm
    ///   hist_InRealm       hist_InRealm     hist_InRealm
    ///   hist_Sync          hist_Sync        hist_Sync
    ///
    /// </pre>
    ///
    /// This function must return \ref hist_None when, and only when
    /// get_history() returns null.
    virtual HistoryType get_history_type() const noexcept = 0;

    /// Returns an object that gives access to the history of changesets in a
    /// way that allows for continuous transactions to work
    /// (Group::advance_transact() in particular).
    ///
    /// This function must return null when, and only when get_history_type()
    /// returns \ref hist_None.
    virtual _impl::History* get_history() = 0;

    virtual ~Replication() noexcept {}

protected:
    Replication();


    //@{

    /// do_initiate_transact() is called by initiate_transact(), and likewise
    /// for do_prepare_commit), do_finalize_commit(), and do_abort_transact().
    ///
    /// With respect to exception safety, the Replication implementation has two
    /// options: It can prepare to accept the accumulated changeset in
    /// do_prepapre_commit() by allocating all required resources, and delay the
    /// actual acceptance to do_finalize_commit(), which requires that the final
    /// acceptance can be done without any risk of failure. Alternatively, the
    /// Replication implementation can fully accept the changeset in
    /// do_prepapre_commit() (allowing for failure), and then discard that
    /// changeset during the next invocation of do_initiate_transact() if
    /// `current_version` indicates that the previous transaction failed.

    virtual void do_initiate_transact(version_type current_version, bool history_updated) = 0;
    virtual version_type do_prepare_commit(version_type orig_version) = 0;
    virtual void do_finalize_commit() noexcept = 0;
    virtual void do_abort_transact() noexcept = 0;

    //@}


    virtual void do_interrupt() noexcept = 0;

    virtual void do_clear_interrupt() noexcept = 0;

    // Part of a temporary ugly hack to avoid generating new transaction logs
    // during application of ones that have olready been created elsewhere. See
    // ReplicationImpl::do_initiate_transact() in
    // realm/replication/simplified/provider.cpp for more on this.
    static void set_replication(Group&, Replication*) noexcept;

    friend class _impl::TransactReverser;
};


class Replication::Interrupted: public std::exception {
public:
    const char* what() const noexcept override
    {
        return "Interrupted";
    }
};


class TrivialReplication: public Replication {
public:
    ~TrivialReplication() noexcept {}

protected:
    typedef Replication::version_type version_type;

    TrivialReplication(const std::string& database_file);

    virtual version_type prepare_changeset(const char* data, size_t size,
                                           version_type orig_version) = 0;
    virtual void finalize_changeset() noexcept = 0;

    static void apply_changeset(const char* data, size_t size, SharedGroup& target,
                                util::Logger* logger = nullptr);

    bool is_history_updated() const noexcept;

    BinaryData get_uncommitted_changes() const noexcept;

    std::string get_database_path() override;
    void initialize(SharedGroup&) override;
    void do_initiate_transact(version_type, bool) override;
    version_type do_prepare_commit(version_type orig_version) override;
    void do_finalize_commit() noexcept override;
    void do_abort_transact() noexcept override;
    void do_interrupt() noexcept override;
    void do_clear_interrupt() noexcept override;
    void transact_log_reserve(size_t n, char** new_begin, char** new_end) override;
    void transact_log_append(const char* data, size_t size, char** new_begin,
                             char** new_end) override;

private:
    const std::string m_database_file;
    util::Buffer<char> m_transact_log_buffer;
    bool m_history_updated;
    void internal_transact_log_reserve(size_t, char** new_begin, char** new_end);

    size_t transact_log_size();
};




// Implementation:

inline Replication::Replication():
    _impl::TransactLogConvenientEncoder(static_cast<_impl::TransactLogStream&>(*this))
{
}

inline void Replication::initiate_transact(version_type current_version, bool history_updated)
{
    do_initiate_transact(current_version, history_updated);
    reset_selection_caches();
}

inline Replication::version_type Replication::prepare_commit(version_type orig_version)
{
    return do_prepare_commit(orig_version);
}

inline void Replication::finalize_commit() noexcept
{
    do_finalize_commit();
}

inline void Replication::abort_transact() noexcept
{
    do_abort_transact();
}

inline void Replication::interrupt() noexcept
{
    do_interrupt();
}

inline void Replication::clear_interrupt() noexcept
{
    do_clear_interrupt();
}

inline TrivialReplication::TrivialReplication(const std::string& database_file):
    m_database_file(database_file)
{
}

inline bool TrivialReplication::is_history_updated() const noexcept
{
    return m_history_updated;
}

inline BinaryData TrivialReplication::get_uncommitted_changes() const noexcept
{
    const char* data = m_transact_log_buffer.data();
    size_t size = write_position() - data;
    return BinaryData(data, size);
}

inline size_t TrivialReplication::transact_log_size()
{
    return write_position() - m_transact_log_buffer.data();
}

inline void TrivialReplication::transact_log_reserve(size_t n, char** new_begin, char** new_end)
{
    internal_transact_log_reserve(n, new_begin, new_end);
}

inline void TrivialReplication::internal_transact_log_reserve(size_t n, char** new_begin, char** new_end)
{
    char* data = m_transact_log_buffer.data();
    size_t size = write_position() - data;
    m_transact_log_buffer.reserve_extra(size, n);
    data = m_transact_log_buffer.data(); // May have changed
    *new_begin = data + size;
    *new_end = data + m_transact_log_buffer.size();
}

} // namespace realm

#endif // REALM_REPLICATION_HPP
