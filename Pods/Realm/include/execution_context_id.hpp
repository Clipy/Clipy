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
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or utilied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#ifndef REALM_OS_EXECUTION_CONTEXT_ID_HPP
#define REALM_OS_EXECUTION_CONTEXT_ID_HPP

#include "util/aligned_union.hpp"

#include <cstring>
#include <thread>

#include <realm/util/assert.hpp>
#include <realm/util/optional.hpp>

namespace realm {

// An identifier for an execution context other than a thread.
// Different execution contexts must have different IDs.
using AbstractExecutionContextID = uintptr_t;

// Identifies an execution context in which a Realm will be used.
// Can contain either a std::thread::id or an AbstractExecutionContextID.
//
// FIXME: This can eventually be:
//        using AnyExecutionContextID = std::variant<std::thread::id, AbstractExecutionContextID>;
class AnyExecutionContextID {
    enum class Type {
        Thread,
        Abstract,
    };

public:

    // Convert from the representation used by Realm::Config, where the absence of an
    // explicit abstract execution context indicates that the current thread's identifier
    // should be used.
    AnyExecutionContextID(util::Optional<AbstractExecutionContextID> maybe_abstract_id)
    {
        if (maybe_abstract_id)
            *this = AnyExecutionContextID(*maybe_abstract_id);
        else
            *this = AnyExecutionContextID(std::this_thread::get_id());
    }

    AnyExecutionContextID(std::thread::id thread_id) : AnyExecutionContextID(Type::Thread, std::move(thread_id)) { }
    AnyExecutionContextID(AbstractExecutionContextID abstract_id) : AnyExecutionContextID(Type::Abstract, abstract_id) { }

    template <typename StorageType>
    bool contains() const
    {
        return TypeForStorageType<StorageType>::value == m_type;
    }

    template <typename StorageType>
    StorageType get() const
    {
        REALM_ASSERT_DEBUG(contains<StorageType>());
        return *reinterpret_cast<const StorageType*>(&m_storage);
    }

    bool operator==(const AnyExecutionContextID& other) const
    {
        return m_type == other.m_type && std::memcmp(&m_storage, &other.m_storage, sizeof(m_storage)) == 0;
    }

    bool operator!=(const AnyExecutionContextID& other) const
    {
        return !(*this == other);
    }

private:
    template <typename T>
    AnyExecutionContextID(Type type, T value) : m_type(type)
    {
        // operator== relies on being able to compare the raw bytes of m_storage,
        // so zero everything before intializing the portion in use.
        std::memset(&m_storage, 0, sizeof(m_storage));
        new (&m_storage) T(std::move(value));
    }

    template <typename> struct TypeForStorageType;

    util::AlignedUnion<1, std::thread::id, AbstractExecutionContextID>::type m_storage;
    Type m_type;
};

template <>
struct AnyExecutionContextID::TypeForStorageType<std::thread::id> {
    constexpr static Type value = Type::Thread;
};

template <>
struct AnyExecutionContextID::TypeForStorageType<AbstractExecutionContextID> {
    constexpr static Type value = Type::Abstract;
};

} // namespace realm

#endif // REALM_OS_EXECUTION_CONTEXT_ID_HPP
