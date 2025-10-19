#ifndef HTTP_MESSAGE_H
#define HTTP_MESSAGE_H

#include <algorithm>
#include <any>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <unordered_map>
#include <uvent/net/Socket.h>
#include <uvent/system/SystemContext.h>
#include <variant>

// Local includes
#include "Components/Compression/CompressionBase.h"

// #include "Components/DataTypes/Application/UrlEncoded.h"
// #include "Components/DataTypes/Multipart/FormData.h"

// #include "Components/Headers/Headers.h"
#include "Components/URL/URL.h"
#include "Protocols/HTTP/Headers.h"
#include "utils/HTTPUtils/HTTPUtils.h"
#include "utils/utils.h"


namespace usub::server::protocols::http {

    /// Forward declarations to avoid circular dependencies
    class HTTPEndpointHandler;
    struct Route;

    /**
     * @enum VERSION
     * @brief Enumerates supported HTTP versions.
     */
    enum class VERSION : uint16_t {
        NONE = 0,    ///< No HTTP version specified.
        HTTP_0_9 = 1,///< HTTP/0.9.
        HTTP_1_0 = 2,///< HTTP/1.0.
        HTTP_1_1 = 3,///< HTTP/1.1.
        HTTP_2_0 = 4,///< HTTP/2.0.
        HTTP_3_0 = 5,///< HTTP/3.0.
        BROKEN = 6   ///< Indicates a broken or unsupported HTTP version.
    };

    /**
     * @enum REQUEST_STATE
     * @brief States for the HTTP request parsing state machine.
     *
     * The state machine handles parsing the request in multiple layers:
     * - States <= 11: Parsing headers or request line.
     * - 12 <= States <= 100: Reserved for parsing data.
     * - States >= 300: Represent error states.
     * - 100 < States < 300: Reserved for future use.
     *
     * @details The parser uses a SAX-like approach, where each state corresponds to a specific part of the HTTP request.
     */
    enum class REQUEST_STATE : uint16_t {
        // General states
        METHOD = 1,                 ///< Parsing the HTTP method.
        SCHEME = 2,                 ///< Parsing the URI scheme.
        AUTHORITY = 3,              ///< Parsing the authority component of the URI.
        TARGET_START = 4,           // Starting to parse the request target.
        ORIGIN_FORM = 5,            // Parsing the origin form (e.g., /path?query#fragment).
        ABSOLUTE_FORM_SCHEME = 6,   // Parsing the scheme in absolute form (e.g., http://).
        ABSOLUTE_FORM_AUTHORITY = 7,// Parsing the authority in absolute form (e.g., example.com:8042).
        PATH = 8,                   // Parsing the path component.
        QUERY_KEY = 9,              // Parsing the query parameter key.
        QUERY_VALUE = 10,           // Parsing the query parameter value.
        FRAGMENT = 11,              // Parsing the fragment.
        AUTHORITY_FORM = 12,        // Parsing the authority form (e.g., example.com:443).
        ASTERISK_FORM = 13,         // Parsing the asterisk form (e.g., *).
        VERSION = 14,               ///< Parsing the HTTP version.
        PRE_HEADERS = 15,           ///< Request Line or Pseudo Headers were parsed
        HEADERS_KEY = 16,           ///< Parsing the header key.
        HEADERS_VALUE = 17,         ///< Parsing the header value.
        HEADERS_PARSED = 18,        ///< Headers have been fully parsed.
        DATA_CONTENT_LENGTH = 19,   ///< Parsing the message body if Content-Length header was specified.
        DATA_CHUNKED_SIZE = 20,     ///< Parsing the dody if chunked transfer-encoding was specified.
        DATA_CHUNKED = 21,          ///< Parsing chunked data.
        DATA_FRAGMENT = 22,         ///< Parsing chunked data.
        FINISHED = 23,              ///< Parsing is complete.
        // SENDER STATES
        SENDING_CONTENT_LENGTH = 50,///< Sending the response with Content-Length.
        SENDING_CHUNKED = 51,       ///< Sending the response in chunks.
        SENT = 52,                  ///< Response has been sent.
        FAILED = 53,                ///< Response sending failed.
        // HTTP-specific error states
        BAD_REQUEST = 400,                    ///< 400 Bad Request.
        NOT_FOUND = 404,                      ///< 404 Not Found.
        METHOD_NOT_ALLOWED = 405,             ///< 405 Method Not Allowed.
        LENGTH_REQUIRED = 411,                ///< 411 Length Required.
        URI_TOO_LONG = 414,                   ///< 414 URI Too Long.
        UNSUPPORTED_MEDIA_TYPE = 415,         ///< 415 Unsupported Media Type.
        UNPROCESSABLE_ENTITY = 422,           ///< 422 Unprocessable Entity.
        REQUEST_HEADER_FIELDS_TOO_LARGE = 431,///< 431 Request Header Fields Too Large.
        ERROR = 1000,                         ///< Parsing encountered an error.
    };

