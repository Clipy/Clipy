////////////////////////////////////////////////////////////////////////////
//
// Copyright 2020 Realm Inc.
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

#ifndef REALM_OS_COPYABLE_ATOMIC_HPP
#define REALM_OS_COPYABLE_ATOMIC_HPP

namespace realm {
namespace util {

// std::atomic is not copyable because the resulting semantics are not useful
// for many of the things atomics can be used for (in particular, anything
// involving a memory order than `relaxed` is probably broken). In addition,
// the copying itself cannot be thread-safe. These limitations make this type
// suitable for storing Results/List's object schema pointer, but not most things.
template<typename T>
struct CopyableAtomic : std::atomic<T> {
    using std::atomic<T>::atomic;

    CopyableAtomic(CopyableAtomic const& a) noexcept : std::atomic<T>(a.load()) { }
    CopyableAtomic(CopyableAtomic&& a) noexcept : std::atomic<T>(a.load()) { }
    CopyableAtomic& operator=(CopyableAtomic const& a) noexcept
    {
        this->store(a.load());
        return *this;
    }
    CopyableAtomic& operator=(CopyableAtomic&& a) noexcept
    {
        this->store(a.load());
        return *this;
    }
};

}
}
#endif // REALM_OS_COPYABLE_ATOMIC_HPP
