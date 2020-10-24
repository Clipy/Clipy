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
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#include "util/scheduler.hpp"
#include <realm/version_id.hpp>

#if REALM_USE_UV
#include "util/uv/scheduler.hpp"
#elif REALM_USE_CF
#include "util/apple/scheduler.hpp"
#elif REALM_USE_ALOOPER
#include "util/android/scheduler.hpp"
#else
#include "util/generic/scheduler.hpp"
#endif

namespace {
using namespace realm;

class FrozenScheduler : public util::Scheduler {
public:
    FrozenScheduler(VersionID version)
    : m_version(version)
    { }

    void notify() override {}
    void set_notify_callback(std::function<void()>) override {}
    bool is_on_thread() const noexcept override { return true; }
    bool is_same_as(const Scheduler* other) const noexcept override
    {
        auto o = dynamic_cast<const FrozenScheduler*>(other);
        return (o && (o->m_version == m_version));
    }
    bool can_deliver_notifications() const noexcept override { return false; }

private:
    VersionID m_version;
};
} // anonymous namespace

namespace realm {
namespace util {

Scheduler::~Scheduler() = default;

std::shared_ptr<Scheduler> Scheduler::get_frozen(VersionID version)
{
    return std::make_shared<FrozenScheduler>(version);
}
} // namespace util
} // namespace realm
