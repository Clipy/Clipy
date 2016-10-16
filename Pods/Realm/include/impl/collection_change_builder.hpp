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

#ifndef REALM_COLLECTION_CHANGE_BUILDER_HPP
#define REALM_COLLECTION_CHANGE_BUILDER_HPP

#include "collection_notifications.hpp"

#include <realm/util/optional.hpp>

#include <unordered_map>

namespace realm {
namespace _impl {
class CollectionChangeBuilder : public CollectionChangeSet {
public:
    CollectionChangeBuilder(CollectionChangeBuilder const&) = default;
    CollectionChangeBuilder(CollectionChangeBuilder&&) = default;
    CollectionChangeBuilder& operator=(CollectionChangeBuilder const&) = default;
    CollectionChangeBuilder& operator=(CollectionChangeBuilder&&) = default;

    CollectionChangeBuilder(IndexSet deletions = {},
                            IndexSet insertions = {},
                            IndexSet modification = {},
                            std::vector<Move> moves = {});

    // Calculate where rows need to be inserted or deleted from old_rows to turn
    // it into new_rows, and check all matching rows for modifications
    // If `move_candidates` is supplied they it will be used to do more accurate
    // determination of which rows moved. This is only supported when the rows
    // are in table order (i.e. not sorted or from a LinkList)
    static CollectionChangeBuilder calculate(std::vector<size_t> const& old_rows,
                                             std::vector<size_t> const& new_rows,
                                             std::function<bool (size_t)> row_did_change,
                                             util::Optional<IndexSet> const& move_candidates = util::none);

    // generic operations {
    CollectionChangeSet finalize() &&;
    void merge(CollectionChangeBuilder&&);

    void insert(size_t ndx, size_t count=1, bool track_moves=true);
    void modify(size_t ndx);
    void erase(size_t ndx);
    void clear(size_t old_size);
    // }

    // operations only implemented for LinkList semantics {
    void clean_up_stale_moves();
    void move(size_t from, size_t to);
    // }

    // operations only implemented for Row semantics {
    void move_over(size_t ndx, size_t last_ndx, bool track_moves=true);
    // must be followed by move_over(old_ndx, ...)
    // precondition: `new_ndx` must be a new insertion
    void subsume(size_t old_ndx, size_t new_ndx, bool track_moves=true);
    void swap(size_t ndx_1, size_t ndx_2, bool track_moves=true);

    void parse_complete();
    // }

private:
    std::unordered_map<size_t, size_t> m_move_mapping;

    void verify();
};
} // namespace _impl
} // namespace realm

#endif // REALM_COLLECTION_CHANGE_BUILDER_HPP
