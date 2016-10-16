/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2015] Realm Inc
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of Realm Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Realm Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Realm Incorporated.
 *
 **************************************************************************/

#ifndef REALM_IMPL_INSTRUCTIONS_HPP
#define REALM_IMPL_INSTRUCTIONS_HPP

#include <stddef.h> // size_t
#include <vector>

#include <realm/util/string_buffer.hpp>
#include <realm/string_data.hpp>
#include <realm/binary_data.hpp>
#include <realm/data_type.hpp>
#include <realm/timestamp.hpp>

namespace realm {
namespace _impl {

class TransactLogParser;
class TransactLogEncoder;
class InputStream;

#define REALM_FOR_EACH_INSTRUCTION_TYPE(X) \
    X(SelectTable) \
    X(SelectDescriptor) \
    X(SelectLinkList) \
    X(InsertGroupLevelTable) \
    X(EraseGroupLevelTable) \
    X(RenameGroupLevelTable) \
    X(MoveGroupLevelTable) \
    X(InsertEmptyRows) \
    X(Remove) \
    X(MoveLastOver) \
    X(Swap) \
    X(MergeRows) \
    X(Set) \
    X(SetDefault) \
    X(SetUnique) \
    X(AddInteger) \
    X(InsertSubstring) \
    X(EraseSubstring) \
    X(ClearTable) \
    X(OptimizeTable) \
    X(InsertColumn) \
    X(EraseColumn) \
    X(RenameColumn) \
    X(MoveColumn) \
    X(AddSearchIndex) \
    X(RemoveSearchIndex) \
    X(SetLinkType) \
    X(LinkListSet) \
    X(LinkListInsert) \
    X(LinkListMove) \
    X(LinkListSwap) \
    X(LinkListErase) \
    X(LinkListClear) \


enum class InstrType {
#define REALM_DEFINE_INSTRUCTION_TYPE(X) X,
REALM_FOR_EACH_INSTRUCTION_TYPE(REALM_DEFINE_INSTRUCTION_TYPE)
#undef REALM_DEFINE_INSTRUCTION_TYPE
};

struct StringBufferRange {
    size_t offset, size;
};


// Note: All specializations must be "POD", so that they can be
// part of a union.
template <InstrType> struct Instr;

template <> struct Instr<InstrType::SelectTable> {
    size_t group_level_ndx;
    size_t num_pairs;
    size_t pairs[2]; // FIXME: max 1 level of subtables
};

template <> struct Instr<InstrType::SelectDescriptor> {
    size_t num_pairs;
    size_t pairs[2]; // FIXME: max 1 level of subtables
};

template <> struct Instr<InstrType::SelectLinkList> {
    size_t col_ndx;
    size_t row_ndx;
    size_t link_target_group_level_ndx;
};

template <> struct Instr<InstrType::InsertGroupLevelTable> {
    Instr() {}
    size_t table_ndx;
    size_t num_tables;
    StringBufferRange name;
};

template <> struct Instr<InstrType::EraseGroupLevelTable> {
    size_t table_ndx;
    size_t num_tables;
};

template <> struct Instr<InstrType::RenameGroupLevelTable> {
    size_t table_ndx;
    StringBufferRange new_name;
};

template <> struct Instr<InstrType::MoveGroupLevelTable> {
    size_t table_ndx_1;
    size_t table_ndx_2;
};

template <> struct Instr<InstrType::InsertEmptyRows> {
    size_t row_ndx;
    size_t num_rows_to_insert;
    size_t prior_num_rows;
};

template <> struct Instr<InstrType::Remove> {
    size_t row_ndx;
    size_t num_rows_to_erase;
    size_t prior_num_rows;
};

template <> struct Instr<InstrType::MoveLastOver> {
    size_t row_ndx;
    size_t num_rows_to_erase;
    size_t prior_num_rows;
};

template <> struct Instr<InstrType::Swap> {
    size_t row_ndx_1;
    size_t row_ndx_2;
};

template <> struct Instr<InstrType::MergeRows> {
    size_t row_ndx;
    size_t new_row_ndx;
};

template <> struct Instr<InstrType::Set> {
    size_t col_ndx;
    size_t row_ndx;

    struct Payload {
        DataType type;

        struct LinkPayload {
            size_t target_row; // npos means null
            size_t target_group_level_ndx;
            bool implicit_nullify;
        };

        union PayloadData {
            bool boolean;
            int64_t integer;
            float fnum;
            double dnum;
            StringBufferRange str;
            Timestamp timestamp;
            LinkPayload link;

            PayloadData() {}
            PayloadData(const PayloadData&) = default;
            PayloadData& operator=(const PayloadData&) = default;
        };
        PayloadData data;

        bool is_null() const;
    };

    Payload payload;
};

template <> struct Instr<InstrType::AddInteger> {
    size_t col_ndx;
    size_t row_ndx;
    int64_t value;
};

template <> struct Instr<InstrType::SetDefault> : Instr<InstrType::Set> {
};

template <> struct Instr<InstrType::SetUnique> : Instr<InstrType::Set> {
    size_t prior_num_rows;
};

template<> struct Instr<InstrType::InsertSubstring> {
    Instr() {}
    size_t col_ndx;
    size_t row_ndx;
    size_t pos;
    StringBufferRange value;
};

template<> struct Instr<InstrType::EraseSubstring> {
    Instr() {}
    size_t col_ndx;
    size_t row_ndx;
    size_t pos;
    size_t size;
};

template <> struct Instr<InstrType::ClearTable> {
};

template <> struct Instr<InstrType::OptimizeTable> {
};

template <> struct Instr<InstrType::LinkListSet> {
    size_t link_ndx;
    size_t value;
    size_t prior_size;
};

template <> struct Instr<InstrType::LinkListInsert> {
    size_t link_ndx;
    size_t value;
    size_t prior_size;
};

template <> struct Instr<InstrType::LinkListMove> {
    size_t link_ndx_1;
    size_t link_ndx_2;
};

template <> struct Instr<InstrType::LinkListErase> {
    size_t link_ndx;
    bool implicit_nullify;
    size_t prior_size;
};

template <> struct Instr<InstrType::LinkListSwap> {
    size_t link_ndx_1;
    size_t link_ndx_2;
};

template <> struct Instr<InstrType::LinkListClear> {
    size_t num_links;
};

template <> struct Instr<InstrType::InsertColumn> {
    size_t col_ndx;
    DataType type;
    StringBufferRange name;
    size_t link_target_table_ndx;
    size_t backlink_col_ndx;
    bool nullable;
};

template <> struct Instr<InstrType::EraseColumn> {
    size_t col_ndx;
    size_t link_target_table_ndx;
    size_t backlink_col_ndx;
};

template <> struct Instr<InstrType::RenameColumn> {
    size_t col_ndx;
    StringBufferRange new_name;
};

template <> struct Instr<InstrType::MoveColumn> {
    size_t col_ndx_1;
    size_t col_ndx_2;
};

template <> struct Instr<InstrType::AddSearchIndex> {
    size_t col_ndx;
};

template <> struct Instr<InstrType::RemoveSearchIndex> {
    size_t col_ndx;
};

template <> struct Instr<InstrType::SetLinkType> {
    size_t col_ndx;
    LinkType type;
};

struct AnyInstruction {
    using Type = InstrType;

