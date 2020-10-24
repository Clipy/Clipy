/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2017] Realm Inc
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

#ifndef REALM_SYNC_CHANGESET_HPP
#define REALM_SYNC_CHANGESET_HPP

#include <realm/sync/instructions.hpp>
#include <realm/util/optional.hpp>
#include <realm/util/allocation_metrics.hpp>
#include <realm/util/metered/vector.hpp>

#include <type_traits>

namespace realm {
namespace sync {

using InternStrings = util::metered::vector<StringBufferRange>;

struct BadChangesetError : ExceptionWithBacktrace<std::exception> {
    const char* m_message;
    BadChangesetError() : BadChangesetError("Bad changeset") {}
    BadChangesetError(const char* msg) : m_message(msg) {}
    const char* message() const noexcept override
    {
        return m_message;
    }
};

struct Changeset {
    struct Range;
    using timestamp_type = uint_fast64_t;
    using file_ident_type = uint_fast64_t;
    using version_type = uint_fast64_t; // FIXME: Get from `History`.
    using StringBuffer = util::BasicStringBuffer<MeteredAllocator>;

    Changeset();
    struct share_buffers_tag {};
    Changeset(const Changeset&, share_buffers_tag);
    Changeset(Changeset&&) = default;
    Changeset& operator=(Changeset&&) = default;
    Changeset(const Changeset&) = delete;
    Changeset& operator=(const Changeset&) = delete;

    InternString intern_string(StringData); // Slow!
    InternString find_string(StringData) const noexcept; // Slow!
    StringData string_data() const noexcept;

    StringBuffer& string_buffer() noexcept;
    const StringBuffer& string_buffer() const noexcept;
    const InternStrings& interned_strings() const noexcept;
    InternStrings& interned_strings() noexcept;

    StringBufferRange get_intern_string(InternString) const noexcept;
    util::Optional<StringBufferRange> try_get_intern_string(InternString) const noexcept;
    util::Optional<StringData> try_get_string(StringBufferRange) const noexcept;
    StringData get_string(StringBufferRange) const noexcept;
    StringData get_string(InternString) const noexcept;
    StringBufferRange append_string(StringData);

    /// Mark the changeset as "dirty" (i.e. modified by the merge algorithm).
    void set_dirty(bool dirty = true) noexcept;

    /// Whether or not the changeset is "dirty" (i.e. has been modified by the
    /// merge algorithm).
    bool is_dirty() const noexcept;

    // Interface to imitate std::vector:
    template <bool is_const> struct IteratorImpl;
    using iterator = IteratorImpl<false>;
    using const_iterator = IteratorImpl<true>;
    using value_type = Instruction;
    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;
    bool empty() const noexcept;

    /// Size of the Changeset, not counting tombstones.
    ///
    /// FIXME: This is an O(n) operation.
    size_t size() const noexcept;

    void clear() noexcept;

    //@{
    /// Insert instructions, invalidating all iterators.
    iterator insert(const_iterator pos, Instruction);
    template <class InputIt>
    iterator insert(const_iterator pos, InputIt begin, InputIt end);
    //@}

    /// Erase an instruction, invalidating all iterators.
    iterator erase(const_iterator);

    /// Insert an instruction at the end, invalidating all iterators.
    void push_back(const Instruction&);

    //@{
    /// Insert instructions at \a position without invalidating other
    /// iterators.
    ///
    /// Only iterators created before any call to `insert_stable()` may be
    /// considered stable across calls to `insert_stable()`. In addition,
    /// "iterator stability" has a very specific meaning here: Other copies of
    /// \a position in the program will point to the newly inserted elements
    /// after calling `insert_stable()`, rather than point to the value at the
    /// position prior to insertion. This is different from, say, a tree
    /// structure, where iterator stability signifies the property that
    /// iterators keep pointing to the same element after insertion before or
    /// after that position.
    ///
    /// For the purpose of supporting `ChangesetIndex`, and the OT merge
    /// algorithm, these semantics are acceptable, since prepended instructions
    /// can never create new object or table references.
    iterator insert_stable(const_iterator position, Instruction);
    template <class InputIt>
    iterator insert_stable(const_iterator position, InputIt begin, InputIt end);
    //@}

