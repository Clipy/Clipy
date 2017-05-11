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

#ifndef REALM_LIST_HPP
#define REALM_LIST_HPP

#include "collection_notifications.hpp"
#include "impl/collection_notifier.hpp"

#include <realm/link_view_fwd.hpp>
#include <realm/row.hpp>

#include <functional>
#include <memory>

namespace realm {
using RowExpr = BasicRowExpr<Table>;

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
    List(std::shared_ptr<Realm> r, LinkViewRef l) noexcept;
    ~List();

    List(const List&);
    List& operator=(const List&);
    List(List&&);
    List& operator=(List&&);

    const std::shared_ptr<Realm>& get_realm() const { return m_realm; }
    Query get_query() const;
    const ObjectSchema& get_object_schema() const;
    size_t get_origin_row_index() const;

    bool is_valid() const;
    void verify_attached() const;
    void verify_in_transaction() const;

    size_t size() const;
    RowExpr get(size_t row_ndx) const;
    size_t get_unchecked(size_t row_ndx) const noexcept;
    size_t find(ConstRow const& row) const;

    void add(size_t target_row_ndx);
    void insert(size_t list_ndx, size_t target_row_ndx);
    void move(size_t source_ndx, size_t dest_ndx);
    void remove(size_t list_ndx);
    void remove_all();
    void set(size_t row_ndx, size_t target_row_ndx);
    void swap(size_t ndx1, size_t ndx2);

    void delete_all();

    Results sort(SortDescriptor order);
    Results filter(Query q);

    // Return a Results representing a snapshot of this List.
    Results snapshot() const;

    // Get the min/max/average/sum of the given column
    // All but sum() returns none when there are zero matching rows
    // sum() returns 0, except for when it returns none
    // Throws UnsupportedColumnTypeException for sum/average on timestamp or non-numeric column
    // Throws OutOfBoundsIndexException for an out-of-bounds column
    util::Optional<Mixed> max(size_t column);
    util::Optional<Mixed> min(size_t column);
    util::Optional<Mixed> average(size_t column);
    util::Optional<Mixed> sum(size_t column);

    bool operator==(List const& rgt) const noexcept;

    NotificationToken add_notification_callback(CollectionChangeCallback cb) &;

    // These are implemented in object_accessor.hpp
    template <typename ValueType, typename ContextType>
    void add(ContextType ctx, ValueType value);

    template <typename ValueType, typename ContextType>
    void insert(ContextType ctx, ValueType value, size_t list_ndx);

    template <typename ValueType, typename ContextType>
    void set(ContextType ctx, ValueType value, size_t list_ndx);

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
    _impl::CollectionNotifier::Handle<_impl::ListNotifier> m_notifier;

    void verify_valid_row(size_t row_ndx, bool insertion = false) const;

    friend struct std::hash<List>;
};
} // namespace realm

namespace std {
template<> struct hash<realm::List> {
    size_t operator()(realm::List const&) const;
};
}

#endif /* REALM_LIST_HPP */
