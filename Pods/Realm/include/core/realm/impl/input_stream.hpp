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

#ifndef REALM_IMPL_INPUT_STREAM_HPP
#define REALM_IMPL_INPUT_STREAM_HPP

#include <algorithm>

#include <realm/binary_data.hpp>
#include <realm/impl/continuous_transactions_history.hpp>


namespace realm {
namespace _impl {


class InputStream {
public:
    /// Read bytes from this input stream and place them in the specified
    /// buffer. The returned value is the actual number of bytes that were read,
    /// and this is some number `n` such that `n <= min(size, m)` where `m` is
    /// the number of bytes that could have been read from this stream before
    /// reaching its end. Also, `n` cannot be zero unless `m` or `size` is
    /// zero. The intention is that `size` should be non-zero, a the return
    /// value used as the end-of-input indicator.
    ///
    /// Implementations are only allowed to block (put the calling thread to
    /// sleep) up until the point in time where the first byte can be made
    /// availble.
    virtual size_t read(char* buffer, size_t size) = 0;

    virtual ~InputStream() noexcept {}
};


class SimpleInputStream: public InputStream {
public:
    SimpleInputStream(const char* data, size_t size) noexcept:
        m_ptr(data),
        m_end(data + size)
    {
    }
    size_t read(char* buffer, size_t size) override
    {
        size_t n = std::min(size, size_t(m_end-m_ptr));
        const char* begin = m_ptr;
        m_ptr += n;
        const char* end = m_ptr;
        std::copy(begin, end, buffer);
        return n;
    }
private:
    const char* m_ptr;
    const char* const m_end;
};


class NoCopyInputStream {
public:
    /// \return the number of accessible bytes.
    /// A value of zero indicates end-of-input.
    /// For non-zero return value, \a begin and \a end are
    /// updated to reflect the start and limit of a
    /// contiguous memory chunk.
    virtual size_t next_block(const char*& begin, const char*& end) = 0;

    virtual ~NoCopyInputStream() noexcept {}
};


class NoCopyInputStreamAdaptor: public NoCopyInputStream {
public:
    NoCopyInputStreamAdaptor(InputStream& in, char* buffer, size_t buffer_size) noexcept:
        m_in(in),
        m_buffer(buffer),
        m_buffer_size(buffer_size)
    {
    }
    size_t next_block(const char*& begin, const char*& end) override
    {
        size_t n = m_in.read(m_buffer, m_buffer_size);
        begin = m_buffer;
        end = m_buffer + n;
        return n;
    }
private:
    InputStream& m_in;
    char* m_buffer;
    size_t m_buffer_size;
};


class SimpleNoCopyInputStream: public NoCopyInputStream {
public:
    SimpleNoCopyInputStream(const char* data, size_t size):
        m_data(data),
        m_size(size)
    {
    }

    size_t next_block(const char*& begin, const char*& end) override
    {
        if (m_size == 0)
            return 0;
        size_t size = m_size;
        begin = m_data;
        end = m_data + size;
        m_size = 0;
        return size;
    }

private:
    const char* m_data;
    size_t m_size;
};

class MultiLogNoCopyInputStream: public NoCopyInputStream {
public:
    MultiLogNoCopyInputStream(const BinaryData* logs_begin, const BinaryData* logs_end):
        m_logs_begin(logs_begin), m_logs_end(logs_end)
    {
        if (m_logs_begin != m_logs_end)
            m_curr_buf_remaining_size = m_logs_begin->size();
    }

    size_t read(char* buffer, size_t size)
    {
        if (m_logs_begin == m_logs_end)
            return 0;
        for (;;) {
            if (m_curr_buf_remaining_size > 0) {
                size_t offset = m_logs_begin->size() - m_curr_buf_remaining_size;
                const char* data = m_logs_begin->data() + offset;
                size_t size_2 = std::min(m_curr_buf_remaining_size, size);
                m_curr_buf_remaining_size -= size_2;
                // FIXME: Eliminate the need for copying by changing the API of
                // Replication::InputStream such that blocks can be handed over
                // without copying. This is a straight forward change, but the
                // result is going to be more complicated and less conventional.
                std::copy(data, data + size_2, buffer);
                return size_2;
            }

            ++m_logs_begin;
            if (m_logs_begin == m_logs_end)
                return 0;
            m_curr_buf_remaining_size = m_logs_begin->size();
        }
    }

    size_t next_block(const char*& begin, const char*& end) override
    {
        while (m_logs_begin < m_logs_end) {
            size_t result = m_logs_begin->size();
            const char* data = m_logs_begin->data();
            m_logs_begin++;
            if (result == 0)
                continue; // skip empty blocks
            begin = data;
            end = data + result;
            return result;
        }
        return 0;
    }

private:
    const BinaryData* m_logs_begin;
    const BinaryData* m_logs_end;
    size_t m_curr_buf_remaining_size;
};


class ChangesetInputStream: public NoCopyInputStream {
public:
    using version_type = History::version_type;
    ChangesetInputStream(History&, version_type begin_version, version_type end_version);
    size_t next_block(const char*& begin, const char*& end) override;
private:
    History& m_history;
    version_type m_begin_version, m_end_version;
    BinaryData m_changesets[8]; // Buffer
    BinaryData* m_changesets_begin = 0;
    BinaryData* m_changesets_end = 0;
};


inline ChangesetInputStream::ChangesetInputStream(History& hist, version_type begin_version,
                                                  version_type end_version):
    m_history(hist),
    m_begin_version(begin_version),
    m_end_version(end_version)
{
}

inline size_t ChangesetInputStream::next_block(const char*& begin, const char*& end)
{
    for (;;) {
        if (REALM_UNLIKELY(m_changesets_begin == m_changesets_end)) {
            if (m_begin_version == m_end_version)
                return 0; // End of input
            version_type n = sizeof m_changesets / sizeof m_changesets[0];
            version_type avail = m_end_version - m_begin_version;
            if (n > avail)
                n = avail;
            version_type end_version = m_begin_version + n;
            m_history.get_changesets(m_begin_version, end_version, m_changesets);
            m_begin_version = end_version;
            m_changesets_begin = m_changesets;
            m_changesets_end = m_changesets_begin + n;
        }

        BinaryData changeset = *m_changesets_begin++;
        if (changeset.size() > 0) {
            begin = changeset.data();
            end   = changeset.data() + changeset.size();
            return changeset.size();
        }
    }
}


} // namespace _impl
} // namespace realm

#endif // REALM_IMPL_INPUT_STREAM_HPP
