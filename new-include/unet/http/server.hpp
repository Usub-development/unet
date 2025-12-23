#pragma once

#include <string_view>
#include <uvent/Uvent.h>


#include "unet/http/parser.hpp"
#include "unet/http/request.hpp"
#include "unet/http/response.hpp"
#include "unet/http/version/http1.hpp"

namespace usub::unet::http {


    template<class RouterType>
    class Dispatcher {
    public:
        Dispatcher() = default;
        ~Dispatcher() = default;

        usub::uvent::task::Awaitable<void> on_read(std::string_view data, usub::uvent::net::TCPClientSocket &socket) {
            // We need to determine the http version here, non tls allows for http0.9 http1.0 http1.1 and h2c

            handler_.on_read(data, socket);
            co_return;

            // We really expect that PRI will come in full, but no guarantees, this is not a commonly used protocol
            // and just buffering requests strings until we can make a decision is not acceptable performance wise.
            constexpr std::string_view h2c_preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
            const bool looks_like_h2c = data.size() >= h2c_preface.size() && data.substr(0, h2c_preface.size()) == h2c_preface;
            if (looks_like_h2c) {
                // TODO: initialize and hand off to HTTP/2 (h2c) handler
            } else {
                // TODO: initialize and hand off to HTTP/1.x parser/dispatcher
            }
            co_return;
        };
        usub::uvent::task::Awaitable<void> on_close() {
            handler_.on_close();
            co_return;
        }
        usub::uvent::task::Awaitable<void> on_error(int error_code) {
            handler_.on_error(error_code);
            co_return;
        }

    private:
        std::shared_ptr<RouterType> router_;
        //TODO: propper multi protocol handling
        HTTP<VERSION::HTTP1, RouterType> handler_;
    };

    template<class RouterType>
    class ServerImpl {
    public:
        //TODO: implement constructors
        explicit ServerImpl(const ServerConfig &config);
        explicit ServerImpl(usub::Uvent &uvent);
        ServerImpl(const ServerConfig &config, usub::Uvent &uvent);
        ServerConfig() = default;

        ~ServerImpl() = default;

        auto &handle(auto &&...args) {
            return this->router_->addHandler(std::forward<decltype(args)>(args)...);
        }

        auto &addMiddleware(auto &&...args) {// allow for modifying router when wanted
            return this->router_->addMiddleware(std::forward<decltype(args)>(args)...);
        }

    private:
        ServerConfig config_;
        std::shared_ptr<RouterType> router_;
        std::shared_ptr<usub::Uvent> uvent_;
        Dispatcher<RouterType> dispatcher_;
    };


}// namespace usub::unet::http
