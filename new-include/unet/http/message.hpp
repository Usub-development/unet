#pragma once

#include <array>
#include <cstdint>
#include <string_view>


namespace usub::unet::http {

    enum STATUS_CODE : uint16_t {
        // 1XX Informational
        CONTINUE = 100,
        SWITCHING_PROTOCOLS = 101,
        PROCESSING = 102,
        EARLY_HINTS = 103,

        // 2XX Success
        OK = 200,
        CREATED = 201,
        ACCEPTED = 202,
        NON_AUTHORITATIVE_INFORMATION = 203,
        NO_CONTENT = 204,
        RESET_CONTENT = 205,
        PARTIAL_CONTENT = 206,
        MULTI_STATUS = 207,
        ALREADY_REPORTED = 208,
        IM_USED = 226,

        // 3XX Redirection
        MULTIPLE_CHOICES = 300,
        MOVED_PERMANENTLY = 301,
        FOUND = 302,
        SEE_OTHER = 303,
        NOT_MODIFIED = 304,
        USE_PROXY = 305,
        TEMPORARY_REDIRECT = 307,
        PERMANENT_REDIRECT = 308,

        // 4XX Client Errors
        BAD_REQUEST = 400,
        UNAUTHORIZED = 401,
        PAYMENT_REQUIRED = 402,
        FORBIDDEN = 403,
        NOT_FOUND = 404,
        METHOD_NOT_ALLOWED = 405,
        NOT_ACCEPTABLE = 406,
        PROXY_AUTHENTICATION_REQUIRED = 407,
        REQUEST_TIMEOUT = 408,
        CONFLICT = 409,
        GONE = 410,
        LENGTH_REQUIRED = 411,
        PRECONDITION_FAILED = 412,
        PAYLOAD_TOO_LARGE = 413,
        URI_TOO_LONG = 414,
        UNSUPPORTED_MEDIA_TYPE = 415,
        RANGE_NOT_SATISFIABLE = 416,
        EXPECTATION_FAILED = 417,
        IM_A_TEAPOT = 418,
        AUTHENTICATION_TIMEOUT = 419,// non-standard
        MISDIRECTED_REQUEST = 421,
        UNPROCESSABLE_ENTITY = 422,
        LOCKED = 423,
        FAILED_DEPENDENCY = 424,
        TOO_EARLY = 425,
        UPGRADE_REQUIRED = 426,
        PRECONDITION_REQUIRED = 428,
        TOO_MANY_REQUESTS = 429,
        REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
        UNAVAILABLE_FOR_LEGAL_REASONS = 451,
        CLIENT_CLOSED_REQUEST = 499,// nginx

        // 5XX Server Errors
        INTERNAL_SERVER_ERROR = 500,
        NOT_IMPLEMENTED = 501,
        BAD_GATEWAY = 502,
        SERVICE_UNAVAILABLE = 503,
        GATEWAY_TIMEOUT = 504,
        HTTP_VERSION_NOT_SUPPORTED = 505,
        VARIANT_ALSO_NEGOTIATES = 506,
        INSUFFICIENT_STORAGE = 507,
        LOOP_DETECTED = 508,
        NOT_EXTENDED = 510,
        NETWORK_AUTHENTICATION_REQUIRED = 511,

        // Non-RFC (Cloudflare etc.)
        UNKNOWN_ERROR = 520,
        WEB_SERVER_IS_DOWN = 521,
        CONNECTION_TIMED_OUT = 522,
        ORIGIN_IS_UNREACHABLE = 523,
        A_TIMEOUT_OCCURRED = 524,
        SSL_HANDSHAKE_FAILED = 525,
        INVALID_SSL_CERTIFICATE = 526,
    };

    static constexpr std::array<std::string_view, 600> status_messages = [] {
        std::array<std::string_view, 600> arr{};

        arr[100] = "Continue";
        arr[101] = "Switching Protocols";
        arr[102] = "Processing";
        arr[103] = "Early Hints";

        arr[200] = "OK";
        arr[201] = "Created";
        arr[202] = "Accepted";
        arr[203] = "Non-Authoritative Information";
        arr[204] = "No Content";
        arr[205] = "Reset Content";
        arr[206] = "Partial Content";
        arr[207] = "Multi-Status";
        arr[208] = "Already Reported";
        arr[226] = "IM Used";

        arr[300] = "Multiple Choices";
        arr[301] = "Moved Permanently";
        arr[302] = "Found";
        arr[303] = "See Other";
        arr[304] = "Not Modified";
        arr[305] = "Use Proxy";
        arr[307] = "Temporary Redirect";
        arr[308] = "Permanent Redirect";

        arr[400] = "Bad Request";
        arr[401] = "Unauthorized";
        arr[402] = "Payment Required";
        arr[403] = "Forbidden";
        arr[404] = "Not Found";
        arr[405] = "Method Not Allowed";
        arr[406] = "Not Acceptable";
        arr[407] = "Proxy Authentication Required";
        arr[408] = "Request Timeout";
        arr[409] = "Conflict";
        arr[410] = "Gone";
        arr[411] = "Length Required";
        arr[412] = "Precondition Failed";
        arr[413] = "Payload Too Large";
        arr[414] = "URI Too Long";
        arr[415] = "Unsupported Media Type";
        arr[416] = "Range Not Satisfiable";
        arr[417] = "Expectation Failed";
        arr[418] = "I'm a teapot";
        arr[419] = "Authentication Timeout";
        arr[421] = "Misdirected Request";
        arr[422] = "Unprocessable Entity";
        arr[423] = "Locked";
        arr[424] = "Failed Dependency";
        arr[425] = "Too Early";
        arr[426] = "Upgrade Required";
        arr[428] = "Precondition Required";
        arr[429] = "Too Many Requests";
        arr[431] = "Request Header Fields Too Large";
        arr[451] = "Unavailable For Legal Reasons";
        arr[499] = "Client Closed Request";

        arr[500] = "Internal Server Error";
        arr[501] = "Not Implemented";
        arr[502] = "Bad Gateway";
        arr[503] = "Service Unavailable";
        arr[504] = "Gateway Timeout";
        arr[505] = "HTTP Version Not Supported";
        arr[506] = "Variant Also Negotiates";
        arr[507] = "Insufficient Storage";
        arr[508] = "Loop Detected";
        arr[510] = "Not Extended";
        arr[511] = "Network Authentication Required";

        arr[520] = "Unknown Error";
        arr[521] = "Web Server Is Down";
        arr[522] = "Connection Timed Out";
        arr[523] = "Origin Is Unreachable";
        arr[524] = "A Timeout Occurred";
        arr[525] = "SSL Handshake Failed";
        arr[526] = "Invalid SSL Certificate";

        return arr;
    }();

    // TODO: Think about FramingPolicy: wether to allow/disallow certain encodings: only chunked, only content-length, both, etc.
    // Allow trailers or not, etc.
    enum class FRAMING_POLICY : uint8_t {
        ALL = 0,
        CONTENT_LENGTH_ONLY = 1,
        CHUNKED_ONLY = 2,
    };

    struct MessagePolicy {
        static const FRAMING_POLICY framing = FRAMING_POLICY::ALL;// Allow both chunked and content-length // NO SUPPORT YET
        static const bool allow_trailers = false;                 // Disallow trailers for now // NO SUPPORT FOR TRAILERS YET
        std::size_t max_header_size = 64 * 1024;
        std::size_t max_body_size = 8 * 1024 * 1024;
    };

}// namespace usub::unet::http