    /// Erase instruction at \a position without invalidating other iterators.
    /// If erasing the object would invalidate other iterators, it is turned
    /// into a tombstone instead, and subsequent derefencing of the iterator
    /// will return `nullptr`. An iterator pointing to a tombstone remains valid
    /// and can be incremented.
    ///
    /// Only iterators created before any call to `insert_stable()` may be
    /// considered stable across calls to `erase_stable()`. If other copies of
    /// \a position exist in the program, they will either point to the
    /// subsequent element if that element was previously inserted with
    /// `insert_stable()`, or otherwise it will be turned into a tombstone.
    iterator erase_stable(const_iterator position);

#if REALM_DEBUG
    struct Reflector;
    struct Printer;
    void verify() const;
    void print(std::ostream&) const;
    void print() const; // prints to std::err
#endif

    /// The version that this changeset produced. Note: This may not be the
    /// version produced by this changeset on the client on which this changeset
    /// originated, but may for instance be the version produced on the server
    /// after receiving and re-sending this changeset to another client.
    ///
    /// FIXME: The explanation above is confusing. The truth is that if this
    /// changeset was received by a client from the server, then \a version is
    /// the version that was produced on the server by this changeset.
    ///
    /// FIXME: This property, as well as \a last_integrated_remote_version, \a
    /// origin_timestamp, and \a origin_file_ident should probably be removed
    /// from this class, as they are not a logical part of a changeset, and also
    /// are difficult to document without knowing more about what context the
    /// changeset object occurs. Also, functions such as
    /// InstructionApplier::apply() that a changeset as argument, but do not
    /// care about those properties.
    version_type version = 0;

    /// On clients, the last integrated server version. On the server, this is
    /// the last integrated client version.
    ///
    /// FIXME: The explanation above is confusing. The truth is that if this
    /// changeset was received by a client from the server, then \a
    /// last_integrated_remote_version is the last client version that was
    /// integrated by the server at the server version referencened by \a
    /// version.
    version_type last_integrated_remote_version = 0;

    /// Timestamp at origin when the original untransformed changeset was
    /// produced.
    timestamp_type origin_timestamp = 0;

    /// The identifier of the file in the context of which the original
    /// untransformed changeset was produced.
    file_ident_type origin_file_ident = 0;

private:
    struct MultiInstruction {
        util::metered::vector<Instruction> instructions;
    };
    static_assert(sizeof(MultiInstruction) <= Instruction::max_instruction_size, "Instruction::max_instruction_size too low");

    // In order to achieve iterator semi-stability (just enough to be able to
    // run the merge algorithm while maintaining a ChangesetIndex), a Changeset
    // is really a list of lists. A Changeset is a vector of
    // `InstructionContainer`s, and each `InstructionContainer` represents 0-N
    // "real" instructions.
    //
    // As an optimization, there is a special case for when the
    // `InstructionContainer` represents exactly 1 instruction, in which case it
    // is represented inside the `InstructionContainer` without any additional
    // allocations or indirections. The `InstructionContainer` derived from
    // the `Instruction` struct, and co-opts the `type` field such that if the
    // (invalid) value of `type` is 0xff, the contents of the `Instruction` are
    // instead interpreted as an instance of `MultiInstruction`, which holds
    // a vector of `Instruction`s.
    //
    // The size of the `MultiInstruction` may also be zero, in which case it is
    // considered a "tombstone" - always as a result of a call to
    // `Changeset::erase_stable()`. The potential existence of these tombstones
    // is the reason that the value type of `Changeset::iterator` is
    // `Instruction*`, rather than `Instruction&`.
    //
    // FIXME: It would be better if `Changeset::iterator::value_type` could be
    // `util::Optional<Instruction&>`, but this is prevented by a bug in
    // `util::Optional`.
    struct InstructionContainer : Instruction {
        InstructionContainer();
        InstructionContainer(const Instruction& instr);
        InstructionContainer(InstructionContainer&&) noexcept;
        InstructionContainer(const InstructionContainer&);
        ~InstructionContainer();
        InstructionContainer& operator=(InstructionContainer&&) noexcept;
        InstructionContainer& operator=(const InstructionContainer&);

        bool is_multi() const noexcept;
        void convert_to_multi();
        void insert(size_t position, Instruction instr);
        void erase(size_t position);
        size_t size() const noexcept;
        bool is_empty() const noexcept;

