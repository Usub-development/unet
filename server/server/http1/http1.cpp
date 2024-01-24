//
// Created by Kirill Zhukov on 07.01.2024.
//
#include "http1.h"

void read_callback(struct bufferevent *bev, void *ctx) {
    struct evbuffer *input = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input);
    char *data = new char[len];
    evbuffer_copyout(input, data, len);

    // Process HTTP/1.1 request data in 'data'
    // ...

    delete[] data;
    data = nullptr;
}

void write_callback(struct bufferevent *bev, void *ctx) {
    // Write callback function (if needed)
}

void event_callback(struct bufferevent *bev, short events, void *ctx) {
    if (events & BEV_EVENT_ERROR)
        perror("Error in bufferevent");
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        bufferevent_free(bev);
    }
}


void unit::server::request_handler(evhttp_request* req, void* arg) {
    const evkeyvalq* headers = evhttp_request_get_input_headers(req);
    auto http_request = data::HttpRequest(-1);
    for (const evkeyval* header = headers->tqh_first; header != nullptr; header = header->next.tqe_next) {
        http_request.headers.emplace(header->key, header->value);
    }
    evhttp_uri* parsed_uri = evhttp_uri_parse(evhttp_request_get_uri(req));
    if (!parsed_uri) {
        evhttp_send_error(req, HTTP_NOTFOUND, "Document not found");
        return;
    }
    const char* path = evhttp_uri_get_path(parsed_uri);
    http_request.headers.emplace(":path", path);
    auto global_handler = static_cast<Http1Handler *>(arg);
    auto type = static_cast<request::type>(evhttp_request_get_command(req));
    auto match = global_handler->endpoint_handler->match(type, path);
    data::Http1Response res = data::Http1Response(req);
    res.setStatusError(global_handler->statusError);
    if (match.has_value()) {
        match.value()(http_request, res);
    }
    else {
        global_handler->setErrorPage(&res);
        res.setStatus(404);
    }
    evhttp_uri_free(parsed_uri);
    res.send();
}

bufferevent* unit::server::bevcb(event_base* base, void* arg) {
    auto ctx = static_cast<std::pair<SSL_CTX *, int> *>(arg);
    return bufferevent_openssl_socket_new(base, ctx->second, SSL_new(ctx->first), BUFFEREVENT_SSL_ACCEPTING,
                                          BEV_OPT_CLOSE_ON_FREE);
}


unit::server::Http1Handler::Http1Handler(std::shared_ptr<regex::basic::BasicEndpointHandler>&basic_endpoint_handler,
                                         event_base* event_base, SSL_CTX* ssl_ctx, int port, const std::string&ip_addr,
                                         int backlog, IPV ipv, bool is_ssl, bool statusError,
                                         const std::string&err_path) : endpoint_handler(basic_endpoint_handler),
                                                                       ctx(ssl_ctx),
                                                                       evbase(event_base), port(port), ip_addr(ip_addr),
                                                                       statusError(statusError),
                                                                       error_path(err_path) {
    this->soc_fd = net::create_socket(port, ip_addr, backlog, ipv);
    this->http = evhttp_new(this->evbase);
    if (!this->http) {
        error_log("Couldn't create an event_base: Exiting\n");
        exit(9);
    }
    if (evhttp_accept_socket(this->http, this->soc_fd) != 0) {
        error_log("Couldn't bind to port: %d. Exiting\n", this->port);
        close(this->soc_fd);
        evhttp_free(this->http);
        event_base_free(this->evbase);
        exit(9);
    }

    evhttp_set_gencb(this->http, request_handler, this);
    if (is_ssl) {
        std::pair data = {this->ctx, this->soc_fd};
        evhttp_set_bevcb(this->http, bevcb, &data);
    }
    info_log("Server is running on: %d", this->port);
    event_base_dispatch(this->evbase); {
        std::lock_guard<std::mutex> lock(eventBasesMutex);
        eventBases.push_back(this->evbase);
    }
}

unit::server::Http1Handler::~Http1Handler() {
    if (this->soc_fd) {
        close(this->soc_fd);
    }
    if (this->http) {
        evhttp_free(this->http);
    }
}

void unit::server::Http1Handler::setErrorPage(data::Http1Response* http_response) const {
    if (this->error_path.empty()) {
        const std::string data = "<html><head><title>404</title></head>"
                "<body><h1>404 Not Found</h1></body></html>";
        http_response->writeRawData(reinterpret_cast<const uint8_t *>(data.data()), data.length());
        http_response->addHeader((char *)"Content-Type", (char *)"text/html; charset=utf-8");
    }
    else {
        http_response->writeFile(this->error_path);
        http_response->addHeader((char *)":status", (char *)"404");
        http_response->addHeader((char *)"Content-Type", (char *)"text/html; charset=utf-8");
    }
}
