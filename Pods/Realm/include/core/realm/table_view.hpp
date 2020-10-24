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

#ifndef REALM_TABLE_VIEW_HPP
#define REALM_TABLE_VIEW_HPP

#include <realm/sort_descriptor.hpp>
#include <realm/table.hpp>
#include <realm/util/features.h>
#include <realm/obj_list.hpp>

namespace realm {

// Views, tables and synchronization between them:
//
// Views are built through queries against either tables or another view.
// Views may be restricted to only hold entries provided by another view.
// this other view is called the "restricting view".
// Views may be sorted in ascending or descending order of values in one ore more columns.
//
// Views remember the query from which it was originally built.
// Views remember the table from which it was originally built.
// Views remember a restricting view if one was used when it was originally built.
// Views remember the sorting criteria (columns and direction)
//
// A view may be operated in one of two distinct modes: *reflective* and *imperative*.
// Sometimes the term "reactive" is used instead of "reflective" with the same meaning.
//
// Reflective views:
// - A reflective view *always* *reflect* the result of running the query.
//   If the underlying tables or tableviews change, the reflective view changes as well.
//   A reflective view may need to rerun the query it was generated from, a potentially
//   costly operation which happens on demand.
// - It does not matter whether changes are explicitly done within the transaction, or
//   occur implicitly as part of advance_read() or promote_to_write().
//
// Imperative views:
// - An imperative view only *initially* holds the result of the query. An imperative
//   view *never* reruns the query. To force the view to match it's query (by rerunning it),
//   the view must be operated in reflective mode.
//   An imperative view can be modified explicitly. References can be added, removed or
//   changed.
//
// - In imperative mode, the references in the view tracks movement of the referenced data:
//   If you delete an entry which is referenced from a view, said reference is detached,
//   not removed.
// - It does not matter whether the delete is done in-line (as part of the current transaction),
//   or if it is done implicitly as part of advance_read() or promote_to_write().
//
// The choice between reflective and imperative views might eventually be represented by a
// switch on the tableview, but isn't yet. For now, clients (bindings) must call sync_if_needed()
// to get reflective behavior.
//
// Use cases:
//
// 1. Presenting data
// The first use case (and primary motivator behind the reflective view) is to just track
// and present the state of the database. In this case, the view is operated in reflective
// mode, it is not modified within the transaction, and it is not used to modify data in
// other parts of the database.
//
// 2. Handover
// The second use case is "handover." The implicit rerun of the query in our first use case
// may be too costly to be acceptable on the main thread. Instead you want to run the query
// on a worker thread, but display it on the main thread. To achieve this, you need two
// SharedGroups locked on to the same version of the database. If you have that, you can
// *handover* a view from one thread/SharedGroup to the other.
//
// Handover is a two-step procedure. First, the accessors are *exported* from one SharedGroup,
// called the sourcing group, then it is *imported* into another SharedGroup, called the
// receiving group. The thread associated with the sourcing SharedGroup will be
// responsible for the export operation, while the thread associated with the receiving
// SharedGroup will do the import operation.
//
// 3. Iterating a view and changing data
// The third use case (and a motivator behind the imperative view) is when you want
// to make changes to the database in accordance with a query result. Imagine you want to
// find all employees with a salary below a limit and raise their salaries to the limit (pseudocode):
//
//    promote_to_write();
//    view = table.where().less_than(salary_column,limit).find_all();
//    for (size_t i = 0; i < view.size(); ++i) {
//        view.set_int(salary_column, i, limit);
//        // add this to get reflective mode: view.sync_if_needed();
//    }
//    commit_and_continue_as_read();
//
// This is idiomatic imperative code and it works if the view is operated in imperative mode.
//
// If the view is operated in reflective mode, the behaviour surprises most people: When the
// first salary is changed, the entry no longer fullfills the query, so it is dropped from the
// view implicitly. view[0] is removed, view[1] moves to view[0] and so forth. But the next
// loop iteration has i=1 and refers to view[1], thus skipping view[0]. The end result is that
// every other employee get a raise, while the others don't.
//
// 4. Iterating intermixed with implicit updates
// This leads us to use case 4, which is similar to use case 3, but uses promote_to_write()
// intermixed with iterating a view. This is actually quite important to some, who do not want
// to end up with a large write transaction.
//
//    view = table.where().less_than(salary_column,limit).find_all();
//    for (size_t i = 0; i < view.size(); ++i) {
//        promote_to_write();
//        view.set_int(salary_column, i, limit);
//        commit_and_continue_as_write();
//    }
//
// Anything can happen at the call to promote_to_write(). The key question then becomes: how
// do we support a safe way of realising the original goal (raising salaries) ?
//
// using the imperative operating mode:
//
//    view = table.where().less_than(salary_column,limit).find_all();
//    for (size_t i = 0; i < view.size(); ++i) {
//        promote_to_write();
//        // add r.sync_if_needed(); to get reflective mode
//        if (r.is_row_attached(i)) {
//            Row r = view[i];
//            r.set_int(salary_column, limit);
//        }
//        commit_and_continue_as_write();
//    }
//
// This is safe, and we just aim for providing low level safety: is_row_attached() can tell
// if the reference is valid, and the references in the view continue to point to the
// same object at all times, also following implicit updates. The rest is up to the
// application logic.
//
// It is important to see, that there is no guarantee that all relevant employees get
// their raise in cases whith concurrent updates. At every call to promote_to_write() new
// employees may be added to the underlying table, but as the view is in imperative mode,
// these new employees are not added to the view. Also at promote_to_write() an existing
// employee could recieve a (different, larger) raise which would then be overwritten and lost.
// However, these are problems that you should expect, since the activity is spread over multiple
// transactions.


/// A ConstTableView gives read access to the parent table, but no
/// write access. The view itself, though, can be changed, for
/// example, it can be sorted.
///
/// Note that methods are declared 'const' if, and only if they leave
/// the view unmodified, and this is irrespective of whether they
/// modify the parent table.
///
/// A ConstTableView has both copy and move semantics. See TableView
/// for more on this.
class ConstTableView : public ObjList {
public:

    /// Construct null view (no memory allocated).
    ConstTableView()
        : ObjList(&m_table_view_key_values)
        , m_table_view_key_values(Allocator::get_default())
    {
    }


    /// Construct empty view, ready for addition of row indices.
    ConstTableView(ConstTableRef parent);
    ConstTableView(ConstTableRef parent, Query& query, size_t start, size_t end, size_t limit);
    ConstTableView(ConstTableRef parent, ColKey column, const ConstObj& obj);
    ConstTableView(ConstTableRef parent, ConstLnkLstPtr link_list);

    enum DistinctViewTag { DistinctView };
    ConstTableView(DistinctViewTag, ConstTableRef parent, ColKey column_key);

    /// Copy constructor.
    ConstTableView(const ConstTableView&);

    /// Move constructor.
    ConstTableView(ConstTableView&&) noexcept;

    ConstTableView& operator=(const ConstTableView&);
    ConstTableView& operator=(ConstTableView&&) noexcept;

    ConstTableView(const ConstTableView& source, Transaction* tr, PayloadPolicy mode);
    ConstTableView(ConstTableView& source, Transaction* tr, PayloadPolicy mode);

    ~ConstTableView()
    {
        m_key_values->destroy(); // Shallow
    }
    // - not in use / implemented yet:   ... explicit calls to sync_if_needed() must be used
    //                                       to get 'reflective' mode.
    //    enum mode { mode_Reflective, mode_Imperative };
    //    void set_operating_mode(mode);
    //    mode get_operating_mode();
    bool is_empty() const noexcept
    {
        return m_key_values->size() == 0;
    }

    // Tells if the table that this TableView points at still exists or has been deleted.
    bool is_attached() const noexcept
    {
        return bool(m_table);
    }

    bool is_obj_valid(size_t row_ndx) const noexcept
    {
        return m_table->is_valid(ObjKey(m_key_values->get(row_ndx)));
    }

    // Get the query used to create this TableView
    // The query will have a null source table if this tv was not created from
    // a query
    const Query& get_query() const noexcept
    {
        return m_query;
    }

    std::unique_ptr<ConstTableView> clone() const
    {
        return std::unique_ptr<ConstTableView>(new ConstTableView(*this));
    }

    // handover machinery entry points based on dynamic type. These methods:
    // a) forward their calls to the static type entry points.
    // b) new/delete patch data structures.
    std::unique_ptr<ConstTableView> clone_for_handover(Transaction* tr, PayloadPolicy mode) const
    {
        std::unique_ptr<ConstTableView> retval(new ConstTableView(*this, tr, mode));
        return retval;
    }
    template <Action action, typename T, typename R>
    R aggregate(ColKey column_key, size_t* result_count = nullptr, ObjKey* return_key = nullptr) const;
    template <typename T>
    size_t aggregate_count(ColKey column_key, T count_target) const;

