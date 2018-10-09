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

#include <vector>
#include <unordered_map>
#include <iosfwd> // string conversion, debug prints
#include <memory> // shared_ptr
#include <type_traits>

#include <realm/util/string_buffer.hpp>
#include <realm/string_data.hpp>
#include <realm/binary_data.hpp>
#include <realm/data_type.hpp>
#include <realm/timestamp.hpp>
#include <realm/sync/object_id.hpp>
#include <realm/impl/input_stream.hpp>
#include <realm/table_ref.hpp>
#include <realm/link_view_fwd.hpp>

namespace realm {
namespace sync {

// CAUTION: Any change to the order or number of instructions is a
// protocol-breaking change!
#define REALM_FOR_EACH_INSTRUCTION_TYPE(X) \
    X(SelectTable) \
    X(SelectField) \
    X(AddTable) \
    X(EraseTable) \
    X(CreateObject) \
    X(EraseObject) \
    X(Set) \
    X(AddInteger) \
    X(InsertSubstring) \
    X(EraseSubstring) \
    X(ClearTable) \
    X(AddColumn) \
    X(EraseColumn) \
    X(ArraySet) \
    X(ArrayInsert) \
    X(ArrayMove) \
    X(ArraySwap) \
    X(ArrayErase) \
    X(ArrayClear) \


enum class ContainerType {
    None = 0,
    Reserved0 = 1,
    Array = 2,
    Set = 3,
    Dictionary = 4,
};

struct Instruction {
    // Base classes for instructions with common fields. They enable the merge
    // algorithm to reuse some code without resorting to templates, and can be
    // combined to allow optimal memory layout of instructions (size <= 64).
    struct PayloadInstructionBase;
    struct ObjectInstructionBase;
    struct FieldInstructionBase;

#define REALM_DECLARE_INSTRUCTION_STRUCT(X) struct X;
    REALM_FOR_EACH_INSTRUCTION_TYPE(REALM_DECLARE_INSTRUCTION_STRUCT)
#undef REALM_DECLARE_INSTRUCTION_STRUCT

    enum class Type: uint8_t {
#define REALM_DEFINE_INSTRUCTION_TYPE(X) X,
    REALM_FOR_EACH_INSTRUCTION_TYPE(REALM_DEFINE_INSTRUCTION_TYPE)
#undef REALM_DEFINE_INSTRUCTION_TYPE
    };

    struct Payload;
    template <Type t> struct GetType;
    template <class T> struct GetInstructionType;

    Instruction() {}
    template <class T>
    Instruction(T instr);

    static const size_t max_instruction_size = 64;
    std::aligned_storage_t<max_instruction_size, 16> m_storage;
    Type type;

    template <class F>
    auto visit(F&& lambda);
    template <class F>
    auto visit(F&& lambda) const;

    template <class T> T& get_as()
    {
        REALM_ASSERT(type == GetInstructionType<T>::value);
        return *reinterpret_cast<T*>(&m_storage);
    }

    template <class T>
    const T& get_as() const
    {
        return const_cast<Instruction*>(this)->template get_as<T>();
    }

    bool operator==(const Instruction& other) const noexcept;
    bool operator!=(const Instruction& other) const noexcept
    {
        return !(*this == other);
    }
};

// 0x3f is the largest value that fits in a single byte in the variable-length
// encoded integer instruction format.
static constexpr uint8_t InstrTypeInternString = 0x3f;

// This instruction code is only ever used internally by the Changeset class
// to allow insertion/removal while keeping iterators stable. Should never
// make it onto the wire.
static constexpr uint8_t InstrTypeMultiInstruction = 0xff;

struct StringBufferRange {
    uint32_t offset, size;
};

struct InternString {
    static const InternString npos;
    explicit constexpr InternString(uint32_t v = uint32_t(-1)): value(v) {}

    uint32_t value;

    bool operator==(const InternString& other) const noexcept { return value == other.value; }
};

struct Instruction::Payload {
    struct Link {
        sync::ObjectID target; // can be nothing = null
        InternString target_table;
    };

