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

#ifndef REALM_THREAD_SAFE_REFERENCE_HPP
#define REALM_THREAD_SAFE_REFERENCE_HPP


#include <realm/group_shared.hpp>

namespace realm {
class LinkView;
class List;
class Object;
class Query;
class Realm;
class Results;
class TableView;
template<typename T> class BasicRow;
typedef BasicRow<Table> Row;

// Opaque type representing an object for handover
class ThreadSafeReferenceBase {
public:
    ThreadSafeReferenceBase(const ThreadSafeReferenceBase&) = delete;
    ThreadSafeReferenceBase& operator=(const ThreadSafeReferenceBase&) = delete;
    ThreadSafeReferenceBase(ThreadSafeReferenceBase&&) = default;
    ThreadSafeReferenceBase& operator=(ThreadSafeReferenceBase&&) = default;
    ThreadSafeReferenceBase();
    virtual ~ThreadSafeReferenceBase();

    bool is_invalidated() const { return m_source_realm == nullptr; };

protected:
    // Precondition: The associated Realm is for the current thread and is not in a write transaction;.
    ThreadSafeReferenceBase(std::shared_ptr<Realm> source_realm);

    SharedGroup& get_source_shared_group() const;

    template <typename V, typename T>
    V invalidate_after_import(Realm& destination_realm, T construct_with_shared_group);

private:
    friend Realm;

    VersionID m_version_id;
    std::shared_ptr<Realm> m_source_realm; // Strong reference keeps alive so version stays pinned! Don't touch!!

    bool has_same_config(Realm& realm) const;
    void invalidate();
};

template <typename T>
class ThreadSafeReference;

template<>
class ThreadSafeReference<List>: public ThreadSafeReferenceBase {
    friend class Realm;

    std::unique_ptr<SharedGroup::Handover<LinkView>> m_link_view;
    std::unique_ptr<SharedGroup::Handover<Table>> m_table;

    // Precondition: The associated Realm is for the current thread and is not in a write transaction;.
    ThreadSafeReference(List const& value);

    // Precondition: Realm and handover are on same version.
    List import_into_realm(std::shared_ptr<Realm> realm) &&;
};

template<>
class ThreadSafeReference<Object>: public ThreadSafeReferenceBase {
    friend class Realm;

    std::unique_ptr<SharedGroup::Handover<Row>> m_row;
    std::string m_object_schema_name;

    // Precondition: The associated Realm is for the current thread and is not in a write transaction;.
    ThreadSafeReference(Object const& value);

    // Precondition: Realm and handover are on same version.
    Object import_into_realm(std::shared_ptr<Realm> realm) &&;
};

template<>
class ThreadSafeReference<Results>: public ThreadSafeReferenceBase {
    friend class Realm;

    std::unique_ptr<SharedGroup::Handover<Query>> m_query;
    DescriptorOrdering::HandoverPatch m_ordering_patch;

    // Precondition: The associated Realm is for the current thread and is not in a write transaction;.
    ThreadSafeReference(Results const& value);

    // Precondition: Realm and handover are on same version.
    Results import_into_realm(std::shared_ptr<Realm> realm) &&;
};
}

#endif /* REALM_THREAD_SAFE_REFERENCE_HPP */
