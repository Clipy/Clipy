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

#ifndef REALM_UTIL_HTTP_HPP
#define REALM_UTIL_HTTP_HPP

#include <map>
#include <system_error>
#include <iosfwd>

#include <realm/util/optional.hpp>
#include <realm/util/network.hpp>
#include <realm/string_data.hpp>

namespace realm {
namespace util {
enum class HTTPParserError {
    None = 0,
    ContentTooLong,
    HeaderLineTooLong,
    MalformedResponse,
    MalformedRequest,
};
std::error_code make_error_code(HTTPParserError);
} // namespace util
} // namespace realm

namespace std {
    template<>
    struct is_error_code_enum<realm::util::HTTPParserError>: std::true_type {};
}

namespace realm {
namespace util {

/// See: https://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
///
/// It is guaranteed that the backing integer value of this enum corresponds
/// to the numerical code representing the status.
enum class HTTPStatus {
    Unknown = 0,

    Continue = 100,
    SwitchingProtocols = 101,

    Ok = 200,
    Created = 201,
    Accepted = 202,
    NonAuthoritative = 203,
    NoContent = 204,
    ResetContent = 205,
    PartialContent = 206,

    MultipleChoices = 300,
    MovedPermanently = 301,
    Found = 302,
    SeeOther = 303,
    NotModified = 304,
    UseProxy = 305,
    SwitchProxy = 306,
    TemporaryRedirect = 307,
    PermanentRedirect = 308,

    BadRequest = 400,
    Unauthorized = 401,
    PaymentRequired = 402,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    NotAcceptable = 406,
    ProxyAuthenticationRequired = 407,
    RequestTimeout = 408,
    Conflict = 409,
    Gone = 410,
    LengthRequired = 411,
    PreconditionFailed = 412,
    PayloadTooLarge = 413,
    UriTooLong = 414,
    UnsupportedMediaType = 415,
    RangeNotSatisfiable = 416,
    ExpectationFailed = 417,
    ImATeapot = 418,
    MisdirectedRequest = 421,
    UpgradeRequired = 426,
    PreconditionRequired = 428,
    TooManyRequests = 429,
    RequestHeaderFieldsTooLarge = 431,
    UnavailableForLegalReasons = 451,

    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    ServiceUnavailable = 503,
    GatewayTimeout = 504,
    HttpVersionNotSupported = 505,
    VariantAlsoNegotiates = 506,
    NotExtended = 510,
    NetworkAuthenticationRequired = 511,
};

/// See: https://www.w3.org/Protocols/rfc2616/rfc2616-sec9.html
enum class HTTPMethod {
    Options,
    Get,
    Head,
    Post,
    Put,
    Delete,
    Trace,
    Connect,
};

struct CaseInsensitiveCompare {
    bool operator()(const std::string& a, const std::string& b) const
    {
        auto cmp = [](char lhs, char rhs) {
            return std::tolower(lhs, std::locale::classic()) <
                   std::tolower(rhs, std::locale::classic());
        };
        return std::lexicographical_compare(begin(a), end(a), begin(b), end(b), cmp);
    }
};

/// Case-insensitive map suitable for storing HTTP headers.
using HTTPHeaders = std::map<std::string, std::string, CaseInsensitiveCompare>;

struct HTTPRequest {
    HTTPMethod method = HTTPMethod::Get;
    HTTPHeaders headers;
    std::string path;

    /// If the request object has a body, the Content-Length header MUST be
    /// set to a string representation of the number of bytes in the body.
    /// FIXME: Relax this restriction, and also support Transfer-Encoding
    /// and other HTTP/1.1 features.
    Optional<std::string> body;
};

struct HTTPResponse {
    HTTPStatus status = HTTPStatus::Unknown;
    HTTPHeaders headers;

    // A body is only read from the response stream if the server sent the
    // Content-Length header.
    // FIXME: Support other transfer methods, including Transfer-Encoding and
    // HTTP/1.1 features.
    Optional<std::string> body;
};


/// Serialize HTTP request to output stream.
std::ostream& operator<<(std::ostream&, const HTTPRequest&);
/// Serialize HTTP response to output stream.
std::ostream& operator<<(std::ostream&, const HTTPResponse&);
/// Serialize HTTP method to output stream ("GET", "POST", etc.).
std::ostream& operator<<(std::ostream&, HTTPMethod);
/// Serialize HTTP status to output stream, include reason string ("200 OK" etc.)
std::ostream& operator<<(std::ostream&, HTTPStatus);

struct HTTPParserBase {
    // FIXME: Generally useful?
    struct CallocDeleter {
        void operator()(void* ptr)
        {
            std::free(ptr);
        }
    };

    HTTPParserBase()
    {
        // Allocating read buffer with calloc to avoid accidentally spilling
        // data from other sessions in case of a buffer overflow exploit.
        m_read_buffer.reset(static_cast<char*>(std::calloc(read_buffer_size, 1)));
    }
    virtual ~HTTPParserBase() {}

