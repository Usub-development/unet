//
// Created by Kirill Zhukov on 15.11.2023.
//

#ifndef SERVER_H
#define SERVER_H

#include <vector>
#include <functional>
#include <thread>
#include <memory>
#include "data/types.h"
#include "HandlerModule.h"
#include "loop/loop.h"
#include "configuration/ConfigReader.h"


namespace unit::server {
    namespace data {
        class HttpRequest;
        class HttpResponse;
    }; // data

    namespace handler {
        class Server : std::enable_shared_from_this<Server> {
        public:
            explicit Server(const std::string&config_path);

            ~Server();

            void handle(request::type request_type, const std::string&endpoint,
                        const std::function<void (data::HttpRequest&, data::HttpResponse&)>& function);

            void addModule(module::HandlerModule &module);

            size_t getHandlerSize() const;

            void start();
	    friend void startServerThread(Server* server);

        private:
            void startListen(event_base *evbase, SSL_CTX* ssl_ctx, event_loop::app_context *app_ctx);
        public:
            std::shared_ptr<configuration::ConfigReader> config;
	    evconnlistener* listener;
	    event_loop::server_data* server_dt;

        private:
            std::shared_ptr<regex::basic::BasicEndpointHandler> endpoint_handler;
            std::vector<std::thread> handlers;
        };
    }; // handler
}; // unit::server

#endif //SERVER_H
