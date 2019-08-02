////////////////////////////////////////////////////////////////////////////
//
// Copyright 2015 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#include "impl/external_commit_helper.hpp"
#include "impl/realm_coordinator.hpp"
#include "util/fifo.hpp"

#include <realm/group_shared_options.hpp>

#include <asl.h>
#include <assert.h>
#include <fcntl.h>
#include <sstream>
#include <sys/event.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <system_error>
#include <unistd.h>

using namespace realm;
using namespace realm::_impl;

namespace {
// Write a byte to a pipe to notify anyone waiting for data on the pipe
void notify_fd(int fd, int read_fd)
{
    while (true) {
        char c = 0;
        ssize_t ret = write(fd, &c, 1);
        if (ret == 1) {
            break;
        }

        // If the pipe's buffer is full, we need to read some of the old data in
        // it to make space. We don't just read in the code waiting for
        // notifications so that we can notify multiple waiters with a single
        // write.
        assert(ret == -1 && errno == EAGAIN);
        char buff[1024];
        read(read_fd, buff, sizeof buff);
    }
}

} // anonymous namespace

void ExternalCommitHelper::FdHolder::close()
{
    if (m_fd != -1) {
        ::close(m_fd);
    }
    m_fd = -1;
}

// Inter-thread and inter-process notifications of changes are done using a
// named pipe in the filesystem next to the Realm file. Everyone who wants to be
// notified of commits waits for data to become available on the pipe, and anyone
// who commits a write transaction writes data to the pipe after releasing the
// write lock. Note that no one ever actually *reads* from the pipe: the data
// actually written is meaningless, and trying to read from a pipe from multiple
// processes at once is fraught with race conditions.

// When a RLMRealm instance is created, we add a CFRunLoopSource to the current
// thread's runloop. On each cycle of the run loop, the run loop checks each of
// its sources for work to do, which in the case of CFRunLoopSource is just
// checking if CFRunLoopSourceSignal has been called since the last time it ran,
// and if so invokes the function pointer supplied when the source is created,
// which in our case just invokes `[realm handleExternalChange]`.

