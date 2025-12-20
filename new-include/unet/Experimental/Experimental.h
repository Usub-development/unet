#pragma once

#include <array>
#include <concepts>
#include <expected>
#include <optional>
#include <string>


namespace usub::server::experimental {

    struct ParseProgress {
        PARSE_PHASE phase{PARSE_PHASE::START_LINE};
        std::size_t header_count{0};
        std::size_t bytes_total{0};
        std::size_t body_bytes{0};
        std::optional<std::size_t> content_length_expected;

        // current in-flight header 
        std::string cur_header_name;
        std::string cur_header_value;

        // optional bounded tail buffer for debugging
        static constexpr std::size_t kTail{256};
        std::array<char, kTail> tail{};
        std::size_t tail_pos{0};

        void feed_tail(std::string_view chunk) {
            bytes_total += chunk.size();
            for (char c: chunk) tail[tail_pos++ % kTail] = c;
        }
        void reset_current_header() {
            cur_header_name.clear();
            cur_header_value.clear();
        }

        ParseErrorInfo error_info;
        operator bool() const {
            return error_info.code != 0;
        }
    };

    struct RequestBase {
        Settings settings;
        // ParseProgress progress; // Moved to the parser class
    };

    template<class BodyType, class HeaderType>
    struct RequestT : public RequestBase {
        BodyType body;
        HeaderType headers;
    };


    // TODO: Think about FramingPolicy: wether to allow/disallow certain encodings: only chunked, only content-length, both, etc.
    // Allow trailers or not, etc.
    enum class FRAMING_POLICY : uint8_t {
        ALL = 0,
        CONTENT_LENGTH_ONLY = 1,
        CHUNKED_ONLY = 2,
    };

    struct RoutePolicy {
        static const FRAMING_POLICY framing = FRAMING_POLICY::ALL; // Allow both chunked and content-length by default NO SUPPORT YET
        static const bool allow_trailers = false; // Disallow trailers for now NO SUPPORT
        std::size_t max_header_size = 64 * 1024;
        std::size_t max_body_size = 8 * 1024 * 1024;
    };

    static std::size_t max_method_token_size = 40;// Arguably should be much smaller, but let's be generous
    static std::size_t max_uri_size = 16 * 1024;  // Very generous limit for URI

    // Right now we assume every route can accept both chunked and content-length, and no trailers.
    // max_header_size and max_body_size are per-request limits. They can be changed per-route.
    // They also can both be modified in SETTINGS middleware, and body size can still be modified in HEADER middleware.
    // max_method_token_size, max_uri_size can only be set globally, since that part is parsed before any routing is possible.
    template<class BodyType, class HeaderType, class HTTPParser>
    struct RequestParserT : public RequestT<BodyType, HeaderType> {
        friend HTTPParser;

        using RequestT = RequestT<BodyType, HeaderType>;

        RoutePolicy policy;

        void clean() {
            this->progress = ParseProgress{};
            this->settings = Settings{};
            this->headers = HeaderType{};
            this->body = BodyType{};
        }
    };

    enum class TYPE : uint8_t {
        DOM = 0,   // Never goes into body Middleware, no matter the Transfer-Encoding
                   // The library will accumulate the whole body before calling parse
                   // parse is called once with the full body and move semantics
        MANUAL = 1,// Same as DOM, but user BodyType is responsible for accumulation
        SAX = 2,   // Will go into body Middleware when Transfer-Encoding: chunked,
                   // the library will feed body chunks to BodyType::parse as they arrive
    };

    // Concepts for BodyType
    // Okay, this requires a lot of explanation. I'll clarify on some of the behaviour andthe meaning I put into those parser types.
    // First of all, the BodyType class is only responsible for parsing the pure body content, without any framing.
    // Framing (Transfer-Encoding, Content-Length) is handled by the library, and the body parser is only fed the actual body data.
    // Secondly parsing is done in a single pass, without any resets or rewinds. So chunks of chunked encoding are parsed in the next order
    // assuming SAX or MANUAL parsers: read size -> feed chunk to parser (min(chunk_size, remaining_packet_size)) -> read next size ->
    // repeat until 0 size chunk is read. You should keep that in mind when implementing your BodyType parsers.
    // DOM - expects to receive the whole body at once, and parse it in one go. The library is responsible for accumulating
    // and feeding the whole body. move semantics is enforced to try and avoid copies. This may not be possible in all cases,
    // but at least we try. It will never go into body Middleware, no matter how big the body is, and how many frames or packets
    // it was split into.
    // Manual - same as DOM, but the user BodyType is responsible for accumulation. The library will just feed it the body subview, where the
    // data must be. Since the library can't be fully responsible for validation of framing in that case due to it being a single pass,
    // the framing may bleed into the body data if the client is malicious or buggy.
    // It's likely that such behaviour will break parsing on the subsequent chunk size read attempt. But this is an important part for safety.
    // SAX - the library will feed body chunks to BodyType::parse as they arrive. And the intermediate results are shown in BODY middleware.
    // Every parser is expected to fully consume the chunk it is given, otherwise the data WILL be lost. The library won't try to re-feed any unconsumed data.
    // At least for now. We may switch to iterators or something similar in the future to allow partial consumption.
    // But that would mean that user parser would be partially responsible for framing validation, which is not ideal.

    template<class T>
    concept has_type_tag = requires { { T::type } -> std::convertible_to<TYPE>; };

    template<class T>
    concept is_dom_parser = has_type_tag<T> && (T::type == TYPE::DOM);
    template<class T>
    concept is_manual_parser = has_type_tag<T> && (T::type == TYPE::MANUAL);
    template<class T>
    concept is_sax_parser = has_type_tag<T> && (T::type == TYPE::SAX);

    template<class T>
    concept dom_parser_compatable = requires(T t) {
        { t.parse(std::declval<std::string &&>()) } -> std::same_as<std::expected<void, ParseErrorInfo>>;
    };

    template<class T>
    concept manual_parser_compatable = requires(T t, std::string_view view) {
        { t.parse(view) } -> std::same_as<std::expected<void, ParseErrorInfo>>;
    };

    template<class T>
    concept sax_parser_compatable = requires(T t, std::string_view chunk) {
        { t.parse(chunk) } -> std::same_as<std::expected<void, ParseErrorInfo>>;// TODO: maybe some other signature? Dafuq
    };

    template<class T>
    concept body_type_compatible =
            (is_dom_parser<T> && dom_parser_compatable<T>) ||
            (is_sax_parser<T> && sax_parser_compatable<T>) ||
            (is_manual_parser<T> && manual_parser_compatable<T>);


    struct StringBody {
        static constexpr TYPE type = TYPE::SAX;
        std::string data;

        std::expected<void, ParseErrorInfo> parse(std::string_view chunk) {
            data.append(chunk);
            return {};
        }
    };

    template<class T>
    struct JsonParser {
        T data;
        static constexpr TYPE type = TYPE::DOM;

        std::expected<void, ParseErrorInfo> parse(std::string &&body) {
            try {
                data = T::from_json(body);
            } catch (const std::exception &e) {
                return std::unexpected(ParseErrorInfo{1, e.what()});
            }
            return {};
        }
    };


}// namespace usub::server::experimental