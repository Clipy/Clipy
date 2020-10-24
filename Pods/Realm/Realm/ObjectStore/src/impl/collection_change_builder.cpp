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

#include "impl/collection_change_builder.hpp"

#include <realm/util/assert.hpp>

#include <algorithm>

using namespace realm;
using namespace realm::_impl;

CollectionChangeBuilder::CollectionChangeBuilder(IndexSet deletions,
                                                 IndexSet insertions,
                                                 IndexSet modifications,
                                                 std::vector<Move> moves)
: CollectionChangeSet({std::move(deletions), std::move(insertions), std::move(modifications), {}, std::move(moves)})
{
    for (auto&& move : this->moves) {
        this->deletions.add(move.from);
        this->insertions.add(move.to);
    }
}

void CollectionChangeBuilder::merge(CollectionChangeBuilder&& c)
{
    if (c.empty())
        return;
    if (empty()) {
        *this = std::move(c);
        return;
    }

    verify();
    c.verify();

    // FIXME: this is comically wasteful
    std::unordered_set<int64_t> col_keys;
    if (m_track_columns) {
        for (auto& col : columns)
            col_keys.insert(col.first);
        for (auto& col : c.columns)
            col_keys.insert(col.first);
    }

    auto for_each_col = [&](auto&& f) {
        f(modifications, c.modifications);
        if (m_track_columns) {
            for (auto col : col_keys)
                f(columns[col], c.columns[col]);
        }
    };

    // First update any old moves
    if (!c.moves.empty() || !c.deletions.empty() || !c.insertions.empty()) {
        auto it = std::remove_if(begin(moves), end(moves), [&](auto& old) {
            // Check if the moved row was moved again, and if so just update the destination
            auto it = find_if(begin(c.moves), end(c.moves), [&](auto const& m) {
                return old.to == m.from;
            });
            if (it != c.moves.end()) {
                for_each_col([&](auto& col, auto& other) {
                    if (col.contains(it->from))
                        other.add(it->to);
                });
                old.to = it->to;
                *it = c.moves.back();
                c.moves.pop_back();
                return false;
            }

            // Check if the destination was deleted
            // Removing the insert for this move will happen later
            if (c.deletions.contains(old.to))
                return true;

            // Update the destination to adjust for any new insertions and deletions
            old.to = c.insertions.shift(c.deletions.unshift(old.to));
            return false;
        });
        moves.erase(it, end(moves));
    }

    // Ignore new moves of rows which were previously inserted (the implicit
    // delete from the move will remove the insert)
    if (!insertions.empty() && !c.moves.empty()) {
        c.moves.erase(std::remove_if(begin(c.moves), end(c.moves),
                              [&](auto const& m) { return insertions.contains(m.from); }),
                    end(c.moves));
    }

    // Ensure that any previously modified rows which were moved are still modified
    if (!modifications.empty() && !c.moves.empty()) {
        for (auto const& move : c.moves) {
            for_each_col([&](auto& col, auto& other) {
                if (col.contains(move.from))
                    other.add(move.to);
            });
        }
    }

    // Update the source position of new moves to compensate for the changes made
    // in the old changeset
    if (!deletions.empty() || !insertions.empty()) {
        for (auto& move : c.moves)
            move.from = deletions.shift(insertions.unshift(move.from));
    }

    moves.insert(end(moves), begin(c.moves), end(c.moves));

    // New deletion indices have been shifted by the insertions, so unshift them
    // before adding
    deletions.add_shifted_by(insertions, c.deletions);

    // Drop any inserted-then-deleted rows, then merge in new insertions
    insertions.erase_at(c.deletions);
    insertions.insert_at(c.insertions);

    clean_up_stale_moves();

    for_each_col([&](auto& col, auto& other) {
        col.erase_at(c.deletions);
        col.shift_for_insert_at(c.insertions);
        col.add(other);
    });

    c = {};
    verify();
}

void CollectionChangeBuilder::clean_up_stale_moves()
{
    // Look for moves which are now no-ops, and remove them plus the associated
    // insert+delete. Note that this isn't just checking for from == to due to
    // that rows can also be shifted by other inserts and deletes
    moves.erase(std::remove_if(begin(moves), end(moves), [&](auto const& move) {
        if (move.from - deletions.count(0, move.from) != move.to - insertions.count(0, move.to))
            return false;
        deletions.remove(move.from);
        insertions.remove(move.to);
        return true;
    }), end(moves));
}

