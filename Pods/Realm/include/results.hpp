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
#include "impl/collection_notifier.hpp"
#include "list.hpp"
#include "object.hpp"
#include "object_schema.hpp"
#include "property.hpp"
#include "shared_realm.hpp"
#include "util/checked_mutex.hpp"
#include "util/copyable_atomic.hpp"

#include <realm/table_view.hpp>
#include <realm/util/optional.hpp>

namespace realm {
class Mixed;
class ObjectSchema;

namespace _impl {
    class ResultsNotifierBase;
}

class Results {
public:
    // Results can be either be backed by nothing, a thin wrapper around a table,
    // or a wrapper around a query and a sort order which creates and updates
    // the tableview as needed
    Results();
    Results(std::shared_ptr<Realm> r, ConstTableRef table);
    Results(std::shared_ptr<Realm> r, std::shared_ptr<LstBase> list);
    Results(std::shared_ptr<Realm> r, std::shared_ptr<LstBase> list, DescriptorOrdering o);
    Results(std::shared_ptr<Realm> r, Query q, DescriptorOrdering o = {});
    Results(std::shared_ptr<Realm> r, TableView tv, DescriptorOrdering o = {});
    Results(std::shared_ptr<Realm> r, std::shared_ptr<LnkLst> list, util::Optional<Query> q = {}, SortDescriptor s = {});
    ~Results();

    // Results is copyable and moveable
    Results(Results&&);
    Results& operator=(Results&&);
    Results(const Results&);
    Results& operator=(const Results&);

    // Get the Realm
    std::shared_ptr<Realm> get_realm() const { return m_realm; }

    // Object schema describing the vendored object type
    const ObjectSchema &get_object_schema() const REQUIRES(!m_mutex);

    // Get a query which will match the same rows as is contained in this Results
    // Returned query will not be valid if the current mode is Empty
    Query get_query() const REQUIRES(!m_mutex);

    // Get the Lst this Results is derived from, if any
    std::shared_ptr<LstBase> const& get_list() const { return m_list; }

    // Get the list of sort and distinct operations applied for this Results.
    DescriptorOrdering const& get_descriptor_ordering() const noexcept { return m_descriptor_ordering; }

    // Get a tableview containing the same rows as this Results
    TableView get_tableview() REQUIRES(!m_mutex);

    // Get the object type which will be returned by get()
    StringData get_object_type() const noexcept;

    PropertyType get_type() const REQUIRES(!m_mutex);

    // Get the size of this results
    // Can be either O(1) or O(N) depending on the state of things
    size_t size() REQUIRES(!m_mutex);

    // Get the row accessor for the given index
    // Throws OutOfBoundsIndexException if index >= size()
    template<typename T = Obj>
    T get(size_t index) REQUIRES(!m_mutex);

    // Get the boxed row accessor for the given index
    // Throws OutOfBoundsIndexException if index >= size()
    template<typename Context>
    auto get(Context&, size_t index) REQUIRES(!m_mutex);

    // Get a row accessor for the first/last row, or none if the results are empty
    // More efficient than calling size()+get()
    template<typename T = Obj>
    util::Optional<T> first() REQUIRES(!m_mutex);
    template<typename T = Obj>
    util::Optional<T> last() REQUIRES(!m_mutex);

    // Get the index of the first row matching the query in this table
    size_t index_of(Query&& q) REQUIRES(!m_mutex);

    // Get the first index of the given value in this results, or not_found
    // Throws DetachedAccessorException if row is not attached
    // Throws IncorrectTableException if row belongs to a different table
    template<typename T>
    size_t index_of(T const& value) REQUIRES(!m_mutex);

    // Delete all of the rows in this Results from the Realm
    // size() will always be zero afterwards
    // Throws InvalidTransactionException if not in a write transaction
    void clear() REQUIRES(!m_mutex);

    // Create a new Results by further filtering or sorting this Results
    Results filter(Query&& q) const REQUIRES(!m_mutex);
    Results sort(SortDescriptor&& sort) const REQUIRES(!m_mutex);
    Results sort(std::vector<std::pair<std::string, bool>> const& keypaths) const REQUIRES(!m_mutex);

    // Create a new Results by removing duplicates
    Results distinct(DistinctDescriptor&& uniqueness) const REQUIRES(!m_mutex);
    Results distinct(std::vector<std::string> const& keypaths) const REQUIRES(!m_mutex);

    // Create a new Results with only the first `max_count` entries
    Results limit(size_t max_count) const REQUIRES(!m_mutex);

    // Create a new Results by adding sort and distinct combinations
    Results apply_ordering(DescriptorOrdering&& ordering) REQUIRES(!m_mutex);

