#include "unet/http/parser/http1.hpp"

#include <array>
#include <cstring>
#include <limits>
#include <utility>

// TODO: Recheck on all that uses ctx....

namespace usub::unet::http::parser::http1 {
    namespace {
        constexpr std::array<std::uint8_t, 256> build_tchar_table() {
            std::array<std::uint8_t, 256> table{};
            for (char c = 'A'; c <= 'Z'; ++c) table[static_cast<unsigned char>(c)] = 1;
            for (char c = 'a'; c <= 'z'; ++c) table[static_cast<unsigned char>(c)] = 1;
            for (char c = '0'; c <= '9'; ++c) table[static_cast<unsigned char>(c)] = 1;
            for (char c: {'!', '#', '$', '%', '&', '\'', '*', '+', '-', '.', '^', '_', '`', '|', '~'}) {
                table[static_cast<unsigned char>(c)] = 1;
            }
            return table;
        }

        constexpr std::array<std::uint8_t, 256> build_vchar_obs_table() {
            std::array<std::uint8_t, 256> table{};
            for (char c = '!'; c <= '~'; ++c) table[static_cast<unsigned char>(c)] = 1;
            for (unsigned char c = 128; c <= 255 && c >= 128; ++c) table[c] = 1;
            return table;
        }

        constexpr std::array<std::uint8_t, 256> build_scheme_table() {
            std::array<std::uint8_t, 256> table{};
            for (char c = 'A'; c <= 'Z'; ++c) table[static_cast<unsigned char>(c)] = 1;
            for (char c = 'a'; c <= 'z'; ++c) table[static_cast<unsigned char>(c)] = 1;
            for (char c = '0'; c <= '9'; ++c) table[static_cast<unsigned char>(c)] = 1;
            for (char c: {'+', '-', '.'}) {
                table[static_cast<unsigned char>(c)] = 1;
            }
            return table;
        }

        constexpr std::array<std::uint8_t, 256> build_path_table() {
            std::array<std::uint8_t, 256> table{};
            for (char c = 'A'; c <= 'Z'; ++c) table[static_cast<unsigned char>(c)] = 1;
            for (char c = 'a'; c <= 'z'; ++c) table[static_cast<unsigned char>(c)] = 1;
            for (char c = '0'; c <= '9'; ++c) table[static_cast<unsigned char>(c)] = 1;
            for (char c: {'-', '.', '_', '~'}) {
                table[static_cast<unsigned char>(c)] = 1;
            }
            for (char c: {'!', '$', '&', '\'', '(', ')', '*', '+', ',', ';', '='}) {
                table[static_cast<unsigned char>(c)] = 1;
            }
            for (char c: {':', '@', '/'}) {
                table[static_cast<unsigned char>(c)] = 1;
            }
            return table;
        }

        constexpr std::array<std::uint8_t, 256> build_query_table() {
            std::array<std::uint8_t, 256> table{};
            for (char c = 'A'; c <= 'Z'; ++c) table[static_cast<unsigned char>(c)] = 1;
            for (char c = 'a'; c <= 'z'; ++c) table[static_cast<unsigned char>(c)] = 1;
            for (char c = '0'; c <= '9'; ++c) table[static_cast<unsigned char>(c)] = 1;
            for (char c: {'-', '.', '_', '~'}) {
                table[static_cast<unsigned char>(c)] = 1;
            }
            for (char c: {'!', '$', '&', '\'', '(', ')', '*', '+', ',', ';', '='}) {
                table[static_cast<unsigned char>(c)] = 1;
            }
            for (char c: {':', '@', '/', '?', '%'}) {
                table[static_cast<unsigned char>(c)] = 1;
            }
            return table;
        }

        constexpr std::array<std::uint8_t, 256> build_host_table() {
            std::array<std::uint8_t, 256> table{};
            for (char c = 'A'; c <= 'Z'; ++c) table[static_cast<unsigned char>(c)] = 1;
            for (char c = 'a'; c <= 'z'; ++c) table[static_cast<unsigned char>(c)] = 1;
            for (char c = '0'; c <= '9'; ++c) table[static_cast<unsigned char>(c)] = 1;
            for (char c: {'-', '.', '_', '~'}) {
                table[static_cast<unsigned char>(c)] = 1;
            }
            for (char c: {'!', '$', '&', '\'', '(', ')', '*', '+', ',', ';', '=', '%'}) {
                table[static_cast<unsigned char>(c)] = 1;
            }
            return table;
        }

