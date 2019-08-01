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
    enum class TableBehavior {
        Class,
        Array,
        Ignore
    };

    explicit InstructionReplication(const std::string& realm_path);
    void set_short_circuit(bool) noexcept;
    bool is_short_circuited() const noexcept;

    // reset() resets the encoder, the selected tables and the cache. It is
    // called by do_initiate_transact(), but can be called at the other times
    // as well.
    virtual void reset();

    ChangesetEncoder& get_instruction_encoder() noexcept;
    const ChangesetEncoder& get_instruction_encoder() const noexcept;

    //@{
    /// Generate instructions for Object Store tables. These must be called
    /// prior to calling the equivalent functions in Core's API. When creating a
    /// class-like table, `add_class()` must be called prior to
    /// `Group::insert_group_level_table()`. Similarly, `create_object()` or
    /// `create_object_with_primary_key()` must be called prior to
    /// `Table::insert_empty_row()` and/or `Table::set_int_unique()` or
    /// `Table::set_string_unique()` or `Table::set_null_unique()`.
    ///
    /// If a class-like table is added, or an object-like row is inserted,
    /// without calling these methods first, an exception will be thrown.
    ///
    /// A "class-like table" is defined as a table whose name begins with
    /// "class_" (this is the convention used by Object Store). Non-class-like
    /// tables can be created and modified using Core's API without calling
    /// these functions, because they do not result in instructions being
    /// emitted.
    void add_class(StringData table_name);
    void add_class_with_primary_key(StringData table_name, DataType pk_type, StringData pk_field, bool nullable);
    void create_object(const Table*, ObjectID);
    void create_object_with_primary_key(const Table*, ObjectID, StringData);
    void create_object_with_primary_key(const Table*, ObjectID, int_fast64_t);
    void create_object_with_primary_key(const Table*, ObjectID, realm::util::None);
    void prepare_erase_table(StringData table_name);
    //@}

    // TrivialReplication interface:
    void initialize(SharedGroup&) override;

    // TransactLogConvenientEncoder interface:
    void insert_group_level_table(size_t table_ndx, size_t num_tables, StringData name) override;
    void erase_group_level_table(size_t table_ndx, size_t num_tables) override;
    void rename_group_level_table(size_t table_ndx, StringData new_name) override;
    void insert_column(const Descriptor&, size_t col_ndx, DataType type, StringData name, LinkTargetInfo& link,
                               bool nullable = false) override;
    void erase_column(const Descriptor&, size_t col_ndx) override;
    void rename_column(const Descriptor&, size_t col_ndx, StringData name) override;

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

    template <class T>
    void emit(T instruction);

    TableBehavior select_table(const Table*);
    const Table* selected_table() const noexcept;

protected:
    // Replication interface:
    void do_initiate_transact(TransactionType, version_type current_version) override;
private:
    bool m_short_circuit = false;

    ChangesetEncoder m_encoder;
    SharedGroup* m_sg = nullptr;
    std::unique_ptr<TableInfoCache> m_cache;


    // FIXME: The base class already caches this.
    ConstTableRef m_selected_table;
    TableBehavior m_selected_table_behavior; // cache
    ConstLinkViewRef m_selected_link_list = nullptr;

    // Consistency checks:
    std::string m_table_being_created;
    std::string m_table_being_created_primary_key;
    std::string m_table_being_erased;
    util::Optional<ObjectID> m_object_being_created;

    REALM_NORETURN void unsupported_instruction(); // Throws TransformError
    TableBehavior select_table(const Descriptor&);
    TableBehavior select_table_inner(const Table* table);
    bool select_link_list(const LinkView&); // returns true if table behavior != ignored

    TableBehavior get_table_behavior(const Table*) const;

    template <class T>
    void set(const Table*, size_t row_ndx, size_t col_ndx, T payload,
             _impl::Instruction variant);
    template <class T>
    void set_pk(const Table*, size_t row_ndx, size_t col_ndx, T payload,
                _impl::Instruction variant);
    template <class T>
    auto as_payload(T value);
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

inline const ChangesetEncoder& InstructionReplication::get_instruction_encoder() const noexcept
{
    return m_encoder;
}

template <class T>
inline void InstructionReplication::emit(T instruction)
{
    REALM_ASSERT(!m_short_circuit);
    m_encoder(instruction);
}

inline auto InstructionReplication::select_table(const Table* table) -> TableBehavior
{
    if (m_selected_table == table) {
        return m_selected_table_behavior;
    }
    return select_table_inner(table);
}

inline const Table* InstructionReplication::selected_table() const noexcept
{
    return m_selected_table.get();
}

// Temporarily short-circuit replication
class TempShortCircuitReplication {
public:
    TempShortCircuitReplication(InstructionReplication& bridge): m_bridge(bridge)
    {
        m_was_short_circuited = bridge.is_short_circuited();
        bridge.set_short_circuit(true);
    }

    ~TempShortCircuitReplication()
    {
        m_bridge.set_short_circuit(m_was_short_circuited);
    }

    bool was_short_circuited() const noexcept
    {
        return m_was_short_circuited;
    }
private:
    InstructionReplication& m_bridge;
    bool m_was_short_circuited;
};

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_IMPL_INSTRUCTION_REPLICATION_HPP
