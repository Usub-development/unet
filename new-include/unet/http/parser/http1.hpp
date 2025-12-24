#pragma once

#include <cstdint>
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
            ORIGIN_PATH,
            ORIGIN_QUERY,
            ORIGIN_FRAGMENT,
            ABSOLUTE_FORM,
            AUTHORITY_FORM,
            ASTERISK_FORM,
            VERSION,
            METADATA_CR,
            METADATA_LF,
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
            FAILED// ERROR STATE, can't name it ERROR because of conflict with ERROR macro on Windows, my kindest regards to windows devs.
        };

        struct ParserContext {
            STATE state{STATE::METHOD_TOKEN};
            std::pair<std::string, std::string> kv_buffer{};
            std::size_t headers_size{0};

            std::size_t body_bytes_read{0};
            std::size_t chunk_bytes_read{0};

            // TODO: another name or maybe store it in body bytes read
            // On a completely side node, thank LLMs for doing absolutely nothing
            // MANUALLY Moving from Message.cpp would've been fu... FASTER
            // Now I have to go through this mess and clean up
            // Well today I once again proved that using agents is as useless as it can be
            std::size_t line_size{0};

            std::size_t uri_size{0};
            std::size_t chunk_size{0};
            std::optional<std::size_t> content_length{};

            std::uint8_t uri_state{0};
            std::uint8_t uri_port_digits{0};
            std::uint8_t chunk_size_digits{0};
            bool chunk_extension{false};
            bool chunk_after_size{false};
            bool saw_cr{false};
            bool chunked{false};
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
