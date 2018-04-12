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
#include <stddef.h>
#include <memory>

#include <realm/binary_data.hpp>

namespace realm {
namespace util {
namespace compression {

enum class error {
    out_of_memory = 1,
    compress_buffer_too_small = 2,
    compress_error = 3,
    corrupt_input = 4,
    incorrect_decompressed_size = 5,
    decompress_error = 6
};

const std::error_category& error_category() noexcept;

std::error_code make_error_code(error) noexcept;

} // namespace compression
} // namespace util
} // namespace realm

namespace std {

template<> struct is_error_code_enum<realm::util::compression::error> {
    static const bool value = true;
};

} // namespace std

namespace realm {
namespace util {
namespace compression {

class Alloc {
public:
    // Returns null on "out of memory"
    virtual void* alloc(size_t size) = 0;
    virtual void free(void* addr) noexcept = 0;
    virtual ~Alloc() {}
};

class CompressMemoryArena: public Alloc {
public:
    void* alloc(size_t size) override final
    {
        size_t offset = m_offset;
        size_t padding = offset % alignof (std::max_align_t);
        if (padding > m_size - offset)
            return nullptr;
        offset += padding;
        void* addr = m_buffer.get() + offset;
        if (size > m_size - offset)
            return nullptr;
        m_offset = offset + size;
        return addr;
    }

    void free(void*) noexcept override final
    {
        // No-op
    }

    void reset() noexcept
    {
        m_offset = 0;
    }

    size_t size() const noexcept
    {
        return m_size;
    }

    void resize(size_t size)
    {
        m_buffer = std::make_unique<char[]>(size); // Throws
        m_size = size;
        m_offset = 0;
    }

private:
    size_t m_size = 0, m_offset = 0;
    std::unique_ptr<char[]> m_buffer;
};


/// compress_bound() calculates an upper bound on the size of the compressed
/// data. The caller can use this function to allocate memory buffer calling
/// compress(). \a uncompressed_buf is the buffer with uncompresed data. The
/// size of the uncompressed data is \a uncompressed_size. \a compression_level
/// is described under compress(). \a bound is set to the upper bound at
/// return. The returned error code is of category compression::error_category.
std::error_code compress_bound(const char* uncompressed_buf, size_t uncompressed_size,
                               size_t& bound, int compression_level = 1);

/// compress() compresses the data in the \a uncompressed_buf of size \a
/// uncompressed_size into \a compressed_buf. compress() resizes \a
/// compressed_buf. At return, \a compressed_buf has the size of the compressed
/// data. \a compression_level is [1-9] with 1 the fastest for the current zlib
/// implementation. The returned error code is of category
/// compression::error_category.
std::error_code compress(const char* uncompressed_buf, size_t uncompressed_size,
                         char* compressed_buf, size_t compressed_buf_size,
                         size_t& compressed_size, int compression_level = 1,
                         Alloc* custom_allocator = nullptr);

/// decompress() decompresses the data in \param compressed_buf of size \a
/// compresed_size into \a decompressed_buf. \a decompressed_size is the
/// expected size of the decompressed data. \a decompressed_buf must have size
/// at least \a decompressed_size. decompress() throws on errors, including the
/// error where the size of the decompressed data is unequal to
/// decompressed_size.  The returned error code is of category
/// compression::error_category.
std::error_code decompress(const char* compressed_buf, size_t compressed_size,
                           char* decompressed_buf, size_t decompressed_size);


size_t allocate_and_compress(CompressMemoryArena& compress_memory_arena,
                             BinaryData uncompressed_buf,
                             std::vector<char>& compressed_buf);



} // namespace compression
} // namespace util
} // namespace realm

#endif // REALM_UTIL_COMPRESSION_HPP