void CollectionChangeBuilder::modify(size_t ndx, size_t col)
{
    modifications.add(ndx);
    if (!m_track_columns || col == IndexSet::npos)
        return;
    columns[col].add(ndx);
}

template<typename Func>
void CollectionChangeBuilder::for_each_col(Func&& f)
{
    f(modifications);
    if (m_track_columns) {
        for (auto& col : columns)
            f(col.second);
    }
}

void CollectionChangeBuilder::insert(size_t index, size_t count, bool track_moves)
{
    REALM_ASSERT(count != 0);

    for_each_col([=](auto& col) { col.shift_for_insert_at(index, count); });
    if (!track_moves)
        return;

    insertions.insert_at(index, count);

    for (auto& move : moves) {
        if (move.to >= index)
            move.to += count;
    }
}

void CollectionChangeBuilder::erase(size_t index)
{
    for_each_col([=](auto& col) { col.erase_at(index); });
    size_t unshifted = insertions.erase_or_unshift(index);
    if (unshifted != IndexSet::npos)
        deletions.add_shifted(unshifted);

    for (size_t i = 0; i < moves.size(); ++i) {
        auto& move = moves[i];
        if (move.to == index) {
            moves.erase(moves.begin() + i);
            --i;
        }
        else if (move.to > index)
            --move.to;
    }
}

void CollectionChangeBuilder::clear(size_t old_size)
{
    for (auto range : deletions)
        old_size += range.second - range.first;
    for (auto range : insertions)
        old_size -= range.second - range.first;

    modifications.clear();
    insertions.clear();
    moves.clear();
    columns.clear();
    deletions.set(old_size);
}

void CollectionChangeBuilder::move(size_t from, size_t to)
{
    REALM_ASSERT(from != to);

    bool updated_existing_move = false;
    for (auto& move : moves) {
        if (move.to != from) {
            // Shift other moves if this row is moving from one side of them
            // to the other
            if (move.to >= to && move.to < from)
                ++move.to;
            else if (move.to <= to && move.to > from)
                --move.to;
            continue;
        }
        REALM_ASSERT(!updated_existing_move);

        // Collapse A -> B, B -> C into a single A -> C move
        move.to = to;
        updated_existing_move = true;

        insertions.erase_at(from);
        insertions.insert_at(to);
    }

    if (!updated_existing_move) {
        auto shifted_from = insertions.erase_or_unshift(from);
        insertions.insert_at(to);

        // Don't report deletions/moves for newly inserted rows
        if (shifted_from != IndexSet::npos) {
            shifted_from = deletions.add_shifted(shifted_from);
            moves.push_back({shifted_from, to});
        }
    }

    for_each_col([=](auto& col) {
        bool modified = col.contains(from);
        col.erase_at(from);

        if (modified)
            col.insert_at(to);
        else
            col.shift_for_insert_at(to);
    });
}

void CollectionChangeBuilder::verify()
{
#ifdef REALM_DEBUG
    for (auto&& move : moves) {
        REALM_ASSERT(deletions.contains(move.from));
        REALM_ASSERT(insertions.contains(move.to));
    }
#endif
}

