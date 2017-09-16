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

#ifndef REALM_OS_DESCRIPTOR_ORDERING
#define REALM_OS_DESCRIPTOR_ORDERING

#include "feature_checks.hpp"

#include <realm/views.hpp>

#if !REALM_HAVE_COMPOSABLE_DISTINCT

// realm-core v3.0.0 improves the semantics of sort and distinct by making them composable.
// It also changed the API through which they're accessed. This file emulates the new API for
// older versions of core, while maintaining the older version's semantics. This is intended
// to allow supporting both versions of core with a minimal amount of conditional logic.

namespace realm {

using DistinctDescriptor = SortDescriptor;

struct DescriptorOrdering {

    struct HandoverPatch {
        SortDescriptor::HandoverPatch sort;
        DistinctDescriptor::HandoverPatch distinct;
    };

    // These replace the existing criteria, rather than append to, which matches the
    // semantics provided by old versions of Core.
    void append_sort(SortDescriptor sort) { this->sort = std::move(sort); }
    void append_distinct(DistinctDescriptor distinct) { this->distinct = std::move(distinct); }

    bool is_empty() const { return !sort && !distinct; }

    bool will_apply_sort() const { return bool(sort); }
    bool will_apply_distinct() const { return bool(distinct); }

    static void generate_patch(DescriptorOrdering const& ordering, HandoverPatch& patch)
    {
        SortDescriptor::generate_patch(ordering.sort, patch.sort);
        DistinctDescriptor::generate_patch(ordering.distinct, patch.distinct);
    }

    DescriptorOrdering static create_from_and_consume_patch(HandoverPatch& patch, Table const& table)
    {
        return {SortDescriptor::create_from_and_consume_patch(patch.sort, table),
                DistinctDescriptor::create_from_and_consume_patch(patch.distinct, table)};
    }

    SortDescriptor sort;
    DistinctDescriptor distinct;
};

} // namespace realm

#endif // !REALM_HAVE_COMPOSABLE_DISTINCT

#endif // REALM_OS_DESCRIPTOR_ORDERING
