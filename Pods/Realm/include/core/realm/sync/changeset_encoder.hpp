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

#ifndef REALM_SYNC_CHANGESET_ENCODER_HPP
#define REALM_SYNC_CHANGESET_ENCODER_HPP

#include <realm/sync/changeset.hpp>

namespace realm {
namespace sync {

struct ChangesetEncoder: InstructionHandler {
    util::AppendBuffer<char> release() noexcept;
    void reset() noexcept;
    const util::AppendBuffer<char>& buffer() const noexcept;
    InternString intern_string(StringData);

    void set_intern_string(uint32_t index, StringBufferRange) override;
    // FIXME: This doesn't copy the input, but the drawback is that there can
    // only be a single StringBufferRange per instruction. Luckily, no
    // instructions exist that require two or more.
    StringBufferRange add_string_range(StringData) override;
    void operator()(const Instruction&) override;

#define REALM_DEFINE_INSTRUCTION_HANDLER(X) void operator()(const Instruction::X&);
    REALM_FOR_EACH_INSTRUCTION_TYPE(REALM_DEFINE_INSTRUCTION_HANDLER)
#undef REALM_DEFINE_INSTRUCTION_HANDLER

protected:
    template<class E> static void encode(E& encoder, const Instruction&);

    StringData get_string(StringBufferRange) const noexcept;

private:
    template<class... Args>
    void append(Instruction::Type t, Args&&...);
    void append_string(StringBufferRange); // does not intern the string
    void append_bytes(const void*, size_t);

    template<class T> void append_int(T);
    template<class T> char* encode_int(char* buffer, T value);
    void append_payload(const Instruction::Payload&);
    void append_value(DataType);
    void append_value(bool);
    void append_value(uint8_t);
    void append_value(int64_t);
    void append_value(uint32_t);
    void append_value(uint64_t);
    void append_value(float);
    void append_value(double);
    void append_value(InternString);
    void append_value(sync::ObjectID);
    void append_value(Timestamp);

    util::AppendBuffer<char> m_buffer;
    std::unordered_map<std::string, uint32_t> m_intern_strings_rev;
    StringData m_string_range;
};

void encode_changeset(const Changeset&, util::AppendBuffer<char>& out_buffer);




// Implementation

inline const util::AppendBuffer<char>& ChangesetEncoder::buffer() const noexcept
{
    return m_buffer;
}

inline void ChangesetEncoder::operator()(const Instruction& instr)
{
    encode(*this, instr); // Throws
}

template<class E> inline void ChangesetEncoder::encode(E& encoder, const Instruction& instr)
{
    instr.visit(encoder); // Throws
}

inline StringData ChangesetEncoder::get_string(StringBufferRange range) const noexcept
{
    const char* data = m_string_range.data() + range.offset;
    std::size_t size = std::size_t(range.size);
    return StringData{data, size};
}

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_CHANGESET_ENCODER_HPP
