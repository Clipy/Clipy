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
#include "audit.hpp"
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
, m_mutex(m_realm && m_realm->is_frozen())
{
}

Results::Results(SharedRealm r, ConstTableRef table)
: m_realm(std::move(r))
, m_table(table)
, m_mode(Mode::Table)
, m_mutex(m_realm && m_realm->is_frozen())
{
}

Results::Results(std::shared_ptr<Realm> r, std::shared_ptr<LstBase> list)
: m_realm(std::move(r))
, m_list(list)
, m_mode(Mode::List)
, m_mutex(m_realm && m_realm->is_frozen())
{
}

Results::Results(std::shared_ptr<Realm> r, std::shared_ptr<LstBase> list, DescriptorOrdering o)
: m_realm(std::move(r))
, m_descriptor_ordering(std::move(o))
, m_list(std::move(list))
, m_mode(Mode::List)
, m_mutex(m_realm && m_realm->is_frozen())
{
}

Results::Results(std::shared_ptr<Realm> r, TableView tv, DescriptorOrdering o)
: m_realm(std::move(r))
, m_table_view(std::move(tv))
, m_descriptor_ordering(std::move(o))
, m_mode(Mode::TableView)
, m_mutex(m_realm && m_realm->is_frozen())
{
    m_table = m_table_view.get_parent();
}

Results::Results(std::shared_ptr<Realm> r, std::shared_ptr<LnkLst> lv, util::Optional<Query> q, SortDescriptor s)
: m_realm(std::move(r))
, m_link_list(std::move(lv))
, m_mode(Mode::LinkList)
, m_mutex(m_realm && m_realm->is_frozen())
{
    m_table = m_link_list->get_target_table();
    if (q) {
        m_query = std::move(*q);
        m_mode = Mode::Query;
    }
    m_descriptor_ordering.append_sort(std::move(s));
}

Results::Results(const Results&) = default;
Results& Results::operator=(const Results&) = default;
Results::Results(Results&&) = default;
Results& Results::operator=(Results&&) = default;

Results::Mode Results::get_mode() const noexcept
{
    CheckedUniqueLock lock(m_mutex);
    return m_mode;
}

bool Results::is_valid() const
{
    if (m_realm) {
        m_realm->verify_thread();
    }

    // Here we cannot just use if (m_table) as it combines a check if the
    // reference contains a value and if that value is valid.
    // First we check if a table is referenced ...
    if (m_table.unchecked_ptr() != nullptr)
        return !!m_table; // ... and then we check if it is valid

    if (m_list)
        return m_list->is_attached();

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
    util::CheckedUniqueLock lock(m_mutex);
    return do_size();
}

size_t Results::do_size()
{
    validate_read();
    switch (m_mode) {
        case Mode::Empty:    return 0;
        case Mode::Table:    return m_table->size();
        case Mode::LinkList: return m_link_list->size();
        case Mode::List:
            evaluate_sort_and_distinct_on_list();
            return m_list_indices ? m_list_indices->size() : m_list->size();
        case Mode::Query:
            m_query.sync_view_if_needed();
            if (!m_descriptor_ordering.will_apply_distinct())
                return m_query.count(m_descriptor_ordering);
            REALM_FALLTHROUGH;
        case Mode::TableView:
            do_evaluate_query_if_needed();
            return m_table_view.size();
    }
    REALM_COMPILER_HINT_UNREACHABLE();
}

const ObjectSchema& Results::get_object_schema() const
{
    validate_read();

    auto object_schema = m_object_schema.load();
    if (!object_schema) {
        REALM_ASSERT(m_realm);
        auto it = m_realm->schema().find(get_object_type());
        REALM_ASSERT(it != m_realm->schema().end());
        m_object_schema = object_schema = &*it;
    }

    return *object_schema;
}

StringData Results::get_object_type() const noexcept
{
    if (!m_table) {
        return StringData();
    }

    return ObjectStore::object_type_for_table_name(m_table->get_name());
}

template<typename T>
auto& Results::list_as() const
{
    return static_cast<Lst<T>&>(*m_list);
}