        constexpr std::array<std::uint8_t, 256> build_version_table() {
            std::array<std::uint8_t, 256> table{};
            table['H'] = 1;
            table['T'] = 1;
            table['P'] = 1;
            table['1'] = 1;
            table['0'] = 1;
            table['/'] = 1;
            return table;
        }

        constexpr std::array<std::uint8_t, 256> tchar_table = build_tchar_table();
        constexpr std::array<std::uint8_t, 256> vchar_obs_table = build_vchar_obs_table();
        constexpr std::array<std::uint8_t, 256> scheme_table = build_scheme_table();
        constexpr std::array<std::uint8_t, 256> path_table = build_path_table();
        constexpr std::array<std::uint8_t, 256> query_table = build_query_table();
        constexpr std::array<std::uint8_t, 256> host_table = build_host_table();
        constexpr std::array<std::uint8_t, 256> version_table = build_version_table();

        inline bool is_version(unsigned char c) {
            return version_table[c] != 0;
        }

        inline bool is_alpha(unsigned char c) {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        }

        inline bool is_tchar(unsigned char c) {
            return tchar_table[c] != 0;
        }

        inline bool is_vchar_or_obs(unsigned char c) {
            return vchar_obs_table[c] != 0;
        }

        inline bool is_scheme_char(unsigned char c) {
            return scheme_table[c] != 0;
        }

        inline bool is_path_char(unsigned char c) {
            return path_table[c] != 0;
        }

        inline bool is_query_char(unsigned char c) {
            return query_table[c] != 0;
        }

        inline bool is_host_char(unsigned char c) {
            return host_table[c] != 0;
        }

        inline char ascii_lower(char c) {
            return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
        }

        inline bool is_hex_digit(unsigned char c) {
            return (c >= '0' && c <= '9') ||
                   (c >= 'a' && c <= 'f') ||
                   (c >= 'A' && c <= 'F');
        }

        inline std::uint8_t hex_value(unsigned char c) {
            if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
            if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(10 + (c - 'a'));
            return static_cast<std::uint8_t>(10 + (c - 'A'));
        }

        inline std::string_view trim_ows(const std::string &value) {
            std::size_t start = 0;
            std::size_t end = value.size();
            while (start < end && (value[start] == ' ' || value[start] == '\t')) {
                ++start;
            }
            while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t')) {
                --end;
            }
            return std::string_view(value.data() + start, end - start);
        }

        inline bool contains_chunked_token(std::string_view value) {
            constexpr std::string_view token = "chunked";
            std::size_t i = 0;
            while (i < value.size()) {
                while (i < value.size() && (value[i] == ' ' || value[i] == '\t' || value[i] == ',')) {
                    ++i;
                }
                if (i + token.size() > value.size()) break;
                bool match = true;
                for (std::size_t j = 0; j < token.size(); ++j) {
                    char c = value[i + j];
                    if (ascii_lower(c) != token[j]) {
                        match = false;
                        break;
                    }
                }
                if (match) return true;
                while (i < value.size() && value[i] != ',') {
                    ++i;
                }
            }
            return false;
        }