    int64_t sum_int(ColKey column_key) const;
    int64_t maximum_int(ColKey column_key, ObjKey* return_key = nullptr) const;
    int64_t minimum_int(ColKey column_key, ObjKey* return_key = nullptr) const;
    double average_int(ColKey column_key, size_t* value_count = nullptr) const;
    size_t count_int(ColKey column_key, int64_t target) const;

    double sum_float(ColKey column_key) const;
    float maximum_float(ColKey column_key, ObjKey* return_key = nullptr) const;
    float minimum_float(ColKey column_key, ObjKey* return_key = nullptr) const;
    double average_float(ColKey column_key, size_t* value_count = nullptr) const;
    size_t count_float(ColKey column_key, float target) const;

    double sum_double(ColKey column_key) const;
    double maximum_double(ColKey column_key, ObjKey* return_key = nullptr) const;
    double minimum_double(ColKey column_key, ObjKey* return_key = nullptr) const;
    double average_double(ColKey column_key, size_t* value_count = nullptr) const;
    size_t count_double(ColKey column_key, double target) const;

    Timestamp minimum_timestamp(ColKey column_key, ObjKey* return_key = nullptr) const;
    Timestamp maximum_timestamp(ColKey column_key, ObjKey* return_key = nullptr) const;
    size_t count_timestamp(ColKey column_key, Timestamp target) const;

    /// Search this view for the specified key. If found, the index of that row
    /// within this view is returned, otherwise `realm::not_found` is returned.
    size_t find_by_source_ndx(ObjKey key) const noexcept
    {
        return m_key_values->find_first(key);
    }

    // Conversion
    void to_json(std::ostream&, size_t link_depth = 0, std::map<std::string, std::string>* renames = nullptr) const;

    // Determine if the view is 'in sync' with the underlying table
    // as well as other views used to generate the view. Note that updates
    // through views maintains synchronization between view and table.
    // It doesnt by itself maintain other views as well. So if a view
    // is generated from another view (not a table), updates may cause
    // that view to be outdated, AND as the generated view depends upon
    // it, it too will become outdated.
    bool is_in_sync() const override;

    // A TableView is frozen if it is a) obtained from a query against a frozen table
    // and b) is synchronized (is_in_sync())
    bool is_frozen() { return m_table->is_frozen() && is_in_sync(); }
    // Tells if this TableView depends on a LinkList or row that has been deleted.
    bool depends_on_deleted_object() const;

    // Synchronize a view to match a table or tableview from which it
    // has been derived. Synchronization is achieved by rerunning the
    // query used to generate the view. If derived from another view, that
    // view will be synchronized as well.
    //
    // "live" or "reactive" views are implemented by calling sync_if_needed
    // before any of the other access-methods whenever the view may have become
    // outdated.
    //
    // This will make the TableView empty and in sync with the highest possible table version
    // if the TableView depends on an object (LinkView or row) that has been deleted.
    void sync_if_needed() const override;
    // Return the version of the source it was created from.
    TableVersions get_dependency_versions() const
    {
        TableVersions ret;
        get_dependencies(ret);
        return ret;
    }


    // Sort m_key_values according to one column
    void sort(ColKey column, bool ascending = true);

    // Sort m_key_values according to multiple columns
    void sort(SortDescriptor order);

    // Remove rows that are duplicated with respect to the column set passed as argument.
    // distinct() will preserve the original order of the row pointers, also if the order is a result of sort()
    // If two rows are indentical (for the given set of distinct-columns), then the last row is removed.
    // You can call sync_if_needed() to update the distinct view, just like you can for a sorted view.
    // Each time you call distinct() it will compound on the previous calls
    void distinct(ColKey column);
    void distinct(DistinctDescriptor columns);
    void limit(LimitDescriptor limit);
    void include(IncludeDescriptor include_paths);
    IncludeDescriptor get_include_descriptors();

    // Replace the order of sort and distinct operations, bypassing manually
    // calling sort and distinct. This is a convenience method for bindings.
    void apply_descriptor_ordering(const DescriptorOrdering& new_ordering);

    // Gets a readable and parsable string which completely describes the sort and
    // distinct operations applied to this view.
    std::string get_descriptor_ordering_description() const;

    // Returns whether the rows are guaranteed to be in table order.
    // This is true only of unsorted TableViews created from either:
    // - Table::find_all()
    // - Query::find_all() when the query is not restricted to a view.
    bool is_in_table_order() const;

