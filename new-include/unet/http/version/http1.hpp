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
            auto result = this->request_reader_.parse(request_, begin, end);
            if (!result) {
                goto send_body;
            }
            switch (this->request_reader_.getContext().state) {
                case HTTP1RequestParser::STATE::METADATA_DONE:
                    auto match = router_->match(this->request_);
                    if (!match) {
                        break;
                    }

                    this->current_route_ = match.value();
                    auto middleware_result = this->invoke_middleware(MIDDLEWARE_PHASE::METADATA, request_, response_);
                    if (!middleware_result) {
                        break;
                    }
                    goto continue_parse;
                    break;
                case HTTP1RequestParser::STATE::HEADERS_DONE:
                case HTTP1RequestParser::STATE::TRAILERS_DONE:
                    auto middleware_result = this->invoke_middleware(MIDDLEWARE_PHASE::HEADER, request_, response_);
                    if (!middleware_result) {
                        break;
                    }
                    goto continue_parse;
                    break;
                case HTTP1RequestParser::STATE::DATA_CHUNK_DONE:
                    auto middleware_result = this->invoke_middleware(MIDDLEWARE_PHASE::BODY, request_, response_);
                    if (!middleware_result) {
                        break;
                    }
                    goto continue_parse;
                    break;
                case HTTP1RequestParser::STATE::COMPLETE:

                    auto handler = this->current_route_->handler();
                    co_await handler(request_, response_);

                    break;
                case HTTP1RequestParser::STATE::FAILED:
                    // Handle parse error
                    break;
                default:
                    goto end;
                    break;
            }
        send_body:
            co_await this->write_response(socket);
        end:
            co_return;
        };
        usub::uvent::task::Awaitable<void> on_close() {
            co_return;
        }
        usub::uvent::task::Awaitable<void> on_error(int error_code) {
            co_return;
        }

        usub::uvent::task::Awaitable<void> write_response(usub::uvent::net::TCPClientSocket &socket) {
            // const auto &responseString = this->response_.string();
            // TODO: Implement HTTP/1 response serialization

            ssize_t wrsz = co_await *socket.async_write((uint8_t *) responseString.data(), responseString.size());
            if (wrsz <= 0) {
                co_return;
            }
        }

    private:
        Request request_{};
        Response response_{};
        HTTP1RequestParser request_reader_{};
        // HTTP1ResponseSerializer response_writer_;
        std::shared_ptr<RouterType> router_;
        // TODO: Better way to handle current route?
        std::optional<Route *> current_route_;


        bool invoke_middleware(const MiddlewarePhase &phase, Request &req, Response &res) {
            return this->router_->getMiddlewareChain().execute(phase, req, res)
                           ? this->middleware_chain.execute(phase, req, res)
                           : false;
        }
    };
}// namespace usub::unet::http
