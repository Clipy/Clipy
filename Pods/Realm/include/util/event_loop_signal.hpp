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

#ifndef REALM_EVENT_LOOP_SIGNAL_HPP
#define REALM_EVENT_LOOP_SIGNAL_HPP

#include <realm/util/features.h>

#if (defined(REALM_HAVE_UV) && REALM_HAVE_UV && !REALM_PLATFORM_APPLE) || (defined(REALM_PLATFORM_NODE) && REALM_PLATFORM_NODE)
#define REALM_USE_UV 1
#else
#define REALM_USE_UV 0
#endif

#if !defined(REALM_USE_CF) && REALM_PLATFORM_APPLE
#define REALM_USE_CF 1
#elif !defined(REALM_USE_ALOOPER) && REALM_ANDROID
#define REALM_USE_ALOOPER 1
#endif

#if REALM_USE_UV
#include "util/uv/event_loop_signal.hpp"
#elif REALM_USE_CF
#include "util/apple/event_loop_signal.hpp"
#elif REALM_USE_ALOOPER
#include "util/android/event_loop_signal.hpp"
#else
#include "util/generic/event_loop_signal.hpp"
#endif

#endif // REALM_EVENT_LOOP_SIGNAL_HPP