    enum class RESPONSE_STATE : uint16_t {
        VERSION = 1,
        STATUS_CODE = 2,
        STATUS_MESSAGE = 3,
        HEADERS_KEY = 4,
        HEADERS_VALUE = 5,
        HEADERS_PARSED = 6,
        DATA_CONTENT_LENGTH = 7,
        DATA_CHUNKED_SIZE = 8,
        DATA_CHUNKED = 9,

        SENDING = 50,
        SENDING_CONTENT_LENGTH = 51,
        SENDING_CHUNKED = 52,
        SENT = 53,
    };

    struct HelperData {
        bool reciever{true};
        bool buffer_{true};
        bool add_metadata_{true};
        bool chunked_{false};
        size_t size_{0};
        size_t offset_{0};
        size_t chunk_size_{4096};
        size_t max_write_size_{4096 * 16};
    };


    /**
     * @brief Reserved for future use for now those vars are in separate vars in a class, which inflates it
     */
    struct RequestStatusLine {
        std::string method_token_{};
    };

    /**
     * @class Message
     * @brief Base class for HTTP Request and Response messages.
     *
     * This class encapsulates the common data and functionality shared between HTTP requests and responses.
     */
    class Message {
    protected:
        bool reciever_{true};
        /**
         * @brief HTTP version used in the message.
         */
        VERSION http_version_{};

        /**
         * @brief Indicates if the connection is secure (e.g., HTTPS).
         */
        bool secure{};

        /**
         * @brief Pointer to the matched route for optimized request processing.
         *
         * @details Avoids linear search for route matching by storing the matched route.
         */
        Route *matched_route_{nullptr};

        /**
         * @brief Socket associated with the message for sending responses.
         *
         * @details Used to send the response back through the same socket the request was received on.
         */
        usub::uvent::net::TCPClientSocket *socket_{};

        /**
         * @brief Headers of the HTTP message.
         */
        Headers headers_{};

        /**
         * @brief Temporary storage for partial data during parsing.
         *
         * @details Stores incomplete key-value pairs when data arrives in fragments.
         * Ensure to clear this after processing to avoid stale data.
         */
        std::pair<std::string, std::string> data_value_pair_{};

        std::string body_{};

        std::vector<std::shared_ptr<component::CompressionBase>> encryptors_chain{};

        // For sending files
        int fd_{-1};

        mutable HelperData helper_{};

    public:
        /**
         * @brief Retrieves a reference to the message headers.
         *
         * @return usub::server::component::Headers& Reference to the headers.
         */
        usub::server::protocols::http::Headers &getHeaders();

        /**
         * @brief Retrieves a reference to the message data as a vector of unsigned characters.
         *
         * @return std::vector<unsigned char>& Reference to the data vector.
         * 
         * @return const std::vector<unsigned char>& Constant reference to the data vector. (read-only)
         */
        std::vector<unsigned char> &getData();
        const std::vector<unsigned char> &getData() const;

        /**
         * @brief Retrieves a reference to the HTTP version used in the message.
         *
         * @return VERSION& Reference to the HTTP version.
         * 
         * @return const VERSION& Constant reference to the HTTP version. (read-only)
         * 
         * @see VERSION
         */
        VERSION &getHTTPVersion();
        const VERSION &getHTTPVersion() const;

        /**
         * @brief Sets the HTTP version used in the message.
         *
         * @param HTTPVersion The HTTP version to set.
         * @see VERSION
         */
        void setHTTPVersion(const VERSION &HTTPVersion);

        // Future implementations:
        // void addHeader(const std::string &key, const std::string &value);

        // Imitate std behavior
        // virtual std::string toString() const = 0;
        // virtual size_t size() const = 0;
    };

    /**
     * @class Request
     * @brief Represents an HTTP Request message.
     *
     * Inherits from `Message` and includes additional data and functionality specific to HTTP *requests*.
     */
    class Request : public Message {
    private:
        /**
         * @brief Helper flags for managing parsing states.
         *
         * @details Used to track carriage returns and newlines during parsing to avoid overpopulating the `REQUEST_STATE` enum.
         */
        bool carriage_return{}, newline{};