    union Data {
        bool boolean;
        int64_t integer;
        float fnum;
        double dnum;
        StringBufferRange str;
        Timestamp timestamp;
        Link link;

        Data() noexcept {}
        Data(const Data&) noexcept = default;
        Data& operator=(const Data&) noexcept = default;
    };
    Data data;
    int8_t type; // -1 = null, -2 = implicit_nullify

    Payload(): Payload(realm::util::none) {}
    explicit Payload(bool value)      noexcept: type(type_Bool) { data.boolean = value; }
    explicit Payload(int64_t value)   noexcept: type(type_Int) { data.integer = value; }
    explicit Payload(float value)     noexcept: type(type_Float) { data.fnum = value; }
    explicit Payload(double value)    noexcept: type(type_Double) { data.dnum = value; }
    explicit Payload(Timestamp value) noexcept: type(type_Timestamp) { data.timestamp = value; }
    explicit Payload(Link value)      noexcept: type(type_Link) { data.link = value; }
    explicit Payload(StringBufferRange value) noexcept: type(type_String) { data.str = value; }
    explicit Payload(realm::util::None, bool implicit_null = false) noexcept {
        type = (implicit_null ? -2 : -1);
    }

    Payload(const Payload&) noexcept = default;
    Payload& operator=(const Payload&) noexcept = default;

    bool is_null() const;
    bool is_implicit_null() const;
};

struct Instruction::ObjectInstructionBase {
    sync::ObjectID object;
};

struct Instruction::FieldInstructionBase
    : Instruction::ObjectInstructionBase
{
    InternString field;
};

struct Instruction::PayloadInstructionBase {
    Payload payload;
};


struct Instruction::SelectTable {
    InternString table;
};

struct Instruction::SelectField
    : Instruction::FieldInstructionBase
{
    InternString link_target_table;
};

struct Instruction::AddTable {
    InternString table;
    InternString primary_key_field;
    DataType primary_key_type;
    bool has_primary_key;
    bool primary_key_nullable;
};

struct Instruction::EraseTable {
    InternString table;
};

struct Instruction::CreateObject
    : Instruction::PayloadInstructionBase
    , Instruction::ObjectInstructionBase
{
    bool has_primary_key;
};

struct Instruction::EraseObject
    : Instruction::ObjectInstructionBase
{};

struct Instruction::Set
    : Instruction::PayloadInstructionBase
    , Instruction::FieldInstructionBase
{
    bool is_default;
};

struct Instruction::AddInteger
    : Instruction::FieldInstructionBase
{
    int64_t value;
};

struct Instruction::InsertSubstring
    : Instruction::FieldInstructionBase
{
    StringBufferRange value;
    uint32_t pos;
};

struct Instruction::EraseSubstring
    : Instruction::FieldInstructionBase
{
    uint32_t pos;
    uint32_t size;
};

struct Instruction::ClearTable {
};

struct Instruction::ArraySet {
    Instruction::Payload payload;
    uint32_t ndx;
    uint32_t prior_size;
};

struct Instruction::ArrayInsert {
    // payload carries the value in case of LinkList
    // payload is empty in case of Array, Dict or any other container type
    Instruction::Payload payload;
    uint32_t ndx;
    uint32_t prior_size;
};

struct Instruction::ArrayMove {
    uint32_t ndx_1;
    uint32_t ndx_2;
};

struct Instruction::ArrayErase {
    uint32_t ndx;
    uint32_t prior_size;
    bool implicit_nullify;
};

struct Instruction::ArraySwap {
    uint32_t ndx_1;
    uint32_t ndx_2;
};

struct Instruction::ArrayClear {
    uint32_t prior_size;
};


// If container_type != ContainerType::none, creates a subtable:
// +---+---+-------+
// | a | b |   c   |
// +---+---+-------+
// |   |   | +---+ |
// |   |   | | v | |
// |   |   | +---+ |
// | 1 | 2 | | 3 | |
// |   |   | | 4 | |
// |   |   | | 5 | |
// |   |   | +---+ |
// +---+---+-------+
struct Instruction::AddColumn {
    InternString field;
    InternString link_target_table;
    DataType type;
    ContainerType container_type;
    bool nullable;
};

struct Instruction::EraseColumn {
    InternString field;
};

struct InstructionHandler {
    /// Notify the handler that an InternString meta-instruction was found.
    virtual void set_intern_string(uint32_t index, StringBufferRange) = 0;