void Results::evaluate_sort_and_distinct_on_list()
{
    if (m_descriptor_ordering.is_empty())
        return;

    // We can't use the sorted list from the notifier if we're in a write
    // transaction as we only check the transaction version to see if the data matches
    if (m_notifier && m_notifier->get_list_indices(m_list_indices) && !m_realm->is_in_transaction())
        return;

    bool needs_update = m_list->has_changed();
    if (!m_list_indices) {
        m_list_indices = std::vector<size_t>{};
        needs_update = true;
    }
    if (!needs_update)
        return;
    if (m_list->is_empty()) {
        m_list_indices->clear();
        return;
    }

    util::Optional<bool> sort_order;
    bool do_distinct = false;
    auto sz = m_descriptor_ordering.size();
    for (size_t i = 0; i < sz; i++) {
        auto descr = m_descriptor_ordering[i];
        if (descr->get_type() == DescriptorType::Sort)
            sort_order = static_cast<const SortDescriptor*>(descr)->is_ascending(0);
        if (descr->get_type() == DescriptorType::Distinct)
            do_distinct = true;
    }

    if (do_distinct)
        m_list->distinct(*m_list_indices, sort_order);
    else if (sort_order)
        m_list->sort(*m_list_indices, *sort_order);
}

template<typename T>
util::Optional<T> Results::try_get(size_t ndx)
{
    validate_read();
    if (m_mode == Mode::List) {
        evaluate_sort_and_distinct_on_list();
        if (m_list_indices) {
            if (ndx < m_list_indices->size())
                return list_as<T>().get((*m_list_indices)[ndx]);
        }
        else {
            if (ndx < m_list->size())
                return list_as<T>().get(ndx);
        }
    }
    return util::none;
}

Results::IteratorWrapper::IteratorWrapper(IteratorWrapper const& rgt)
{
    *this = rgt;
}

Results::IteratorWrapper& Results::IteratorWrapper::operator=(IteratorWrapper const& rgt)
{
    if (rgt.m_it)
        m_it = std::make_unique<Table::ConstIterator>(*rgt.m_it);
    return *this;
}

Obj Results::IteratorWrapper::get(Table const& table, size_t ndx)
{
    // Using a Table iterator is much faster for repeated access into a table
    // than indexing into it as the iterator caches the cluster the last accessed
    // object is stored in.
    if (!m_it && table.size() > 5) {
        m_it = std::make_unique<Table::ConstIterator>(table.begin());
    }
    if (!m_it) {
        return const_cast<Table&>(table).get_object(ndx);
    }
    try {
        return (*m_it)[ndx];
    }
    catch (...) {
        // Iterator might be outdated
        m_it = std::make_unique<Table::ConstIterator>(table.begin());
        return (*m_it)[ndx];
    }
}

template<>
util::Optional<Obj> Results::try_get(size_t row_ndx)
{
    validate_read();
    switch (m_mode) {
        case Mode::Empty:
        case Mode::List:
            break;
        case Mode::Table:
            if (row_ndx < m_table->size())
                return m_table_iterator.get(*m_table, row_ndx);
            break;
        case Mode::LinkList:
            if (update_linklist()) {
                if (row_ndx < m_link_list->size())
                    return m_link_list->get_object(row_ndx);
                break;
            }
            REALM_FALLTHROUGH;
        case Mode::Query:
        case Mode::TableView:
            do_evaluate_query_if_needed();
            if (row_ndx >= m_table_view.size())
                break;
            if (m_update_policy == UpdatePolicy::Never && !m_table_view.is_obj_valid(row_ndx))
                return Obj{};
            return m_table_view.get(row_ndx);
    }
    return util::none;
}

template<typename T>
T Results::get(size_t row_ndx)
{
    util::CheckedUniqueLock lock(m_mutex);
    if (auto row = try_get<T>(row_ndx)) {
        return *row;
    }
    throw OutOfBoundsIndexException{row_ndx, do_size()};
}