        /**
         * @brief Token representing the HTTP method used in the request.
         *
         * @details Utilized for route matching.
         */
        std::string method_token_{};

        /**
         * @brief URL object containing the path and query parameters.
         *
         * @see usub::server::component::URL
         */
        usub::server::component::URN urn_;

        /**
         * @brief authority or duplicate of Host header in HTTP1.1.
         *
         * @details Used for security purposes in TLS or the HTTP/2.0 protocol.
         */
        std::string authority_{};

        /**
         * @brief Current size of the line or data segment being parsed.
         */
        size_t line_size_{};

        const size_t max_uri_size_{8192};
        size_t max_headers_size_{16384};
        ssize_t max_data_size_{65536};

        /**
         * @brief Current state of the request parsing process.
         */
        REQUEST_STATE state_{REQUEST_STATE::METHOD};

    public:
        /**
         * @brief Map storing URI parameters extracted from the route.
         *
         * @details Parameters defined within `{}` brackets in the route (handler), similar to Spring Boot.
         * 
         * @see addHandler()
         */
        std::unordered_map<std::string, std::string> uri_params;
        mutable std::any user_data;

    public:
        /**
         * @brief Default constructor.
         */
        Request() = default;

        /**
         * @brief Default destructor.
         */
        ~Request() = default;

        /**
         * @brief Constructs the full URL with query parameters.
         *
         * @return std::string The complete URL.
         */
        std::string getFullURL();

        /**
         * @brief Retrieves the path component of the URL.
         *
         * @return std::string& Reference to the URL path.
         * 
         * @return const std::string& Constant reference to the URL path. (read-only)
         * 
         */
        std::string &getURL();
        const std::string &getURL() const;

        /**
         * @brief Retrieves the server name or authority.
         *
         * @return std::pair<std::string, std::string>& Reference to the server name pair.
         * 
         * @return const std::pair<std::string, std::string>& Constant reference to the server name pair. (read-only)
         *
         * @todo Implement this function.
         */
        std::pair<std::string, std::string> &getServerName();
        const std::pair<std::string, std::string> &getServerName() const;

        /**
         * @brief Retrieves the query parameters object.
         *
         * Provides access to the query parameters. The non-const version allows 
         * modification of the parameters, while the const version provides read-only access.
         * 
         * @return usub::server::component::url::QueryParams& 
         * Reference to the query parameters (modifiable).
         * 
         * @return const usub::server::component::url::QueryParams& 
         * Reference to the query parameters (read-only).
         */
        usub::server::component::url::QueryParams &getQueryParams();
        const usub::server::component::url::QueryParams &getQueryParams() const;

        /**
         * @brief Retrieves the HTTP method of the request.
         *
         * @return std::string& Reference to the HTTP method string.
         * 
         * @return const std::string& Constant reference to the HTTP method string. (read-only)
         * 
         */
        std::string &getRequestMethod();
        const std::string &getRequestMethod() const;

        /**
         * @brief Retrieves the current state of the request parsing.
         *
         * @return REQUEST_STATE& Reference to the current parsing state.
         * 
         * @return const REQUEST_STATE& Constant reference to the current parsing state. (read-only)
         * 
         */
        REQUEST_STATE &getState();
        const REQUEST_STATE &getState() const;

        /**
         * @brief Sets the URI of the request.
         *
         * @param uri The URI string to set.
         */
        void setUri(const std::string &uri);

        /**
         * @brief Sets the GET parameters of the request.
         *
         * @param getParams A multimap containing GET parameters.
         */
        void setGetParams(const std::multimap<std::string, std::string> &getParams);

        /**
         * @brief Sets the request type (e.g., GET, POST).
         *
         * @param requestType A string view representing the request type.
         */
        void setRequestType(std::string_view requestType);

        /**
         * @brief Sets the current state of the request parsing.
         *
         * @param state The new state to set.
         */
        void setState(const REQUEST_STATE &state);

        /**
         * @brief Sets the server name or authority.
         *
         * @param serverName A pair containing the server name and authority.
         */
        void setServerName(const std::pair<std::string, std::string> &serverName);

        /**
         * @brief Adds a GET parameter to the request.
         *
         * @param key The parameter key.
         * @param value The parameter value.
         */
        void addGetParam(const std::string &key, const std::string &value);

        /**
         * @brief Retrieves the message data as a string.
         *
         * @return std::string The data string.
         */
        std::string getBody();

