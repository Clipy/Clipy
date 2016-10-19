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

#ifndef REALM_IMPL_CONTINUOUS_TRANSACTIONS_HISTORY_HPP
#define REALM_IMPL_CONTINUOUS_TRANSACTIONS_HISTORY_HPP

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

    /// May be called during a read transaction to gain early access to the
    /// history as it appears in a new snapshot that succeeds the one bound in
    /// the current read transaction.
    ///
    /// May also be called at other times as long as the caller owns a read lock
    /// (SharedGroup::grab_read_lock()) on the Realm for the specified file size
    /// and top ref, and the allocator is in a 'free space clean' state
    /// (SlabAlloc::is_free_space_clean()).
    ///
    /// This function may cause a remapping of the Realm file
    /// (SlabAlloc::remap()) if it needs to make the new snapshot fully visible
    /// in memory.
    ///
    /// Note that this method of gaining early access to the history in a new
    /// snaphot only gives read access. It does not allow for modifications of
    /// the history or any other part of the new snapshot. For modifications to
    /// be allowed, `Group::m_top` (the parent of the history) would first have
    /// to be updated to reflect the new snapshot, but at that time we are no
    /// longer in an 'early access' situation.
    ///
    /// This is not a problem from the point of view of this history interface,
    /// as it only contains methods for reading from the history, but some
    /// implementations will want to also provide for ways to modify the
    /// history, but in those cases, modifications must occur only after the
    /// Group accessor has been fully updated to reflect the new snapshot.
    virtual void update_early_from_top_ref(version_type new_version, size_t new_file_size, ref_type new_top_ref) = 0;

    virtual void update_from_parent(version_type current_version) = 0;

    /// Get all changesets between the specified versions. References to those
    /// changesets will be made availble in successive entries of `buffer`. The
    /// number of retreived changesets is exactly `end_version -
    /// begin_version`. If this number is greater than zero, the changeset made
    /// avaialable in `buffer[0]` is the one that brought the database from
    /// `begin_version` to `begin_version + 1`.
    ///
    /// It is an error to specify a version (for \a begin_version or \a
    /// end_version) that is outside the range [V,W] where V is the version that
    /// immediately precedes the first changeset available in the history as the
    /// history appears in the **latest** available snapshot, and W is the
    /// versionm that immediately succeeds the last changeset available in the
    /// history as the history appears in the snapshot bound to the **current**
    /// transaction. This restriction is necessary to allow for different kinds
    /// of implementations of the history (separate standalone history or
    /// history as part of versioned Realm state).
    ///
    /// The calee retains ownership of the memory referenced by those entries,
    /// i.e., the memory referenced by `buffer[i].changeset` is **not** handed
    /// over to the caller.
    ///
    /// This function may be called only during a transaction (prior to
    /// initiation of commit operation), and only after a successfull invocation
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
    /// implementations to add new history entries before triming off old ones,
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
    /// passed in a preceeding invocation.
    ///
    /// This function should be called as late as possible, to maximize the
    /// trimming opportunity, but at a time where the write transaction is still
    /// open for additional modifications. This is necessary because some types
    /// of histories are stored inside the Realm file.
    virtual void set_oldest_bound_version(version_type version) = 0;

    /// Get the list of uncommited changes accumulated so far in the current
    /// write transaction.
    ///
    /// The callee retains ownership of the referenced memory. The ownership is
    /// not handed over the the caller.
    ///
    /// This function may be called only during a write transaction (prior to
    /// initiation of commit operation). In that case, the caller may assume that the
    /// returned memory reference stays valid for the remainder of the transaction (up
    /// until initiation of the commit operation).
    virtual BinaryData get_uncommitted_changes() noexcept = 0;

    virtual void verify() const = 0;

    virtual ~History() noexcept
    {
    }
};


/// This class is intended to eventually become a basis for implementing the
/// Replication API for the purpose of supporting continuous transactions. That
/// is, its purpose is to replace the current implementation in commit_log.cpp,
/// which places the history in separate files.
///
/// By ensuring that the root node of the history is correctly configured with
/// Group::m_top as its parent, this class allows for modifications of the
/// history as long as those modifications happen after the remainder of the
/// Group accessor is updated to reflect the new snapshot (see
/// History::update_early_from_top_ref()).
class InRealmHistory : public History {
public:
    void initialize(Group&);

    /// Must never be called more than once per transaction. Returns the version
    /// produced by the added changeset.
    version_type add_changeset(BinaryData);

    void update_early_from_top_ref(version_type, size_t, ref_type) override;
    void update_from_parent(version_type) override;
    void get_changesets(version_type, version_type, BinaryIterator*) const noexcept override;
    void set_oldest_bound_version(version_type) override;

    void verify() const override;

private:
    Group* m_group = nullptr;

    /// Version on which the first changeset in the history is based, or if the
    /// history is empty, the version associatede with currently bound
    /// snapshot. In general, the version associatede with currently bound
    /// snapshot is equal to `m_base_version + m_size`, but after
    /// add_changeset() is called, it is equal to one minus that.
    version_type m_base_version;

    /// Current number of entries in the history. A cache of
    /// `m_changesets->size()`.
    size_t m_size;

    /// A list of changesets, one for each entry in the history. If null, the
    /// history is empty.
    ///
    /// FIXME: Ideally, the B+tree accessor below should have been just
    /// Bptree<BinaryData>, but Bptree<BinaryData> seems to not allow that yet.
    ///
    /// FIXME: The memory-wise indirection is an unfortunate consequence of the
    /// fact that it is impossible to construct a BinaryColumn without already
    /// having a ref to a valid underlying node structure. This, in turn, is an
    /// unfortunate consequence of the fact that a column accessor contains a
    /// dynamically allocated root node accessor, and the type of the required
    /// root node accessor depends on the size of the B+-tree.
    std::unique_ptr<BinaryColumn> m_changesets;

    void update_from_ref(ref_type, version_type);
};

} // namespace _impl
} // namespace realm

#endif // REALM_IMPL_CONTINUOUS_TRANSACTIONS_HISTORY_HPP