template<typename T>
util::Optional<T> Results::first()
{
    util::CheckedUniqueLock lock(m_mutex);
    return try_get<T>(0);
}

template<typename T>
util::Optional<T> Results::last()
{
    util::CheckedUniqueLock lock(m_mutex);
    validate_read();
    if (m_mode == Mode::Query)
        do_evaluate_query_if_needed(); // avoid running the query twice (for size() and for get())
    return try_get<T>(do_size() - 1);
}

bool Results::update_linklist()
{
    REALM_ASSERT(m_update_policy == UpdatePolicy::Auto);

    if (!m_descriptor_ordering.is_empty()) {
        m_query = do_get_query();
        m_mode = Mode::Query;
        do_evaluate_query_if_needed();
        return false;
    }
    return true;
}

void Results::evaluate_query_if_needed(bool wants_notifications)
{
    util::CheckedUniqueLock lock(m_mutex);
    validate_read();
    do_evaluate_query_if_needed(wants_notifications);
}

void Results::do_evaluate_query_if_needed(bool wants_notifications)
{
    if (m_update_policy == UpdatePolicy::Never) {
        REALM_ASSERT(m_mode == Mode::TableView);
        return;
    }

    switch (m_mode) {
        case Mode::Empty:
        case Mode::Table:
        case Mode::List:
        case Mode::LinkList:
            return;
        case Mode::Query:
            if (m_notifier && m_notifier->get_tableview(m_table_view)) {
                m_mode = Mode::TableView;
                break;
            }
            m_query.sync_view_if_needed();
            if (m_update_policy == UpdatePolicy::Auto)
                m_table_view = m_query.find_all(m_descriptor_ordering);
            m_mode = Mode::TableView;
            REALM_FALLTHROUGH;
        case Mode::TableView:
            if (wants_notifications && !m_notifier)
                prepare_async(ForCallback{false});
            else if (m_notifier)
                m_notifier->get_tableview(m_table_view);
            if (m_update_policy == UpdatePolicy::Auto)
                m_table_view.sync_if_needed();
            if (auto audit = m_realm->audit_context())
                audit->record_query(m_realm->read_transaction_version(), m_table_view);
            break;
    }
}

template<>
size_t Results::index_of(Obj const& row)
{
    util::CheckedUniqueLock lock(m_mutex);
    validate_read();
    if (!row.is_valid()) {
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
        case Mode::List:
            return not_found;
        case Mode::Table:
            return m_table->get_object_ndx(row.get_key());
        case Mode::LinkList:
            if (update_linklist())
                return m_link_list->Lst<ObjKey>::find_first(row.get_key());
            REALM_FALLTHROUGH;
        case Mode::Query:
        case Mode::TableView:
            do_evaluate_query_if_needed();
            return m_table_view.find_by_source_ndx(row.get_key());
    }
    REALM_COMPILER_HINT_UNREACHABLE();
}

template<typename T>
size_t Results::index_of(T const& value)
{
    util::CheckedUniqueLock lock(m_mutex);
    validate_read();
    if (m_mode != Mode::List)
        return not_found; // Non-List results can only ever contain Objects
    evaluate_sort_and_distinct_on_list();
    if (m_list_indices) {
        for (size_t i = 0; i < m_list_indices->size(); ++i) {
            if (list_as<T>().get((*m_list_indices)[i]) == value)
                return i;
        }
        return not_found;
    }
    return list_as<T>().find_first(value);
}

size_t Results::index_of(Query&& q)
{
    if (m_descriptor_ordering.will_apply_sort()) {
        Results filtered(filter(std::move(q)));
        filtered.assert_unlocked();
        auto first = filtered.first();
        return first ? index_of(*first) : not_found;
    }

    auto query = get_query().and_query(std::move(q));
    query.sync_view_if_needed();
    ObjKey row = query.find();
    return row ? index_of(const_cast<Table&>(*m_table).get_object(row)) : not_found;
}