    std::string m_write_buffer;
    std::unique_ptr<char[], CallocDeleter> m_read_buffer;
    Optional<size_t> m_found_content_length;
    static const size_t read_buffer_size = 8192;
    static const size_t max_header_line_length = read_buffer_size;

    /// Parses the contents of m_read_buffer as a HTTP header line,
    /// and calls on_header() as appropriate. on_header() will be called at
    /// most once per invocation.
    /// Returns false if the contents of m_read_buffer is not a valid HTTP
    /// header line.
    bool parse_header_line(size_t len);

    virtual std::error_code on_first_line(StringData line) = 0;
    virtual void on_header(StringData key, StringData value) = 0;
    virtual void on_body(StringData body) = 0;
    virtual void on_complete(std::error_code = std::error_code{}) = 0;

    /// If the input matches a known HTTP method string, return the appropriate
    /// HTTPMethod enum value. Otherwise, returns none.
    static Optional<HTTPMethod> parse_method_string(StringData method);

    /// Interpret line as the first line of an HTTP request. If the return value
    /// is true, out_method and out_uri have been assigned the appropriate
    /// values found in the request line.
    static bool parse_first_line_of_request(StringData line, HTTPMethod& out_method,
                                            StringData& out_uri);

    /// Interpret line as the first line of an HTTP response. If the return
    /// value is true, out_status and out_reason have been assigned the
    /// appropriate values found in the response line.
    static bool parse_first_line_of_response(StringData line, HTTPStatus& out_status,
                                             StringData& out_reason);

    void set_write_buffer(const HTTPRequest&);
    void set_write_buffer(const HTTPResponse&);
};

template<class Socket>
struct HTTPParser: protected HTTPParserBase {
    explicit HTTPParser(Socket& socket) : m_socket(socket)
    {}

    void read_first_line()
    {
        auto handler = [this](std::error_code ec, size_t n) {
            if (ec == error::operation_aborted) {
                return;
            }
            if (ec) {
                on_complete(ec);
                return;
            }
            ec = on_first_line(StringData(m_read_buffer.get(), n));
            if (ec) {
                on_complete(ec);
                return;
            }
            read_headers();
        };
        m_socket.async_read_until(m_read_buffer.get(), max_header_line_length, '\n',
                                  std::move(handler));
    }

    void read_headers()
    {
        auto handler = [this](std::error_code ec, size_t n) {
            if (ec == error::operation_aborted) {
                return;
            }
            if (ec) {
                on_complete(ec);
                return;
            }
            if (n <= 2) {
                read_body();
                return;
            }
            parse_header_line(n);
            // FIXME: Limit the total size of headers. Apache uses 8K.
            read_headers();
        };
        m_socket.async_read_until(m_read_buffer.get(), max_header_line_length, '\n',
                                  std::move(handler));
    }

    void read_body()
    {
        if (m_found_content_length) {
            // FIXME: Support longer bodies.
            // FIXME: Support multipart and other body types (no body shaming).
            if (*m_found_content_length > read_buffer_size) {
                on_complete(HTTPParserError::ContentTooLong);
                return;
            }

            auto handler = [this](std::error_code ec, size_t n) {
                if (ec == error::operation_aborted) {
                    return;
                }
                if (!ec) {
                    on_body(StringData(m_read_buffer.get(), n));
                }
                on_complete(ec);
            };
            m_socket.async_read(m_read_buffer.get(), *m_found_content_length,
                                std::move(handler));
        }
        else {
            // No body, just finish.
            on_complete();
        }
    }

    void write_buffer(std::function<void(std::error_code, size_t)> handler)
    {
        m_socket.async_write(m_write_buffer.data(), m_write_buffer.size(),
                             std::move(handler));
    }

    Socket& m_socket;
};

template<class Socket>
struct HTTPClient: protected HTTPParser<Socket> {
    using Handler = void(HTTPResponse, std::error_code);

    explicit HTTPClient(Socket& socket) : HTTPParser<Socket>(socket) {}

    /// Serialize and send \a request over the connected socket asynchronously.
    ///
    /// When the response has been received, or an error occurs, \a handler will
    /// be invoked with the appropriate parameters. The HTTPResponse object
    /// passed to \a handler will only be complete in non-error conditions, but
    /// may be partially populated.
    ///
    /// It is an error to start a request before the \a handler of a previous
    /// request has been invoked. It is permitted to call async_request() from
    /// the handler, unless an error has been reported representing a condition
    /// where the underlying socket is no longer able to communicate (for
    /// example, if it has been closed).
    ///
    /// If a request is already in progress, an exception will be thrown.
    ///
    /// This method is *NOT* thread-safe.
    void async_request(const HTTPRequest& request, std::function<Handler> handler)
    {
        if (REALM_UNLIKELY(m_handler)) {
            throw std::runtime_error("Request already in progress.");
        }
        this->set_write_buffer(request);
        m_handler = std::move(handler);
        this->write_buffer([this](std::error_code ec, size_t bytes_written) {
            static_cast<void>(bytes_written);
            if (ec == error::operation_aborted) {
                return;
            }
            if (ec) {
                this->on_complete(ec);
                return;
            }
            this->read_first_line();
        });
    }

private:
    std::function<Handler> m_handler;
    HTTPResponse m_response;

