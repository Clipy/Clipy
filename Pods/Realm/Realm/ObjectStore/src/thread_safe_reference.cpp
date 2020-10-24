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

#include "thread_safe_reference.hpp"

#include "list.hpp"
#include "object.hpp"
#include "object_schema.hpp"
#include "results.hpp"
#include "shared_realm.hpp"

#include "impl/realm_coordinator.hpp"

#include <realm/db.hpp>
#include <realm/keys.hpp>

namespace realm {
class ThreadSafeReference::Payload {
public:
    virtual ~Payload() = default;
    Payload(Realm& realm)
    : m_transaction(realm.is_in_read_transaction() ? realm.duplicate() : nullptr)
    , m_coordinator(Realm::Internal::get_coordinator(realm).shared_from_this())
    , m_created_in_write_transaction(realm.is_in_transaction())
    {
    }

    void refresh_target_realm(Realm&);

protected:
    const TransactionRef m_transaction;

private:
    const std::shared_ptr<_impl::RealmCoordinator> m_coordinator;
    const bool m_created_in_write_transaction;
};

void ThreadSafeReference::Payload::refresh_target_realm(Realm& realm)
{
    if (!realm.is_in_read_transaction()) {
        if (m_created_in_write_transaction)
            realm.read_group();
        else
            Realm::Internal::begin_read(realm, m_transaction->get_version_of_current_transaction());
    }
    else {
        auto version = realm.read_transaction_version();
        auto target_version = m_transaction->get_version_of_current_transaction();
        if (version < target_version || (version == target_version && m_created_in_write_transaction))
            realm.refresh();
    }
}

template<>
class ThreadSafeReference::PayloadImpl<List> : public ThreadSafeReference::Payload {
public:
    PayloadImpl(List const& list)
    : Payload(*list.get_realm())
    , m_key(list.get_parent_object_key())
    , m_table_key(list.get_parent_table_key())
    , m_col_key(list.get_parent_column_key())
    {
    }

    List import_into(std::shared_ptr<Realm> const& r)
    {
        Obj obj = r->read_group().get_table(m_table_key)->get_object(m_key);
        return List(r, obj, m_col_key);
    }

private:
    ObjKey m_key;
    TableKey m_table_key;
    ColKey m_col_key;
};

template<>
class ThreadSafeReference::PayloadImpl<Object> : public ThreadSafeReference::Payload {
public:
    PayloadImpl(Object const& object)
    : Payload(*object.get_realm())
    , m_key(object.obj().get_key())
    , m_object_schema_name(object.get_object_schema().name)
    {
    }

    Object import_into(std::shared_ptr<Realm> const& r)
    {
        return Object(r, m_object_schema_name, m_key);
    }

private:
    ObjKey m_key;
    std::string m_object_schema_name;
};

template<>
class ThreadSafeReference::PayloadImpl<Results> : public ThreadSafeReference::Payload {
public:
    PayloadImpl(Results const& r)
    : Payload(*r.get_realm())
    , m_ordering(r.get_descriptor_ordering())
    {
        if (auto list = r.get_list()) {
            m_key = list->get_key();
            m_table_key = list->get_table()->get_key();
            m_col_key = list->get_col_key();
        }
        else {
            Query q(r.get_query());
            if (!q.produces_results_in_table_order() && r.get_realm()->is_in_transaction()) {
                // FIXME: This is overly restrictive. It's only a problem if
                // the parent of the List or LinkingObjects was created in this
                // write transaction, but Query doesn't expose a way to check
                // if the source view is valid so we have to forbid it always.
                throw std::logic_error("Cannot create a ThreadSafeReference to Results backed by a List of objects or LinkingObjects inside a write transaction");
            }
            m_query = m_transaction->import_copy_of(q, PayloadPolicy::Stay);
        }
    }

    Results import_into(std::shared_ptr<Realm> const& r)
    {
        if (m_key) {
            LstBasePtr list;
            auto table = r->read_group().get_table(m_table_key);
            try {
                list = table->get_object(m_key).get_listbase_ptr(m_col_key);
            }
            catch (InvalidKey const&) {
                // Create a detached list of the appropriate type so that we
                // return an invalid Results rather than an Empty Results, to
                // match what happens for other types of handover where the
                // object doesn't exist.
                switch_on_type(ObjectSchema::from_core_type(*table, m_col_key), [&](auto* t) -> void {
                    list = std::make_unique<Lst<NonObjTypeT<decltype(*t)>>>();
                });
            }
            return Results(r, std::move(list), m_ordering);
        }
        auto q = r->import_copy_of(*m_query, PayloadPolicy::Stay);
        return Results(std::move(r), std::move(*q), m_ordering);
    }

private:
    DescriptorOrdering m_ordering;
    std::unique_ptr<Query> m_query;
    ObjKey m_key;
    TableKey m_table_key;
    ColKey m_col_key;
};

template<>
class ThreadSafeReference::PayloadImpl<std::shared_ptr<Realm>> : public ThreadSafeReference::Payload {
public:
    PayloadImpl(std::shared_ptr<Realm> const& realm)
    : Payload(*realm)
    , m_realm(realm)
    {
    }

    std::shared_ptr<Realm> get_realm()
    {
        return std::move(m_realm);
    }

private:
    std::shared_ptr<Realm> m_realm;
};

ThreadSafeReference::ThreadSafeReference() noexcept = default;
ThreadSafeReference::~ThreadSafeReference() = default;
ThreadSafeReference::ThreadSafeReference(ThreadSafeReference&&) noexcept = default;
ThreadSafeReference& ThreadSafeReference::operator=(ThreadSafeReference&&) noexcept = default;

template<typename T>
ThreadSafeReference::ThreadSafeReference(T const& value)
{
    auto realm = value.get_realm();
    realm->verify_thread();
    m_payload.reset(new PayloadImpl<T>(value));
}

template<>
ThreadSafeReference::ThreadSafeReference(std::shared_ptr<Realm> const& value)
{
    m_payload.reset(new PayloadImpl<std::shared_ptr<Realm>>(value));
}

template ThreadSafeReference::ThreadSafeReference(List const&);
template ThreadSafeReference::ThreadSafeReference(Results const&);
template ThreadSafeReference::ThreadSafeReference(Object const&);

template<typename T>
T ThreadSafeReference::resolve(std::shared_ptr<Realm> const& realm)
{
    REALM_ASSERT(realm);
    realm->verify_thread();

    REALM_ASSERT(m_payload);
    auto& payload = static_cast<PayloadImpl<T>&>(*m_payload);
    REALM_ASSERT(typeid(payload) == typeid(PayloadImpl<T>));

    m_payload->refresh_target_realm(*realm);
    try {
        return payload.import_into(realm);
    }
    catch (InvalidKey const&) {
        // Object was deleted in a version after when the TSR was created
        return {};
    }
}

template<>
std::shared_ptr<Realm> ThreadSafeReference::resolve<std::shared_ptr<Realm>>(std::shared_ptr<Realm> const&)
{
    REALM_ASSERT(m_payload);
    auto& payload = static_cast<PayloadImpl<std::shared_ptr<Realm>>&>(*m_payload);
    REALM_ASSERT(typeid(payload) == typeid(PayloadImpl<std::shared_ptr<Realm>>));

    return payload.get_realm();
}

template Results ThreadSafeReference::resolve<Results>(std::shared_ptr<Realm> const&);
template List ThreadSafeReference::resolve<List>(std::shared_ptr<Realm> const&);
template Object ThreadSafeReference::resolve<Object>(std::shared_ptr<Realm> const&);

} // namespace realm
