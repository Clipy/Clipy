////////////////////////////////////////////////////////////////////////////
//
// Copyright 2015 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#ifndef REALM_COORDINATOR_HPP
#define REALM_COORDINATOR_HPP

#include "shared_realm.hpp"

#include <condition_variable>
#include <mutex>

namespace realm {
class Replication;
class Schema;
class SharedGroup;
class StringData;
class SyncSession;

namespace _impl {
class CollectionNotifier;
class ExternalCommitHelper;
class WeakRealmNotifier;

// RealmCoordinator manages the weak cache of Realm instances and communication
// between per-thread Realm instances for a given file
class RealmCoordinator : public std::enable_shared_from_this<RealmCoordinator> {
public:
    // Get the coordinator for the given path, creating it if neccesary
    static std::shared_ptr<RealmCoordinator> get_coordinator(StringData path);
    // Get the coordinator for the given config, creating it if neccesary
    static std::shared_ptr<RealmCoordinator> get_coordinator(const Realm::Config&);
    // Get the coordinator for the given path, or null if there is none
    static std::shared_ptr<RealmCoordinator> get_existing_coordinator(StringData path);

    // Get a thread-local shared Realm with the given configuration
    // If the Realm is already open on another thread, validates that the given
    // configuration is compatible with the existing one
    std::shared_ptr<Realm> get_realm(Realm::Config config);
    std::shared_ptr<Realm> get_realm();

    Realm::Config get_config() const { return m_config; }

    const Schema* get_schema() const noexcept;
    uint64_t get_schema_version() const noexcept { return m_schema_version; }
    const std::string& get_path() const noexcept { return m_config.path; }
    const std::vector<char>& get_encryption_key() const noexcept { return m_config.encryption_key; }
    bool is_in_memory() const noexcept { return m_config.in_memory; }

    // Asynchronously call notify() on every Realm instance for this coordinator's
    // path, including those in other processes
    void send_commit_notifications(Realm&);
    
    void wake_up_notifier_worker();

    // Clear the weak Realm cache for all paths
    // Should only be called in test code, as continuing to use the previously
    // cached instances will have odd results
    static void clear_cache();

    // Clears all caches on existing coordinators
    static void clear_all_caches();

    // Explicit constructor/destructor needed for the unique_ptrs to forward-declared types
    RealmCoordinator();
    ~RealmCoordinator();

    // Called by Realm's destructor to ensure the cache is cleaned up promptly
    // Do not call directly
    void unregister_realm(Realm* realm);

    // Called by m_notifier when there's a new commit to send notifications for
    void on_change();

    // Update the cached schema
    void update_schema(Schema const& new_schema, uint64_t new_schema_version);

    static void register_notifier(std::shared_ptr<CollectionNotifier> notifier);

    // Advance the Realm to the most recent transaction version which all async
    // work is complete for
    void advance_to_ready(Realm& realm);

    // Advance the Realm to the most recent transaction version, blocking if
    // async notifiers are not yet ready for that version
    // returns whether it actually changed the version
    bool advance_to_latest(Realm& realm);

    // Deliver any notifications which are ready for the Realm's version
    void process_available_async(Realm& realm);

    void set_transaction_callback(std::function<void(VersionID, VersionID)>);

    // Deliver notifications for the Realm, blocking if some aren't ready yet
    // The calling Realm must be in a write transaction
    void promote_to_write(Realm& realm);

    // Commit a Realm's current write transaction and send notifications to all
    // other Realm instances for that path, including in other processes
    void commit_write(Realm& realm);

    template<typename Pred>
    std::unique_lock<std::mutex> wait_for_notifiers(Pred&& wait_predicate);

private:
    Realm::Config m_config;
    Schema m_schema;
    uint64_t m_schema_version = -1;

    std::mutex m_realm_mutex;
    std::vector<WeakRealmNotifier> m_weak_realm_notifiers;

    std::mutex m_notifier_mutex;
    std::condition_variable m_notifier_cv;
    std::vector<std::shared_ptr<_impl::CollectionNotifier>> m_new_notifiers;
    std::vector<std::shared_ptr<_impl::CollectionNotifier>> m_notifiers;
    VersionID m_notifier_skip_version = {0, 0};

    // SharedGroup used for actually running async notifiers
    // Will have a read transaction iff m_notifiers is non-empty
    std::unique_ptr<Replication> m_notifier_history;
    std::unique_ptr<SharedGroup> m_notifier_sg;

    // SharedGroup used to advance notifiers in m_new_notifiers to the main shared
    // group's transaction version
    // Will have a read transaction iff m_new_notifiers is non-empty
    std::unique_ptr<Replication> m_advancer_history;
    std::unique_ptr<SharedGroup> m_advancer_sg;
    std::exception_ptr m_async_error;

    std::unique_ptr<_impl::ExternalCommitHelper> m_notifier;
    std::function<void(VersionID, VersionID)> m_transaction_callback;

    std::shared_ptr<SyncSession> m_sync_session;

    // must be called with m_notifier_mutex locked
    void pin_version(VersionID version);

    void set_config(const Realm::Config&);
    void create_sync_session();

    void run_async_notifiers();
    void open_helper_shared_group();
    void advance_helper_shared_group_to_latest();
    void clean_up_dead_notifiers();

    std::vector<std::shared_ptr<_impl::CollectionNotifier>> notifiers_for_realm(Realm&);
};


template<typename Pred>
std::unique_lock<std::mutex> RealmCoordinator::wait_for_notifiers(Pred&& wait_predicate)
{
    std::unique_lock<std::mutex> lock(m_notifier_mutex);
    bool first = true;
    m_notifier_cv.wait(lock, [&] {
        if (wait_predicate())
            return true;
        if (first) {
            wake_up_notifier_worker();
            first = false;
        }
        return false;
    });
    return lock;
}

} // namespace _impl
} // namespace realm

#endif /* REALM_COORDINATOR_HPP */
