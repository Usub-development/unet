//
// Created by Kirill Zhukov on 15.11.2023.
//

#include "Server.h"
#include "Server.h"
#include <utility>
#include "data/HttpRequest.h"
#include "BasicEndpointHandler.h"
#include "ssl/CTX_util.h"
#include "http1/http1.h"
#include "logging/logger.h"
#include "socket/socket.h"
#include "event.h"
#include <event2/listener.h>
#include <event2/thread.h>
#include "globals.h"

namespace unit::server::handler {
    void startServerThread(Server* server) {
        bool is_ssl = server->config->is_ssl();
        auto http_v = static_cast<protocols::http>(server->config->getProtocolVersion());
        auto key_file = server->config->getKeyFilePath();
        auto cert_file = server->config->getPemFilePath();
        event_base* evbase = event_base_new(); {
            std::lock_guard<std::mutex> lock(eventBasesMutex);
            eventBases.push_back(evbase);
        }
        if (!key_file.empty() && !cert_file.empty() && is_ssl) {
            if (http_v == 2) {
                info_log("Uses HTTP2");
                SSL_CTX* ssl_ctx = event_loop::create_ssl_ctx(key_file.c_str(),
                                                              server->config->getPemFilePath().c_str());
                event_loop::app_context app_ctx(ssl_ctx, evbase);

                evthread_use_pthreads();
                server->startListen(evbase, ssl_ctx, &app_ctx);

                event_base_loop(evbase, 0);
                delete server->server_dt;
                evconnlistener_free(server->listener);
                event_base_free(evbase);
                SSL_CTX_free(ssl_ctx);
            }
            else {
                int backlog = server->config->getBacklog();
                auto ipv = static_cast<IPV>(server->config->getIPV());
                std::string ip_addr = server->config->getIPAddr();
                info_log("Uses HTTP/1.1 with SSL");
                SSL_CTX* ssl_ctx = create_http1_ctx(key_file.c_str(),
                                                    cert_file.c_str());
                auto http1_handler = Http1Handler(server->endpoint_handler, evbase, ssl_ctx,
                                                  std::stoi(server->config->getPort()), ip_addr, backlog, ipv, true,
                                                  server->config->getStatusError());
            }
        }
        else {
            int backlog = server->config->getBacklog();
            auto ipv = static_cast<IPV>(server->config->getIPV());
            std::string ip_addr = server->config->getIPAddr();
            info_log("Uses HTTP/1.1 without SSL");
            auto http1_handler = Http1Handler(server->endpoint_handler, evbase, nullptr,
                                              std::stoi(server->config->getPort()), ip_addr, backlog, ipv, false,
                                              server->config->getStatusError());
        }
    }

    Server::Server(const std::string&config_path) : listener(nullptr) {
        this->config = std::make_shared<configuration::ConfigReader>(config_path);
        this->endpoint_handler = std::make_shared<regex::basic::BasicEndpointHandler>();
    }

    Server::~Server() {
    }

    void Server::handle(const request::type request_type, const std::string&endpoint,
                        const std::function<void (data::HttpRequest&,
                                                  data::HttpResponse&)>&function) {
        this->endpoint_handler->addHandler(request_type, std::regex(endpoint), function);
    }

    void Server::addModule(module::HandlerModule&module) {
        this->endpoint_handler->addModule(module.getNativeHandler());
    }

    size_t Server::getHandlerSize() const {
        return this->endpoint_handler->getSize();
    }

    void Server::start() {
        int threads = this->config->getThreads();
        for (int i = 0; i < threads - 1; i++) {
            info_log("new thread started: %d", i);
            std::thread serverThread(startServerThread, this);
            serverThread.detach();
        }
        startServerThread(this);
    }

    void Server::startListen(event_base* evbase, SSL_CTX* ssl_ctx,
                             event_loop::app_context* app_ctx) {
        int backlog = this->config->getBacklog();
        auto ipv = static_cast<IPV>(this->config->getIPV());
        std::string ip_addr = this->config->getIPAddr();
        int soc_fd = net::create_socket(std::stoi(this->config->getPort()), ip_addr, backlog, ipv);
        this->server_dt = new event_loop::server_data(app_ctx, this->endpoint_handler);
        this->listener = evconnlistener_new(evbase, event_loop::acceptcb, (void *)server_dt,
                                            LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, 50, soc_fd);
        if (listener) {
            info_log("Server is running on: %s", this->config->getPort().c_str());
            return;
        }
        else {
            const char* error_msg = evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR());
            error_log("Could not create a listener: %s", error_msg);
        }
    }
} // namespace unit::server::handler