namespace {
struct RowInfo {
    int64_t key;
    size_t prev_tv_index;
    size_t tv_index;
};

#if 0 // FIXME: this is applicable to backlinks still
// Calculates the insertions/deletions required for a query on a table without
// a sort, where `removed` includes the rows which were modified to no longer
// match the query (but not outright deleted rows, which are filtered out long
// before any of this logic), and `move_candidates` tracks the rows which may
// be the result of a move.
//
// This function is not strictly required, as calculate_moves_sorted() will
// produce correct results even for the scenarios where this function is used.
// However, this function has asymptotically better worst-case performance and
// extremely cheap best-case performance, and is guaranteed to produce a minimal
// diff.
void calculate_moves_backlinks(std::vector<RowInfo>& new_rows, IndexSet& removed,
                               IndexSet const& move_candidates,
                               CollectionChangeSet& changeset)
{
    // Here we track which row we expect to see, which in the absence of swap()
    // is always the row immediately after the last row which was not moved.
    size_t expected = 0;
    for (auto& row : new_rows) {
        if (row.shifted_tv_index == expected) {
            ++expected;
            continue;
        }

        // We didn't find the row we were expecting to find, which means that
        // either a row was moved forward to here, the row we were expecting was
        // removed, or the row we were expecting moved back.

        // First check if this row even could have moved. If it can't, just
        // treat it as a match and move on, and we'll handle the row we were
        // expecting when we hit it later.
        if (!move_candidates.contains(row.key)) {
            expected = row.shifted_tv_index + 1;
            continue;
        }

        // Next calculate where we expect this row to be based on the insertions
        // and removals (i.e. rows changed to not match the query), as it could
        // be that the row actually ends up in this spot due to the rows before
        // it being removed.
        size_t calc_expected = row.tv_index - changeset.insertions.count(0, row.tv_index) + removed.count(0, row.prev_tv_index);
        if (row.shifted_tv_index == calc_expected) {
            expected = calc_expected + 1;
            continue;
        }

        // The row still isn't the expected one, so record it as a move
        changeset.moves.push_back({row.prev_tv_index, row.tv_index});
        changeset.insertions.add(row.tv_index);
        removed.add(row.prev_tv_index);
    }
}
#endif

class LongestCommonSubsequenceCalculator {
public:
    // A pair of an object key and an index in the table view
    struct Row {
        int64_t key;
        size_t tv_index;
    };

    struct Match {
        // The index in `a` at which this match begins
        size_t i;
        // The index in `b` at which this match begins
        size_t j;
        // The length of this match
        size_t size;
        // The number of rows in this block which were modified
        size_t modified;
    };
    std::vector<Match> m_longest_matches;

    LongestCommonSubsequenceCalculator(std::vector<Row>& a, std::vector<Row>& b,
                                       size_t start_index,
                                       IndexSet const& modifications)
    : m_modified(modifications)
    , a(a), b(b)
    {
        find_longest_matches(start_index, a.size(),
                             start_index, b.size());
        m_longest_matches.push_back({a.size(), b.size(), 0});
    }

private:
    IndexSet const& m_modified;

    // The two arrays of rows being diffed
    // a is sorted by tv_index, b is sorted by key
    std::vector<Row> &a, &b;

    // Find the longest matching range in (a + begin1, a + end1) and (b + begin2, b + end2)
    // "Matching" is defined as "has the same row index"; the TV index is just
    // there to let us turn an index in a/b into an index which can be reported
    // in the output changeset.
    //
    // This is done with the O(N) space variant of the dynamic programming
    // algorithm for longest common subsequence, where N is the maximum number
    // of the most common row index (which for everything but linkview-derived
    // TVs will be 1).
    Match find_longest_match(size_t begin1, size_t end1, size_t begin2, size_t end2)
    {
        struct Length {
            size_t j, len;
        };
        // The length of the matching block for each `j` for the previously checked row
        std::vector<Length> prev;
        // The length of the matching block for each `j` for the row currently being checked
        std::vector<Length> cur;

        // Calculate the length of the matching block *ending* at b[j], which
        // is 1 if b[j - 1] did not match, and b[j - 1] + 1 otherwise.
        auto length = [&](size_t j) -> size_t {
            for (auto const& pair : prev) {
                if (pair.j + 1 == j)
                    return pair.len + 1;
            }
            return 1;
        };

        // Iterate over each `j` which has the same row index as a[i] and falls
        // within the range begin2 <= j < end2
        auto for_each_b_match = [&](size_t i, auto&& f) {
            auto ai = a[i].key;
            // Find the TV indicies at which this row appears in the new results
            // There should always be at least one (or it would have been
            // filtered out earlier), but there can be multiple if there are dupes
            auto it = lower_bound(begin(b), end(b), ai,
                                  [](auto lft, auto rgt) { return lft.key < rgt; });
            REALM_ASSERT(it != end(b) && it->key == ai);
            for (; it != end(b) && it->key == ai; ++it) {
                size_t j = it->tv_index;
                if (j < begin2)
                    continue;
                if (j >= end2)
                    break; // b is sorted by tv_index so this can't transition from false to true
                f(j);
            }
        };

        Match best = {begin1, begin2, 0, 0};
        for (size_t i = begin1; i < end1; ++i) {
            // prev = std::move(cur), but avoids discarding prev's heap allocation
            cur.swap(prev);
            cur.clear();

            for_each_b_match(i, [&](size_t j) {
                size_t size = length(j);

                cur.push_back({j, size});

                // If the matching block ending at a[i] and b[j] is longer than
                // the previous one, select it as the best
                if (size > best.size)
                    best = {i - size + 1, j - size + 1, size, IndexSet::npos};
                // Given two equal-length matches, prefer the one with fewer modified rows
                else if (size == best.size) {
                    if (best.modified == IndexSet::npos)
                        best.modified = m_modified.count(best.j - size + 1, best.j + 1);
                    auto count = m_modified.count(j - size + 1, j + 1);
                    if (count < best.modified)
                        best = {i - size + 1, j - size + 1, size, count};
                }

                // The best block should always fall within the range being searched
                REALM_ASSERT(best.i >= begin1 && best.i + best.size <= end1);
                REALM_ASSERT(best.j >= begin2 && best.j + best.size <= end2);
            });
        }
        return best;
    }

