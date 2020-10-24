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

#include <realm/keys.hpp>

#include <functional>
#include <unordered_set>
#include <vector>

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
    static CollectionChangeBuilder calculate(std::vector<int64_t> const& old_rows,
                                             std::vector<int64_t> const& new_rows,
                                             std::function<bool (int64_t)> key_did_change,
                                             bool in_table_order);
    static CollectionChangeBuilder calculate(std::vector<size_t> const& old_rows,
                                             std::vector<size_t> const& new_rows,
                                             std::function<bool (int64_t)> key_did_change);

    // generic operations {
    CollectionChangeSet finalize() &&;
    void merge(CollectionChangeBuilder&&);

    void insert(size_t ndx, size_t count=1, bool track_moves=true);
    void modify(size_t ndx, size_t col=-1);
    void erase(size_t ndx);
    void clear(size_t old_size);
    // }

    // operations only implemented for LinkList semantics {
    void clean_up_stale_moves();
    void move(size_t from, size_t to);
    // }

private:
    bool m_track_columns = true;

    template<typename Func>
    void for_each_col(Func&& f);

    void verify();
};
} // namespace _impl
} // namespace realm

#endif // REALM_COLLECTION_CHANGE_BUILDER_HPP