        /**
         * @brief Parses the query parameters from the URI.
         *
         * @param query_params A string view containing the query parameters.
         * @return uint8_t Status code indicating success or failure.
         *
         * @deprecated This function is deleted and not implemented.
         */
        [[maybe_unused]] uint8_t parseQueryParams(std::string_view query_params) = delete;

        /**
         * @brief Parses an HTTP/0.9 request.
         *
         * @param request The HTTP request string.
         * @return std::string::const_iterator Iterator pointing to the parsing position.
         *
         * @deprecated HTTP/0.9 parsing is not implemented. HTTP/0.9 is not widely used. And introduces security vulnerabilities, overhead, and complexity.
         */
        [[maybe_unused]] std::string::const_iterator parseHttp0_9(const std::string &request) = delete;

        /**
         * @brief Parses an HTTP/1.0 request. Overwrites the version to HTTP/1.0.
         *
         * @param request The HTTP request string.
         * @param start_pos Optional iterator indicating the start position for parsing.
         * @return std::string::const_iterator Iterator pointing to the parsing position.
         */
        [[maybe_unused]] std::string::const_iterator parseHTTP1_0(const std::string &request, std::string::const_iterator start_pos = {});

        /**
         * @brief Parses an HTTP/1.1 request. Overwrites the version to HTTP/1.1.
         *
         * @param request The HTTP request string.
         * @param start_pos Optional iterator indicating the start position for parsing.
         * @return std::string::const_iterator Iterator pointing to the parsing position.
         */
        [[maybe_unused]] std::string::const_iterator parseHTTP1_1(const std::string &request, std::string::const_iterator start_pos = {});

        /**
         * @brief Parses any HTTP/1.X request determining its version on the run.
         *
         * @param request The HTTP request string.
         * @param start_pos Optional iterator indicating the start position for parsing.
         * @return std::string::const_iterator Iterator pointing to the parsing position.
         * 
         * This function is a SAX parser of HTTP/1.X requests. It has multiple states and transitions to parse the request.
         * It starts with request line that contains the method, URI, and HTTP version, ending with a CRLF.
         * `<METHOD> <URI> <HTTP_VERSION>\r\n`
         * then it parses the headers, each header is a key-value pair separated by a colon and a space.
         * `<KEY>: <VALUE>\r\n`
         * ending with an empty CRLF line that separates the headers from the body.
         * `\r\n`
         * as soon as it reaches it, it exits out to give the control back to the caller returning the pos it ended on, which should verify the headers and the request line.
         * here it's a header middleware @see Middlewares for more details.
         * on a subsequent call, if not all data has been processed, with start pos set to the returned value. and it will continue parsing
         * 
         * The parser can recieve partial data, and it will continue parsing from the last state it was in. This is useful for streaming data, 
         * and for how network data is transmitted generally.
         * 
         * @note that headers are case-insensitive, and the keys are stored in lowercase.
         *  
         */
        [[maybe_unused]] std::string::const_iterator parseHTTP1_X(const std::string &request, std::string::const_iterator start_pos = {});

        /**
         * @brief Parses an HTTP/2.0 request.
         *
         * @param request The HTTP request string.
         * @return uint8_t Status code indicating success or failure.
         *
         * @todo HTTP/2.0 parsing is not implemented.
         */
        [[maybe_unused]] uint8_t parseHttp2_0(const std::string &request) = delete;

        /**
         * @brief Parses an HTTP/3.0 request.
         *
         * @param request The HTTP request string.
         * @return uint8_t Status code indicating success or failure.
         *
         * @todo HTTP/3.0 parsing is not implemented.
         */
        [[maybe_unused]] uint8_t parseHttp3_0(const std::string &request) = delete;

        /**
         * @brief Attempts to upgrade the request (e.g., to WebSocket).
         *
         * @return uint8_t Status code indicating the result of the upgrade attempt.
         */
        [[maybe_unused]] uint8_t upgrade();

        // Imitate std behavior
        // std::string toString() const;
        // size_t size() const;

        /**
         * @brief Clears the request data and resets its state.
         */
        void clear();

        /**
         * @brief Compares this request with another for equality.
         *
         * @param other The other request to compare with.
         * @return true If both requests are equal.
         * @return false Otherwise.
         */
        bool operator==(const Request &other) const;

        /**
         * @brief Assigns the contents of another request to this one.
         *
         * @param other The other request to assign from.
         * @return Request& Reference to this request.
         */
        Request &operator=(const Request &other);

