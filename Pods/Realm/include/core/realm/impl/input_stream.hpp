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

#ifndef REALM_IMPL_INPUT_STREAM_HPP
#define REALM_IMPL_INPUT_STREAM_HPP

#include <algorithm>

#include <realm/binary_data.hpp>
#include <realm/impl/continuous_transactions_history.hpp>
#include <realm/util/buffer.hpp>


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

    virtual ~InputStream() noexcept
    {
    }
};


class SimpleInputStream : public InputStream {
public:
    SimpleInputStream(const char* data, size_t size) noexcept
        : m_ptr(data)
        , m_end(data + size)
    {
    }
    size_t read(char* buffer, size_t size) override
    {
        size_t n = std::min(size, size_t(m_end - m_ptr));
        const char* begin = m_ptr;
        m_ptr += n;
        realm::safe_copy_n(begin, n, buffer);
        return n;
    }

private:
    const char* m_ptr;
    const char* const m_end;
};


class NoCopyInputStream {
public:
    /// \return if any bytes was read.
    /// A value of false indicates end-of-input.
    /// If return value is true, \a begin and \a end are
    /// updated to reflect the start and limit of a
    /// contiguous memory chunk.
    virtual bool next_block(const char*& begin, const char*& end) = 0;

    virtual ~NoCopyInputStream() noexcept
    {
    }
};


class NoCopyInputStreamAdaptor : public NoCopyInputStream {
public:
    NoCopyInputStreamAdaptor(InputStream& in, char* buffer, size_t buffer_size) noexcept
        : m_in(in)
        , m_buffer(buffer)
        , m_buffer_size(buffer_size)
    {
    }
    bool next_block(const char*& begin, const char*& end) override
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


class SimpleNoCopyInputStream : public NoCopyInputStream {
public:
    SimpleNoCopyInputStream(const char* data, size_t size)
        : m_data(data)
        , m_size(size)
    {
    }

    bool next_block(const char*& begin, const char*& end) override
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

class MultiLogNoCopyInputStream : public NoCopyInputStream {
public:
    MultiLogNoCopyInputStream(const BinaryData* logs_begin, const BinaryData* logs_end)
        : m_logs_begin(logs_begin)
        , m_logs_end(logs_end)
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
                realm::safe_copy_n(data, size_2, buffer);
                return size_2;
            }

            ++m_logs_begin;
            if (m_logs_begin == m_logs_end)
                return 0;
            m_curr_buf_remaining_size = m_logs_begin->size();
        }
    }

    bool next_block(const char*& begin, const char*& end) override
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


class ChangesetInputStream : public NoCopyInputStream {
public:
    using version_type = History::version_type;
    static constexpr unsigned NB_BUFFERS = 8;

    ChangesetInputStream(History& hist, version_type begin_version, version_type end_version)
        : m_history(hist)
        , m_begin_version(begin_version)
        , m_end_version(end_version)
    {
        get_changeset();
    }

    bool next_block(const char*& begin, const char*& end) override
    {
        while (m_valid) {
            BinaryData actual = m_changesets_begin->get_next();

            if (actual.size() > 0) {
                begin = actual.data();
                end = actual.data() + actual.size();
                return true;
            }

            m_changesets_begin++;

            if (REALM_UNLIKELY(m_changesets_begin == m_changesets_end)) {
                get_changeset();
            }
        }
        return false; // End of input
    }

private:
    History& m_history;
    version_type m_begin_version, m_end_version;
    BinaryIterator m_changesets[NB_BUFFERS]; // Buffer
    BinaryIterator* m_changesets_begin = nullptr;
    BinaryIterator* m_changesets_end = nullptr;
    bool m_valid;

    void get_changeset()
    {
        auto versions_to_get = m_end_version - m_begin_version;
        m_valid = versions_to_get > 0;
        if (m_valid) {
            if (versions_to_get > NB_BUFFERS)
                versions_to_get = NB_BUFFERS;
            version_type end_version = m_begin_version + versions_to_get;
            m_history.get_changesets(m_begin_version, end_version, m_changesets);
            m_begin_version = end_version;
            m_changesets_begin = m_changesets;
            m_changesets_end = m_changesets_begin + versions_to_get;
        }
    }
};

} // namespace _impl
} // namespace realm

#endif // REALM_IMPL_INPUT_STREAM_HPP
