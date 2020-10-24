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

#ifndef REALM_OS_THREAD_SAFE_REFERENCE_HPP
#define REALM_OS_THREAD_SAFE_REFERENCE_HPP

#include <memory>

namespace realm {
class List;
class Object;
class Realm;
class Results;

// Opaque type-ereased wrapper for a Realm object which can be imported into another Realm
class ThreadSafeReference {
public:
    ThreadSafeReference() noexcept;
    ~ThreadSafeReference();
    ThreadSafeReference(const ThreadSafeReference&) = delete;
    ThreadSafeReference& operator=(const ThreadSafeReference&) = delete;
    ThreadSafeReference(ThreadSafeReference&&) noexcept;
    ThreadSafeReference& operator=(ThreadSafeReference&&) noexcept;

    template<typename T>
    ThreadSafeReference(T const& value);

    // Import the object into the destination Realm
    template<typename T>
    T resolve(std::shared_ptr<Realm> const&);

    explicit operator bool() const noexcept { return !!m_payload; }

private:
    class Payload;
    template<typename> class PayloadImpl;
    std::unique_ptr<Payload> m_payload;
};

template<> ThreadSafeReference::ThreadSafeReference(std::shared_ptr<Realm> const&);
template<> std::shared_ptr<Realm> ThreadSafeReference::resolve(std::shared_ptr<Realm> const&);
}

#endif /* REALM_OS_THREAD_SAFE_REFERENCE_HPP */
