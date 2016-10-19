////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
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

#ifndef REALM_COLLECTION_NOTIFICATIONS_HPP
#define REALM_COLLECTION_NOTIFICATIONS_HPP

#include "index_set.hpp"
#include "util/atomic_shared_ptr.hpp"

#include <exception>
#include <functional>
#include <memory>
#include <vector>

namespace realm {
namespace _impl {
    class CollectionNotifier;
}

// A token which keeps an asynchronous query alive
struct NotificationToken {
    NotificationToken() = default;
    NotificationToken(std::shared_ptr<_impl::CollectionNotifier> notifier, size_t token);
    ~NotificationToken();

    NotificationToken(NotificationToken&&);
    NotificationToken& operator=(NotificationToken&&);

    NotificationToken(NotificationToken const&) = delete;
    NotificationToken& operator=(NotificationToken const&) = delete;

private:
    util::AtomicSharedPtr<_impl::CollectionNotifier> m_notifier;
    size_t m_token;
};

struct CollectionChangeSet {
    struct Move {
        size_t from;
        size_t to;

        bool operator==(Move m) const { return from == m.from && to == m.to; }
    };

    // Indices which were removed from the _old_ collection
    IndexSet deletions;

    // Indices in the _new_ collection which are new insertions
    IndexSet insertions;

    // Indices of objects in the _old_ collection which were modified
    IndexSet modifications;

    // Indices in the _new_ collection which were modified. This will always
    // have the same number of indices as `modifications` and conceptually
    // represents the same entries, just in different versions of the collection.
    // It exists for the sake of code which finds it easier to process
    // modifications after processing deletions and insertions rather than before.
    IndexSet modifications_new;

    // Rows in the collection which moved.
    //
    // Every `from` index will also be present in `deletions` and every `to`
    // index will be present in `insertions`.
    //
    // This is currently not reliably calculated for all types of collections. A
    // reported move will always actually be a move, but there may also be
    // unreported moves which show up only as a delete/insert pair.
    std::vector<Move> moves;

    bool empty() const
    {
        return deletions.empty() && insertions.empty() && modifications.empty()
            && modifications_new.empty() && moves.empty();
    }
};

// A type-erasing wrapper for the callback for collection notifications. Can be
// constructed with either any callable compatible with the signature
// `void (CollectionChangeSet, std::exception_ptr)`, an object with member
// functions `void before(CollectionChangeSet)`, `void after(CollectionChangeSet)`,
// `void error(std::exception_ptr)`, or a pointer to such an object. If a pointer
// is given, the caller is responsible for ensuring that the pointed-to object
// outlives the collection.
class CollectionChangeCallback {
public:
    CollectionChangeCallback(std::nullptr_t={}) { }

    template<typename Callback>
    CollectionChangeCallback(Callback cb) : m_impl(make_impl(std::move(cb))) { }
    template<typename Callback>
    CollectionChangeCallback& operator=(Callback cb) { m_impl = make_impl(std::move(cb)); return *this; }

    // Explicitly default the copy/move constructors as otherwise they'll use
    // the above ones and add an extra layer of wrapping
    CollectionChangeCallback(CollectionChangeCallback&&) = default;
    CollectionChangeCallback(CollectionChangeCallback const&) = default;
    CollectionChangeCallback& operator=(CollectionChangeCallback&&) = default;
    CollectionChangeCallback& operator=(CollectionChangeCallback const&) = default;

    void before(CollectionChangeSet const& c) { m_impl->before(c); }
    void after(CollectionChangeSet const& c) { m_impl->after(c); }
    void error(std::exception_ptr e) { m_impl->error(e); }

    explicit operator bool() const { return !!m_impl; }

private:
    struct Base {
        virtual void before(CollectionChangeSet const&)=0;
        virtual void after(CollectionChangeSet const&)=0;
        virtual void error(std::exception_ptr)=0;
    };

    template<typename Callback, typename = decltype(std::declval<Callback>()(CollectionChangeSet(), std::exception_ptr()))>
    std::shared_ptr<Base> make_impl(Callback cb)
    {
        return std::make_shared<Impl<Callback>>(std::move(cb));
    }

    template<typename Callback, typename = decltype(std::declval<Callback>().after(CollectionChangeSet())), typename = void>
    std::shared_ptr<Base> make_impl(Callback cb)
    {
        return std::make_shared<Impl2<Callback>>(std::move(cb));
    }

    template<typename Callback, typename = decltype(std::declval<Callback>().after(CollectionChangeSet())), typename = void>
    std::shared_ptr<Base> make_impl(Callback* cb)
    {
        return std::make_shared<Impl3<Callback>>(cb);
    }

    template<typename T>
    struct Impl : public Base {
        T impl;
        Impl(T impl) : impl(std::move(impl)) { }
        void before(CollectionChangeSet const&) override { }
        void after(CollectionChangeSet const& change) override { impl(change, {}); }
        void error(std::exception_ptr error) override { impl({}, error); }
    };
    template<typename T>
    struct Impl2 : public Base {
        T impl;
        Impl2(T impl) : impl(std::move(impl)) { }
        void before(CollectionChangeSet const& c) override { impl.before(c); }
        void after(CollectionChangeSet const& c) override { impl.after(c); }
        void error(std::exception_ptr error) override { impl.error(error); }
    };
    template<typename T>
    struct Impl3 : public Base {
        T* impl;
        Impl3(T* impl) : impl(impl) { }
        void before(CollectionChangeSet const& c) override { impl->before(c); }
        void after(CollectionChangeSet const& c) override { impl->after(c); }
        void error(std::exception_ptr error) override { impl->error(error); }
    };

    std::shared_ptr<Base> m_impl;
};
} // namespace realm

#endif // REALM_COLLECTION_NOTIFICATIONS_HPP
