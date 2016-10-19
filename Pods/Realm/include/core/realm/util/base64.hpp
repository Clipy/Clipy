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

#ifndef REALM_UTIL_BASE64_HPP
#define REALM_UTIL_BASE64_HPP

#include <realm/string_data.hpp>
#include <realm/util/optional.hpp>

namespace realm {
namespace util {


/// base64_encode() encodes the bnary data in \param in_buffer of size \param in_buffer_size.
/// The encoded data is placed in \param out_buffer. The size of \param \out_buffer is passed in
/// \param out_buffer_size. The output buffer \param out_buffer must be
/// large enough to hold the base64 encoded data. The size can be obtained from the function
/// base64_encoded_size. \param out_buffer_size is only used to assert that the output buffer is
/// large enough.
size_t base64_encode(const char *in_buffer, size_t in_buffer_size, char* out_buffer, size_t out_buffer_size) noexcept;

/// base64_encoded_size() returns the exact size of the base64 encoded
/// data as a function of the size of the input data.
inline size_t base64_encoded_size(size_t in_buffer_size) noexcept
{
    return 4 * ((in_buffer_size + 2) / 3);
}


/// Decode base64-encoded string in input, and places the result in out_buffer.
/// The length of the out_buffer must be at least 3 * input.size() / 4.
///
/// The input must be padded base64 (i.e. the number of non-whitespace
/// characters in the input must be a multiple of 4). Whitespace (spaces, tabs,
/// newlines) is ignored.
///
/// The algorithm stops when the first character not in the base64 character
/// set is encountered, or when the end of the input is reached.
///
/// \returns the number of successfully decoded bytes written to out_buffer, or
/// none if the whole input was not valid base64.
Optional<size_t> base64_decode(StringData input, char* out_buffer, size_t out_buffer_len) noexcept;

/// Return an upper bound on the decoded size of a Base64-encoded data
/// stream of length \a base64_size. The returned value is suitable for
/// allocation of buffers containing decoded data.
inline size_t base64_decoded_size(size_t base64_size) noexcept
{
    return (base64_size * 3 + 3) / 4;
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_BASE64_HPP

