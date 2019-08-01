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

#ifndef REALM_IMPL_CONT_TRANSACT_HIST_HPP
#define REALM_IMPL_CONT_TRANSACT_HIST_HPP

#include <cstdint>
#include <memory>

#include <realm/column_binary.hpp>
#include <realm/version_id.hpp>

namespace realm {

class Group;

namespace _impl {

/// Read-only access to history of changesets as needed to enable continuous
/// transactions.
class History {
public:
    using version_type = VersionID::version_type;

    virtual ~History() noexcept {}

    /// May be called during any transaction
    ///
    /// It is a precondition for calls to this function that the reader view is
    /// updated - that is, the mapping is updated to provide full visibility to
    /// the file.
    ///
    virtual void update_from_ref_and_version(ref_type ref, version_type version) = 0;
    virtual void update_from_parent(version_type version) = 0;

    /// Get all changesets between the specified versions. References to those
    /// changesets will be made available in successive entries of `buffer`. The
    /// number of retrieved changesets is exactly `end_version -
    /// begin_version`. If this number is greater than zero, the changeset made
    /// available in `buffer[0]` is the one that brought the database from
    /// `begin_version` to `begin_version + 1`.
    ///
    /// It is an error to specify a version (for \a begin_version or \a
    /// end_version) that is outside the range [V,W] where V is the version that
    /// immediately precedes the first changeset available in the history as the
    /// history appears in the **latest** available snapshot, and W is the
    /// version that immediately succeeds the last changeset available in the
    /// history as the history appears in the snapshot bound to the **current**
    /// transaction. This restriction is necessary to allow for different kinds
    /// of implementations of the history (separate standalone history or
    /// history as part of versioned Realm state).
    ///
    /// The callee retains ownership of the memory referenced by those entries,
    /// i.e., the memory referenced by `buffer[i].changeset` is **not** handed
    /// over to the caller.
    ///
    /// This function may be called only during a transaction (prior to
    /// initiation of commit operation), and only after a successful invocation
    /// of update_early_from_top_ref(). In that case, the caller may assume that
    /// the memory references stay valid for the remainder of the transaction
    /// (up until initiation of the commit operation).
    virtual void get_changesets(version_type begin_version, version_type end_version, BinaryIterator* buffer) const
        noexcept = 0;

    /// \brief Specify the version of the oldest bound snapshot.
    ///
    /// This function must be called by the associated SharedGroup object during
    /// each successfully committed write transaction. It must be called before
    /// the transaction is finalized (Replication::finalize_commit()) or aborted
    /// (Replication::abort_transact()), but after the initiation of the commit
    /// operation (Replication::prepare_commit()). This allows history
    /// implementations to add new history entries before trimming off old ones,
    /// and this, in turn, guarantees that the history never becomes empty,
    /// except in the initial empty Realm state.
    ///
    /// The caller must pass the version (\a version) of the oldest snapshot
    /// that is currently (or was recently) bound via a transaction of the
    /// current session. This gives the history implementation an opportunity to
    /// trim off leading (early) history entries.
    ///
    /// Since this function must be called during a write transaction, there
    /// will always be at least one snapshot that is currently bound via a
    /// transaction.
    ///
    /// The caller must guarantee that the passed version (\a version) is less
    /// than or equal to `begin_version` in all future invocations of
    /// get_changesets().
    ///
    /// The caller is allowed to pass a version that is less than the version
    /// passed in a preceding invocation.
    ///
    /// This function should be called as late as possible, to maximize the
    /// trimming opportunity, but at a time where the write transaction is still
    /// open for additional modifications. This is necessary because some types
    /// of histories are stored inside the Realm file.
    virtual void set_oldest_bound_version(version_type version) = 0;

    /// Get the list of uncommitted changes accumulated so far in the current
    /// write transaction.
    ///
    /// The callee retains ownership of the referenced memory. The ownership is
    /// not handed over to the caller.
    ///
    /// This function may be called only during a write transaction (prior to
    /// initiation of commit operation). In that case, the caller may assume that the
    /// returned memory reference stays valid for the remainder of the transaction (up
    /// until initiation of the commit operation).
    virtual BinaryData get_uncommitted_changes() noexcept = 0;

    virtual void verify() const = 0;

    void set_updated(bool updated)
    {
        m_updated = updated;
    }

    void ensure_updated(version_type version) const
    {
        if (!m_updated) {
            const_cast<History*>(this)->update_from_parent(version);
            m_updated = true;
        }
    }

private:
    mutable bool m_updated = false;
};

} // namespace _impl
} // namespace realm

#endif // REALM_IMPL_CONTINUOUS_TRANSACTIONS_HISTORY_HPP