DataType Results::prepare_for_aggregate(ColKey column, const char* name)
{
    DataType type;
    switch (m_mode) {
        case Mode::Table:
            type = m_table->get_column_type(column);
            break;
        case Mode::List:
            type = m_list->get_table()->get_column_type(m_list->get_col_key());
            break;
        case Mode::LinkList:
            m_query = do_get_query();
            m_mode = Mode::Query;
            REALM_FALLTHROUGH;
        case Mode::Query:
        case Mode::TableView:
            do_evaluate_query_if_needed();
            type = m_table->get_column_type(column);
            break;
        default:
            REALM_COMPILER_HINT_UNREACHABLE();
    }
    switch (type) {
        case type_Timestamp: case type_Double: case type_Float: case type_Int: break;
        default: throw UnsupportedColumnTypeException{column, *m_table, name};
    }
    return type;
}

namespace {
template<typename T, typename Table>
struct AggregateHelper;

template<typename Table>
struct AggregateHelper<int64_t, Table> {
    Table& table;
    Mixed min(ColKey col, ObjKey* obj)   { return table.minimum_int(col, obj);   }
    Mixed max(ColKey col, ObjKey* obj)   { return table.maximum_int(col, obj);   }
    Mixed sum(ColKey col)                { return table.sum_int(col);            }
    Mixed avg(ColKey col, size_t* count) { return table.average_int(col, count); }
};

template<typename Table>
struct AggregateHelper<double, Table> {
    Table& table;
    Mixed min(ColKey col, ObjKey* obj)   { return table.minimum_double(col, obj);   }
    Mixed max(ColKey col, ObjKey* obj)   { return table.maximum_double(col, obj);   }
    Mixed sum(ColKey col)                { return table.sum_double(col);            }
    Mixed avg(ColKey col, size_t* count) { return table.average_double(col, count); }
};

template<typename Table>
struct AggregateHelper<float, Table> {
    Table& table;
    Mixed min(ColKey col, ObjKey* obj)   { return table.minimum_float(col, obj);   }
    Mixed max(ColKey col, ObjKey* obj)   { return table.maximum_float(col, obj);   }
    Mixed sum(ColKey col)                { return table.sum_float(col);            }
    Mixed avg(ColKey col, size_t* count) { return table.average_float(col, count); }
};

template<typename Table>
struct AggregateHelper<Timestamp, Table> {
    Table& table;
    Mixed min(ColKey col, ObjKey* obj) { return table.minimum_timestamp(col, obj);   }
    Mixed max(ColKey col, ObjKey* obj) { return table.maximum_timestamp(col, obj);   }
    Mixed sum(ColKey col)          { throw Results::UnsupportedColumnTypeException{col, table, "sum"}; }
    Mixed avg(ColKey col, size_t*) { throw Results::UnsupportedColumnTypeException{col, table, "avg"}; }
};

struct ListAggregateHelper {
    LstBase& list;
    Mixed min(ColKey, size_t* ndx)   { return list.min(ndx);   }
    Mixed max(ColKey, size_t* ndx)   { return list.max(ndx);   }
    Mixed sum(ColKey)                { return list.sum();      }
    Mixed avg(ColKey, size_t* count) { return list.avg(count); }
};

template<> struct AggregateHelper<int64_t, LstBase&> : ListAggregateHelper
{ AggregateHelper(LstBase& l) : ListAggregateHelper{l} {} };
template<> struct AggregateHelper<double,  LstBase&> : ListAggregateHelper
{ AggregateHelper(LstBase& l) : ListAggregateHelper{l} {} };
template<> struct AggregateHelper<float,   LstBase&> : ListAggregateHelper
{ AggregateHelper(LstBase& l) : ListAggregateHelper{l} {} };

template<>
struct AggregateHelper<Timestamp, LstBase&> : ListAggregateHelper {
    AggregateHelper(LstBase& l) : ListAggregateHelper{l} {}
    Mixed sum(ColKey)          { throw Results::UnsupportedColumnTypeException{list.get_col_key(), *list.get_table(), "sum"}; }
    Mixed avg(ColKey, size_t*) { throw Results::UnsupportedColumnTypeException{list.get_col_key(), *list.get_table(), "avg"}; }
};

template<typename Table, typename Func>
Mixed call_with_helper(Func&& func, Table&& table, DataType type)
{
    switch (type) {
        case type_Timestamp: return func(AggregateHelper<Timestamp, Table>{table});
        case type_Double:    return func(AggregateHelper<double, Table>{table});
        case type_Float:     return func(AggregateHelper<Float, Table>{table});
        case type_Int:       return func(AggregateHelper<Int, Table>{table});
        default: REALM_COMPILER_HINT_UNREACHABLE();
    }
}

struct ReturnIndexHelper {
    ObjKey key;
    size_t index = npos;
    operator ObjKey*() { return &key; }
    operator size_t*() { return &index; }
    operator bool() { return key || index != npos; }
};
} // anonymous namespace

