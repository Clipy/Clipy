////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
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

#ifndef REALM_OS_SYNC_CLIENT_HPP
#define REALM_OS_SYNC_CLIENT_HPP

#include <realm/sync/client.hpp>

#include <thread>

namespace realm {
namespace _impl {

using Reconnect = sync::Client::Reconnect;

struct SyncClient {
    sync::Client client;

    SyncClient(std::unique_ptr<util::Logger> logger,
               std::function<sync::Client::ErrorHandler> handler,
               Reconnect reconnect_mode = Reconnect::normal,
               bool verify_ssl = true)
    : client(make_client(*logger, reconnect_mode, verify_ssl)) // Throws
    , m_logger(std::move(logger))
    , m_thread([this, handler=std::move(handler)] {
        client.set_error_handler(std::move(handler));
        client.run();
    }) // Throws
    {
    }

    ~SyncClient()
    {
        client.stop();
        m_thread.join();
    }

private:
    static sync::Client make_client(util::Logger& logger, Reconnect reconnect_mode, bool verify_ssl)
    {
        sync::Client::Config config;
        config.logger = &logger;
        config.reconnect = std::move(reconnect_mode);
        config.verify_servers_ssl_certificate = verify_ssl;
        return sync::Client(std::move(config)); // Throws
    }

    const std::unique_ptr<util::Logger> m_logger;
    std::thread m_thread;
};

}
}

#endif // REALM_OS_SYNC_CLIENT_HPP