        /**
         * @brief Deleted operator[] to prevent ambiguous implementations.
         *
         * @param param_name The parameter name.
         * @return std::string& Reference to the parameter value.
         *
         * @note This operator is deleted because its implementation would be ambiguous. Use `getHeaders()` for Headers, uri_params for uri params.
         */
        std::string &operator[](const std::string &param_name) = delete;
    };

    /**
     * @class Response
     * @brief Represents an HTTP Response message.
     *
     * Inherits from `Message` and includes additional data and functionality specific to HTTP responses.
     */
    class Response : public Message {
    private:
        /**
         * @brief HTTP status code as a string. default: "500".
         */
        std::string status_code_{"500"};

        /**
         * @brief HTTP status message corresponding to the status code. default: "Internal Server Error".
         */
        std::string status_message_{"Internal Server Error"};

        /**
         * @brief Mapping of status codes to their corresponding messages. Used in HTTP/1.0 and HTTP/1.1 responses.
         */
        static const std::unordered_map<std::string, std::string> code_status_map_;

        RESPONSE_STATE state_{RESPONSE_STATE::SENDING};

        /**
         * @brief Pointer to the matched route for optimized response handling.
         */
        Route *matched_route_{nullptr};

    public:
        /**
         * @brief Default constructor.
         */
        Response() = default;

        /**
         * @brief Default destructor.
         */
        ~Response();

        /**
         * @brief Checks if the response has been sent.
         *
         * @return true If the response has been sent.
         * @return false Otherwise.
         */
        bool isSent();

        /**
         * @brief Retrieves the current send state of the response.
         *
         * @return SEND_STATE The current send state.
         */
        // SEND_STATE getState();

        /**
         * @brief Sets the response state to sent.
         *
         * @return Response& Reference to this response object.
         */
        Response &setSent();

        /**
         * @brief Associates a socket with the response.
         *
         * @param socket Pointer to the socket.
         * @return Response& Reference to this response object.
         */
        Response &setSocket(usub::uvent::net::TCPClientSocket *socket);


        /**
         * @brief Associates a route with the response.
         *
         * @param route Pointer to the matched route.
         * @return Response& Reference to this response object.
         */
        Response &setRoute(Route *route);

        /**
         * @brief Adds a header to the response.
         *
         * @param key The header key.
         * @param value The header value.
         * @return Response& Reference to this response object.
         */
        Response &addHeader(const std::string &key, const std::string &value);

        /**
         * @brief Sets the HTTP status code of the response.
         *
         * @param status_code The HTTP status code as an unsigned integer.
         * @return Response& Reference to this response object.
         */
        Response &setStatus(uint16_t status_code);

        /**
         * @brief Sets the HTTP status code of the response using a string view.
         *
         * @param status_code The HTTP status code as a string view.
         * @return Response& Reference to this response object.
         */
        Response &setStatus(std::string_view status_code);

        /**
         * @brief Sets the HTTP status message of the response.
         *
         * @param message The status message as a string view.
         * @return Response& Reference to this response object.
         */
        Response &setMessage(std::string_view message);

        /**
         * @brief Sets the response data as a string. Automatically sets the Content-Length header.
         *
         * @param data The data string to set.
         * @return Response& Reference to this response object.
         */
        Response &setBody(const std::string &data, const std::string &content_type = "");

        /**
         * @brief Sets the response data as a string. Automatically sets the Content-Length header.
         *
         * @param data The data string to set.
         * @return Response& Reference to this response object.
         */
        Response &setFile(const std::string &filename, const std::string &content_type = "");

        Response &setChunked();

        Response &setContentLength();

        std::string pull();

        /**
         * @brief Converts the response to a string representation.
         *
         * @return std::string The response as a string.
         */
        std::string string() const;

        /**
         * @brief Retrieves the size of the response.
         *
         * @return size_t The size of the response.
         */
        size_t size() const;

        /**
         * @brief Clears the response data and resets its state.
         */
        void clear();

        /**
         * @brief Initiates the coroutine to send the response.
         *
         * @return usub::uvent::task::AwaitableBase* Pointer to the awaitable coroutine.
         */
        usub::uvent::task::Awaitable<void> send_coro();

        /**
         * @brief Sends the response asynchronously.
         *
         * @return usub::uvent::task::Awaitable<void> Awaitable task representing the send operation.
         */
        usub::uvent::task::Awaitable<void> send();
    };

}// namespace usub::server::protocols::http

#endif// HTTP_MESSAGE_H