        Instruction& at(size_t pos) noexcept;
        const Instruction& at(size_t pos) const noexcept;

        MultiInstruction& get_multi() noexcept;
        const MultiInstruction& get_multi() const noexcept;
    };

    util::metered::vector<InstructionContainer> m_instructions;
    std::shared_ptr<StringBuffer> m_string_buffer;
    std::shared_ptr<InternStrings> m_strings;
    bool m_is_dirty = false;

    iterator const_iterator_to_iterator(const_iterator);
};

/// An iterator type that hides the implementation details of the support for
/// iterator stability.
///
/// A `Changeset::iterator` is composed of an
/// `std::vector<InstructionContainer>::iterator` and a `size_t` representing
/// the index into the current `InstructionContainer`. If that container is
/// empty, and the position is zero, the iterator is pointing to a tombstone.
template <bool is_const>
struct Changeset::IteratorImpl {
    using list_type = util::metered::vector<InstructionContainer>;
    using inner_iterator_type = std::conditional_t<is_const, list_type::const_iterator, list_type::iterator>;

    // reference_type is a pointer because we have no way to create a reference
    // to a tombstone instruction. Alternatively, it could have been
    // `util::Optional<Instruction&>`, but that runs into other issues.
    using reference_type = std::conditional_t<is_const, const Instruction*, Instruction*>;

    using pointer_type   = std::conditional_t<is_const, const Instruction*, Instruction*>;
    using difference_type = std::ptrdiff_t;

    IteratorImpl() : m_pos(0) {}
    template <bool is_const_ = is_const>
    IteratorImpl(const IteratorImpl<false>& other, std::enable_if_t<is_const_>* = nullptr)
        : m_inner(other.m_inner), m_pos(other.m_pos) {}
    IteratorImpl(inner_iterator_type inner, size_t pos = 0) : m_inner(inner), m_pos(pos) {}

    inline IteratorImpl& operator++()
    {
        ++m_pos;
        if (m_pos >= m_inner->size()) {
            ++m_inner;
            m_pos = 0;
        }
        return *this;
    }

    IteratorImpl operator++(int)
    {
        auto copy = *this;
        ++(*this);
        return copy;
    }

    IteratorImpl& operator--()
    {
        if (m_pos == 0) {
            --m_inner;
            m_pos = m_inner->size();
            if (m_pos != 0)
                --m_pos;
        }
        else {
            --m_pos;
        }
        return *this;
    }

    IteratorImpl operator--(int)
    {
        auto copy = *this;
        --(*this);
        return copy;
    }

    reference_type operator*() const
    {
        if (m_inner->size()) {
            return &m_inner->at(m_pos);
        }
        // It was a tombstone.
        return nullptr;
    }

    pointer_type operator->() const
    {
        if (m_inner->size()) {
            return &m_inner->at(m_pos);
        }
        // It was a tombstone.
        return nullptr;
    }

    bool operator==(const IteratorImpl& other) const
    {
        return m_inner == other.m_inner && m_pos == other.m_pos;
    }

    bool operator!=(const IteratorImpl& other) const
    {
        return !(*this == other);
    }

    bool operator<(const IteratorImpl& other) const
    {
        if (m_inner == other.m_inner)
            return m_pos < other.m_pos;
        return m_inner < other.m_inner;
    }

    bool operator<=(const IteratorImpl& other) const
    {
        if (m_inner == other.m_inner)
            return m_pos <= other.m_pos;
        return m_inner < other.m_inner;
    }

    bool operator>(const IteratorImpl& other) const
    {
        if (m_inner == other.m_inner)
            return m_pos > other.m_pos;
        return m_inner > other.m_inner;
    }

    bool operator>=(const IteratorImpl& other) const
    {
        if (m_inner == other.m_inner)
            return m_pos >= other.m_pos;
        return m_inner > other.m_inner;
    }

    inner_iterator_type m_inner;
    size_t m_pos;
};

struct Changeset::Range {
    iterator begin;
    iterator end;
};

#if REALM_DEBUG
struct Changeset::Reflector {
    struct Tracer {
        virtual void name(StringData) = 0;
        virtual void field(StringData, StringData) = 0;
        virtual void field(StringData, GlobalKey) = 0;
        virtual void field(StringData, int64_t) = 0;
        virtual void field(StringData, double) = 0;
        virtual void after_each() {}
        virtual void before_each() {}
    };

