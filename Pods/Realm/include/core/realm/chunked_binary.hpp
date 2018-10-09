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

#ifndef REALM_NOINST_CHUNKED_BINARY_HPP
#define REALM_NOINST_CHUNKED_BINARY_HPP

#include <realm/binary_data.hpp>
#include <realm/column_binary.hpp>
#include <realm/table.hpp>

#include <realm/util/buffer_stream.hpp>
#include <realm/impl/input_stream.hpp>


namespace realm {

/// ChunkedBinaryData manages a vector of BinaryData. It is used to facilitate
/// extracting large binaries from binary columns and tables.
class ChunkedBinaryData {
public:

    ChunkedBinaryData();
    ChunkedBinaryData(const BinaryData& bd);
    ChunkedBinaryData(const BinaryIterator& bd);
    ChunkedBinaryData(const BinaryColumn& col, size_t index);

    /// size() returns the number of bytes in the chunked binary.
    /// FIXME: This operation is O(n).
    size_t size() const noexcept;

    /// is_null returns true if the chunked binary has zero chunks or if
    /// the first chunk points to the nullptr.
    bool is_null() const;

    /// FIXME: O(n)
    char operator[](size_t index) const;

    std::string hex_dump(const char* separator = " ", int min_digits = -1) const;

    void write_to(util::ResettableExpandableBufferOutputStream& out) const;

    /// copy_to() copies the chunked binary data to \a buffer of size
    /// \a buffer_size starting at \a offset in the ChunkedBinary.
    /// copy_to() copies until the end of \a buffer or the end of
    /// the ChunkedBinary whichever comes first.
    /// copy_to() returns the number of copied bytes.
    size_t copy_to(char* buffer, size_t buffer_size, size_t offset) const;

    /// copy_to() allocates a buffer of size() in \a dest and
    /// copies the chunked binary data to \a dest.
    size_t copy_to(std::unique_ptr<char[]>& dest) const;

    /// get_first_chunk() is used in situations
    /// where it is known that there is exactly one
    /// chunk. This is the case if the ChunkedBinary
    /// has been constructed from BinaryData.
    BinaryData get_first_chunk() const;

private:
    BinaryIterator m_begin;
    friend class ChunkedBinaryInputStream;
};

// FIXME: When ChunkedBinaryData is moved into Core, this should be moved as well.
class ChunkedBinaryInputStream : public _impl::NoCopyInputStream {
public:
    explicit ChunkedBinaryInputStream(const ChunkedBinaryData& chunks)
        : m_it(chunks.m_begin)
    {
    }

    bool next_block(const char*& begin, const char*& end) override
    {
        BinaryData block = m_it.get_next();
        begin = block.data();
        end = begin + block.size();
        return begin != end;
    }

private:
    BinaryIterator m_it;
};


/// Implementation:


inline ChunkedBinaryData::ChunkedBinaryData()
{
}

inline ChunkedBinaryData::ChunkedBinaryData(const BinaryData& bd) : m_begin{bd}
{
}

inline ChunkedBinaryData::ChunkedBinaryData(const BinaryIterator& bd) : m_begin{bd}
{
}

inline ChunkedBinaryData::ChunkedBinaryData(const BinaryColumn& col, size_t index)
    : m_begin{&col, index}
{
}


} // namespace realm

#endif // REALM_NOINST_CHUNKED_BINARY_HPP