template<typename AggregateFunction>
util::Optional<Mixed> Results::aggregate(ColKey column, const char* name,
                                         AggregateFunction&& func)
{
    util::CheckedUniqueLock lock(m_mutex);
    validate_read();
    if (!m_table && !m_list)
        return none;

    auto type = prepare_for_aggregate(column, name);
    switch (m_mode) {
        case Mode::Table:
            return call_with_helper(func, *m_table, type);
        case Mode::List:
            return call_with_helper(func, *m_list, type);
        default:
            return call_with_helper(func, m_table_view, type);
    }
}

util::Optional<Mixed> Results::max(ColKey column)
{
    ReturnIndexHelper return_ndx;
    auto results = aggregate(column, "max", [&](auto&& helper) {
        return helper.max(column, return_ndx);
    });
    return return_ndx ? results : none;
}

util::Optional<Mixed> Results::min(ColKey column)
{
    ReturnIndexHelper return_ndx;
    auto results = aggregate(column, "min", [&](auto&& helper) {
        return helper.min(column, return_ndx);
    });
    return return_ndx ? results : none;
}

util::Optional<Mixed> Results::sum(ColKey column)
{
    return aggregate(column, "sum", [&](auto&& helper) { return helper.sum(column); });
}

util::Optional<double> Results::average(ColKey column)
{
    size_t value_count = 0;
    auto results = aggregate(column, "avg", [&](auto&& helper) {
        return helper.avg(column, &value_count);
    });
    return value_count == 0 ? none : util::make_optional(results->get_double());
}

void Results::clear()
{
    util::CheckedUniqueLock lock(m_mutex);
    switch (m_mode) {
        case Mode::Empty:
            return;
        case Mode::Table:
            validate_write();
            if (m_realm->is_partial())
                m_table->where().find_all().clear();
            else
                const_cast<Table&>(*m_table).clear();
            break;
        case Mode::Query:
            // Not using Query:remove() because building the tableview and
            // clearing it is actually significantly faster
        case Mode::TableView:
            validate_write();
            do_evaluate_query_if_needed();

            switch (m_update_policy) {
                case UpdatePolicy::Auto:
                    m_table_view.clear();
                    break;
                case UpdatePolicy::AsyncOnly:
                case UpdatePolicy::Never: {
                    // Copy the TableView because a frozen Results shouldn't let its size() change.
                    TableView copy(m_table_view);
                    copy.clear();
                    break;
                }
            }
            break;
        case Mode::List:
            validate_write();
            m_list->clear();
            break;
        case Mode::LinkList:
            validate_write();
            m_link_list->remove_all_target_rows();
            break;
    }
}

PropertyType Results::get_type() const
{
    util::CheckedUniqueLock lock(m_mutex);
    return do_get_type();
}

PropertyType Results::do_get_type() const
{
    validate_read();
    switch (m_mode) {
        case Mode::Empty:
        case Mode::LinkList:
            return PropertyType::Object;
        case Mode::Query:
        case Mode::TableView:
        case Mode::Table:
            return PropertyType::Object;
        case Mode::List:
            return ObjectSchema::from_core_type(*m_list->get_table(), m_list->get_col_key());
    }
    REALM_COMPILER_HINT_UNREACHABLE();
}

