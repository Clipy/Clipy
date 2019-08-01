/*************************************************************************
 *
 * Copyright 2016 Realm Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 **************************************************************************/

#ifndef REALM_HANDOVER_DEFS
#define REALM_HANDOVER_DEFS

#include <memory>
#include <vector>

namespace realm {

enum class ConstSourcePayload { Copy, Stay };
enum class MutableSourcePayload { Move };

struct RowBaseHandoverPatch;
struct TableViewHandoverPatch;

struct TableHandoverPatch {
    bool m_is_sub_table;
    size_t m_table_num;
    size_t m_col_ndx;
    size_t m_row_ndx;
};

struct LinkViewHandoverPatch {
    std::unique_ptr<TableHandoverPatch> m_table;
    size_t m_col_num;
    size_t m_row_ndx;
};

// Base class for handover patches for query nodes. Subclasses are declared in query_engine.hpp.
struct QueryNodeHandoverPatch {
    virtual ~QueryNodeHandoverPatch() = default;
};

using QueryNodeHandoverPatches = std::vector<std::unique_ptr<QueryNodeHandoverPatch>>;

struct QueryHandoverPatch {
    std::unique_ptr<TableHandoverPatch> m_table;
    std::unique_ptr<TableViewHandoverPatch> table_view_data;
    std::unique_ptr<LinkViewHandoverPatch> link_view_data;
    QueryNodeHandoverPatches m_node_data;
};

enum class DescriptorType { Sort, Distinct, Limit, Include };

struct DescriptorLinkPath {
    DescriptorLinkPath(size_t column_index, size_t table_index, bool column_is_backlink)
        : col_ndx(column_index)
        , table_ndx(table_index)
        , is_backlink(column_is_backlink)
    {
    }

    size_t col_ndx;
    size_t table_ndx;
    bool is_backlink = false;
};

struct DescriptorExport {
    DescriptorType type;
    std::vector<std::vector<DescriptorLinkPath>> columns;
    std::vector<bool> ordering;
    size_t limit;
};

struct DescriptorOrderingHandoverPatch {
    std::vector<DescriptorExport> descriptors;
};

struct TableViewHandoverPatch {
    std::unique_ptr<TableHandoverPatch> m_table;
    std::unique_ptr<RowBaseHandoverPatch> linked_row;
    size_t linked_col;
    bool was_in_sync;
    QueryHandoverPatch query_patch;
    std::unique_ptr<LinkViewHandoverPatch> linkview_patch;
    std::unique_ptr<DescriptorOrderingHandoverPatch> descriptors_patch;
};


struct RowBaseHandoverPatch {
    std::unique_ptr<TableHandoverPatch> m_table;
    size_t row_ndx;
};


} // end namespace Realm

#endif
