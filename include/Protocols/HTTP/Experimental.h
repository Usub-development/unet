#pragma once

#include <array>
#include <concepts>
#include <expected>
#include <optional>
#include <string>


namespace usub::server::experimental {

    /**
     * @enum VERSION
     * @brief Enumerates supported HTTP versions.
     */
    enum class VERSION : uint16_t {
        NONE = 0,     ///< No HTTP version specified. Error state.
        HTTP_0_9 = 9, ///< HTTP/0.9.
        HTTP_1_0 = 10,///< HTTP/1.0.
        HTTP_1_1 = 11,///< HTTP/1.1.
        HTTP_2_0 = 20,///< HTTP/2.0.
        HTTP_3_0 = 30 ///< HTTP/3.0.
    };

    struct Settings {
        std::string method;            // Method is a string as the implementation before
        std::string path;              // URI class
        std::string scheme;            // h2/h3 can fill it out, h1 requires manual
        std::string authority;         // h2/h3 or Host for h1
        VERSION version{VERSION::NONE};// h1 can fill it out, h2,h3 requires manual feeding
    };

    enum class PARSE_PHASE : uint8_t {
        START_LINE,  // parsing method/path/version (H1) or pseudo (H2/H3)
        HEADER_NAME, // currently accumulating header name fragments
        HEADER_VALUE,// currently accumulating header value fragments
        HEADER_FIELD,// just finished a header field
        HEADERS_DONE,// saw CRLFCRLF (H1) / END_HEADERS (H2/H3)
        BODY_CHUNK,  // feeding a body fragment
        DONE
    };

    struct ParseErrorInfo {
        int code = 0;       // your internal error code
        std::string message;// textual explanation
    };

    struct ParseProgress {
        PARSE_PHASE phase{PARSE_PHASE::START_LINE};
        std::size_t header_count{0};
        std::size_t bytes_total{0};
        std::size_t body_bytes{0};
        std::optional<std::size_t> content_length_expected;

        // current in-flight header (fragment-friendly)
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
        ParseProgress progress;
    };

    template<class BodyType, class HeaderType>
    struct RequestT : public RequestBase {
        BodyType body;
        HeaderType headers;
    };

    template<class BodyType, class HeaderType>
    struct RequestParserT : public RequestT<BodyType, HeaderType> {
        using RequestT = RequestT<BodyType, HeaderType>;

        std::expected<void, ParseErrorInfo> parse(std::string_view data);

        void reset() {
            this->progress = ParseProgress{};
            this->settings = Settings{};
            this->headers = HeaderType{};
            this->body = BodyType{};
        }
    };

    enum class TYPE : uint8_t {
        DOM = 0,
        SAX = 1
    };

    template<class T>
    concept body_type_compatible = requires(T b, std::string_view data) {
        // Has member function: std::expected<void, ParseErrorInfo> parse(ct, bytes)
        { b.parse(data) } -> std::same_as<std::expected<void, ParseErrorInfo>>;
        { T::type } -> std::convertible_to<TYPE>;
    };

    template<class T>
    concept is_sax_parser = requires { { T::type } -> std::convertible_to<TYPE>; } && (T::type == TYPE::SAX);

    template<class T>
    concept is_dom_parser = requires { { T::type } -> std::convertible_to<TYPE>; } && (T::type == TYPE::DOM);


}// namespace usub::server::experimental