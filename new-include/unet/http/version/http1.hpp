#pragma once

#include <memory>
#include <string_view>

#include "unet/http/parser/http1.hpp"
#include "unet/http/request.hpp"
#include "unet/http/response.hpp"

namespace usub::unet::http {
    using HTTP1RequestParser = usub::unet::http::parser::http1::RequestParser;

    //Eveything is HTTP1 for now
    template<enum VERSION, typename RouterType>
    class HTTP {
    public:
        HTTP() = default;
        ~HTTP() = default;

        usub::uvent::task::Awaitable<void> on_read(std::string_view data, usub::uvent::net::TCPClientSocket &socket) {
            std::string_view::const_iterator begin = data.begin();
            const std::string_view::const_iterator end = data.end();

        continue_parse:
            std::expected<void, usub::unet::http::parser::Error> result = parser_.parse(request_, begin, end);
            if (!result) {
                // Handle parse error
                co_return;
            }
            switch (parser_.getContext().state_) {
                case HTTP1RequestParser::STATE::METADATA_DONE:
                    // TODO: match the route
                    goto continue_parse;
                    break;
                case HTTP1RequestParser::STATE::HEADERS_DONE:
                case HTTP1RequestParser::STATE::TRAILERS_DONE:
                    // TODO: Call the header middleware
                    goto continue_parse;
                    break;
                case HTTP1RequestParser::STATE::DATA_CHUNK_DONE:
                    // TODO: Call the body middleware
                    goto continue_parse;
                    break;
                case HTTP1RequestParser::STATE::COMPLETE:

                    response_.metadata.version = VERSION::HTTP_1_1;
                    response_.metadata.status_code = STATUS_CODE::OK;
                    response_.metadata.status_message = "OK";
                    response_.headers.addHeader("Content-Type", "text/plain");
                    response_.body = "Hello, World!";
                    response_.headers.addHeader("Content-Length", std::to_string(response_.body.size()));

                    // TODO: Send the response
                    break;
                case default:
                    break;
            }
        };
        usub::uvent::task::Awaitable<void> on_close() {
            co_return;
        }
        usub::uvent::task::Awaitable<void> on_error(int error_code) {
            co_return;
        }

    private:
        Request request_;
        Response response_;
        HTTP1RequestParser parser_;
        std::shared_ptr<RouterType> router_;
        // TODO: Better way to handle current route?
        std::optional<Route *> current_route_;
    };
}// namespace usub::unet::http
