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

#include "impl/realm_coordinator.hpp"
#include "list.hpp"
#include "object.hpp"
#include "object_schema.hpp"
#include "results.hpp"

#include <realm/util/scope_exit.hpp>

using namespace realm;

ThreadSafeReferenceBase::ThreadSafeReferenceBase(SharedRealm source_realm) : m_source_realm(std::move(source_realm))
{
    m_source_realm->verify_thread();
    if (m_source_realm->is_in_transaction()) {
        throw InvalidTransactionException("Cannot obtain thread safe reference during a write transaction.");
    }

    try {
        m_version_id = get_source_shared_group().pin_version();
    } catch (...) {
        invalidate();
        throw;
    }
}

ThreadSafeReferenceBase::~ThreadSafeReferenceBase()
{
    if (!is_invalidated())
        invalidate();
}

template <typename V, typename T>
V ThreadSafeReferenceBase::invalidate_after_import(Realm& destination_realm, T construct_with_shared_group) {
    destination_realm.verify_thread();
    REALM_ASSERT_DEBUG(!m_source_realm->is_in_transaction());
    REALM_ASSERT_DEBUG(!is_invalidated());

    SharedGroup& destination_shared_group = *Realm::Internal::get_shared_group(destination_realm);
    auto unpin_version = util::make_scope_exit([&]() noexcept { invalidate(); });

    return construct_with_shared_group(destination_shared_group);
}

SharedGroup& ThreadSafeReferenceBase::get_source_shared_group() const {
    return *Realm::Internal::get_shared_group(*m_source_realm);
}

bool ThreadSafeReferenceBase::has_same_config(Realm& realm) const {
    return &Realm::Internal::get_coordinator(*m_source_realm) == &Realm::Internal::get_coordinator(realm);
}

void ThreadSafeReferenceBase::invalidate() {
    REALM_ASSERT_DEBUG(m_source_realm);
    SharedRealm thread_local_realm = Realm::Internal::get_coordinator(*m_source_realm).get_realm();
    Realm::Internal::get_shared_group(*thread_local_realm)->unpin_version(m_version_id);
    m_source_realm = nullptr;
}

ThreadSafeReference<List>::ThreadSafeReference(List const& list)
: ThreadSafeReferenceBase(list.get_realm())
, m_link_view(get_source_shared_group().export_linkview_for_handover(list.m_link_view))
, m_table(get_source_shared_group().export_table_for_handover(list.m_table))
{ }

List ThreadSafeReference<List>::import_into_realm(SharedRealm realm) && {
    return invalidate_after_import<List>(*realm, [&](SharedGroup& shared_group) {
        if (auto link_view = shared_group.import_linkview_from_handover(std::move(m_link_view)))
            return List(std::move(realm), std::move(link_view));
        return List(std::move(realm), shared_group.import_table_from_handover(std::move(m_table)));
    });
}

ThreadSafeReference<Object>::ThreadSafeReference(Object const& object)
: ThreadSafeReferenceBase(object.realm())
, m_row(get_source_shared_group().export_for_handover(Row(object.row())))
, m_object_schema_name(object.get_object_schema().name) { }

Object ThreadSafeReference<Object>::import_into_realm(SharedRealm realm) && {
    return invalidate_after_import<Object>(*realm, [&](SharedGroup& shared_group) {
        Row row = *shared_group.import_from_handover(std::move(m_row));
        auto object_schema = realm->schema().find(m_object_schema_name);
        REALM_ASSERT_DEBUG(object_schema != realm->schema().end());
        return Object(std::move(realm), *object_schema, row);
    });
}

ThreadSafeReference<Results>::ThreadSafeReference(Results const& results)
: ThreadSafeReferenceBase(results.get_realm())
, m_query(get_source_shared_group().export_for_handover(results.get_query(), ConstSourcePayload::Copy))
, m_ordering_patch([&]() {
    DescriptorOrdering::HandoverPatch ordering_patch;
    DescriptorOrdering::generate_patch(results.get_descriptor_ordering(), ordering_patch);
    return ordering_patch;
}()){ }

Results ThreadSafeReference<Results>::import_into_realm(SharedRealm realm) && {
    return invalidate_after_import<Results>(*realm, [&](SharedGroup& shared_group) {
        Query query = *shared_group.import_from_handover(std::move(m_query));
        Table& table = *query.get_table();
        DescriptorOrdering descriptors = DescriptorOrdering::create_from_and_consume_patch(m_ordering_patch, table);
        return Results(std::move(realm), std::move(query), std::move(descriptors));
    });
}
