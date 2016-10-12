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
#ifndef REALM_IMPL_OUTPUT_STREAM_HPP
#define REALM_IMPL_OUTPUT_STREAM_HPP

#include <cstddef>
#include <ostream>

#include <stdint.h>

#include <realm/util/features.h>

#include <realm/impl/array_writer.hpp>

namespace realm {
namespace _impl {


class OutputStream: public ArrayWriterBase {
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

inline OutputStream::OutputStream(std::ostream& out):
    m_next_ref(0),
    m_out(out)
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
