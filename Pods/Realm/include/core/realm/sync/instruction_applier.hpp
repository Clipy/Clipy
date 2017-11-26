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

#ifndef REALM_SYNC_IMPL_INSTRUCTION_APPLIER_HPP
#define REALM_SYNC_IMPL_INSTRUCTION_APPLIER_HPP

#include <realm/sync/instructions.hpp>
#include <realm/sync/changeset.hpp>
#include <realm/util/logger.hpp>

namespace realm {
namespace sync {

struct Changeset;

struct InstructionApplier {
    explicit InstructionApplier(Group& group) noexcept;

    void apply(const Changeset& log, util::Logger* logger);

protected:
    StringData get_string(InternString) const;
    StringData get_string(StringBufferRange) const;
#define REALM_DECLARE_INSTRUCTION_HANDLER(X) void operator()(const Instruction::X&);
    REALM_FOR_EACH_INSTRUCTION_TYPE(REALM_DECLARE_INSTRUCTION_HANDLER)
#undef REALM_DECLARE_INSTRUCTION_HANDLER
    friend struct Instruction; // to allow visitor

    template<class A> static void apply(A& applier, const Changeset& log, util::Logger* logger);

private:
    const Changeset* m_log = nullptr;
    util::Logger* m_logger = nullptr;
    Group& m_group;
    TableRef m_selected_table;
    TableRef m_selected_array;
    LinkViewRef m_selected_link_list;
    TableRef m_link_target_table;

    template <class... Args>
    void log(const char* fmt, Args&&... args)
    {
        if (m_logger) {
            m_logger->trace(fmt, std::forward<Args>(args)...); // Throws
        }
    }

    void bad_transaction_log(const char*) const; // Throws

    TableRef table_for_class_name(StringData) const; // Throws
};




// Implementation

inline InstructionApplier::InstructionApplier(Group& group) noexcept:
    m_group(group)
{
}

template<class A>
inline void InstructionApplier::apply(A& applier, const Changeset& log, util::Logger* logger)
{
    applier.m_log = &log;
    applier.m_logger = logger;
    for (auto instr: log) {
        if (!instr)
            continue;
        instr->visit(applier); // Throws
    }
    applier.m_log = nullptr;
    applier.m_logger = nullptr;
    applier.m_selected_table = TableRef{};
    applier.m_selected_link_list = LinkViewRef{};
    applier.m_link_target_table = TableRef{};
}

inline void InstructionApplier::apply(const Changeset& log, util::Logger* logger)
{
    apply(*this, log, logger); // Throws
}

} // namespace sync
} // namespace realm

#endif // REALM_SYNC_IMPL_INSTRUCTION_APPLIER_HPP
