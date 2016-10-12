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
***************************************************************************/

#ifndef REALM_ARRAY_WRITER_HPP
#define REALM_ARRAY_WRITER_HPP

#include <realm/alloc.hpp>

namespace realm {
namespace _impl {

class ArrayWriterBase {
public:
    virtual ~ArrayWriterBase() {}

    /// Write the specified array data and its checksum into free
    /// space.
    ///
    /// Returns the ref (position in the target stream) of the written copy of
    /// the specified array data.
    virtual ref_type write_array(const char* data, size_t size, uint32_t checksum) = 0;
};

} // namespace impl_
} // namespace realm

#endif // REALM_ARRAY_WRITER_HPP
