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
#include "object.hpp"
#include "property.hpp"
#include "util/copyable_atomic.hpp"

#include <realm/mixed.hpp>
#include <realm/list.hpp>

#include <functional>
#include <memory>

namespace realm {
class Obj;
class ObjectSchema;
class Query;
class Realm;
class Results;
class SortDescriptor;
class ThreadSafeReference;
struct ColKey;
struct ObjKey;

namespace _impl {
class ListNotifier;
}

class List {
public:
    List() noexcept;
    List(std::shared_ptr<Realm> r, const Obj& parent_obj, ColKey col);
    List(std::shared_ptr<Realm> r, const LstBase& list);
    ~List();

    List(const List&);
    List& operator=(const List&);
    List(List&&);
    List& operator=(List&&);

    const std::shared_ptr<Realm>& get_realm() const { return m_realm; }
    Query get_query() const;

    ColKey get_parent_column_key() const;
    ObjKey get_parent_object_key() const;
    TableKey get_parent_table_key() const;

    // Get the type of the values contained in this List
    PropertyType get_type() const { return m_type; }

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

    template<typename T = Obj>
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

    // Returns a frozen copy of this result
    List freeze(std::shared_ptr<Realm> const& realm) const;

    // Returns whether or not this List is frozen.
    bool is_frozen() const noexcept;

    // Get the min/max/average/sum of the given column
    // All but sum() returns none when there are zero matching rows
    // sum() returns 0,
    // Throws UnsupportedColumnTypeException for sum/average on timestamp or non-numeric column
    // Throws OutOfBoundsIndexException for an out-of-bounds column
    util::Optional<Mixed> max(ColKey column={}) const;
    util::Optional<Mixed> min(ColKey column={}) const;
    util::Optional<double> average(ColKey column={}) const;
    Mixed sum(ColKey column={}) const;

    bool operator==(List const& rgt) const noexcept;

    NotificationToken add_notification_callback(CollectionChangeCallback cb) &;

    template<typename Context>
    auto get(Context&, size_t row_ndx) const;
    template<typename T, typename Context>
    size_t find(Context&, T&& value) const;

    template<typename T, typename Context>
    void add(Context&, T&& value, CreatePolicy=CreatePolicy::ForceCreate);
    template<typename T, typename Context>
    void insert(Context&, size_t list_ndx, T&& value, CreatePolicy=CreatePolicy::ForceCreate);
    template<typename T, typename Context>
    void set(Context&, size_t row_ndx, T&& value, CreatePolicy=CreatePolicy::ForceCreate);

    // Replace the values in this list with the values from an enumerable object
    template<typename T, typename Context>
    void assign(Context&, T&& value, CreatePolicy=CreatePolicy::ForceCreate);

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
    std::shared_ptr<Realm> m_realm;
    PropertyType m_type;
    mutable util::CopyableAtomic<const ObjectSchema*> m_object_schema = nullptr;
    _impl::CollectionNotifier::Handle<_impl::ListNotifier> m_notifier;
    std::shared_ptr<LstBase> m_list_base;

    void verify_valid_row(size_t row_ndx, bool insertion = false) const;
    void validate(const Obj&) const;

    template<typename Fn>
    auto dispatch(Fn&&) const;
    template<typename T>
    auto& as() const;

    template<typename T, typename Context>
    void set_if_different(Context&, size_t row_ndx, T&& value, CreatePolicy);

    friend struct std::hash<List>;
};

template<typename T>
auto& List::as() const
{
    return static_cast<Lst<T>&>(*m_list_base);
}

template<>
inline auto& List::as<Obj>() const
{
    return static_cast<LnkLst&>(*m_list_base);
}

template<typename Fn>
auto List::dispatch(Fn&& fn) const
{
    verify_attached();
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
    return dispatch([&](auto t) { return this->find(ctx.template unbox<std::decay_t<decltype(*t)>>(value, CreatePolicy::Skip)); });
}

template<typename T, typename Context>
void List::add(Context& ctx, T&& value, CreatePolicy policy)
{
    dispatch([&](auto t) { this->add(ctx.template unbox<std::decay_t<decltype(*t)>>(value, policy)); });
}

template<typename T, typename Context>
void List::insert(Context& ctx, size_t list_ndx, T&& value, CreatePolicy policy)
{
    dispatch([&](auto t) { this->insert(list_ndx, ctx.template unbox<std::decay_t<decltype(*t)>>(value, policy)); });
}

template<typename T, typename Context>
void List::set(Context& ctx, size_t row_ndx, T&& value, CreatePolicy policy)
{
    dispatch([&](auto t) { this->set(row_ndx, ctx.template unbox<std::decay_t<decltype(*t)>>(value, policy)); });
}

namespace _impl {
template <class T>
inline ObjKey help_get_current_row(const T&)
{
    return ObjKey();
}

template <>
inline ObjKey help_get_current_row(const ConstObj& v)
{
    return v.get_key();
}

template <>
inline ObjKey help_get_current_row(const Obj& v)
{
    return v.get_key();
}

template <class T>
inline bool help_compare_values(const T& v1, const T& v2)
{
    return v1 != v2;
}
template <>
inline bool help_compare_values(const Obj& v1, const Obj& v2)
{
    return v1.get_table() != v2.get_table() || v1.get_key() != v2.get_key();
}
}

template<typename T, typename Context>
void List::set_if_different(Context& ctx, size_t row_ndx, T&& value, CreatePolicy policy)
{
    dispatch([&](auto t) {
        using U = std::decay_t<decltype(*t)>;
        auto old_value =  this->get<U>(row_ndx);
        auto new_value = ctx.template unbox<U>(value, policy, _impl::help_get_current_row(old_value));
        if (_impl::help_compare_values(old_value, new_value))
            this->set(row_ndx, new_value);
    });
}


template<typename T, typename Context>
void List::assign(Context& ctx, T&& values, CreatePolicy policy)
{
    if (ctx.is_same_list(*this, values))
        return;

    if (ctx.is_null(values)) {
        remove_all();
        return;
    }

    if (policy == CreatePolicy::UpdateModified) {
        size_t sz = size();
        size_t index = 0;
        ctx.enumerate_list(values, [&](auto&& element) {
            if (index < sz) {
                this->set_if_different(ctx, index, element, policy);
            }
            else {
                this->add(ctx, element, policy);
            }
            index++;
        });
        while (index < sz) {
            remove(--sz);
        }
    }
    else {
        remove_all();
        ctx.enumerate_list(values, [&](auto&& element) {
            this->add(ctx, element, policy);
        });
    }
}
} // namespace realm

namespace std {
template<> struct hash<realm::List> {
    size_t operator()(realm::List const&) const;
};
}

#endif // REALM_OS_LIST_HPP