    // Return a snapshot of this Results that never updates to reflect changes in the underlying data.
    Results snapshot() const& REQUIRES(!m_mutex);
    Results snapshot() && REQUIRES(!m_mutex);

    // Returns a frozen copy of this result
    Results freeze(std::shared_ptr<Realm> const& realm) REQUIRES(!m_mutex);

    // Returns whether or not this Results is frozen.
    bool is_frozen() REQUIRES(!m_mutex);

    // Get the min/max/average/sum of the given column
    // All but sum() returns none when there are zero matching rows
    // sum() returns 0, except for when it returns none
    // Throws UnsupportedColumnTypeException for sum/average on timestamp or non-numeric column
    // Throws OutOfBoundsIndexException for an out-of-bounds column
    util::Optional<Mixed> max(ColKey column={}) REQUIRES(!m_mutex);
    util::Optional<Mixed> min(ColKey column={}) REQUIRES(!m_mutex);
    util::Optional<double> average(ColKey column={}) REQUIRES(!m_mutex);
    util::Optional<Mixed> sum(ColKey column={}) REQUIRES(!m_mutex);

    util::Optional<Mixed> max(StringData column_name) REQUIRES(!m_mutex) { return max(key(column_name)); }
    util::Optional<Mixed> min(StringData column_name) REQUIRES(!m_mutex) { return min(key(column_name)); }
    util::Optional<double> average(StringData column_name) REQUIRES(!m_mutex) { return average(key(column_name)); }
    util::Optional<Mixed> sum(StringData column_name) REQUIRES(!m_mutex) { return sum(key(column_name)); }

    enum class Mode {
        Empty, // Backed by nothing (for missing tables)
        Table, // Backed directly by a Table
        List,  // Backed by a list-of-primitives that is not a link list.
        Query, // Backed by a query that has not yet been turned into a TableView
        LinkList,  // Backed directly by a LinkList
        TableView, // Backed by a TableView created from a Query
    };
    // Get the currrent mode of the Results
    // Ideally this would not be public but it's needed for some KVO stuff
    Mode get_mode() const noexcept REQUIRES(!m_mutex);

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
        ColKey column_key;
        StringData column_name;
        PropertyType property_type;

        UnsupportedColumnTypeException(ColKey column, Table const& table, const char* operation);
        UnsupportedColumnTypeException(ColKey column, TableView const& tv, const char* operation);
    };

    // The property request does not exist in the schema
    struct InvalidPropertyException : public std::logic_error {
        InvalidPropertyException(StringData object_type, StringData property_name);
        const std::string object_type;
        const std::string property_name;
	};

    // The requested operation is valid, but has not yet been implemented
    struct UnimplementedOperationException : public std::logic_error {
        UnimplementedOperationException(const char *message);
    };

    // Create an async query from this Results
    // The query will be run on a background thread and delivered to the callback,
    // and then rerun after each commit (if needed) and redelivered if it changed
    NotificationToken add_notification_callback(CollectionChangeCallback cb) &;

    // Returns whether the rows are guaranteed to be in table order.
    bool is_in_table_order() const;

    template<typename Context> auto first(Context&) REQUIRES(!m_mutex);
    template<typename Context> auto last(Context&) REQUIRES(!m_mutex);

    template<typename Context, typename T>
    size_t index_of(Context&, T value) REQUIRES(!m_mutex);

    // Batch updates all items in this collection with the provided value
    // Must be called inside a transaction
    // Throws an exception if the value does not match the type for given prop_name
    template<typename ValueType, typename ContextType>
    void set_property_value(ContextType& ctx, StringData prop_name, ValueType value) REQUIRES(!m_mutex);

    // Execute the query immediately if needed. When the relevant query is slow, size()
    // may cost similar time compared with creating the tableview. Use this function to
    // avoid running the query twice for size() and other accessors.
    void evaluate_query_if_needed(bool wants_notifications = true) REQUIRES(!m_mutex);

    enum class UpdatePolicy {
        Auto,      // Update automatically to reflect changes in the underlying data.
        AsyncOnly, // Only update via ResultsNotifier and never run queries synchronously
        Never,     // Never update.
    };
    // For tests only. Use snapshot() for normal uses.
    void set_update_policy(UpdatePolicy policy) { m_update_policy = policy; }

private:
    std::shared_ptr<Realm> m_realm;
    mutable util::CopyableAtomic<const ObjectSchema*> m_object_schema = nullptr;
    Query m_query GUARDED_BY(m_mutex);
    TableView m_table_view GUARDED_BY(m_mutex);
    ConstTableRef m_table;
    DescriptorOrdering m_descriptor_ordering;
    std::shared_ptr<LnkLst> m_link_list;
    std::shared_ptr<LstBase> m_list;
    util::Optional<std::vector<size_t>> m_list_indices GUARDED_BY(m_mutex);

