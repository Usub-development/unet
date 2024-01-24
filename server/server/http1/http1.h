//
// Created by Kirill Zhukov on 07.01.2024.
//

#ifndef HTTP1LOOP_H
#define HTTP1LOOP_H


#include <cstdio>
#include <cstdlib>
#include <string>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <netinet/tcp.h>
#include <cstring>
#include <cerrno>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <event.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/bufferevent_ssl.h>
#include <event2/listener.h>
#include "data/types.h"
#include "logging/logger.h"
#include "data/HttpRequest.h"
#include "handler/BasicEndpointHandler.h"
#include "socket/socket.h"
#include "globals.h"
#include "loop/loop.h"

namespace unit::server {
    bufferevent* bevcb(struct event_base* base, void* arg);

    class Http1Handler {
    public:
        Http1Handler(std::shared_ptr<regex::basic::BasicEndpointHandler>&basic_endpoint_handler, event_base* evbase,
                     SSL_CTX* ssl_ctx, int port, const std::string&ip_addr, int backlog, IPV ipv, bool is_ssl, bool statusError = true, const std::string& err_path = "");

        ~Http1Handler();

        friend void request_handler(struct evhttp_request* req, void* arg);
    private:
        [[nodiscard]] void setErrorPage(data::Http1Response* http_response) const;
    private:
        std::shared_ptr<regex::basic::BasicEndpointHandler> endpoint_handler;
        SSL_CTX* ctx;
        event_base* evbase;
        evhttp* http;
        int port;
        const std::string&ip_addr;
        int soc_fd;
        bool statusError;
        std::string error_path;
    };
}; // unit::server

#endif //HTTP1LOOP_H