Query Results::get_query() const
{
    util::CheckedUniqueLock lock(m_mutex);
    return do_get_query();
}

Query Results::do_get_query() const
{
    validate_read();
    switch (m_mode) {
        case Mode::Empty:
        case Mode::Query:
        case Mode::List:
            return m_query;
        case Mode::TableView: {
            if (const_cast<Query&>(m_query).get_table())
                return m_query;

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
            return Query(m_table, std::unique_ptr<ConstTableView>(new TableView(m_table_view)));
        }
        case Mode::LinkList:
            return m_table->where(*m_link_list);
        case Mode::Table:
            return m_table->where();
    }
    REALM_COMPILER_HINT_UNREACHABLE();
}

TableView Results::get_tableview()
{
    util::CheckedUniqueLock lock(m_mutex);
    validate_read();
    switch (m_mode) {
        case Mode::Empty:
        case Mode::List:
            return {};
        case Mode::LinkList:
            if (update_linklist())
                return m_table->where(*m_link_list).find_all();
            REALM_FALLTHROUGH;
        case Mode::Query:
        case Mode::TableView:
            do_evaluate_query_if_needed();
            return m_table_view;
        case Mode::Table:
            return m_table->where().find_all();
    }
    REALM_COMPILER_HINT_UNREACHABLE();
}

static std::vector<ColKey> parse_keypath(StringData keypath, Schema const& schema,
                                         const ObjectSchema *object_schema)
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

    std::vector<ColKey> indices;
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

        indices.push_back(ColKey(prop->column_key));
        if (prop->type == PropertyType::Object)
            object_schema = &*schema.find(prop->object_type);
    }
    return indices;
}

Results Results::sort(std::vector<std::pair<std::string, bool>> const& keypaths) const
{
    if (keypaths.empty())
        return *this;
    auto type = get_type();
    if (type != PropertyType::Object) {
        if (keypaths.size() != 1)
            throw std::invalid_argument(util::format("Cannot sort array of '%1' on more than one key path",
                                                     string_for_property_type(type & ~PropertyType::Flags)));
        if (keypaths[0].first != "self")
            throw std::invalid_argument(
                util::format("Cannot sort on key path '%1': arrays of '%2' can only be sorted on 'self'",
                             keypaths[0].first, string_for_property_type(type & ~PropertyType::Flags)));
        return sort({{{}}, {keypaths[0].second}});
    }

    std::vector<std::vector<ColKey>> column_keys;
    std::vector<bool> ascending;
    column_keys.reserve(keypaths.size());
    ascending.reserve(keypaths.size());

    for (auto& keypath : keypaths) {
        column_keys.push_back(parse_keypath(keypath.first, m_realm->schema(),
                                            &get_object_schema()));
        ascending.push_back(keypath.second);
    }
    return sort({std::move(column_keys), std::move(ascending)});
}

Results Results::sort(SortDescriptor&& sort) const
{
    util::CheckedUniqueLock lock(m_mutex);
    DescriptorOrdering new_order = m_descriptor_ordering;
    new_order.append_sort(std::move(sort));
    if (m_mode == Mode::LinkList)
        return Results(m_realm, m_link_list, util::none, std::move(sort));
    else if (m_mode == Mode::List)
        return Results(m_realm, m_list, std::move(new_order));
    return Results(m_realm, do_get_query(), std::move(new_order));
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
        switch (ordering.get_type(i)) {
            case DescriptorType::Sort: {
                auto sort = dynamic_cast<const SortDescriptor*>(ordering[i]);
                new_order.append_sort(std::move(*sort));
                break;
            }
            case DescriptorType::Distinct: {
                auto distinct = dynamic_cast<const DistinctDescriptor*>(ordering[i]);
                new_order.append_distinct(std::move(*distinct));
                break;
            }
            case DescriptorType::Limit: {
                auto limit = dynamic_cast<const LimitDescriptor*>(ordering[i]);
                new_order.append_limit(std::move(*limit));
                break;
            }
            case DescriptorType::Include: {
                auto include = dynamic_cast<const IncludeDescriptor*>(ordering[i]);
                new_order.append_include(std::move(*include));
                break;
            }
        }
    }
    return Results(m_realm, get_query(), std::move(new_order));
}