// Listening for external changes is done using kqueue() on a background thread.
// kqueue() lets us efficiently wait until the amount of data which can be read
// from one or more file descriptors has changed, and tells us which of the file
// descriptors it was that changed. We use this to wait on both the shared named
// pipe, and a local anonymous pipe. When data is written to the named pipe, we
// signal the runloop source and wake up the target runloop, and when data is
// written to the anonymous pipe the background thread removes the runloop
// source from the runloop and and shuts down.
ExternalCommitHelper::ExternalCommitHelper(RealmCoordinator& parent)
: m_parent(parent)
{
    m_kq = kqueue();
    if (m_kq == -1) {
        throw std::system_error(errno, std::system_category());
    }

#if !TARGET_OS_TV


    // Object Store needs to create a named pipe in order to coordinate notifications.
    // This can be a problem on some file systems (e.g. FAT32) or due to security policies in SELinux. Most commonly
    // it is a problem when saving Realms on external storage: https://stackoverflow.com/questions/2740321/how-to-create-named-pipe-mkfifo-in-android
    //
    // For this reason we attempt to create this file in a temporary location known to be safe to write these files.
    //
    // In order of priority we attempt to write the file in the following locations:
    //  1) Next to the Realm file itself
    //  2) A location defined by `Realm::Config::fifo_files_fallback_path`
    //  3) A location defined by `SharedGroupOptions::set_sys_tmp_dir()`
    //
    // Core has a similar policy for its named pipes.
    //
    // Also see https://github.com/realm/realm-java/issues/3140
    // Note that hash collisions are okay here because they just result in doing extra work instead of resulting
    // in correctness problems.

    std::string path;
    std::string temp_dir = util::normalize_dir(parent.get_config().fifo_files_fallback_path);
    std::string sys_temp_dir = util::normalize_dir(SharedGroupOptions::get_sys_tmp_dir());

    path = parent.get_path() + ".note";
    bool fifo_created = util::try_create_fifo(path);
    if (!fifo_created && !temp_dir.empty()) {
        path = util::format("%1realm_%2.note", temp_dir, std::hash<std::string>()(parent.get_path()));
        fifo_created = util::try_create_fifo(path);
    }
    if (!fifo_created && !sys_temp_dir.empty()) {
        path = util::format("%1realm_%2.note", sys_temp_dir, std::hash<std::string>()(parent.get_path()));
        util::create_fifo(path);
    }

    m_notify_fd = open(path.c_str(), O_RDWR);
    if (m_notify_fd == -1) {
        throw std::system_error(errno, std::system_category());
    }

    // Make writing to the pipe return -1 when the pipe's buffer is full
    // rather than blocking until there's space available
    int ret = fcntl(m_notify_fd, F_SETFL, O_NONBLOCK);
    if (ret == -1) {
        throw std::system_error(errno, std::system_category());
    }

#else // !TARGET_OS_TV

    // tvOS does not support named pipes, so use an anonymous pipe instead
    int notification_pipe[2];
    int ret = pipe(notification_pipe);
    if (ret == -1) {
        throw std::system_error(errno, std::system_category());
    }

    m_notify_fd = notification_pipe[0];
    m_notify_fd_write = notification_pipe[1];

#endif // TARGET_OS_TV

    // Create the anonymous pipe for shutdown notifications
    int shutdown_pipe[2];
    ret = pipe(shutdown_pipe);
    if (ret == -1) {
        throw std::system_error(errno, std::system_category());
    }

    m_shutdown_read_fd = shutdown_pipe[0];
    m_shutdown_write_fd = shutdown_pipe[1];

    m_thread = std::thread([=] {
        try {
            listen();
        }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        catch (std::exception const& e) {
            fprintf(stderr, "uncaught exception in notifier thread: %s: %s\n", typeid(e).name(), e.what());
            asl_log(nullptr, nullptr, ASL_LEVEL_ERR, "uncaught exception in notifier thread: %s: %s", typeid(e).name(), e.what());
            throw;
        }
        catch (...) {
            fprintf(stderr,  "uncaught exception in notifier thread\n");
            asl_log(nullptr, nullptr, ASL_LEVEL_ERR, "uncaught exception in notifier thread");
            throw;
        }
#pragma clang diagnostic pop
    });
}

ExternalCommitHelper::~ExternalCommitHelper()
{
    notify_fd(m_shutdown_write_fd, m_shutdown_read_fd);
    m_thread.join(); // Wait for the thread to exit
}

void ExternalCommitHelper::listen()
{
    pthread_setname_np("Realm notification listener");

    // Set up the kqueue
    // EVFILT_READ indicates that we care about data being available to read
    // on the given file descriptor.
    // EV_CLEAR makes it wait for the amount of data available to be read to
    // change rather than just returning when there is any data to read.
    struct kevent ke[2];
    EV_SET(&ke[0], m_notify_fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, 0);
    EV_SET(&ke[1], m_shutdown_read_fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, 0);
    int ret = kevent(m_kq, ke, 2, nullptr, 0, nullptr);
    assert(ret == 0);

    while (true) {
        struct kevent event;
        // Wait for data to become on either fd
        // Return code is number of bytes available or -1 on error
        ret = kevent(m_kq, nullptr, 0, &event, 1, nullptr);
        if (ret == 0 || (ret < 0 && errno == EINTR)) {
            // Spurious wakeup; just wait again
            continue;
        }
        assert(ret > 0);

        // Check which file descriptor had activity: if it's the shutdown
        // pipe, then someone called -stop; otherwise it's the named pipe
        // and someone committed a write transaction
        if (event.ident == (uint32_t)m_shutdown_read_fd) {
            return;
        }
        assert(event.ident == (uint32_t)m_notify_fd);

        m_parent.on_change();
    }
}

void ExternalCommitHelper::notify_others()
{
    if (m_notify_fd_write != -1) {
        notify_fd(m_notify_fd_write, m_notify_fd);
    }
    else {
        notify_fd(m_notify_fd, m_notify_fd);
    }
}
