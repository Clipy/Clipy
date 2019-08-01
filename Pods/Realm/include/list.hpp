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

#ifndef REALM_OS_LIST_HPP
#define REALM_OS_LIST_HPP

#include "collection_notifications.hpp"
#include "impl/collection_notifier.hpp"
#include "property.hpp"

#include <realm/link_view_fwd.hpp>
#include <realm/row.hpp>
#include <realm/table_ref.hpp>

#include <functional>
#include <memory>

namespace realm {
class ObjectSchema;
class Query;
class Realm;
class Results;
class SortDescriptor;
template <typename T> class ThreadSafeReference;

namespace _impl {
class ListNotifier;
}

class List {
public:
    List() noexcept;
    List(std::shared_ptr<Realm> r, Table& parent_table, size_t col, size_t row);
    List(std::shared_ptr<Realm> r, LinkViewRef l) noexcept;
    List(std::shared_ptr<Realm> r, TableRef l) noexcept;
    ~List();

    List(const List&);
    List& operator=(const List&);
    List(List&&);
    List& operator=(List&&);

    const std::shared_ptr<Realm>& get_realm() const { return m_realm; }
    Query get_query() const;
    size_t get_origin_row_index() const;

    // Get the type of the values contained in this List
    PropertyType get_type() const;

    // Get the ObjectSchema of the values in this List
    // Only valid if get_type() returns PropertyType::Object
    ObjectSchema const& get_object_schema() const;

    bool is_valid() const;
    void verify_attached() const;
    void verify_in_transaction() const;

    size_t size() const;

    void move(size_t source_ndx, size_t dest_ndx);
    void remove(size_t list_ndx);
    void remove_all();
    void swap(size_t ndx1, size_t ndx2);
    void delete_at(size_t list_ndx);
    void delete_all();

    template<typename T = RowExpr>
    T get(size_t row_ndx) const;
    template<typename T>
    size_t find(T const& value) const;

    // Find the index in the List of the first row matching the query
    size_t find(Query&& query) const;

    template<typename T>
    void add(T value);
    template<typename T>
    void insert(size_t list_ndx, T value);
    template<typename T>
    void set(size_t row_ndx, T value);

    Results sort(SortDescriptor order) const;
    Results sort(std::vector<std::pair<std::string, bool>> const& keypaths) const;
    Results filter(Query q) const;

    // Return a Results representing a live view of this List.
    Results as_results() const;

    // Return a Results representing a snapshot of this List.
    Results snapshot() const;

    // Get the min/max/average/sum of the given column
    // All but sum() returns none when there are zero matching rows
    // sum() returns 0,
    // Throws UnsupportedColumnTypeException for sum/average on timestamp or non-numeric column
    // Throws OutOfBoundsIndexException for an out-of-bounds column
    util::Optional<Mixed> max(size_t column=0);
    util::Optional<Mixed> min(size_t column=0);
    util::Optional<double> average(size_t column=0);
    Mixed sum(size_t column=0);

    bool operator==(List const& rgt) const noexcept;

    NotificationToken add_notification_callback(CollectionChangeCallback cb) &;

    template<typename Context>
    auto get(Context&, size_t row_ndx) const;
    template<typename T, typename Context>
    size_t find(Context&, T&& value) const;

    template<typename T, typename Context>
    void add(Context&, T&& value, bool update=false);
    template<typename T, typename Context>
    void insert(Context&, size_t list_ndx, T&& value, bool update=false);
    template<typename T, typename Context>
    void set(Context&, size_t row_ndx, T&& value, bool update=false);

    // Replace the values in this list with the values from an enumerable object
    template<typename T, typename Context>
    void assign(Context&, T&& value, bool update=false);

    // The List object has been invalidated (due to the Realm being invalidated,
    // or the containing object being deleted)
    // All non-noexcept functions can throw this
    struct InvalidatedException : public std::logic_error {
        InvalidatedException() : std::logic_error("Access to invalidated List object") {}
    };

    // The input index parameter was out of bounds
    struct OutOfBoundsIndexException : public std::out_of_range {
        OutOfBoundsIndexException(size_t r, size_t c);
        size_t requested;
        size_t valid_count;
    };

private:
    friend ThreadSafeReference<List>;

    std::shared_ptr<Realm> m_realm;
    mutable const ObjectSchema* m_object_schema = nullptr;
    LinkViewRef m_link_view;
    TableRef m_table;
    _impl::CollectionNotifier::Handle<_impl::CollectionNotifier> m_notifier;

    void verify_valid_row(size_t row_ndx, bool insertion = false) const;
    void validate(RowExpr) const;

    template<typename Fn>
    auto dispatch(Fn&&) const;

    size_t to_table_ndx(size_t row) const noexcept;

    friend struct std::hash<List>;
};

template<typename Fn>
auto List::dispatch(Fn&& fn) const
{
    return switch_on_type(get_type(), std::forward<Fn>(fn));
}

template<typename Context>
auto List::get(Context& ctx, size_t row_ndx) const
{
    return dispatch([&](auto t) { return ctx.box(this->get<std::decay_t<decltype(*t)>>(row_ndx)); });
}

template<typename T, typename Context>
size_t List::find(Context& ctx, T&& value) const
{
    return dispatch([&](auto t) { return this->find(ctx.template unbox<std::decay_t<decltype(*t)>>(value)); });
}

template<typename T, typename Context>
void List::add(Context& ctx, T&& value, bool update)
{
    dispatch([&](auto t) { this->add(ctx.template unbox<std::decay_t<decltype(*t)>>(value, true, update)); });
}

template<typename T, typename Context>
void List::insert(Context& ctx, size_t list_ndx, T&& value, bool update)
{
    dispatch([&](auto t) { this->insert(list_ndx, ctx.template unbox<std::decay_t<decltype(*t)>>(value, true, update)); });
}

template<typename T, typename Context>
void List::set(Context& ctx, size_t row_ndx, T&& value, bool update)
{
    dispatch([&](auto t) { this->set(row_ndx, ctx.template unbox<std::decay_t<decltype(*t)>>(value, true, update)); });
}

template<typename T, typename Context>
void List::assign(Context& ctx, T&& values, bool update)
{
    if (ctx.is_same_list(*this, values))
        return;

    remove_all();
    if (ctx.is_null(values))
        return;

    ctx.enumerate_list(values, [&](auto&& element) {
        this->add(ctx, element, update);
    });
}
} // namespace realm

namespace std {
template<> struct hash<realm::List> {
    size_t operator()(realm::List const&) const;
};
}

#endif // REALM_OS_LIST_HPP