    void find_longest_matches(size_t begin1, size_t end1, size_t begin2, size_t end2)
    {
        // FIXME: recursion could get too deep here
        // recursion depth worst case is currently O(N) and each recursion uses 320 bytes of stack
        // could reduce worst case to O(sqrt(N)) (and typical case to O(log N))
        // biasing equal selections towards the middle, but that's still
        // insufficient for Android's 8 KB stacks
        auto m = find_longest_match(begin1, end1, begin2, end2);
        if (!m.size)
            return;
        if (m.i > begin1 && m.j > begin2)
            find_longest_matches(begin1, m.i, begin2, m.j);
        m_longest_matches.push_back(m);
        if (m.i + m.size < end2 && m.j + m.size < end2)
            find_longest_matches(m.i + m.size, end1, m.j + m.size, end2);
    }
};

void calculate_moves_sorted(std::vector<RowInfo>& rows, CollectionChangeSet& changeset)
{
    // The RowInfo array contains information about the old and new TV indices of
    // each row, which we need to turn into two sequences of rows, which we'll
    // then find matches in
    std::vector<LongestCommonSubsequenceCalculator::Row> a, b;

    a.reserve(rows.size());
    for (auto& row : rows)
        a.push_back({row.key, row.prev_tv_index});
    std::sort(begin(a), end(a), [](auto lft, auto rgt) {
        return std::tie(lft.tv_index, lft.key) < std::tie(rgt.tv_index, rgt.key);
    });

    // Before constructing `b`, first find the first index in `a` which will
    // actually differ in `b`, and skip everything else if there aren't any
    size_t first_difference = IndexSet::npos;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].key != rows[i].key) {
            first_difference = i;
            break;
        }
    }
    if (first_difference == IndexSet::npos)
        return;

    // Note that `b` is sorted by key, while `a` is sorted by tv_index
    b.reserve(rows.size());
    for (size_t i = 0; i < rows.size(); ++i)
        b.push_back({rows[i].key, i});
    std::sort(begin(b), end(b), [](auto lft, auto rgt) {
        return std::tie(lft.key, lft.tv_index) < std::tie(rgt.key, rgt.tv_index);
    });

    // Calculate the LCS of the two sequences
    auto matches = LongestCommonSubsequenceCalculator(a, b, first_difference,
                                                      changeset.modifications).m_longest_matches;

    // And then insert and delete rows as needed to align them
    size_t i = first_difference, j = first_difference;
    for (auto match : matches) {
        for (; i < match.i; ++i)
            changeset.deletions.add(a[i].tv_index);
        for (; j < match.j; ++j)
            changeset.insertions.add(rows[j].tv_index);
        i += match.size;
        j += match.size;
    }
}

template<typename T>
void verify_changeset(std::vector<T> const& prev_rows,
                      std::vector<T> const& next_rows,
                      CollectionChangeBuilder const& changeset)
{
#ifdef REALM_DEBUG
    { // Verify that applying the calculated change to prev_rows actually produces next_rows
        auto rows = prev_rows;
        auto it = util::make_reverse_iterator(changeset.deletions.end());
        auto end = util::make_reverse_iterator(changeset.deletions.begin());
        for (; it != end; ++it) {
            rows.erase(rows.begin() + it->first, rows.begin() + it->second);
        }

        for (auto i : changeset.insertions.as_indexes()) {
            rows.insert(rows.begin() + i, next_rows[i]);
        }

        REALM_ASSERT(rows == next_rows);
    }
#else
    static_cast<void>(prev_rows);
    static_cast<void>(next_rows);
    static_cast<void>(changeset);
#endif
}