    _impl::CollectionNotifier::Handle<_impl::ResultsNotifierBase> m_notifier;

    Mode m_mode GUARDED_BY(m_mutex) = Mode::Empty;
    UpdatePolicy m_update_policy = UpdatePolicy::Auto;

    bool update_linklist() REQUIRES(m_mutex);

    void validate_read() const;
    void validate_write() const;

    size_t do_size() REQUIRES(m_mutex);
    Query do_get_query() const REQUIRES(m_mutex);
    PropertyType do_get_type() const REQUIRES(m_mutex);

    using ForCallback = util::TaggedBool<class ForCallback>;
    void prepare_async(ForCallback);

    ColKey key(StringData) const;

    template<typename T>
    util::Optional<T> try_get(size_t) REQUIRES(m_mutex);

    template<typename AggregateFunction>
    util::Optional<Mixed> aggregate(ColKey column, const char* name,
                                    AggregateFunction&& func) REQUIRES(!m_mutex);
    DataType prepare_for_aggregate(ColKey column, const char* name) REQUIRES(m_mutex);

    template<typename Fn>
    auto dispatch(Fn&&) const REQUIRES(!m_mutex);

    template<typename T>
    auto& list_as() const;

    void evaluate_sort_and_distinct_on_list() REQUIRES(m_mutex);
    void do_evaluate_query_if_needed(bool wants_notifications = true) REQUIRES(m_mutex);

    class IteratorWrapper {
    public:
        IteratorWrapper() = default;
        IteratorWrapper(IteratorWrapper const&);
        IteratorWrapper& operator=(IteratorWrapper const&);
        IteratorWrapper(IteratorWrapper&&) = default;
        IteratorWrapper& operator=(IteratorWrapper&&) = default;

        Obj get(Table const& table, size_t ndx);
    private:
        std::unique_ptr<Table::ConstIterator> m_it;
    } m_table_iterator;

    util::CheckedOptionalMutex m_mutex;

    // A work around for what appears to be a false positive in clang's thread
    // analysis when constructing a different object of the same type within a
    // member function. Putting the ACQUIRE on the constructor seems like it
    // should work, but doesn't.
    void assert_unlocked() ACQUIRE(!m_mutex) {}
};

template<typename Fn>
auto Results::dispatch(Fn&& fn) const
{
    return switch_on_type(get_type(), std::forward<Fn>(fn));
}

template<typename Context>
auto Results::get(Context& ctx, size_t row_ndx)
{
    return dispatch([&](auto t) { return ctx.box(this->get<std::decay_t<decltype(*t)>>(row_ndx)); });
}

template<typename Context>
auto Results::first(Context& ctx)
{
    // GCC 4.9 complains about `ctx` not being defined within the lambda without this goofy capture
    return dispatch([this, ctx = &ctx](auto t) {
        auto value = this->first<std::decay_t<decltype(*t)>>();
        return value ? static_cast<decltype(ctx->no_value())>(ctx->box(std::move(*value))) : ctx->no_value();
    });
}

template<typename Context>
auto Results::last(Context& ctx)
{
    return dispatch([&](auto t) {
        auto value = this->last<std::decay_t<decltype(*t)>>();
        return value ? static_cast<decltype(ctx.no_value())>(ctx.box(std::move(*value))) : ctx.no_value();
    });
}

template<typename Context, typename T>
size_t Results::index_of(Context& ctx, T value)
{
    return dispatch([&](auto t) {
        return this->index_of(ctx.template unbox<std::decay_t<decltype(*t)>>(value, CreatePolicy::Skip));
    });
}

template <typename ValueType, typename ContextType>
void Results::set_property_value(ContextType& ctx, StringData prop_name, ValueType value) NO_THREAD_SAFETY_ANALYSIS
{
    // Check invariants for calling this method
    validate_write();
    const ObjectSchema& object_schema = get_object_schema();
    const Property* prop = object_schema.property_for_name(prop_name);
    if (!prop) {
        throw InvalidPropertyException(object_schema.name, prop_name);
    }
    if (prop->is_primary && !m_realm->is_in_migration()) {
        throw ModifyPrimaryKeyException(object_schema.name, prop->name);
    }

    // Update all objects in this ResultSets. Use snapshot to avoid correctness problems if the
    // object is removed from the TableView after the property update as well as avoiding to
    // re-evaluating the query too many times.
    auto snapshot = this->snapshot();
    size_t size = snapshot.size();
    for (size_t i = 0; i < size; ++i) {
        Object obj(m_realm, *m_object_schema, snapshot.get(i));
        obj.set_property_value_impl(ctx, *prop, value, CreatePolicy::ForceCreate, false);
    }
}

} // namespace realm

#endif // REALM_RESULTS_HPP