Results Results::distinct(DistinctDescriptor&& uniqueness) const
{
    DescriptorOrdering new_order = m_descriptor_ordering;
    new_order.append_distinct(std::move(uniqueness));
    util::CheckedUniqueLock lock(m_mutex);
    if (m_mode == Mode::List)
        return Results(m_realm, m_list, std::move(new_order));
    return Results(m_realm, do_get_query(), std::move(new_order));
}

Results Results::distinct(std::vector<std::string> const& keypaths) const
{
    if (keypaths.empty())
        return *this;
    auto type = get_type();
    if (type != PropertyType::Object) {
        if (keypaths.size() != 1)
            throw std::invalid_argument(util::format("Cannot sort array of '%1' on more than one key path",
                                                     string_for_property_type(type & ~PropertyType::Flags)));
        if (keypaths[0] != "self")
            throw std::invalid_argument(
                util::format("Cannot sort on key path '%1': arrays of '%2' can only be sorted on 'self'", keypaths[0],
                             string_for_property_type(type & ~PropertyType::Flags)));
        return distinct(DistinctDescriptor({{ColKey()}}));
    }

    std::vector<std::vector<ColKey>> column_keys;
    column_keys.reserve(keypaths.size());
    for (auto& keypath : keypaths)
        column_keys.push_back(parse_keypath(keypath, m_realm->schema(), &get_object_schema()));
    return distinct({std::move(column_keys)});
}

Results Results::snapshot() const&
{
    validate_read();
    auto clone = *this;
    clone.assert_unlocked();
    return static_cast<Results&&>(clone).snapshot();
}

Results Results::snapshot() &&
{
    util::CheckedUniqueLock lock(m_mutex);
    validate_read();
    switch (m_mode) {
        case Mode::Empty:
            return Results();

        case Mode::Table:
        case Mode::LinkList:
            m_query = do_get_query();
            m_mode = Mode::Query;

            REALM_FALLTHROUGH;
        case Mode::Query:
        case Mode::TableView:
        case Mode::List: // FIXME Correct?
            do_evaluate_query_if_needed(false);
            m_notifier.reset();
            m_update_policy = UpdatePolicy::Never;
            return std::move(*this);
    }
    REALM_COMPILER_HINT_UNREACHABLE();
}

// This function cannot be called on frozen results and so does not require locking
void Results::prepare_async(ForCallback force) NO_THREAD_SAFETY_ANALYSIS
{
    REALM_ASSERT(m_realm);
    if (m_notifier)
        return;
    if (!m_realm->verify_notifications_available(force))
        return;
    if (m_update_policy == UpdatePolicy::Never) {
        if (force)
            throw std::logic_error("Cannot create asynchronous query for snapshotted Results.");
        return;
    }

    REALM_ASSERT(!force || !m_realm->is_frozen());
    if (!force) {
        // Don't do implicit background updates if we can't actually deliver them
        if (!m_realm->can_deliver_notifications())
            return;
        // Don't do implicit background updates if there isn't actually anything
        // that needs to be run.
        if (!m_query.get_table() && m_descriptor_ordering.is_empty())
            return;
    }

    if (m_list)
        m_notifier = std::make_shared<_impl::ListResultsNotifier>(*this);
    else
        m_notifier = std::make_shared<_impl::ResultsNotifier>(*this);
    _impl::RealmCoordinator::register_notifier(m_notifier);
}

NotificationToken Results::add_notification_callback(CollectionChangeCallback cb) &
{
    prepare_async(ForCallback{true});
    return {m_notifier, m_notifier->add_callback(std::move(cb))};
}

