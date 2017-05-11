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

#ifndef REALM_UTIL_WEBSOCKET_HPP
#define REALM_UTIL_WEBSOCKET_HPP

#include <random>
#include <system_error>
#include <map>

#include <realm/util/logger.hpp>
#include <realm/util/http.hpp>


namespace realm {
namespace util {
namespace websocket {

using WriteCompletionHandler =
    std::function<void(std::error_code, size_t num_bytes_transferred)>;
using ReadCompletionHandler =
    std::function<void(std::error_code, size_t num_bytes_transferred)>;

class Config {
public:

    /// The Socket uses the caller supplied logger for logging.
    virtual util::Logger& get_logger() = 0;

    /// The Socket needs random numbers to satisfy the Websocket protocol.
    /// The caller must supply a random number generator.
    virtual std::mt19937_64& get_random() = 0;

    /// The three functions below are used by the Socket to read and write to the underlying
    /// stream. The functions will typically be implemented as wrappers to a TCP/TLS stream,
    /// but could also map to pure memory streams. These functions abstract away the details of
    /// the underlying sockets.
    /// The functions have the same semantics as util::Socket.
    virtual void async_write(const char* data, size_t size, WriteCompletionHandler handler) = 0;
    virtual void async_read(char* buffer, size_t size, ReadCompletionHandler handler) = 0;
    virtual void async_read_until(char* buffer, size_t size, char delim, ReadCompletionHandler handler) = 0;


    /// websocket_handshake_completion_handler() is called when the websocket is connected, .i.e.
    /// after the handshake is done. It is not allowed to send messages on the socket before the
    /// handshake is done. No message_received callbacks will be called before the handshake is done.
    virtual void websocket_handshake_completion_handler(const HTTPHeaders&) = 0;

    //@{
    /// websocket_read_error_handler() and websocket_write_error_handler() are called when an
    /// error occurs on the underlying stream given by the async_read and async_write functions above.
    /// The error_code is passed through.
    /// websocket_protocol_error_handler() is called when there is an protocol error in the incoming
    /// messages.
    /// After calling any of these error callbacks, the Socket will move into the stopped state, and
    /// no more messages should be sent, or will be received.
    /// It is safe to destroy the WebSocket object in these handlers.
    virtual void websocket_read_error_handler(std::error_code) = 0;
    virtual void websocket_write_error_handler(std::error_code) = 0;
    virtual void websocket_protocol_error_handler(std::error_code) = 0;
    //@}

    //@{
    /// The five callback functions below are called whenever a full message has arrived.
    /// The Socket defragments fragmented messages internally and delivers a full message.
    /// The message is delivered in the buffer \param data of size \param size.
    /// The buffer is only valid until the function returns.
    /// The return value designates whether the WebSocket object should continue
    /// processing messages. The normal return value is true. False must be returned if the
    /// websocket object is destroyed during execution of the function.
    virtual bool websocket_text_message_received(const char* data, size_t size);
    virtual bool websocket_binary_message_received(const char* data, size_t size);
    virtual bool websocket_close_message_received(const char* data, size_t size);
    virtual bool websocket_ping_message_received(const char* data, size_t size);
    virtual bool websocket_pong_message_received(const char* data, size_t size);
    //@}
};


enum class Opcode {
    continuation =  0,
    text         =  1,
    binary       =  2,
    close        =  8,
    ping         =  9,
    pong         = 10
};


class Socket {
public:
    Socket(Config&);
    Socket(Socket&&) noexcept;
    ~Socket() noexcept;

    /// initiate_client_handshake() starts the Socket in client mode. The Socket
    /// will send the HTTP request that initiates the WebSocket protocol and
    /// wait for the HTTP response from the server. The HTTP request will
    /// contain the \param request_uri in the HTTP request line. The \param host
    /// will be sent as the value in a HTTP Host header line. Extra HTTP headers
    /// can be provided in \a headers.
    ///
    /// When the server responds with a valid HTTP response, the callback
    /// function websocket_handshake_completion_handler() is called. Messages
    /// can only be sent and received after the handshake has completed.
    void initiate_client_handshake(std::string request_uri, std::string host,
                                   HTTPHeaders headers = HTTPHeaders{});

    /// initiate_server_handshake() starts the Socket in server mode. It will
    /// wait for a HTTP request from a client and respond with a HTTP response.
    /// After sending a HTTP response, websocket_handshake_completion_handler()
    /// is called. Messages can only be sent and received after the handshake
    /// has completed.
    void initiate_server_handshake();

    /// initiate_server_websocket_after_handshake() starts the Socket in a state
    /// where it will read and write WebSocket messages but it will expect the
    /// handshake to have been completed by the caller. The use of this
    /// function is to perform HTTP routing externally and then start the
    /// WebSocket in case the HTTP request is an Upgrade to WebSocket.
    /// Typically, the caller will have used make_http_response() to send the
    /// HTTP response itself.
    void initiate_server_websocket_after_handshake();

    /// The async_write_* functions send frames. Only one frame should be sent at a time,
    /// meaning that the user must wait for the handler to be called before sending the next frame.
    /// The handler is type std::function<void()> and is called when the frame has been successfully
    /// sent. In case of errors, the Config::websocket_write_error_handler() is called.

    /// async_write_frame() sends a single frame with the fin bit set to 0 or 1 from \param fin, and the opcode
    /// set by \param opcode. The frame payload is taken from \param data of size \param size. \param handler is
    /// called when the frame has been successfully sent. Error s are reported through
    /// websocket_write_error_handler() in Config.
    /// This function is rather low level and should only be used with knowledge of the WebSocket protocol.
    /// The five utility functions below are recommended for message sending.
    void async_write_frame(bool fin, Opcode opcode, const char* data, size_t size, std::function<void()> handler);

    /// Five utility functions used to send whole messages. These five functions are implemented in terms of
    /// async_write_frame(). These functions send whole unfragmented messages. These functions should be
    /// preferred over async_write_frame() for most use cases.
    void async_write_text(const char* data, size_t size, std::function<void()> handler);
    void async_write_binary(const char* data, size_t size, std::function<void()> handler);
    void async_write_close(const char* data, size_t size, std::function<void()> handler);
    void async_write_ping(const char* data, size_t size, std::function<void()> handler);
    void async_write_pong(const char* data, size_t size, std::function<void()> handler);

    /// stop() stops the socket. The socket will stop processing incoming data, sending data, and calling callbacks.
    /// It is an error to attempt to send a message after stop() has been called. stop() will typically be called
    /// before the underlying TCP/TLS connection is closed. The Socket can be restarted with
    /// initiate_client_handshake() and initiate_server_handshake().
    void stop() noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

/// make_http_response() takes \a request as a WebSocket handshake request,
/// validates it, and makes a HTTP response. If the request is invalid, the
/// return value is None, and ec is set to Error::bad_handshake_request.
util::Optional<HTTPResponse> make_http_response(const HTTPRequest& request,
        std::error_code& ec);

enum class Error {
    bad_handshake_request  = 1, ///< Bad WebSocket handshake response received
    bad_handshake_response = 2, ///< Bad WebSocket handshake response received
    bad_message            = 3, ///< Ill-formed WebSocket message
};

const std::error_category& error_category() noexcept;

std::error_code make_error_code(Error) noexcept;

} // namespace websocket
} // namespace util
} // namespace realm

namespace std {

template<> struct is_error_code_enum<realm::util::websocket::Error> {
    static const bool value = true;
};

} // namespace std

#endif // REALM_UTIL_WEBSOCKET_HPP