    bool is_backlink_view() const
    {
        return m_source_column_key != ColKey();
    }

protected:
    // This TableView can be "born" from 4 different sources:
    // - LinkView
    // - Query::find_all()
    // - Table::get_distinct_view()
    // - Table::get_backlink_view()

    void get_dependencies(TableVersions&) const override;

    void do_sync();

    // The source column index that this view contain backlinks for.
    ColKey m_source_column_key;
    // The target object that rows in this view link to.
    ObjKey m_linked_obj_key;
    ConstTableRef m_linked_table;

    // If this TableView was created from a LinkList, then this reference points to it. Otherwise it's 0
    mutable ConstLnkLstPtr m_linklist_source;

    // m_distinct_column_source != ColKey() if this view was created from distinct values in a column of m_table.
    ColKey m_distinct_column_source;

    // Stores the ordering criteria of applied sort and distinct operations.
    DescriptorOrdering m_descriptor_ordering;

    // A valid query holds a reference to its table which must match our m_table.
    // hence we can use a query with a null table reference to indicate that the view
    // was NOT generated by a query, but follows a table directly.
    Query m_query;
    // parameters for findall, needed to rerun the query
    size_t m_start = 0;
    size_t m_end = size_t(-1);
    size_t m_limit = size_t(-1);

    mutable TableVersions m_last_seen_versions;

private:
    KeyColumn m_table_view_key_values; // We should generally not use this name
    ObjKey find_first_integer(ColKey column_key, int64_t value) const;
    template <class oper>
    Timestamp minmax_timestamp(ColKey column_key, ObjKey* return_key) const;
    RaceDetector m_race_detector;

    friend class Table;
    friend class ConstObj;
    friend class Query;
    friend class DB;
};

enum class RemoveMode { ordered, unordered };


/// A TableView gives read and write access to the parent table.
///
/// A 'const TableView' cannot be changed (e.g. sorted), nor can the
/// parent table be modified through it.
///
/// A TableView is both copyable and movable.
class TableView : public ConstTableView {
public:
    using ConstTableView::ConstTableView;

    TableView() = default;

    TableRef get_parent() noexcept
    {
        return m_table.cast_away_const();
    }

    // Rows
    Obj get(size_t row_ndx);
    Obj front();
    Obj back();
    Obj operator[](size_t row_ndx);

    /// \defgroup table_view_removes
    //@{
    /// \brief Remove the specified row (or rows) from the underlying table.
    ///
    /// remove() removes the specified row from the underlying table,
    /// remove_last() removes the last row in the table view from the underlying
    /// table, and clear removes all the rows in the table view from the
    /// underlying table.
    ///
    /// When rows are removed from the underlying table, they will by necessity
    /// also be removed from the table view. The order of the remaining rows in
    /// the the table view will be maintained.
    ///
    /// \param row_ndx The index within this table view of the row to be removed.
    void remove(size_t row_ndx);
    void remove_last();
    void clear();
    //@}

    std::unique_ptr<TableView> clone() const
    {
        return std::unique_ptr<TableView>(new TableView(*this));
    }

    std::unique_ptr<TableView> clone_for_handover(Transaction* tr, PayloadPolicy policy) const
    {
        std::unique_ptr<TableView> retval(new TableView(*this, tr, policy));
        return retval;
    }

private:
    TableView(TableRef parent);
    TableView(TableRef parent, Query& query, size_t start, size_t end, size_t limit);
    TableView(TableRef parent, ConstLnkLstPtr);
    TableView(DistinctViewTag, TableRef parent, ColKey column_key);

