/*************************************************************************
 *
 * Copyright 2017 Realm Inc.
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

#ifndef REALM_SYNC_IMPL_INSTRUCTION_REPLICATION_HPP
#define REALM_SYNC_IMPL_INSTRUCTION_REPLICATION_HPP

#include <realm/replication.hpp>
#include <realm/sync/instructions.hpp>
#include <realm/sync/object.hpp>
#include <realm/sync/changeset_encoder.hpp>

namespace realm {
namespace sync {


class InstructionReplication: public TrivialReplication, public ObjectIDProvider {
public:
    explicit InstructionReplication(const std::string& realm_path);
    void set_short_circuit(bool) noexcept;
    bool is_short_circuited() const noexcept;

    ChangesetEncoder& get_instruction_encoder() noexcept;

    // AddTable needs special handling because of primary key columns.
    // (`insert_group_level_table()` does nothing on its own.)
    void add_table(StringData name);
    void add_table_with_primary_key(StringData table_name, DataType pk_type, StringData pk_field, bool nullable);

    // TrivialReplication interface:
    void initialize(SharedGroup&) override;

    // TransactLogConvenientEncoder interface:
    void insert_group_level_table(size_t table_ndx, size_t num_tables, StringData name) override;
    void erase_group_level_table(size_t table_ndx, size_t num_tables) override;
    void rename_group_level_table(size_t table_ndx, StringData new_name) override;
    void move_group_level_table(size_t from_table_ndx, size_t to_table_ndx) override;
    void insert_column(const Descriptor&, size_t col_ndx, DataType type, StringData name, LinkTargetInfo& link,
                               bool nullable = false) override;
    void erase_column(const Descriptor&, size_t col_ndx) override;
    void rename_column(const Descriptor&, size_t col_ndx, StringData name) override;
    void move_column(const Descriptor&, size_t from, size_t to) override;

    void set_int(const Table*, size_t col_ndx, size_t ndx, int_fast64_t value, _impl::Instruction variant) override;
    void add_int(const Table*, size_t col_ndx, size_t ndx, int_fast64_t value) override;
    void set_bool(const Table*, size_t col_ndx, size_t ndx, bool value, _impl::Instruction variant) override;
    void set_float(const Table*, size_t col_ndx, size_t ndx, float value, _impl::Instruction variant) override;
    void set_double(const Table*, size_t col_ndx, size_t ndx, double value, _impl::Instruction variant) override;
    void set_string(const Table*, size_t col_ndx, size_t ndx, StringData value, _impl::Instruction variant) override;
    void set_binary(const Table*, size_t col_ndx, size_t ndx, BinaryData value, _impl::Instruction variant) override;
    void set_olddatetime(const Table*, size_t col_ndx, size_t ndx, OldDateTime value,
                                 _impl::Instruction variant) override;
    void set_timestamp(const Table*, size_t col_ndx, size_t ndx, Timestamp value, _impl::Instruction variant) override;
    void set_table(const Table*, size_t col_ndx, size_t ndx, _impl::Instruction variant) override;
    void set_mixed(const Table*, size_t col_ndx, size_t ndx, const Mixed& value, _impl::Instruction variant) override;
    void set_link(const Table*, size_t col_ndx, size_t ndx, size_t value, _impl::Instruction variant) override;
    void set_null(const Table*, size_t col_ndx, size_t ndx, _impl::Instruction variant) override;
    void set_link_list(const LinkView&, const IntegerColumn& values) override;
    void insert_substring(const Table*, size_t col_ndx, size_t row_ndx, size_t pos, StringData) override;
    void erase_substring(const Table*, size_t col_ndx, size_t row_ndx, size_t pos, size_t size) override;
    void insert_empty_rows(const Table*, size_t row_ndx, size_t num_rows_to_insert, size_t prior_num_rows) override;
    void add_row_with_key(const Table*, size_t row_ndx, size_t prior_num_rows, size_t key_col_ndx, int64_t key) override;
    void erase_rows(const Table*, size_t row_ndx, size_t num_rows_to_erase, size_t prior_num_rowsp,
                            bool is_move_last_over) override;
    void swap_rows(const Table*, size_t row_ndx_1, size_t row_ndx_2) override;
    void move_row(const Table*, size_t row_ndx_1, size_t row_ndx_2) override;
    void merge_rows(const Table*, size_t row_ndx, size_t new_row_ndx) override;
    void add_search_index(const Descriptor&, size_t col_ndx) override;
    void remove_search_index(const Descriptor&, size_t col_ndx) override;
    void set_link_type(const Table*, size_t col_ndx, LinkType) override;
    void clear_table(const Table*, size_t prior_num_rows) override;
    void optimize_table(const Table*) override;
    void link_list_set(const LinkView&, size_t ndx, size_t value) override;
    void link_list_insert(const LinkView&, size_t ndx, size_t value) override;
    void link_list_move(const LinkView&, size_t from_ndx, size_t to_ndx) override;
    void link_list_swap(const LinkView&, size_t ndx_1, size_t ndx_2) override;
    void link_list_erase(const LinkView&, size_t ndx) override;
    void link_list_clear(const LinkView&) override;
    void nullify_link(const Table*, size_t col_ndx, size_t ndx) override;
    void link_list_nullify(const LinkView&, size_t ndx) override;

protected:
    // Replication interface:
    void do_initiate_transact(version_type current_version, bool history_updated) override;
private:
    bool m_short_circuit = false;

    ChangesetEncoder m_encoder;
    SharedGroup* m_sg = nullptr;
    std::unique_ptr<TableInfoCache> m_cache;

    // FIXME: The base class already caches this.
    const Table* m_selected_table = nullptr;
    const LinkView* m_selected_link_list = nullptr;

    std::string m_table_being_created;
    std::string m_table_being_created_primary_key;
    bool is_metadata_table(const Table*) const;

    void unsupported_instruction(); // Throws TransformError
    void select_table(const Table*);
    void select_array(const Table* parent, size_t col, size_t row);
    void select_table(const Descriptor&);
    void select_link_list(const LinkView&);
};

inline void InstructionReplication::set_short_circuit(bool b) noexcept
{
    m_short_circuit = b;
}

inline bool InstructionReplication::is_short_circuited() const noexcept
{
    return m_short_circuit;
}

inline ChangesetEncoder& InstructionReplication::get_instruction_encoder() noexcept
{
    return m_encoder;
}

// Temporarily short-circuit replication
class TempShortCircuitReplication {
public:
    TempShortCircuitReplication(InstructionReplication& bridge): m_bridge(bridge)
    {
        m_was_short_circuited = bridge.is_short_circuited();
        bridge.set_short_circuit(true);
    }

    ~TempShortCircuitReplication() {
        m_bridge.set_short_circuit(m_was_short_circuited);
    }
private:
    InstructionReplication& m_bridge;
    bool m_was_short_circuited;
};

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_IMPL_INSTRUCTION_REPLICATION_HPP
