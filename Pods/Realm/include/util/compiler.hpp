////////////////////////////////////////////////////////////////////////////
//
// Copyright 2015 Realm Inc.
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

#ifndef REALM_UTIL_COMPILER_HPP
#define REALM_UTIL_COMPILER_HPP

#ifdef __has_cpp_attribute
#define REALM_HAS_CPP_ATTRIBUTE(attr) __has_cpp_attribute(attr)
#else
#define REALM_HAS_CPP_ATTRIBUTE(attr) 0
#endif

#if REALM_HAS_CPP_ATTRIBUTE(fallthrough)
#define REALM_FALLTHROUGH [[fallthrough]]
#elif REALM_HAS_CPP_ATTRIBUTE(clang::fallthrough)
#define REALM_FALLTHROUGH [[clang::fallthrough]]
#else
#define REALM_FALLTHROUGH
#endif

// This should be renamed to REALM_UNREACHABLE as soon as REALM_UNREACHABLE is renamed to
// REALM_ASSERT_NOT_REACHED which will better reflect its nature
#if __GNUC__ || __clang__
#define REALM_COMPILER_HINT_UNREACHABLE __builtin_unreachable
#else
#define REALM_COMPILER_HINT_UNREACHABLE abort
#endif

#endif // REALM_UTIL_COMPILER_HPP