    AnyInstruction() {}
    template <InstrType t>
    AnyInstruction(Instr<t> instr): type(t)
    {
        get_as<t>() = std::move(instr);
    }

    InstrType type;
    union InstrUnion {
#define REALM_DEFINE_INSTRUCTION_MEMBER(X) Instr<InstrType::X> m_ ## X;
        REALM_FOR_EACH_INSTRUCTION_TYPE(REALM_DEFINE_INSTRUCTION_MEMBER)
#undef REALM_DEFINE_INSTRUCTION_MEMBER

        InstrUnion() {
#if defined(REALM_DEBUG)
            char* mem = reinterpret_cast<char*>(this);
            std::fill(mem, mem + sizeof(*this), 0xef);
#endif // REALM_DEBUG
        }

        InstrUnion(const InstrUnion&) = default;
        InstrUnion& operator=(const InstrUnion&) = default;
    };
    InstrUnion instr;

    template <class F>
    void visit(F lambda);
    template <class F>
    void visit(F lambda) const;

    template<InstrType type> Instr<type>& get_as();

    template <InstrType type>
    const Instr<type>& get_as() const
    {
        return const_cast<AnyInstruction*>(this)->template get_as<type>();
    }
};

#define REALM_DEFINE_GETTER(X) \
    template<> inline Instr<InstrType::X>& AnyInstruction::get_as<InstrType::X>() \
    { \
        return instr.m_ ## X; \
    }
    REALM_FOR_EACH_INSTRUCTION_TYPE(REALM_DEFINE_GETTER)
#undef REALM_DEFINE_GETTER

using InstructionList = std::vector<AnyInstruction>; // FIXME: Consider using std::deque

InstructionList parse_changeset_as_instructions(_impl::TransactLogParser&, _impl::InputStream&,
                                                util::StringBuffer&);
void encode_instructions_as_changeset(const InstructionList&, const util::StringBuffer&,
                                      _impl::TransactLogEncoder&);




/// Implementation:

template <class F>
void AnyInstruction::visit(F lambda)
{
    switch (type) {
#define REALM_VISIT_INSTRUCTION(X) \
        case InstrType::X: return lambda(get_as<InstrType::X>());
        REALM_FOR_EACH_INSTRUCTION_TYPE(REALM_VISIT_INSTRUCTION)
#undef REALM_VISIT_INSTRUCTION
    }
    REALM_UNREACHABLE();
}

template <class F>
void AnyInstruction::visit(F lambda) const
{
    const_cast<AnyInstruction*>(this)->visit(lambda);
}

} // namespace _impl
} // namespace realm

#endif // REALM_IMPL_INSTRUCTIONS_HPP