        enum class UriState : std::uint8_t {
            OriginPath = 0,
            OriginQuery = 1,
            OriginFragment = 2,
            AbsoluteScheme = 3,
            AbsoluteSlash1 = 4,
            AbsoluteSlash2 = 5,
            AbsoluteAuthority = 6,
            AbsolutePath = 7,
            AbsoluteQuery = 8,
            AbsoluteFragment = 9,
            AuthorityHost = 10,
            AuthorityPort = 11,
            AuthorityIPv6 = 12,
        };
    }// namespace

    std::expected<Request, Error> RequestParser::parse(const std::string_view raw_request) {
        RequestParser parser;
        Request request;
        auto begin = raw_request.begin();
        auto end = raw_request.end();
        for (;;) {
            auto result = parser.parse(request, begin, end);
            if (!result) {
                return std::unexpected(result.error());
            }
            if (parser.context_.state == STATE::COMPLETE) {
                return request;
            }
            if (begin == end) {
                Error err{Error::CODE::GENERIC_ERROR, STATUS_CODE::BAD_REQUEST, "Incomplete request", {}};
                return std::unexpected(err);
            }
        }
    }

    std::expected<void, Error> RequestParser::parse(Request &request, std::string_view::const_iterator &begin, const std::string_view::const_iterator end) {
        auto &ctx = this->context_;
        auto &state = ctx.state;
        using Status = usub::unet::http::STATUS_CODE;

        auto fail = [&](Status status, std::string_view message) -> std::expected<void, Error> {
            Error err{};
            err.code = Error::CODE::GENERIC_ERROR;
            err.expected_status = status;
            err.message = std::string(message);
            //TODO: Rethink
            // std::size_t remaining = static_cast<std::size_t>(e - it);
            // std::size_t copy_len = remaining < err.tail.size() ? remaining : err.tail.size();
            std::memset(err.tail.data(), 0, err.tail.size());
            // if (copy_len > 0) {
            //     std::memcpy(err.tail.data(), it, copy_len);
            // }
            state = STATE::FAILED;
            // begin = it;
            return std::unexpected(err);
        };

        while (begin != end) {
            switch (state) {
                case STATE::METHOD_TOKEN: {
                    auto &method = request.metadata.method_token;
                    // TODO: WTF? Ok, imma leave it here for now, because it's 5 o'clock, but hell one time I return to LLMS
                    if (method.empty()) {
                        ctx.uri_size = 0;
                        ctx.uri_state = 0;
                        ctx.uri_port_digits = 0;
                        ctx.headers_size = 0;
                        ctx.body_bytes_read = 0;
                        ctx.chunk_bytes_read = 0;
                        ctx.chunk_size = 0;
                        ctx.chunk_size_digits = 0;
                        ctx.chunk_extension = false;
                        ctx.chunk_after_size = false;
                        ctx.saw_cr = false;
                        ctx.line_size = 0;
                        ctx.kv_buffer = {};
                        ctx.content_length.reset();
                        ctx.chunked = false;
                        request.metadata.uri = {};
                        request.headers = {};
                        request.body.clear();
                    }
                    while (begin != end) {
                        if (is_tchar(*begin)) [[likely]] {
                            if (method.size() >= usub::unet::http::max_method_token_size) {
                                return fail(Status::BAD_REQUEST, "Method token too long");
                            }
                            method.push_back(static_cast<char>(*begin));
                            ++begin;
                            continue;
                        }
                        if (*begin == ' ') {
                            if (method.empty()) {
                                return fail(Status::BAD_REQUEST, "Empty method token");
                            }
                            ++begin;
                            state = STATE::URI;
                            break;
                        }
                        return fail(Status::BAD_REQUEST, "Invalid method token");
                    }
                    break;
                }
                case STATE::URI: {
                    if (begin == end) break;
                    if (*begin == '/') {
                        state = STATE::ORIGIN_PATH;
                    } else if (*begin == '*') {
                        return fail(Status::BAD_REQUEST, "Unsupported");
                        state = STATE::ASTERISK_FORM;
                    } else if (is_alpha(c)) {
                        return fail(Status::BAD_REQUEST, "Unsupported");
                        state = STATE::ABSOLUTE_FORM;
                    } else {
                        return fail(Status::BAD_REQUEST, "Unsupported");
                        state = STATE::AUTHORITY_FORM;
                    }
                    break;
                }
                case STATE::ORIGIN_PATH: {
                    auto &path = request.metadata.uri.path;
                    while (begin != end) {
                        if (is_path_char(c)) {
                            path.push_back(static_cast<char>(*begin));
                            ++begin;
                            ++ctx.uri_size;
                            continue;
                        }
                        if (*begin == '?') {
                            state = STATE::ORIGIN_QUERY;
                            ++begin;
                            break;
                        }
                        if (*begin == '#') {
                            return fail(Status::BAD_REQUEST, "Fragment is diallowed");
                            state = STATE::ORIGIN_FRAGMENT;
                            ++begin;
                            break;
                        }
                        if (*begin == ' ') {
                            state = STATE::VERSION;
                            ++begin;
                            break;
                        } else {
                            return fail(Status::BAD_REQUEST, "Invalid path character");
                        }
                        if (ctx.uri_size >= usub::unet::http::max_uri_size) {
                            return fail(Status::URI_TOO_LONG, "URI too long");
                        }
                    }
                    break;
                }
                case STATE::ORIGIN_QUERY: {
                    auto &query = request.metadata.uri.query;
                    if (is_query_char(*begin)) {
                        query.push_back(static_cast<char>(*begin));
                        ++begin;
                        ++ctx.uri_size;
                        continue;
                    } else if (*begin == '#') {
                        return fail(Status::BAD_REQUEST, "Fragment is diallowed");
                        state = STATE::ORIGIN_FRAGMENT;
                        ++begin;
                        break;
                    } else {
                        return fail(Status::BAD_REQUEST, "Invalid query character");
                    }
                    break;
                }
                case STATE::ORIGIN_FRAGMENT: {
                    return fail(Status::BAD_REQUEST, "Fragment, How did we get here?");
                    break;
                }
                case STATE::ABSOLUTE_FORM: {
                    return fail(Status::BAD_REQUEST, "Absolute form, How did we get here");
                }
                case STATE::AUTHORITY_FORM: {
                    return fail(Status::BAD_REQUEST, "Authority form, How did we get here");
                }
                case STATE::ASTERISK_FORM: {
                    return fail(Status::BAD_REQUEST, "Asterisk form, How did we get here");
                }
                case STATE::VERSION: {
                    auto &version_buf = ctx.kv_buffer.first;
                    while (begin != end) {
                        if (is_version(*begin)) {
                            version_buf.push_back(static_cast<char>(*begin));
                            ++begin;
                        } else if (*begin == '\r') {
                            state = STATE::METADATA_CR;
                            ++begin;
                            break;
                        } else {
                            return fail(Status::BAD_REQUEST, "Wrong Version");
                        }
                        if (version_buf.size() > 8) {
                            return fail(Status::BAD_REQUEST, "Version too large");
                        }
                    }
                }
                case STATE::METADATA_CR:
                    if (begin == end) [[unlikely]] {
                        return;
                    }
                    if (*begin != '\n') {
                        return fail(Status::BAD_REQUEST, "Missing LF");
                    }
                    state = STATE::METADATA_LF;
                    ++begin;
                case STATE::METADATA_LF: {
                    auto &version_buf = ctx.kv_buffer.first;
                    if (begin == end) [[unlikely]] {
                        return;
                    }
                    if (*begin != '\n') {
                        return fail(Status::BAD_REQUEST, "Missing LF");
                    }
                    if (version_buf == "HTTP/1.1") {
                        request.metadata.version = VERSION::HTTP_1_1
                    } else if (version_buf == "HTTP/1.0") {
                        request.metadata.version = VERSION::HTTP_1_0;
                    } else {
                        request.metadata.version = VERSION::HTTP_0_9;
                    }
                    ++begin;
                    state = STATE::METADATA_DONE;
                    return;
                }
                case STATE::METADATA_DONE:
                    ctx.kv_buffer = {};
                    ctx.headers_size = 0;
                    state = STATE::HEADER_KEY;
                    break;
                case STATE::HEADER_KEY: {
                    auto &key = ctx.kv_buffer.first;
                    while (begin != end && state == STATE::HEADER_KEY) {
                        switch (*begin) {
                            case ' ':
                                state = STATE::FAILED;
                                return fail(Status::BAD_REQUEST, "Wrong character in header");
                            case '\r':
                                state = STATE::FAILED;
                                return fail(Status::BAD_REQUEST, "Wrong character in header");

                            case '\n':
                                state = STATE::FAILED;
                                return fail(Status::BAD_REQUEST, "Wrong character in header");
                            case ':':
                                state = STATE::HEADER_VALUE;
                                break;
                            default:
                                key.push_back(static_cast<char>(*begin));
                                break;
                        }
                    }
                    break;
                }
                case STATE::HEADER_VALUE: {
                    auto &value = ctx.kv_buffer.second;
                    while (begin != end) {
                        if (*begin == '\r') {
                            ++begin;
                            state = STATE::HEADER_CR;
                            break;
                        }
                        if (*begin == '\n') {
                            return fail(Status::BAD_REQUEST, "Invalid header line");
                        }
                        if (*begin != ' ' && *begin != '\t' && !is_vchar_or_obs(*begin)) {
                            return fail(Status::BAD_REQUEST, "Invalid header value");
                        }
                        value.push_back(static_cast<char>(*begin));
                        ++begin;
                        ++ctx.headers_size;
                        if (ctx.headers_size > request.policy.max_header_size) {
                            return fail(Status::REQUEST_HEADER_FIELDS_TOO_LARGE, "Headers too large");
                        }
                    }
                    break;
                }
                case STATE::HEADER_CR: {
                    // TODO: impl
                    break;
                }
                case STATE::HEADER_LF: {
                    // TODO: impl
                    break;
                }
                case STATE::HEADERS_DONE: {
                    const auto max_body = request.policy.max_body_size;
                    // TODO: WHY?
                    if (ctx.chunked) {
                        ctx.chunk_size = 0;
                        ctx.chunk_size_digits = 0;
                        ctx.chunk_extension = false;
                        ctx.body_bytes_read = 0;
                        ctx.chunk_bytes_read = 0;
                        ctx.chunk_after_size = false;
                        state = STATE::DATA_CHUNKED_SIZE;
                        break;
                    }
                    if (ctx.content_length.has_value()) {
                        if (*ctx.content_length > max_body) {
                            return fail(Status::PAYLOAD_TOO_LARGE, "Body exceeds configured limit");
                        }
                        if (*ctx.content_length == 0) {
                            state = STATE::COMPLETE;
                            break;
                        }
                        ctx.body_bytes_read = 0;
                        state = STATE::DATA_CONTENT_LENGTH;
                        break;
                    }
                    if (request.metadata.method_token == "GET" || request.metadata.method_token == "HEAD") {
                        state = STATE::COMPLETE;
                        break;
                    }
                    return fail(Status::LENGTH_REQUIRED, "Missing Content-Length");
                }
                case STATE::DATA_CONTENT_LENGTH: {
                    if (!ctx.content_length.has_value()) {
                        return fail(Status::BAD_REQUEST, "Missing Content-Length");
                    }
                    std::size_t remaining = *ctx.content_length - ctx.body_bytes_read;
                    std::size_t available = static_cast<std::size_t>(e - it);
                    std::size_t take = remaining < available ? remaining : available;
                    if (take > 0) {
                        request.body.append(it, take);
                        ctx.body_bytes_read += take;
                        it += take;
                    }
                    if (ctx.body_bytes_read == *ctx.content_length) {
                        state = STATE::DATA_CHUNK_DONE;
                        begin = it;
                        return {};
                    }
                    break;
                }
                case STATE::DATA_CHUNKED_SIZE: {
                    // TODO: Implement
                    break;
                }
                case STATE::DATA_CHUNKED_DATA: {
                    // TODO: Implement
                }
                case STATE::DATA_CHUNKED_DATA_CR:
                    // TODO: Implement
                case STATE::DATA_CHUNKED_DATA_LF: {
                    // TODO: Implement
                }
                case STATE::DATA_CHUNK_DONE:
                    // TODO: Implement
                case STATE::DATA_DONE:
                    // TODO: Implement
                    break;
                case STATE::COMPLETE:
                    return {};
                case STATE::FAILED:
                    begin = end;
                    return fail(Status::BAD_REQUEST, "Parser in failed state");
                default:
                    return fail(Status::BAD_REQUEST, "Invalid parser state");
            }
        }
        return {};
    }

    RequestParser::ParserContext &RequestParser::getContext() {
        return this->context_;
    }
}// namespace usub::unet::http::parser::http1
