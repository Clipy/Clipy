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

#ifndef REALM_RESULTS_HPP
#define REALM_RESULTS_HPP

#include "collection_notifications.hpp"
#include "shared_realm.hpp"
#include "impl/collection_notifier.hpp"

#include <realm/table_view.hpp>
#include <realm/util/optional.hpp>

namespace realm {
template<typename T> class BasicRowExpr;
using RowExpr = BasicRowExpr<Table>;
class Mixed;
class ObjectSchema;

namespace _impl {
    class ResultsNotifier;
}

class Results {
public:
    // Results can be either be backed by nothing, a thin wrapper around a table,
    // or a wrapper around a query and a sort order which creates and updates
    // the tableview as needed
    Results();
    Results(SharedRealm r, Table& table);
    Results(SharedRealm r, Query q, SortDescriptor s = {}, SortDescriptor d = {});
    Results(SharedRealm r, TableView tv, SortDescriptor s = {}, SortDescriptor d = {});
    Results(SharedRealm r, LinkViewRef lv, util::Optional<Query> q = {}, SortDescriptor s = {});
    ~Results();

    // Results is copyable and moveable
    Results(Results&&);
    Results& operator=(Results&&);
    Results(const Results&);
    Results& operator=(const Results&);

    // Get the Realm
    SharedRealm get_realm() const { return m_realm; }

    // Object schema describing the vendored object type
    const ObjectSchema &get_object_schema() const;

    // Get a query which will match the same rows as is contained in this Results
    // Returned query will not be valid if the current mode is Empty
    Query get_query() const;

    // Get the currently applied sort order for this Results
    SortDescriptor const& get_sort() const noexcept { return m_sort; }

    // Get the currently applied distinct condition for this Results
    SortDescriptor const& get_distinct() const noexcept { return m_distinct; }
    
    // Get a tableview containing the same rows as this Results
    TableView get_tableview();

    // Get the object type which will be returned by get()
    StringData get_object_type() const noexcept;

    // Get the LinkView this Results is derived from, if any
    LinkViewRef get_linkview() const { return m_link_view; }

    // Get the size of this results
    // Can be either O(1) or O(N) depending on the state of things
    size_t size();

    // Get the row accessor for the given index
    // Throws OutOfBoundsIndexException if index >= size()
    RowExpr get(size_t index);

    // Get a row accessor for the first/last row, or none if the results are empty
    // More efficient than calling size()+get()
    util::Optional<RowExpr> first();
    util::Optional<RowExpr> last();

    // Get the first index of the given row in this results, or not_found
    // Throws DetachedAccessorException if row is not attached
    // Throws IncorrectTableException if row belongs to a different table
    size_t index_of(size_t row_ndx);
    size_t index_of(Row const& row);

    // Delete all of the rows in this Results from the Realm
    // size() will always be zero afterwards
    // Throws InvalidTransactionException if not in a write transaction
    void clear();

    // Create a new Results by further filtering or sorting this Results
    Results filter(Query&& q) const;
    Results sort(SortDescriptor&& sort) const;

    // Create a new Results by removing duplicates
    // FIXME: The current implementation of distinct() breaks the Results API.
    // This is tracked by the following issues:
    // - https://github.com/realm/realm-object-store/issues/266
    // - https://github.com/realm/realm-core/issues/2332
    Results distinct(SortDescriptor&& uniqueness);
    
    // Return a snapshot of this Results that never updates to reflect changes in the underlying data.
    Results snapshot() const &;
    Results snapshot() &&;

    // Get the min/max/average/sum of the given column
    // All but sum() returns none when there are zero matching rows
    // sum() returns 0, except for when it returns none
    // Throws UnsupportedColumnTypeException for sum/average on timestamp or non-numeric column
    // Throws OutOfBoundsIndexException for an out-of-bounds column
    util::Optional<Mixed> max(size_t column);
    util::Optional<Mixed> min(size_t column);
    util::Optional<Mixed> average(size_t column);
    util::Optional<Mixed> sum(size_t column);

    enum class Mode {
        Empty, // Backed by nothing (for missing tables)
        Table, // Backed directly by a Table
        Query, // Backed by a query that has not yet been turned into a TableView
        LinkView,  // Backed directly by a LinkView
        TableView, // Backed by a TableView created from a Query
    };
    // Get the currrent mode of the Results
    // Ideally this would not be public but it's needed for some KVO stuff
    Mode get_mode() const { return m_mode; }

    // Is this Results associated with a Realm that has not been invalidated?
    bool is_valid() const;

    // The Results object has been invalidated (due to the Realm being invalidated)
    // All non-noexcept functions can throw this
    struct InvalidatedException : public std::logic_error {
        InvalidatedException() : std::logic_error("Access to invalidated Results objects") {}
    };

    // The input index parameter was out of bounds
    struct OutOfBoundsIndexException : public std::out_of_range {
        OutOfBoundsIndexException(size_t r, size_t c);
        const size_t requested;
        const size_t valid_count;
    };

    // The input Row object is not attached
    struct DetatchedAccessorException : public std::logic_error {
        DetatchedAccessorException() : std::logic_error("Atempting to access an invalid object") {}
    };

    // The input Row object belongs to a different table
    struct IncorrectTableException : public std::logic_error {
        IncorrectTableException(StringData e, StringData a, const std::string &error) :
            std::logic_error(error), expected(e), actual(a) {}
        const StringData expected;
        const StringData actual;
    };

    // The requested aggregate operation is not supported for the column type
    struct UnsupportedColumnTypeException : public std::logic_error {
        size_t column_index;
        StringData column_name;
        DataType column_type;

        UnsupportedColumnTypeException(size_t column, const Table* table, const char* operation);
    };

    // Create an async query from this Results
    // The query will be run on a background thread and delivered to the callback,
    // and then rerun after each commit (if needed) and redelivered if it changed
    NotificationToken async(std::function<void (std::exception_ptr)> target);
    NotificationToken add_notification_callback(CollectionChangeCallback cb) &;

    bool wants_background_updates() const { return m_wants_background_updates; }

    // Returns whether the rows are guaranteed to be in table order.
    bool is_in_table_order() const;

    // Helper type to let ResultsNotifier update the tableview without giving access
    // to any other privates or letting anyone else do so
    class Internal {
        friend class _impl::ResultsNotifier;
        static void set_table_view(Results& results, TableView&& tv);
    };
    
private:
    enum class UpdatePolicy {
        Auto,  // Update automatically to reflect changes in the underlying data.
        Never, // Never update.
    };

    SharedRealm m_realm;
    mutable const ObjectSchema *m_object_schema = nullptr;
    Query m_query;
    TableView m_table_view;
    LinkViewRef m_link_view;
    TableRef m_table;
    SortDescriptor m_sort;
    SortDescriptor m_distinct;

    _impl::CollectionNotifier::Handle<_impl::ResultsNotifier> m_notifier;

    Mode m_mode = Mode::Empty;
    UpdatePolicy m_update_policy = UpdatePolicy::Auto;
    bool m_has_used_table_view = false;
    bool m_wants_background_updates = true;

    void update_tableview(bool wants_notifications = true);
    bool update_linkview();

    void validate_read() const;
    void validate_write() const;

    void prepare_async();

    template<typename Int, typename Float, typename Double, typename Timestamp>
    util::Optional<Mixed> aggregate(size_t column,
                                    const char* name,
                                    Int agg_int, Float agg_float,
                                    Double agg_double, Timestamp agg_timestamp);

    void set_table_view(TableView&& tv);
};
}

#endif /* REALM_RESULTS_HPP */