    friend class ConstTableView;
    friend class Table;
    friend class Query;
    friend class LnkLst;
};




// ================================================================================================
// ConstTableView Implementation:

inline ConstTableView::ConstTableView(ConstTableRef parent)
    : ObjList(&m_table_view_key_values, parent) // Throws
    , m_table_view_key_values(Allocator::get_default())
{
    m_table_view_key_values.create();
    if (m_table) {
        m_last_seen_versions.emplace_back(m_table->get_key(), m_table->get_content_version());
    }
}

inline ConstTableView::ConstTableView(ConstTableRef parent, Query& query, size_t start, size_t end, size_t lim)
    : ObjList(&m_table_view_key_values, parent)
    , m_query(query)
    , m_start(start)
    , m_end(end)
    , m_limit(lim)
    , m_table_view_key_values(Allocator::get_default())
{
    m_table_view_key_values.create();
}

inline ConstTableView::ConstTableView(ConstTableRef src_table, ColKey src_column_key, const ConstObj& obj)
    : ObjList(&m_table_view_key_values, src_table) // Throws
    , m_source_column_key(src_column_key)
    , m_linked_obj_key(obj.get_key())
    , m_linked_table(obj.get_table())
    , m_table_view_key_values(Allocator::get_default())
{
    m_table_view_key_values.create();
    if (m_table) {
        m_last_seen_versions.emplace_back(m_table->get_key(), m_table->get_content_version());
        m_last_seen_versions.emplace_back(obj.get_table()->get_key(), obj.get_table()->get_content_version());
    }
}

inline ConstTableView::ConstTableView(DistinctViewTag, ConstTableRef parent, ColKey column_key)
    : ObjList(&m_table_view_key_values, parent) // Throws
    , m_distinct_column_source(column_key)
    , m_table_view_key_values(Allocator::get_default())
{
    REALM_ASSERT(m_distinct_column_source != ColKey());
    m_table_view_key_values.create();
    if (m_table) {
        m_last_seen_versions.emplace_back(m_table->get_key(), m_table->get_content_version());
    }
}

inline ConstTableView::ConstTableView(ConstTableRef parent, ConstLnkLstPtr link_list)
    : ObjList(&m_table_view_key_values, parent) // Throws
    , m_linklist_source(std::move(link_list))
    , m_table_view_key_values(Allocator::get_default())
{
    REALM_ASSERT(m_linklist_source);
    m_table_view_key_values.create();
    if (m_table) {
        m_last_seen_versions.emplace_back(m_table->get_key(), m_table->get_content_version());
    }
}

inline ConstTableView::ConstTableView(const ConstTableView& tv)
    : ObjList(&m_table_view_key_values, tv.m_table)
    , m_source_column_key(tv.m_source_column_key)
    , m_linked_obj_key(tv.m_linked_obj_key)
    , m_linked_table(tv.m_linked_table)
    , m_linklist_source(tv.m_linklist_source ? tv.m_linklist_source->clone() : LnkLstPtr{})
    , m_distinct_column_source(tv.m_distinct_column_source)
    , m_descriptor_ordering(tv.m_descriptor_ordering)
    , m_query(tv.m_query)
    , m_start(tv.m_start)
    , m_end(tv.m_end)
    , m_limit(tv.m_limit)
    , m_last_seen_versions(tv.m_last_seen_versions)
    , m_table_view_key_values(tv.m_table_view_key_values)
{
    m_limit_count = tv.m_limit_count;
}

inline ConstTableView::ConstTableView(ConstTableView&& tv) noexcept
    : ObjList(&m_table_view_key_values, tv.m_table)
    , m_source_column_key(tv.m_source_column_key)
    , m_linked_obj_key(tv.m_linked_obj_key)
    , m_linked_table(tv.m_linked_table)
    , m_linklist_source(std::move(tv.m_linklist_source))
    , m_distinct_column_source(tv.m_distinct_column_source)
    , m_descriptor_ordering(std::move(tv.m_descriptor_ordering))
    , m_query(std::move(tv.m_query))
    , m_start(tv.m_start)
    , m_end(tv.m_end)
    , m_limit(tv.m_limit)
    // if we are created from a table view which is outdated, take care to use the outdated
    // version number so that we can later trigger a sync if needed.
    , m_last_seen_versions(std::move(tv.m_last_seen_versions))
    , m_table_view_key_values(std::move(tv.m_table_view_key_values))
{
    m_limit_count = tv.m_limit_count;
}

inline ConstTableView& ConstTableView::operator=(ConstTableView&& tv) noexcept
{
    m_table = std::move(tv.m_table);

    m_table_view_key_values = std::move(tv.m_table_view_key_values);
    m_query = std::move(tv.m_query);
    m_last_seen_versions = tv.m_last_seen_versions;
    m_start = tv.m_start;
    m_end = tv.m_end;
    m_limit = tv.m_limit;
    m_limit_count = tv.m_limit_count;
    m_source_column_key = tv.m_source_column_key;
    m_linked_obj_key = tv.m_linked_obj_key;
    m_linked_table = tv.m_linked_table;
    m_linklist_source = std::move(tv.m_linklist_source);
    m_descriptor_ordering = std::move(tv.m_descriptor_ordering);
    m_distinct_column_source = tv.m_distinct_column_source;

    return *this;
}

inline ConstTableView& ConstTableView::operator=(const ConstTableView& tv)
{
    if (this == &tv)
        return *this;

    m_table_view_key_values = tv.m_table_view_key_values;

    m_query = tv.m_query;
    m_last_seen_versions = tv.m_last_seen_versions;
    m_start = tv.m_start;
    m_end = tv.m_end;
    m_limit = tv.m_limit;
    m_limit_count = tv.m_limit_count;
    m_source_column_key = tv.m_source_column_key;
    m_linked_obj_key = tv.m_linked_obj_key;
    m_linked_table = tv.m_linked_table;
    m_linklist_source = tv.m_linklist_source ? tv.m_linklist_source->clone() : LnkLstPtr{};
    m_descriptor_ordering = tv.m_descriptor_ordering;
    m_distinct_column_source = tv.m_distinct_column_source;

    return *this;
}

#define REALM_ASSERT_COLUMN(column_key)                                                                              \
    m_table.check();                                                                                                 \
    REALM_ASSERT(m_table->colkey2ndx(column_key))

#define REALM_ASSERT_ROW(row_ndx)                                                                                    \
    m_table.check();                                                                                                 \
    REALM_ASSERT(row_ndx < m_key_values->size())

#define REALM_ASSERT_COLUMN_AND_TYPE(column_key, column_type)                                                        \
    REALM_ASSERT_COLUMN(column_key);                                                                                 \
    REALM_DIAG_PUSH();                                                                                               \
    REALM_DIAG_IGNORE_TAUTOLOGICAL_COMPARE();                                                                        \
    REALM_ASSERT(m_table->get_column_type(column_key) == column_type);                                               \
    REALM_DIAG_POP()

#define REALM_ASSERT_INDEX(column_key, row_ndx)                                                                      \
    REALM_ASSERT_COLUMN(column_key);                                                                                 \
    REALM_ASSERT(row_ndx < m_key_values->size())

#define REALM_ASSERT_INDEX_AND_TYPE(column_key, row_ndx, column_type)                                                \
    REALM_ASSERT_COLUMN_AND_TYPE(column_key, column_type);                                                           \
    REALM_ASSERT(row_ndx < m_key_values->size())

#define REALM_ASSERT_INDEX_AND_TYPE_TABLE_OR_MIXED(column_key, row_ndx)                                              \
    REALM_ASSERT_COLUMN(column_key);                                                                                 \
    REALM_DIAG_PUSH();                                                                                               \
    REALM_DIAG_IGNORE_TAUTOLOGICAL_COMPARE();                                                                        \
    REALM_ASSERT(m_table->get_column_type(column_key) == type_Table ||                                               \
                 (m_table->get_column_type(column_key) == type_Mixed));                                              \
    REALM_DIAG_POP();                                                                                                \
    REALM_ASSERT(row_ndx < m_key_values->size())

template <class T>
ConstTableView ObjList::find_all(ColKey column_key, T value)
{
    ConstTableView tv(m_table);
    auto keys = tv.m_key_values;
    for_each([column_key, value, &keys](ConstObj& o) {
        if (o.get<T>(column_key) == value) {
            keys->add(o.get_key());
        }
        return false;
    });
    return tv;
}

//-------------------------- TableView, ConstTableView implementation:


inline void TableView::remove_last()
{
    if (!is_empty())
        remove(size() - 1);
}

inline TableView::TableView(TableRef parent)
    : ConstTableView(parent)
{
}

inline TableView::TableView(TableRef parent, Query& query, size_t start, size_t end, size_t lim)
    : ConstTableView(parent, query, start, end, lim)
{
}

inline TableView::TableView(TableRef parent, ConstLnkLstPtr link_list)
    : ConstTableView(parent, std::move(link_list))
{
}

inline TableView::TableView(ConstTableView::DistinctViewTag, TableRef parent, ColKey column_key)
    : ConstTableView(ConstTableView::DistinctView, parent, column_key)
{
}

// Rows
inline Obj TableView::get(size_t row_ndx)
{
    REALM_ASSERT_ROW(row_ndx);
    ObjKey key(m_key_values->get(row_ndx));
    REALM_ASSERT(key != realm::null_key);
    return get_parent()->get_object(key);
}

inline Obj TableView::front()
{
    return get(0);
}

inline Obj TableView::back()
{
    size_t last_row_ndx = size() - 1;
    return get(last_row_ndx);
}

inline Obj TableView::operator[](size_t row_ndx)
{
    return get(row_ndx);
}

} // namespace realm

#endif // REALM_TABLE_VIEW_HPP