// This function cannot be called on frozen results and so does not require locking
bool Results::is_in_table_order() const NO_THREAD_SAFETY_ANALYSIS
{
    REALM_ASSERT(!m_realm || !m_realm->is_frozen());
    switch (m_mode) {
        case Mode::Empty:
        case Mode::Table:
        case Mode::List:
            return true;
        case Mode::LinkList:
            return false;
        case Mode::Query:
            return m_query.produces_results_in_table_order()
                && !m_descriptor_ordering.will_apply_sort();
        case Mode::TableView:
            return m_table_view.is_in_table_order();
    }
    REALM_COMPILER_HINT_UNREACHABLE();
}

ColKey Results::key(StringData name) const
{
    return m_table->get_column_key(name);
}
#define REALM_RESULTS_TYPE(T) \
    template T Results::get<T>(size_t); \
    template util::Optional<T> Results::first<T>(); \
    template util::Optional<T> Results::last<T>(); \
    template size_t Results::index_of<T>(T const&);

template Obj Results::get<Obj>(size_t);
template util::Optional<Obj> Results::first<Obj>();
template util::Optional<Obj> Results::last<Obj>();

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

Results Results::freeze(std::shared_ptr<Realm> const& frozen_realm)
{
    util::CheckedUniqueLock lock(m_mutex);
    if (m_mode == Mode::Empty)
        return *this;
    switch (m_mode) {
        case Mode::Table:
            return Results(frozen_realm, frozen_realm->import_copy_of(m_table));
        case Mode::List:
            return Results(frozen_realm, frozen_realm->import_copy_of(*m_list), m_descriptor_ordering);
        case Mode::LinkList: {
            std::shared_ptr<LnkLst> frozen_ll(frozen_realm->import_copy_of(std::make_unique<LnkLst>(*m_link_list)).release());

            // If query/sort was provided for the original Results, mode would have changed to Query, so no need
            // include them here.
            return Results(frozen_realm, std::move(frozen_ll));
        }
        case Mode::Query:
            return Results(frozen_realm, *frozen_realm->import_copy_of(m_query, PayloadPolicy::Copy), m_descriptor_ordering);
        case Mode::TableView: {
            Results results(frozen_realm, *frozen_realm->import_copy_of(m_table_view, PayloadPolicy::Copy), m_descriptor_ordering);
            results.assert_unlocked();
            results.evaluate_query_if_needed(false);
            return results;
        }
        default:
            REALM_COMPILER_HINT_UNREACHABLE();
    }
}

bool Results::is_frozen()
{
    return !m_realm || m_realm->is_frozen();
}

Results::OutOfBoundsIndexException::OutOfBoundsIndexException(size_t r, size_t c)
: std::out_of_range(util::format("Requested index %1 greater than max %2", r, c - 1))
, requested(r), valid_count(c) {}

static std::string unsupported_operation_msg(ColKey column, Table const& table, const char* operation)
{
    auto type = ObjectSchema::from_core_type(table, column);
    const char* column_type = string_for_property_type(type & ~PropertyType::Array);
    if (!is_array(type))
        return util::format("Cannot %1 property '%2': operation not supported for '%3' properties",
                            operation, table.get_column_name(column), column_type);
    return util::format("Cannot %1 '%2' array: operation not supported",
                        operation, column_type);
}

Results::UnsupportedColumnTypeException::UnsupportedColumnTypeException(ColKey column, Table const& table,
                                                                        const char* operation)
: std::logic_error(unsupported_operation_msg(column, table, operation))
, column_key(column)
, column_name(table.get_column_name(column))
, property_type(ObjectSchema::from_core_type(table, ColKey(column)) & ~PropertyType::Array)
{
}

Results::UnsupportedColumnTypeException::UnsupportedColumnTypeException(ColKey column, TableView const& tv,
                                                                        const char* operation)
: UnsupportedColumnTypeException(column, tv.ObjList::get_parent(), operation)
{
}

Results::InvalidPropertyException::InvalidPropertyException(StringData object_type, StringData property_name)
: std::logic_error(util::format("Property '%1.%2' does not exist", object_type, property_name))
, object_type(object_type), property_name(property_name)
{
}

Results::UnimplementedOperationException::UnimplementedOperationException(const char* msg)
: std::logic_error(msg)
{
}

} // namespace realm