    /// Notify the handler of the string value. The handler guarantees that the
    /// returned string range is valid at least until the next invocation of
    /// add_string_range().
    ///
    /// Instances of `StringBufferRange` passed to operator() after invoking
    /// this function are assumed to refer to ranges in this buffer.
    virtual StringBufferRange add_string_range(StringData) = 0;

    /// Handle an instruction.
    virtual void operator()(const Instruction&) = 0;
};


/// Implementation:

#if !defined(__GNUC__) || defined(__clang__) || __GNUC__ > 4 // GCC 4.x does not support std::is_trivially_copyable
#define REALM_CHECK_TRIVIALLY_COPYABLE(X) static_assert(std::is_trivially_copyable<Instruction::X>::value, #X" Instructions must be trivially copyable.");
    REALM_FOR_EACH_INSTRUCTION_TYPE(REALM_CHECK_TRIVIALLY_COPYABLE)
#undef REALM_CHECK_TRIVIALLY_COPYABLE
#endif // __GNUC__

#ifdef _WIN32 // FIXME: Fails in VS. 
#define REALM_CHECK_INSTRUCTION_SIZE(X)
#else
#define REALM_CHECK_INSTRUCTION_SIZE(X) static_assert(sizeof(Instruction::X) <= Instruction::max_instruction_size, #X" Instruction too big.");
    REALM_FOR_EACH_INSTRUCTION_TYPE(REALM_CHECK_INSTRUCTION_SIZE)
#undef REALM_CHECK_INSTRUCTION_SIZE
#endif

#define REALM_DEFINE_INSTRUCTION_GET_TYPE(X) \
    template <> struct Instruction::GetType<Instruction::Type::X> { using Type = Instruction::X; }; \
    template <> struct Instruction::GetInstructionType<Instruction::X> { static const Instruction::Type value = Instruction::Type::X; };
    REALM_FOR_EACH_INSTRUCTION_TYPE(REALM_DEFINE_INSTRUCTION_GET_TYPE)
#undef REALM_DEFINE_INSTRUCTION_GET_TYPE


template <class T>
Instruction::Instruction(T instr): type(GetInstructionType<T>::value)
{
    new(&m_storage) T(std::move(instr));
}

template <class F>
auto Instruction::visit(F&& lambda)
{
    switch (type) {
#define REALM_VISIT_INSTRUCTION(X) \
        case Type::X: return lambda(get_as<Instruction::X>());
        REALM_FOR_EACH_INSTRUCTION_TYPE(REALM_VISIT_INSTRUCTION)
#undef REALM_VISIT_INSTRUCTION
    }
    REALM_UNREACHABLE();
}

inline bool Instruction::operator==(const Instruction& other) const noexcept
{
    if (type != other.type)
        return false;
    size_t valid_size;
    switch (type) {
#define REALM_COMPARE_INSTRUCTION(X) \
        case Type::X: valid_size = sizeof(Instruction::X); break;
        REALM_FOR_EACH_INSTRUCTION_TYPE(REALM_COMPARE_INSTRUCTION)
#undef REALM_COMPARE_INSTRUCTION
        default: REALM_UNREACHABLE();
    }

    // This relies on all instruction types being PODs to work.
    return std::memcmp(&m_storage, &other.m_storage, valid_size) == 0;
}

template <class F>
auto Instruction::visit(F&& lambda) const
{
    return const_cast<Instruction*>(this)->visit(std::forward<F>(lambda));
}

std::ostream& operator<<(std::ostream&, Instruction::Type);

} // namespace _impl
} // namespace realm

#endif // REALM_IMPL_INSTRUCTIONS_HPP
