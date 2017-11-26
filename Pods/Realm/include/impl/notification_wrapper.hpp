////////////////////////////////////////////////////////////////////////////
//
// Copyright 2017 Realm Inc.
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

#ifndef REALM_OS_NOTIFICATION_WRAPPER_HPP
#define REALM_OS_NOTIFICATION_WRAPPER_HPP

#include "collection_notifications.hpp"

namespace realm {
namespace _impl {

// A wrapper that stores a value and an associated notification token.
// The point of this type is to keep the notification token alive
// until the value can be properly processed or handled.
template<typename T>
struct NotificationWrapper : public T {
    using T::T;

    NotificationWrapper(T&& object)
    : T(object)
    { }

    template <typename F>
    void add_notification_callback(F&& callback)
    {
        m_token = T::add_notification_callback(std::forward<F>(callback));
    }

private:
    NotificationToken m_token;
};

} // namespace _impl
} // namespace realm

#endif // REALM_OS_NOTIFICATION_WRAPPER_HPP
