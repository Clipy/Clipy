/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2016] Realm Inc
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

#ifndef REALM_UTIL_COMPRESSION_HPP
#define REALM_UTIL_COMPRESSION_HPP

#include <system_error>
#include <vector>
#include <string>
#include <stdint.h>

namespace realm {
namespace util {
namespace compression {

enum class error {
    out_of_memory = 1,
    deflate_buffer_too_small = 2,
    deflate_error = 3,
    corrupt_input = 4,
    incorrect_inflated_size = 5,
    inflate_error = 6
};


class error_category: public std::error_category {
public:
    const char* name() const noexcept final;
    std::string message(int) const final;
};

const std::error_category& compression_error_category();


std::error_condition make_error_condition(error);

} // namespace compression
} // namespace util
} // namespace realm

namespace std {
    template<>
    struct is_error_condition_enum<realm::util::compression::error> {
        static const bool value = true;
    };
}

namespace realm {
namespace util {
namespace compression {

/// deflate_bound() calculates an upper bound on the size of the compressed data. The caller can use this function to
/// allocate memory buffer calling deflate().
/// \param uncompressed_buf is the buffer with uncompresed data. The size of the uncompressed data is \param
/// uncompressed_size.
/// \param compression_level is described under deflate().
/// \param bound is set to the upper bound at return.
/// The return value is a error_condition of category compression::error_category.
std::error_condition deflate_bound(const char* uncompressed_buf, size_t uncompressed_size, size_t& bound, int compression_level = 1);

/// deflate() deflates the data in the \param uncompressed_buf of size \param uncompressed_size
/// into compressed_buf. deflate() resizes compressed_buf. At return, \param compressed_buf has the
/// size of the deflated data.
/// \param compression_level is [1-9] with 1 the fastest for the current zlib implementation.
/// The return value is a error_condition of category compression::error_category.
std::error_condition deflate(const char* uncompressed_buf, size_t uncompressed_size,
                             char* compressed_buf, size_t compressed_buf_size,
                             size_t& compressed_size, int compression_level = 1);

/// inflate() inflates the data in \param compressed_buf of size \param compresed_size into
/// \param decompressed_buf. \param decompressed_size is the expected size of the inflated data.
/// \param decompressed_buf must have size at least \param decompressed_size.
/// inflate() throws on errors, including the error where the size of the decompressed data is unequal to
/// decompressed_size.
/// The return value is a error_condition of category compression::error_category.
std::error_condition inflate(const char* compressed_buf, size_t compressed_size, char* decompressed_buf, size_t decompressed_size);




} // namespace compression
} // namespace util
} // namespace realm

#endif // REALM_UTIL_COMPRESSION_HPP