    std::error_code on_first_line(StringData line) override final
    {
        HTTPStatus status;
        StringData reason;
        if (this->parse_first_line_of_response(line, status, reason)) {
            m_response.status = status;
            static_cast<void>(reason); // Ignore for now.
            return std::error_code{};
        }
        return HTTPParserError::MalformedResponse;
    }

    void on_header(StringData key, StringData value) override final
    {
        // FIXME: Multiple headers with the same key should show up as a
        // comma-separated list of their values, rather than overwriting.
        m_response.headers[std::string(key)] = std::string(value);
    }

    void on_body(StringData body) override final
    {
        m_response.body = std::string(body);
    }

    void on_complete(std::error_code ec) override final
    {
        auto handler = std::move(m_handler); // Nullifies m_handler
        handler(std::move(m_response), ec);
    }
};

template<class Socket>
struct HTTPServer: protected HTTPParser<Socket> {
    using RequestHandler = void(HTTPRequest, std::error_code);
    using RespondHandler = void(std::error_code);

    explicit HTTPServer(Socket& socket): HTTPParser<Socket>(socket)
    {}

    /// Receive a request on the underlying socket asynchronously.
    ///
    /// This function starts an asynchronous read operation and keeps reading
    /// until an HTTP request has been received. \a handler is invoked when a
    /// request has been received, or an error occurs.
    ///
    /// After a request is received, callers MUST invoke async_send_response()
    /// to provide the client with a valid HTTP response, unless the error
    /// passed to the handler represents a condition where the underlying socket
    /// is no longer able to communicate (for example, if it has been closed).
    ///
    /// It is an error to attempt to receive a request before any previous
    /// requests have been fully responded to, i.e. the \a handler argument of
    /// async_send_response() must have been invoked before attempting to
    /// receive the next request.
    ///
    /// This function is *NOT* thread-safe.
    void async_receive_request(std::function<RequestHandler> handler)
    {
        if (REALM_UNLIKELY(m_request_handler)) {
            throw std::runtime_error("Response already in progress.");
        }
        m_request_handler = std::move(handler);
        this->read_first_line();
    }

    /// Send an HTTP response to a client asynchronously.
    ///
    /// This function starts an asynchronous write operation on the underlying
    /// socket. \a handler is invoked when the response has been written to the
    /// socket, or an error occurs.
    ///
    /// It is an error to call async_receive_request() again before \a handler
    /// has been invoked, and it is an error to call async_send_response()
    /// before the \a handler of a previous invocation has been invoked.
    ///
    /// This function is *NOT* thread-safe.
    void async_send_response(const HTTPResponse& response,
                             std::function<RespondHandler> handler)
    {
        if (REALM_UNLIKELY(!m_request_handler)) {
            throw std::runtime_error("No request in progress.");
        }
        if (m_respond_handler) {
            // FIXME: Proper exception type.
            throw std::runtime_error("Already responding to request");
        }
        m_respond_handler = std::move(handler);
        this->set_write_buffer(response);
        this->write_buffer([this](std::error_code ec, size_t) {
            if (ec == error::operation_aborted) {
                return;
            }
            m_request_handler = nullptr;
            auto handler = std::move(m_respond_handler);
            handler(ec);
        });;
    }

private:
    std::function<RequestHandler> m_request_handler;
    std::function<RespondHandler> m_respond_handler;
    HTTPRequest m_request;

    std::error_code on_first_line(StringData line) override final
    {
        HTTPMethod method;
        StringData uri;
        if (this->parse_first_line_of_request(line, method, uri)) {
            m_request.method = method;
            m_request.path = uri;
            return std::error_code{};
        }
        return HTTPParserError::MalformedRequest;
    }

    void on_header(StringData key, StringData value) override final
    {
        // FIXME: Multiple headers with the same key should show up as a
        // comma-separated list of their values, rather than overwriting.
        m_request.headers[std::string(key)] = std::string(value);
    }

    void on_body(StringData body) override final
    {
        m_request.body = std::string(body);
    }

    void on_complete(std::error_code ec) override final
    {
        // Deliberately not nullifying m_request_handler so that we can
        // check for invariants in async_send_response.
        m_request_handler(std::move(m_request), ec);
    }
};

} // namespace util
} // namespace realm


#endif // REALM_UTIL_HTTP_HPP