    Reflector(Tracer& tracer, const Changeset& log) :
        m_tracer(tracer), m_log(log)
    {}

    void visit_all() const;
private:
    Tracer& m_tracer;
    const Changeset& m_log;

    friend struct Instruction; // permit access for visit()
#define REALM_DEFINE_REFLECTOR_VISITOR(X) void operator()(const Instruction::X&) const;
    REALM_FOR_EACH_INSTRUCTION_TYPE(REALM_DEFINE_REFLECTOR_VISITOR)
#undef REALM_DEFINE_REFLECTOR_VISITOR
};

struct Changeset::Printer : Changeset::Reflector::Tracer {
    explicit Printer(std::ostream& os) : m_out(os)
    {}

    // ChangesetReflector::Tracer interface:
    void name(StringData) final;
    void field(StringData, StringData) final;
    void field(StringData, GlobalKey) final;
    void field(StringData, int64_t) final;
    void field(StringData, double) final;
    void after_each() final;

private:
    std::ostream& m_out;
    bool m_first = true;
    void pad_or_ellipsis(StringData, int width) const;
    void print_field(StringData name, std::string value);
};
#endif // REALM_DEBUG



/// Implementation:

inline Changeset::iterator Changeset::begin() noexcept
{
    return m_instructions.begin();
}

inline Changeset::iterator Changeset::end() noexcept
{
    return m_instructions.end();
}

inline Changeset::const_iterator Changeset::begin() const noexcept
{
    return m_instructions.begin();
}

inline Changeset::const_iterator Changeset::end() const noexcept
{
    return m_instructions.end();
}

inline Changeset::const_iterator Changeset::cbegin() const noexcept
{
    return m_instructions.cbegin();
}

inline Changeset::const_iterator Changeset::cend() const noexcept
{
    return m_instructions.end();
}

inline bool Changeset::empty() const noexcept
{
    return size() == 0;
}

inline size_t Changeset::size() const noexcept
{
    size_t sum = 0;
    for (auto& x: m_instructions)
        sum += x.size();
    return sum;
}

inline void Changeset::clear() noexcept
{
    m_instructions.clear();
}

inline util::Optional<StringBufferRange> Changeset::try_get_intern_string(InternString string) const noexcept
{
    if (string.value >= m_strings->size())
        return util::none;
    return (*m_strings)[string.value];
}

inline StringBufferRange Changeset::get_intern_string(InternString string) const noexcept
{
    auto str = try_get_intern_string(string);
    REALM_ASSERT(str);
    return *str;
}

inline InternStrings& Changeset::interned_strings() noexcept
{
    return *m_strings;
}

inline const InternStrings& Changeset::interned_strings() const noexcept
{
    return *m_strings;
}

inline auto Changeset::string_buffer() noexcept -> StringBuffer&
{
    return *m_string_buffer;
}

inline auto Changeset::string_buffer() const noexcept -> const StringBuffer&
{
    return *m_string_buffer;
}

inline util::Optional<StringData> Changeset::try_get_string(StringBufferRange range) const noexcept
{
    if (range.offset > m_string_buffer->size())
        return util::none;
    if (range.offset + range.size > m_string_buffer->size())
        return util::none;
    return StringData{m_string_buffer->data() + range.offset, range.size};
}

inline StringData Changeset::get_string(StringBufferRange range) const noexcept
{
    auto string = try_get_string(range);
    REALM_ASSERT(string);
    return *string;
}

inline StringData Changeset::get_string(InternString string) const noexcept
{
    return get_string(get_intern_string(string));
}

inline StringData Changeset::string_data() const noexcept
{
    return StringData{m_string_buffer->data(), m_string_buffer->size()};
}

inline StringBufferRange Changeset::append_string(StringData string)
{
    m_string_buffer->reserve(1024); // we expect more strings
    size_t offset = m_string_buffer->size();
    m_string_buffer->append(string.data(), string.size());
    return StringBufferRange{uint32_t(offset), uint32_t(string.size())};
}

inline bool Changeset::is_dirty() const noexcept
{
    return m_is_dirty;
}

inline void Changeset::set_dirty(bool dirty) noexcept
{
    m_is_dirty = dirty;
}

inline Changeset::iterator Changeset::insert(const_iterator pos, Instruction instr)
{
    Instruction* p = &instr;
    return insert(pos, p, p + 1);
}

template <class InputIt>
inline Changeset::iterator Changeset::insert(const_iterator pos, InputIt begin, InputIt end)
{
    if (pos.m_pos == 0)
        return m_instructions.insert(pos.m_inner, begin, end);
    return insert_stable(pos, begin, end);
}

inline Changeset::iterator Changeset::erase(const_iterator pos)
{
    if (pos.m_inner->size() <= 1)
        return m_instructions.erase(pos.m_inner);
    return erase_stable(pos);
}

inline Changeset::iterator Changeset::insert_stable(const_iterator pos, Instruction instr)
{
    Instruction* p = &instr;
    return insert_stable(pos, p, p + 1);
}

template <class InputIt>
inline Changeset::iterator Changeset::insert_stable(const_iterator cpos, InputIt begin, InputIt end)
{
    iterator pos = const_iterator_to_iterator(cpos);
    size_t i = 0;
    for (auto it = begin; it != end; ++it, ++i) {
        pos.m_inner->insert(pos.m_pos + i, *it);
    }
    return pos;
}

inline Changeset::iterator Changeset::erase_stable(const_iterator cpos)
{
    auto pos = const_iterator_to_iterator(cpos);
    auto begin = m_instructions.begin();
    auto end = m_instructions.end();
    REALM_ASSERT(pos.m_inner >= begin);
    REALM_ASSERT(pos.m_inner < end);
    pos.m_inner->erase(pos.m_pos);
    if (pos.m_pos >= pos.m_inner->size()) {
        do {
            ++pos.m_inner;
        } while (pos.m_inner != end && pos.m_inner->is_empty());
        pos.m_pos = 0;
    }
    return pos;
}

inline void Changeset::push_back(const Instruction& instr)
{
    m_instructions.emplace_back(instr);
}

inline auto Changeset::const_iterator_to_iterator(const_iterator cpos) -> iterator
{
    size_t offset = cpos.m_inner - m_instructions.cbegin();
    return iterator{m_instructions.begin() + offset, cpos.m_pos};
}

inline Changeset::InstructionContainer::~InstructionContainer()
{
    if (is_multi()) {
        get_multi().~MultiInstruction();
    }
    // Instruction subtypes are required to be POD-types (trivially
    // destructible), and this is checked by a static_assert in
    // instructions.hpp. Therefore, it is safe to do nothing if this is not a
    // multi-instruction.
}

inline bool Changeset::InstructionContainer::is_multi() const noexcept
{
    return type == Type(InstrTypeMultiInstruction);
}

inline size_t Changeset::InstructionContainer::size() const noexcept
{
    if (is_multi())
        return get_multi().instructions.size();
    return 1;
}

inline bool Changeset::InstructionContainer::is_empty() const noexcept
{
    if (is_multi()) {
        return get_multi().instructions.empty();
    }
    return false;
}

inline Instruction& Changeset::InstructionContainer::at(size_t pos) noexcept
{
    REALM_ASSERT(pos < size());
    if (is_multi())
        return get_multi().instructions[pos];
    return *this;
}

inline const Instruction& Changeset::InstructionContainer::at(size_t pos) const noexcept
{
    REALM_ASSERT(pos < size());
    if (is_multi())
        return get_multi().instructions[pos];
    return *this;
}

inline Changeset::MultiInstruction& Changeset::InstructionContainer::get_multi() noexcept
{
    REALM_ASSERT(is_multi());
    return *reinterpret_cast<MultiInstruction*>(&m_storage);
}

inline const Changeset::MultiInstruction& Changeset::InstructionContainer::get_multi() const noexcept
{
    REALM_ASSERT(is_multi());
    return *reinterpret_cast<const MultiInstruction*>(&m_storage);
}

} // namespace sync
} // namespace realm

namespace std {

template <bool is_const>
struct iterator_traits<realm::sync::Changeset::IteratorImpl<is_const>> {
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::bidirectional_iterator_tag;
};

} // namespace std

#endif // REALM_SYNC_CHANGESET_HPP
