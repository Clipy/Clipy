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

#include "results.hpp"

#include "impl/realm_coordinator.hpp"
#include "impl/results_notifier.hpp"
#include "object_schema.hpp"
#include "object_store.hpp"
#include "schema.hpp"

#include <stdexcept>

namespace realm {

Results::Results() = default;
Results::~Results() = default;

Results::Results(SharedRealm r, Query q, DescriptorOrdering o)
: m_realm(std::move(r))
, m_query(std::move(q))
, m_table(m_query.get_table())
, m_descriptor_ordering(std::move(o))
, m_mode(Mode::Query)
{
}

Results::Results(SharedRealm r, Table& table)
: m_realm(std::move(r))
, m_mode(Mode::Table)
{
    m_table.reset(&table);
}

Results::Results(SharedRealm r, LinkViewRef lv, util::Optional<Query> q, SortDescriptor s)
: m_realm(std::move(r))
, m_link_view(lv)
, m_mode(Mode::LinkView)
{
    m_table.reset(&lv->get_target_table());
    if (q) {
        m_query = std::move(*q);
        m_mode = Mode::Query;
    }
    m_descriptor_ordering.append_sort(std::move(s));
}

Results::Results(SharedRealm r, TableView tv, DescriptorOrdering o)
: m_realm(std::move(r))
, m_table_view(std::move(tv))
, m_descriptor_ordering(std::move(o))
, m_mode(Mode::TableView)
{
    m_table.reset(&m_table_view.get_parent());
}

Results::Results(const Results&) = default;
Results& Results::operator=(const Results&) = default;

Results::Results(Results&& other)
: m_realm(std::move(other.m_realm))
, m_object_schema(std::move(other.m_object_schema))
, m_query(std::move(other.m_query))
, m_table_view(std::move(other.m_table_view))
, m_link_view(std::move(other.m_link_view))
, m_table(std::move(other.m_table))
, m_descriptor_ordering(std::move(other.m_descriptor_ordering))
, m_notifier(std::move(other.m_notifier))
, m_mode(other.m_mode)
, m_update_policy(other.m_update_policy)
, m_has_used_table_view(other.m_has_used_table_view)
, m_wants_background_updates(other.m_wants_background_updates)
{
    if (m_notifier) {
        m_notifier->target_results_moved(other, *this);
    }
}

Results& Results::operator=(Results&& other)
{
    this->~Results();
    new (this) Results(std::move(other));
    return *this;
}

bool Results::is_valid() const
{
    if (m_realm)
        m_realm->verify_thread();

    if (m_table && !m_table->is_attached())
        return false;

    return true;
}

void Results::validate_read() const
{
    // is_valid ensures that we're on the correct thread.
    if (!is_valid())
        throw InvalidatedException();
}

void Results::validate_write() const
{
    validate_read();
    if (!m_realm || !m_realm->is_in_transaction())
        throw InvalidTransactionException("Must be in a write transaction");
}

size_t Results::size()
{
    validate_read();
    switch (m_mode) {
        case Mode::Empty:    return 0;
        case Mode::Table:    return m_table->size();
        case Mode::LinkView: return m_link_view->size();
        case Mode::Query:
            m_query.sync_view_if_needed();
            if (!m_descriptor_ordering.will_apply_distinct())
                return m_query.count(m_descriptor_ordering);
            REALM_FALLTHROUGH;
        case Mode::TableView:
            evaluate_query_if_needed();
            return m_table_view.size();
    }
    REALM_COMPILER_HINT_UNREACHABLE();
}

const ObjectSchema& Results::get_object_schema() const
{
    validate_read();

    if (!m_object_schema) {
        REALM_ASSERT(m_realm);
        auto it = m_realm->schema().find(get_object_type());
        REALM_ASSERT(it != m_realm->schema().end());
        m_object_schema = &*it;
    }

    return *m_object_schema;
}


StringData Results::get_object_type() const noexcept
{
    if (!m_table) {
        return StringData();
    }

    return ObjectStore::object_type_for_table_name(m_table->get_name());
}

namespace {
template<typename T>
auto get(Table& table, size_t row)
{
    return table.get<T>(0, row);
}

template<>
auto get<RowExpr>(Table& table, size_t row)
{
    return table.get(row);
}
}

template<typename T>
util::Optional<T> Results::try_get(size_t row_ndx)
{
    validate_read();
    switch (m_mode) {
        case Mode::Empty: break;
        case Mode::Table:
            if (row_ndx < m_table->size())
                return realm::get<T>(*m_table, row_ndx);
            break;
        case Mode::LinkView:
            if (update_linkview()) {
                if (row_ndx < m_link_view->size())
                    return realm::get<T>(*m_table, m_link_view->get(row_ndx).get_index());
                break;
            }
            REALM_FALLTHROUGH;
        case Mode::Query:
        case Mode::TableView:
            evaluate_query_if_needed();
            if (row_ndx >= m_table_view.size())
                break;
            if (m_update_policy == UpdatePolicy::Never && !m_table_view.is_row_attached(row_ndx))
                return T{};
            return realm::get<T>(*m_table, m_table_view.get(row_ndx).get_index());
    }
    return util::none;
}

template<typename T>
T Results::get(size_t row_ndx)
{
    if (auto row = try_get<T>(row_ndx))
        return *row;
    throw OutOfBoundsIndexException{row_ndx, size()};
}

template<typename T>
util::Optional<T> Results::first()
{
    return try_get<T>(0);
}

template<typename T>
util::Optional<T> Results::last()
{
    validate_read();
    if (m_mode == Mode::Query)
        evaluate_query_if_needed(); // avoid running the query twice (for size() and for get())
    return try_get<T>(size() - 1);
}

bool Results::update_linkview()
{
    REALM_ASSERT(m_update_policy == UpdatePolicy::Auto);

    if (!m_descriptor_ordering.is_empty()) {
        m_query = get_query();
        m_mode = Mode::Query;
        evaluate_query_if_needed();
        return false;
    }
    return true;
}

void Results::evaluate_query_if_needed(bool wants_notifications)
{
    if (m_update_policy == UpdatePolicy::Never) {
        REALM_ASSERT(m_mode == Mode::TableView);
        return;
    }

    switch (m_mode) {
        case Mode::Empty:
        case Mode::Table:
        case Mode::LinkView:
            return;
        case Mode::Query:
            m_query.sync_view_if_needed();
            m_table_view = m_query.find_all(m_descriptor_ordering);
            m_mode = Mode::TableView;
            REALM_FALLTHROUGH;
        case Mode::TableView:
            if (wants_notifications)
                prepare_async(ForCallback{false});
            m_has_used_table_view = true;
            m_table_view.sync_if_needed();
            break;
    }
}

template<>
size_t Results::index_of(RowExpr const& row)
{
    validate_read();
    if (!row) {
        throw DetatchedAccessorException{};
    }
    if (m_table && row.get_table() != m_table) {
        throw IncorrectTableException(
            ObjectStore::object_type_for_table_name(m_table->get_name()),
            ObjectStore::object_type_for_table_name(row.get_table()->get_name()),
            "Attempting to get the index of a Row of the wrong type"
        );
    }

    switch (m_mode) {
        case Mode::Empty:
            return not_found;
        case Mode::Table:
            return row.get_index();
        case Mode::LinkView:
            if (update_linkview())
                return m_link_view->find(row.get_index());
            REALM_FALLTHROUGH;
        case Mode::Query:
        case Mode::TableView:
            evaluate_query_if_needed();
            return m_table_view.find_by_source_ndx(row.get_index());
    }
    REALM_COMPILER_HINT_UNREACHABLE();
}

template<typename T>
size_t Results::index_of(T const& value)
{
    validate_read();
    switch (m_mode) {
        case Mode::Empty:
            return not_found;
        case Mode::Table:
            return m_table->find_first(0, value);
        case Mode::LinkView:
            REALM_UNREACHABLE();
        case Mode::Query:
        case Mode::TableView:
            evaluate_query_if_needed();
            return m_table_view.find_first(0, value);
    }
    REALM_COMPILER_HINT_UNREACHABLE();
}

size_t Results::index_of(Query&& q)
{
    if (m_descriptor_ordering.will_apply_sort()) {
        auto first = filter(std::move(q)).first();
        return first ? index_of(*first) : not_found;
    }

    auto query = get_query().and_query(std::move(q));
    query.sync_view_if_needed();
    size_t row = query.find();
    return row != not_found ? index_of(m_table->get(row)) : row;
}

void Results::prepare_for_aggregate(size_t column, const char* name)
{
    if (column > m_table->get_column_count())
        throw OutOfBoundsIndexException{column, m_table->get_column_count()};
    switch (m_mode) {
        case Mode::Empty: break;
        case Mode::Table: break;
        case Mode::LinkView:
            m_query = this->get_query();
            m_mode = Mode::Query;
            REALM_FALLTHROUGH;
        case Mode::Query:
        case Mode::TableView:
            evaluate_query_if_needed();
            break;
        default:
            REALM_COMPILER_HINT_UNREACHABLE();
    }
    switch (m_table->get_column_type(column)) {
        case type_Timestamp: case type_Double: case type_Float: case type_Int: break;
        default: throw UnsupportedColumnTypeException{column, m_table.get(), name};
    }
}

template<typename Int, typename Float, typename Double, typename Timestamp>
util::Optional<Mixed> Results::aggregate(size_t column,
                                         const char* name,
                                         Int agg_int, Float agg_float,
                                         Double agg_double, Timestamp agg_timestamp)
{
    validate_read();
    if (!m_table)
        return none;
    prepare_for_aggregate(column, name);

    auto do_agg = [&](auto const& getter) {
        return Mixed(m_mode == Mode::Table ? getter(*m_table) : getter(m_table_view));
    };
    switch (m_table->get_column_type(column)) {
        case type_Timestamp: return do_agg(agg_timestamp);
        case type_Double:    return do_agg(agg_double);
        case type_Float:     return do_agg(agg_float);
        case type_Int:       return do_agg(agg_int);
        default: REALM_COMPILER_HINT_UNREACHABLE();
    }
}

util::Optional<Mixed> Results::max(size_t column)
{
    size_t return_ndx = npos;
    auto results = aggregate(column, "max",
                             [&](auto const& table) { return table.maximum_int(column, &return_ndx); },
                             [&](auto const& table) { return table.maximum_float(column, &return_ndx); },
                             [&](auto const& table) { return table.maximum_double(column, &return_ndx); },
                             [&](auto const& table) { return table.maximum_timestamp(column, &return_ndx); });
    return return_ndx == npos ? none : results;
}

util::Optional<Mixed> Results::min(size_t column)
{
    size_t return_ndx = npos;
    auto results = aggregate(column, "min",
                             [&](auto const& table) { return table.minimum_int(column, &return_ndx); },
                             [&](auto const& table) { return table.minimum_float(column, &return_ndx); },
                             [&](auto const& table) { return table.minimum_double(column, &return_ndx); },
                             [&](auto const& table) { return table.minimum_timestamp(column, &return_ndx); });
    return return_ndx == npos ? none : results;
}

util::Optional<Mixed> Results::sum(size_t column)
{
    return aggregate(column, "sum",
                     [=](auto const& table) { return table.sum_int(column); },
                     [=](auto const& table) { return table.sum_float(column); },
                     [=](auto const& table) { return table.sum_double(column); },
                     [=](auto const&) -> Timestamp { throw UnsupportedColumnTypeException{column, m_table.get(), "sum"}; });
}

util::Optional<double> Results::average(size_t column)
{
    size_t value_count = 0;
    auto results = aggregate(column, "average",
                             [&](auto const& table) { return table.average_int(column, &value_count); },
                             [&](auto const& table) { return table.average_float(column, &value_count); },
                             [&](auto const& table) { return table.average_double(column, &value_count); },
                             [&](auto const&) -> Timestamp { throw UnsupportedColumnTypeException{column, m_table.get(), "average"}; });
    return value_count == 0 ? none : util::make_optional(results->get_double());
}

void Results::clear()
{
    switch (m_mode) {
        case Mode::Empty:
            return;
        case Mode::Table:
            validate_write();
            if (m_realm->is_partial())
                Results(m_realm, m_table->where()).clear();
            else
                m_table->clear();
            break;
        case Mode::Query:
            // Not using Query:remove() because building the tableview and
            // clearing it is actually significantly faster
        case Mode::TableView:
            validate_write();
            evaluate_query_if_needed();

            switch (m_update_policy) {
                case UpdatePolicy::Auto:
                    m_table_view.clear(RemoveMode::unordered);
                    break;
                case UpdatePolicy::Never: {
                    // Copy the TableView because a frozen Results shouldn't let its size() change.
                    TableView copy(m_table_view);
                    copy.clear(RemoveMode::unordered);
                    break;
                }
            }
            break;
        case Mode::LinkView:
            validate_write();
            m_link_view->remove_all_target_rows();
            break;
    }
}

PropertyType Results::get_type() const
{
    validate_read();
    switch (m_mode) {
        case Mode::Empty:
        case Mode::LinkView:
            return PropertyType::Object;
        case Mode::Query:
        case Mode::TableView:
        case Mode::Table:
            if (m_table->get_index_in_group() != npos)
                return PropertyType::Object;
            return ObjectSchema::from_core_type(*m_table->get_descriptor(), 0);
    }
    REALM_COMPILER_HINT_UNREACHABLE();
}

Query Results::get_query() const
{
    validate_read();
    switch (m_mode) {
        case Mode::Empty:
        case Mode::Query:
            return m_query;
        case Mode::TableView: {
            // A TableView has an associated Query if it was produced by Query::find_all. This is indicated
            // by TableView::get_query returning a Query with a non-null table.
            Query query = m_table_view.get_query();
            if (query.get_table()) {
                return query;
            }

            // The TableView has no associated query so create one with no conditions that is restricted
            // to the rows in the TableView.
            if (m_update_policy == UpdatePolicy::Auto) {
                m_table_view.sync_if_needed();
            }
            return Query(*m_table, std::unique_ptr<TableViewBase>(new TableView(m_table_view)));
        }
        case Mode::LinkView:
            return m_table->where(m_link_view);
        case Mode::Table:
            return m_table->where();
    }
    REALM_COMPILER_HINT_UNREACHABLE();
}

TableView Results::get_tableview()
{
    validate_read();
    switch (m_mode) {
        case Mode::Empty:
            return {};
        case Mode::LinkView:
            if (update_linkview())
                return m_table->where(m_link_view).find_all();
            REALM_FALLTHROUGH;
        case Mode::Query:
        case Mode::TableView:
            evaluate_query_if_needed();
            return m_table_view;
        case Mode::Table:
            return m_table->where().find_all();
    }
    REALM_COMPILER_HINT_UNREACHABLE();
}

static std::vector<size_t> parse_keypath(StringData keypath, Schema const& schema, const ObjectSchema *object_schema)
{
    auto check = [&](bool condition, const char* fmt, auto... args) {
        if (!condition) {
            throw std::invalid_argument(util::format("Cannot sort on key path '%1': %2.",
                                                     keypath, util::format(fmt, args...)));
        }
    };
    auto is_sortable_type = [](PropertyType type) {
        return !is_array(type) && type != PropertyType::LinkingObjects && type != PropertyType::Data;
    };

    const char* begin = keypath.data();
    const char* end = keypath.data() + keypath.size();
    check(begin != end, "missing property name");

    std::vector<size_t> indices;
    while (begin != end) {
        auto sep = std::find(begin, end, '.');
        check(sep != begin && sep + 1 != end, "missing property name");
        StringData key(begin, sep - begin);
        begin = sep + (sep != end);

        auto prop = object_schema->property_for_name(key);
        check(prop, "property '%1.%2' does not exist", object_schema->name, key);
        check(is_sortable_type(prop->type), "property '%1.%2' is of unsupported type '%3'",
              object_schema->name, key, string_for_property_type(prop->type));
        if (prop->type == PropertyType::Object)
            check(begin != end, "property '%1.%2' of type 'object' cannot be the final property in the key path",
                  object_schema->name, key);
        else
            check(begin == end, "property '%1.%2' of type '%3' may only be the final property in the key path",
                  object_schema->name, key, prop->type_string());

        indices.push_back(prop->table_column);
        if (prop->type == PropertyType::Object)
            object_schema = &*schema.find(prop->object_type);
    }
    return indices;
}

Results Results::sort(std::vector<std::pair<std::string, bool>> const& keypaths) const
{
    if (keypaths.empty())
        return *this;
    if (get_type() != PropertyType::Object) {
        if (keypaths.size() != 1)
            throw std::invalid_argument(util::format("Cannot sort array of '%1' on more than one key path",
                                                     string_for_property_type(get_type())));
        if (keypaths[0].first != "self")
            throw std::invalid_argument(util::format("Cannot sort on key path '%1': arrays of '%2' can only be sorted on 'self'",
                                                     keypaths[0].first, string_for_property_type(get_type())));
        return sort({*m_table, {{0}}, {keypaths[0].second}});
    }

    std::vector<std::vector<size_t>> column_indices;
    std::vector<bool> ascending;
    column_indices.reserve(keypaths.size());
    ascending.reserve(keypaths.size());

    for (auto& keypath : keypaths) {
        column_indices.push_back(parse_keypath(keypath.first, m_realm->schema(), &get_object_schema()));
        ascending.push_back(keypath.second);
    }
    return sort({*m_table, std::move(column_indices), std::move(ascending)});
}

Results Results::sort(SortDescriptor&& sort) const
{
    if (m_mode == Mode::LinkView)
        return Results(m_realm, m_link_view, util::none, std::move(sort));
    DescriptorOrdering new_order = m_descriptor_ordering;
    new_order.append_sort(std::move(sort));
    return Results(m_realm, get_query(), std::move(new_order));
}

Results Results::filter(Query&& q) const
{
    if (m_descriptor_ordering.will_apply_limit())
        throw UnimplementedOperationException("Filtering a Results with a limit is not yet implemented");
    return Results(m_realm, get_query().and_query(std::move(q)), m_descriptor_ordering);
}

Results Results::limit(size_t max_count) const
{
    auto new_order = m_descriptor_ordering;
    new_order.append_limit(max_count);
    return Results(m_realm, get_query(), std::move(new_order));
}

Results Results::apply_ordering(DescriptorOrdering&& ordering)
{
    DescriptorOrdering new_order = m_descriptor_ordering;
    for (size_t i = 0; i < ordering.size(); ++i) {
        auto desc = ordering[i];
        if (auto sort = dynamic_cast<const SortDescriptor*>(desc))
            new_order.append_sort(std::move(*sort));
        else if (auto distinct = dynamic_cast<const DistinctDescriptor*>(desc))
            new_order.append_distinct(std::move(*distinct));
        else if (auto limit = dynamic_cast<const LimitDescriptor*>(desc))
            new_order.append_limit(std::move(*limit));
        else
            REALM_COMPILER_HINT_UNREACHABLE();
    }
    return Results(m_realm, get_query(), std::move(new_order));
}

Results Results::distinct(DistinctDescriptor&& uniqueness) const
{
    DescriptorOrdering new_order = m_descriptor_ordering;
    new_order.append_distinct(std::move(uniqueness));
    return Results(m_realm, get_query(), std::move(new_order));
}

Results Results::distinct(std::vector<std::string> const& keypaths) const
{
    if (keypaths.empty())
        return *this;
    if (get_type() != PropertyType::Object) {
        if (keypaths.size() != 1)
            throw std::invalid_argument(util::format("Cannot sort array of '%1' on more than one key path",
                                                     string_for_property_type(get_type())));
        if (keypaths[0] != "self")
            throw std::invalid_argument(util::format("Cannot sort on key path '%1': arrays of '%2' can only be sorted on 'self'",
                                                     keypaths[0], string_for_property_type(get_type())));
        return distinct({*m_table, {{0}}});
    }

    std::vector<std::vector<size_t>> column_indices;
    column_indices.reserve(keypaths.size());
    for (auto& keypath : keypaths)
        column_indices.push_back(parse_keypath(keypath, m_realm->schema(), &get_object_schema()));
    return distinct({*m_table, std::move(column_indices)});
}

Results Results::snapshot() const &
{
    validate_read();
    return Results(*this).snapshot();
}

Results Results::snapshot() &&
{
    validate_read();

    switch (m_mode) {
        case Mode::Empty:
            return Results();

        case Mode::Table:
        case Mode::LinkView:
            m_query = get_query();
            m_mode = Mode::Query;

            REALM_FALLTHROUGH;
        case Mode::Query:
        case Mode::TableView:
            evaluate_query_if_needed(false);
            m_notifier.reset();
            m_update_policy = UpdatePolicy::Never;
            return std::move(*this);
    }
    REALM_COMPILER_HINT_UNREACHABLE();
}

void Results::prepare_async(ForCallback force)
{
    if (m_notifier) {
        return;
    }
    if (m_realm->config().immutable()) {
        if (force)
            throw InvalidTransactionException("Cannot create asynchronous query for immutable Realms");
        return;
    }
    if (m_realm->is_in_transaction()) {
        if (force)
            throw InvalidTransactionException("Cannot create asynchronous query while in a write transaction");
        return;
    }
    if (m_update_policy == UpdatePolicy::Never) {
        if (force)
            throw std::logic_error("Cannot create asynchronous query for snapshotted Results.");
        return;
    }
    if (!force) {
        // Don't do implicit background updates if we can't actually deliver them
        if (!m_realm->can_deliver_notifications())
            return;
        // Don't do implicit background updates if there isn't actually anything
        // that needs to be run.
        if (!m_query.get_table() && m_descriptor_ordering.is_empty())
            return;
    }

    m_wants_background_updates = true;
    m_notifier = std::make_shared<_impl::ResultsNotifier>(*this);
    _impl::RealmCoordinator::register_notifier(m_notifier);
}

NotificationToken Results::add_notification_callback(CollectionChangeCallback cb) &
{
    prepare_async(ForCallback{true});
    return {m_notifier, m_notifier->add_callback(std::move(cb))};
}

bool Results::is_in_table_order() const
{
    switch (m_mode) {
        case Mode::Empty:
        case Mode::Table:
            return true;
        case Mode::LinkView:
            return false;
        case Mode::Query:
            return m_query.produces_results_in_table_order() && !m_descriptor_ordering.will_apply_sort();
        case Mode::TableView:
            return m_table_view.is_in_table_order();
    }
    REALM_COMPILER_HINT_UNREACHABLE();
}

void Results::Internal::set_table_view(Results& results, TableView &&tv)
{
    REALM_ASSERT(results.m_update_policy != UpdatePolicy::Never);
    // If the previous TableView was never actually used, then stop generating
    // new ones until the user actually uses the Results object again
    if (results.m_mode == Mode::TableView) {
        results.m_wants_background_updates = results.m_has_used_table_view;
    }

    results.m_table_view = std::move(tv);
    results.m_mode = Mode::TableView;
    results.m_has_used_table_view = false;
    REALM_ASSERT(results.m_table_view.is_in_sync());
    REALM_ASSERT(results.m_table_view.is_attached());
}

#define REALM_RESULTS_TYPE(T) \
    template T Results::get<T>(size_t); \
    template util::Optional<T> Results::first<T>(); \
    template util::Optional<T> Results::last<T>(); \
    template size_t Results::index_of<T>(T const&);

template RowExpr Results::get<RowExpr>(size_t);
template util::Optional<RowExpr> Results::first<RowExpr>();
template util::Optional<RowExpr> Results::last<RowExpr>();

REALM_RESULTS_TYPE(bool)
REALM_RESULTS_TYPE(int64_t)
REALM_RESULTS_TYPE(float)
REALM_RESULTS_TYPE(double)
REALM_RESULTS_TYPE(StringData)
REALM_RESULTS_TYPE(BinaryData)
REALM_RESULTS_TYPE(Timestamp)
REALM_RESULTS_TYPE(util::Optional<bool>)
REALM_RESULTS_TYPE(util::Optional<int64_t>)
REALM_RESULTS_TYPE(util::Optional<float>)
REALM_RESULTS_TYPE(util::Optional<double>)

#undef REALM_RESULTS_TYPE

Results::OutOfBoundsIndexException::OutOfBoundsIndexException(size_t r, size_t c)
: std::out_of_range(util::format("Requested index %1 greater than max %2", r, c - 1))
, requested(r), valid_count(c) {}

static std::string unsupported_operation_msg(size_t column, const Table* table, const char* operation)
{
    const char* column_type = string_for_property_type(ObjectSchema::from_core_type(*table->get_descriptor(), column));
    if (table->is_group_level())
        return util::format("Cannot %1 property '%2': operation not supported for '%3' properties",
                            operation, table->get_column_name(column), column_type);
    return util::format("Cannot %1 '%2' array: operation not supported",
                        operation, column_type);
}

Results::UnsupportedColumnTypeException::UnsupportedColumnTypeException(size_t column, const Table* table, const char* operation)
: std::logic_error(unsupported_operation_msg(column, table, operation))
, column_index(column)
, column_name(table->get_column_name(column))
, property_type(ObjectSchema::from_core_type(*table->get_descriptor(), column))
{
}

Results::UnimplementedOperationException::UnimplementedOperationException(const char* msg)
: std::logic_error(msg)
{ }

} // namespace realm
