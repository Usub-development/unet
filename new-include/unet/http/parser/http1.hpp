#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "unet/http/parser/error.hpp"
#include "unet/http/request.hpp"

namespace usub::unet::http::parser::http1 {
    class RequestParser {
    public:
        enum class STATE {
            METHOD_TOKEN,
            URI,
            ORIGIN_FORM,
            ABSOLUTE_FORM,
            AUTHORITY_FORM,
            ASTERISK_FORM,
            VERSION,
            METADATA_DONE,
            HEADER_KEY,
            HEADER_VALUE,
            HEADER_CR,
            HEADER_LF,
            HEADERS_DONE,
            DATA_CONTENT_LENGTH,
            DATA_CHUNKED_SIZE,
            DATA_CHUNKED_DATA,
            DATA_CHUNKED_DATA_CR,
            DATA_CHUNKED_DATA_LF,
            DATA_CHUNK_DONE,
            DATA_DONE,
            TRAILER_KEY,
            TRAILER_VALUE,
            TRAILER_CR,
            TRAILER_LF,
            TRAILERS_DONE,
            COMPLETE,
            FAILED// ERROR STATE, can't name it ERROR because of conflict with ERROR macro on Windows, my kindes regards to windows devs.
        };

        struct ParserContext {
            STATE state_{STATE::METHOD_TOKEN};
            std::pair<std::string, std::string> kv_buffer_{};
            std::size_t headers_size_{0};
            std::size_t body_bytes_read_{0};
        };

    public:
        RequestParser() = default;
        ~RequestParser() = default;

        static std::expected<Request, Error> parse(const std::string_view raw_request);

        std::expected<void, Error> parse(Request &request, std::string_view::const_iterator &begin, const std::string_view::const_iterator end);

        ParserContext &getContext();

    private:
        ParserContext context_;
    };
}// namespace usub::unet::http::parser::http1
