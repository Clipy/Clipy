/*************************************************************************
 *
 * Copyright 2016 Realm Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
#include <realm/util/safe_int_ops.hpp>
#include <realm/util/buffer.hpp>
#include <realm/util/string_buffer.hpp>
#include <realm/impl/cont_transact_hist.hpp>
#include <realm/impl/transact_log.hpp>

namespace realm {
namespace util {
class Logger;
}

// FIXME: Be careful about the possibility of one modification function being called by another where both do
// transaction logging.

// FIXME: The current table/subtable selection scheme assumes that a TableRef of a subtable is not accessed after any
// modification of one of its ancestor tables.

// FIXME: Checking on same Table* requires that ~Table checks and nullifies on match. Another option would be to store
// m_selected_table as a TableRef. Yet another option would be to assign unique identifiers to each Table instance via
// Allocator. Yet another option would be to explicitely invalidate subtables recursively when parent is modified.

/// Replication is enabled by passing an instance of an implementation of this
/// class to the SharedGroup constructor.
class Replication : public _impl::TransactLogConvenientEncoder, protected _impl::TransactLogStream {
public:
    // Be sure to keep this type aligned with what is actually used in
    // SharedGroup.
    using version_type = _impl::History::version_type;
    using InputStream = _impl::NoCopyInputStream;
    class TransactLogApplier;
    class Interrupted; // Exception
    class SimpleIndexTranslator;

    enum class TransactionType { trans_Read, trans_Write };

    /// CAUTION: These values are stored in Realm files, so value reassignment
    /// is not allowed.
    enum HistoryType {
        /// No history available. No support for either continuous transactions
        /// or inter-client synchronization.
        hist_None = 0,

        /// Out-of-Realm history supporting continuous transactions.
        ///
        /// NOTE: This history type is no longer in use. The value needs to stay
        /// reserved in case someone tries to open an old Realm file.
        hist_OutOfRealm = 1,

        /// In-Realm history supporting continuous transactions
        /// (make_in_realm_history()).
        hist_InRealm = 2,

        /// In-Realm history supporting continuous transactions and client-side
        /// synchronization protocol (realm::sync::ClientHistory).
        hist_SyncClient = 3,

        /// In-Realm history supporting continuous transactions and server-side
        /// synchronization protocol (realm::_impl::ServerHistory).
        hist_SyncServer = 4
    };

    virtual std::string get_database_path() const = 0;

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

    /// \defgroup replication_transactions
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

    void initiate_transact(TransactionType transaction_type, version_type current_version, bool history_updated);
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
    /// \param changeset The changes to be applied.
    ///
    /// \param group The destination group to apply the changeset to.
    ///
    /// \param logger If specified, and the library was compiled in debug mode,
    /// then a line describing each individual operation is writted to the
    /// specified logger.
    ///
    /// \throw BadTransactLog If the changeset could not be successfully parsed,
    /// or ended prematurely.
    static void apply_changeset(InputStream& changeset, Group& group, util::Logger* logger = nullptr);

    /// Returns the type of history maintained by this Replication
    /// implementation, or \ref hist_None if no history is maintained by it.
    ///
    /// This type is used to ensure that all session participants agree on
    /// history type, and that the Realm file contains a compatible type of
    /// history, at the beginning of a new session.
    ///
    /// As a special case, if there is no top array (Group::m_top) at the
    /// beginning of a new session, then the history type is still undecided and
    /// all history types (as returned by get_history_type()) are threfore
    /// allowed for the session initiator. Note that this case only arises if
    /// there was no preceding session, or if no transaction was sucessfully
    /// committed during any of the preceding sessions. As soon as a transaction
    /// is successfully committed, the Realm contains at least a top array, and
    /// from that point on, the history type is generally fixed, although still
    /// subject to certain allowed changes (as mentioned below).
    ///
    /// For the sake of backwards compatibility with older Realm files that does
    /// not store any history type, the following rule shall apply:
    ///
    ///   - If the top array of a Realm file (Group::m_top) does not contain a
    ///     history type, because it is too short, it shall be understood as
    ///     implicitly storing the type \ref hist_None.
    ///
    /// Note: In what follows, the meaning of *preceding session* is: The last
    /// preceding session that modified the Realm by sucessfully committing a
    /// new snapshot.
    ///
    /// It shall be allowed to switch to a \ref hist_InRealm history if the
    /// stored history type is \ref hist_None. This can be done simply by adding
    /// a new history to the Realm file. This is possible because histories of
    /// this type a transient in nature, and need not survive from one session
    /// to the next.
    ///
    /// On the other hand, as soon as a history of type \ref hist_InRealm is
    /// added to a Realm file, that history type is binding for all subsequent
    /// sessions. In theory, this constraint is not necessary, and a later
    /// switch to \ref hist_None would be possible because of the transient
    /// nature of it, however, because the \ref hist_InRealm history remains in
    /// the Realm file, there are practical complications, and for that reason,
    /// such switching shall not be supported.
    ///
    /// The \ref hist_SyncClient history type can only be used if the stored
    /// history type is also \ref hist_SyncClient, or when there is no top array
    /// yet. Likewise, the \ref hist_SyncServer history type can only be used if
    /// the stored history type is also \ref hist_SyncServer, or when there is
    /// no top array yet. Additionally, when the stored history type is \ref
    /// hist_SyncClient or \ref hist_SyncServer, then all subsequent sessions
    /// must have the same type. These restrictions apply because such a history
    /// needs to be maintained persistently across sessions.
    ///
    /// In general, if there is no stored history type (no top array) at the
    /// beginning of a new session, or if the stored type disagrees with what is
    /// returned by get_history_type() (which is possible due to particular
    /// allowed changes of history type), the actual history type (as returned
    /// by get_history_type()) used during that session, must be stored in the
    /// Realm during the first successfully committed transaction in that
    /// session. But note that there is still no need to expand the top array to
    /// store the history type \ref hist_None, due to the rule mentioned above.
    ///
    /// This function must return \ref hist_None when, and only when
    /// get_history() returns null.
    virtual HistoryType get_history_type() const noexcept = 0;

    /// Returns the schema version of the history maintained by this Replication
    /// implementation, or 0 if no history is maintained by it. All session
    /// participants must agree on history schema version.
    ///
    /// Must return 0 if get_history_type() returns \ref hist_None.
    virtual int get_history_schema_version() const noexcept = 0;

    /// Implementation may assume that this function is only ever called with a
    /// stored schema version that is less than what was returned by
    /// get_history_schema_version().
    virtual bool is_upgradable_history_schema(int stored_schema_version) const noexcept = 0;

    /// The implementation may assume that this function is only ever called if
    /// is_upgradable_history_schema() was called with the same stored schema
    /// version, and returned true. This implies that the specified stored
    /// schema version is always strictly less than what was returned by
    /// get_history_schema_version().
    virtual void upgrade_history_schema(int stored_schema_version) = 0;

    /// Returns an object that gives access to the history of changesets in a
    /// way that allows for continuous transactions to work
    /// (Group::advance_transact() in particular).
    ///
    /// This function must return null when, and only when get_history_type()
    /// returns \ref hist_None.
    virtual _impl::History* get_history() = 0;

    /// Returns false by default, but must return true if, and only if this
    /// history object represents a session participant that is a sync
    /// agent. This is used to enforce the "maximum one sync agent per session"
    /// constraint.
    virtual bool is_sync_agent() const noexcept;

    virtual ~Replication() noexcept
    {
    }

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

    virtual void do_initiate_transact(TransactionType, version_type current_version) = 0;
    virtual version_type do_prepare_commit(version_type orig_version) = 0;
    virtual void do_finalize_commit() noexcept = 0;
    virtual void do_abort_transact() noexcept = 0;

    //@}


    virtual void do_interrupt() noexcept = 0;

    virtual void do_clear_interrupt() noexcept = 0;

    friend class _impl::TransactReverser;
};


class Replication::Interrupted : public std::exception {
public:
    const char* what() const noexcept override
    {
        return "Interrupted";
    }
};


class TrivialReplication : public Replication {
public:
    ~TrivialReplication() noexcept
    {
    }

    std::string get_database_path() const override;
protected:
    typedef Replication::version_type version_type;

    TrivialReplication(const std::string& database_file);

    virtual version_type prepare_changeset(const char* data, size_t size, version_type orig_version) = 0;
    virtual void finalize_changeset() noexcept = 0;

    static void apply_changeset(const char* data, size_t size, SharedGroup& target, util::Logger* logger = nullptr);

    BinaryData get_uncommitted_changes() const noexcept;

    void initialize(SharedGroup&) override;
    void do_initiate_transact(TransactionType, version_type) override;
    version_type do_prepare_commit(version_type orig_version) override;
    void do_finalize_commit() noexcept override;
    void do_abort_transact() noexcept override;
    void do_interrupt() noexcept override;
    void do_clear_interrupt() noexcept override;
    void transact_log_reserve(size_t n, char** new_begin, char** new_end) override;
    void transact_log_append(const char* data, size_t size, char** new_begin, char** new_end) override;

private:
    const std::string m_database_file;
    util::Buffer<char> m_transact_log_buffer;
    void internal_transact_log_reserve(size_t, char** new_begin, char** new_end);

    size_t transact_log_size();
};


// Implementation:

inline Replication::Replication()
    : _impl::TransactLogConvenientEncoder(static_cast<_impl::TransactLogStream&>(*this))
{
}

inline void Replication::initiate_transact(TransactionType transaction_type, version_type current_version,
                                           bool history_updated)
{
    if (auto hist = get_history()) {
        hist->set_updated(history_updated);
    }
    do_initiate_transact(transaction_type, current_version);
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

inline bool Replication::is_sync_agent() const noexcept
{
    return false;
}

inline TrivialReplication::TrivialReplication(const std::string& database_file)
    : m_database_file(database_file)
{
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
