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

#ifndef REALM_IMPL_OUTPUT_STREAM_HPP
#define REALM_IMPL_OUTPUT_STREAM_HPP

#include <cstddef>
#include <ostream>

#include <cstdint>

#include <realm/util/features.h>

#include <realm/impl/array_writer.hpp>

namespace realm {
namespace _impl {


class OutputStream : public ArrayWriterBase {
public:
    OutputStream(std::ostream&);
    ~OutputStream() noexcept;

    ref_type get_ref_of_next_array() const noexcept;

    void write(const char* data, size_t size);

    ref_type write_array(const char* data, size_t size, uint32_t checksum) override;

private:
    ref_type m_next_ref;
    std::ostream& m_out;

    void do_write(const char* data, size_t size);
};


// Implementation:

inline OutputStream::OutputStream(std::ostream& out)
    : m_next_ref(0)
    , m_out(out)
{
}

inline OutputStream::~OutputStream() noexcept
{
}

inline size_t OutputStream::get_ref_of_next_array() const noexcept
{
    return m_next_ref;
}


} // namespace _impl
} // namespace realm

#endif // REALM_IMPL_OUTPUT_STREAM_HPP