void calculate(CollectionChangeBuilder& ret,
               std::vector<RowInfo> old_rows, std::vector<RowInfo> new_rows,
               std::function<bool (int64_t)> key_did_change, bool in_table_order)
{
    // Now that our old and new sets of rows are sorted by key, we can
    // iterate over them and either record old+new TV indices for rows present
    // in both, or mark them as inserted/deleted if they appear only in one
    size_t i = 0, j = 0;
    while (i < old_rows.size() && j < new_rows.size()) {
        auto old_index = old_rows[i];
        auto& new_index = new_rows[j];
        if (old_index.key == new_index.key) {
            new_index.prev_tv_index = old_rows[i].tv_index;
            ++i;
            ++j;
        }
        else if (old_index.key < new_index.key) {
            ret.deletions.add(old_index.tv_index);
            ++i;
        }
        else {
            ret.insertions.add(new_index.tv_index);
            ++j;
        }
    }

    for (; i < old_rows.size(); ++i)
        ret.deletions.add(old_rows[i].tv_index);
    for (; j < new_rows.size(); ++j)
        ret.insertions.add(new_rows[j].tv_index);

    // Filter out the new insertions since we don't need them for any of the
    // further calculations
    new_rows.erase(std::remove_if(begin(new_rows), end(new_rows),
                                  [](auto& row) { return row.prev_tv_index == IndexSet::npos; }),
                   end(new_rows));
    std::sort(begin(new_rows), end(new_rows),
              [](auto& lft, auto& rgt) { return lft.tv_index < rgt.tv_index; });

    for (auto& row : new_rows) {
        if (key_did_change(row.key)) {
            ret.modifications.add(row.tv_index);
        }
    }

    if (!in_table_order)
        calculate_moves_sorted(new_rows, ret);
}

} // Anonymous namespace

CollectionChangeBuilder CollectionChangeBuilder::calculate(std::vector<int64_t> const& prev_rows,
                                                           std::vector<int64_t> const& next_rows,
                                                           std::function<bool (int64_t)> key_did_change,
                                                           bool in_table_order)
{

    auto build_row_info = [](auto& rows) {
        std::vector<RowInfo> info;
        info.reserve(rows.size());
        for (size_t i = 0; i < rows.size(); ++i)
            info.push_back({rows[i], IndexSet::npos, i});
        std::sort(begin(info), end(info), [](auto& lft, auto& rgt) { return lft.key < rgt.key; });
        return info;
    };

    CollectionChangeBuilder ret;
    ::calculate(ret, build_row_info(prev_rows), build_row_info(next_rows), std::move(key_did_change), in_table_order);
    ret.verify();
    verify_changeset(prev_rows, next_rows, ret);
    return ret;
}

CollectionChangeBuilder CollectionChangeBuilder::calculate(std::vector<size_t> const& prev_rows,
                                                           std::vector<size_t> const& next_rows,
                                                           std::function<bool (int64_t)> key_did_change)
{

    auto build_row_info = [](auto& rows) {
        std::vector<RowInfo> info;
        info.reserve(rows.size());
        for (size_t i = 0; i < rows.size(); ++i)
            info.push_back({static_cast<int64_t>(rows[i]), IndexSet::npos, i});
        std::sort(begin(info), end(info), [](auto& lft, auto& rgt) { return lft.key < rgt.key; });
        return info;
    };

    CollectionChangeBuilder ret;
    ::calculate(ret, build_row_info(prev_rows), build_row_info(next_rows), std::move(key_did_change), false);
    ret.verify();
    verify_changeset(prev_rows, next_rows, ret);
    return ret;
}

CollectionChangeSet CollectionChangeBuilder::finalize() &&
{
    // Calculate which indices in the old collection were modified
    auto modifications_in_old = modifications;
    modifications_in_old.erase_at(insertions);
    modifications_in_old.shift_for_insert_at(deletions);

    // During changeset calculation we allow marking a row as both inserted and
    // modified in case changeset merging results in it no longer being an insert,
    // but we don't want inserts in the final modification set
    modifications.remove(insertions);

    return {
        std::move(deletions),
        std::move(insertions),
        std::move(modifications_in_old),
        std::move(modifications),
        std::move(moves),
        std::move(columns)
    };
}
