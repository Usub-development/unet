//
// Created by Kirill Zhukov on 11.12.2023.
///
#include "loop.h"

namespace unit::server::event_loop {
    server_data::server_data(app_context* app_ctx,
                             std::shared_ptr<regex::basic::BasicEndpointHandler>&
                             basic_endpoint_handler) : app_ctx(app_ctx),
        endpoint_handler(basic_endpoint_handler) {
    }

    http2_stream_data::http2_stream_data(int32_t stream_id) : stream_id(stream_id), fd(-1), http_request(stream_id) {
    }

    http2_stream_data::~http2_stream_data() {
    }

    http_session_data::http_session_data(app_context* app_ctx,
                                         std::shared_ptr<regex::basic::BasicEndpointHandler>&basic_endpoint_handler,
                                         sockaddr* addr, int addrlen) : app_ctx(app_ctx),
                                                                        endpoint_handler(basic_endpoint_handler),
                                                                        client_addr(nullptr) {
        int rv;
        char host[NI_MAXHOST];
        this->app_ctx = app_ctx;
        rv = getnameinfo(addr, (socklen_t)addrlen, host, sizeof(host), NULL, 0,
                         NI_NUMERICHOST);
        if (rv != 0) {
            this->client_addr = strdup("(unknown)");
        }
        else {
            this->client_addr = strdup(host);
        }
    }

    http_session_data::http_session_data(app_context* app_ctx,
                                         std::shared_ptr<regex::basic::BasicEndpointHandler>&basic_endpoint_handler,
                                         char* client_addr) : app_ctx(app_ctx),
                                                              endpoint_handler(basic_endpoint_handler),
                                                              client_addr(strdup(client_addr)) {
    }

    http_session_data::http_session_data() {
    }

    http_session_data::~http_session_data() {
        if (this->client_addr != nullptr) {
            free(this->client_addr);
            this->client_addr = nullptr;
        }
    }

    http2_session_data::http2_session_data(bufferevent* bev, app_context* app_ctx,
                                           std::shared_ptr<regex::basic::BasicEndpointHandler>&
                                           basic_endpoint_handler, char* client_addr,
                                           std::string err_path) : http_session_data(app_ctx, basic_endpoint_handler,
                                                                       client_addr), bev(std::move(bev)),
                                                                   error_path(err_path) {
        int rv;
        char host[NI_MAXHOST];

        this->app_ctx = app_ctx;
        timeval timeout = {10, 0};
        event* timeout_event = event_new(this->app_ctx->evbase, -1, EV_PERSIST, timeout_cb, this);
        if (timeout_event == NULL || evtimer_add(timeout_event, &timeout) < 0) {
            fprintf(stderr, "Timeout obj was not created");
            return;
        }
        this->timeout_event = timeout_event;
    }

    http2_session_data::~http2_session_data() {
        SSL* ssl = bufferevent_openssl_get_ssl(this->bev);
        if (this->timeout_event) {
            event_del(this->timeout_event);
            event_free(this->timeout_event);
        }
        for (auto it = streams.begin(); it != streams.end(); ++it) {
            delete *it; // Delete the object pointed to by the iterator
        }
        this->timeout_event = NULL;
        fprintf(stdout, "%s disconnected\n", this->client_addr);
#if DEBUG_EXTENDED_INFO
        printf("destructor\tSSL_in_init: %d, SSL_in_before: %d, SSL_is_init_finished: %d, SSL_in_connect_init: %d, SSL_in_accept_init: %d, SSL_get_state: %d\n",
            SSL_in_init(ssl), SSL_in_before(ssl), SSL_is_init_finished(ssl), SSL_in_connect_init(ssl), SSL_in_accept_init(ssl), SSL_get_state(ssl));
#endif
        //if (ssl && (!SSL_get_shutdown(ssl) && !SSL_in_init(ssl) && !ERR_peek_error() && !SSL_read(ssl, buf, sizeof(buf)))) {

        if (SSL_get_shutdown(ssl) & SSL_RECEIVED_SHUTDOWN) {
            printf("Shutdown alert recieved from peer.\n");
            int res = SSL_shutdown(ssl);
            if (res == 1) {
                printf("shutdown was succesfull in first attempt\n");
            }
            else if (res == -1) {
                printf("res: %d\n", res);
                unsigned long err = ERR_get_error();
                char* msg = ERR_error_string(err, nullptr);
                error_log("SSL error: %s, err_code: %lu", msg, err);
            }
        }
        else if (!(SSL_get_shutdown(ssl) & (SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN)) && !(events & BEV_EVENT_EOF) &&
                 !(events & BEV_EVENT_ERROR) && !(SSL_get_shutdown(ssl) & SSL_RECEIVED_SHUTDOWN)) {
            printf("Shutdown alert sent to peer: %s.\n", this->client_addr);
            int res = SSL_shutdown(ssl);
            if (res == 1) {
                printf("shutdown was succesfull in first attempt\n");
            }
            else if (res == 0) {
                char buf[1024];
                for (int i = 0; i <= 100; i++) {
                    if (SSL_get_error(ssl, res) == SSL_ERROR_WANT_READ || SSL_get_error(ssl, res) ==
                        SSL_ERROR_WANT_WRITE) {
                        for (int result = SSL_get_error(ssl, SSL_read(ssl, buf, sizeof(buf)));
                             result != SSL_ERROR_ZERO_RETURN;) {
                            printf("waiting for conn to be closed, %d", result);
                        }
                    }
                }
            }
            else {
                printf("res: %d\n", res);
                unsigned long err = ERR_get_error();
                char* msg = ERR_error_string(err, nullptr);
                error_log("SSL error: %s, err_code: %lu", msg, err);
            }
        }
        else {
            printf("shutdown request ignored\n");
        }

        bufferevent_free(this->bev);
        if (this->session) {
            nghttp2_session_del(this->session);
        }
    }

    data::Http2Response* http2_session_data::getErrorPage(int32_t stream_id) {
        auto httpRes = new data::Http2Response(stream_id);
        if (this->error_path.empty()) {
            const std::string data = "<html><head><title>404</title></head>"
                    "<body><h1>404 Not Found</h1></body></html>";
            httpRes->writeRawData(reinterpret_cast<const uint8_t *>(data.data()), data.length());
            httpRes->addHeader((char *)":status", (char *)"404");
            httpRes->addHeader((char *)"Content-Type", (char *)"text/html; charset=utf-8");
        }
        else {
            httpRes->writeFile(this->error_path);
            httpRes->addHeader((char *)":status", (char *)"404");
            httpRes->addHeader((char *)"Content-Type", (char *)"text/html; charset=utf-8");
        }
        return httpRes;
    }

    app_context::app_context(SSL_CTX* ssl_ctx, event_base* event_base) : ssl_ctx(ssl_ctx), evbase(event_base) {
    }

    void http2_session_data::freeSSL() {
        SSL* ssl = bufferevent_openssl_get_ssl(this->bev);
        if (ssl) {
            int res = SSL_shutdown(ssl);
            printf("res: %d\n", res);
            unsigned long err;
            while ((err = ERR_get_error()) != 0) {
                char* msg = ERR_error_string(err, nullptr);
                error_log("SSL error: %s", msg);
            }
        }
    }

#ifndef OPENSSL_NO_NEXTPROTONEG
    int next_proto_cb(SSL* ssl, const unsigned char** data,
                      unsigned int* len, void* arg) {
        (void)ssl;
        (void)arg;

        *data = next_proto_list;
        *len = (unsigned int)next_proto_list_len;
        return SSL_TLSEXT_ERR_OK;
    }
#endif

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
    int alpn_select_proto_cb(SSL* ssl, const unsigned char** out,
                             unsigned char* outlen, const unsigned char* in,
                             unsigned int inlen, void* arg) {
        int rv;
        (void)ssl;
        (void)arg;

        rv = nghttp2_select_next_protocol((unsigned char **)out, outlen, in, inlen);

        if (rv != 1) {
            return SSL_TLSEXT_ERR_NOACK;
        }

        return SSL_TLSEXT_ERR_OK;
    }
#endif

    SSL_CTX* create_ssl_ctx(const char* key_file, const char* cert_file) {
        SSL_CTX* ssl_ctx;

        ssl_ctx = SSL_CTX_new(TLS_server_method());
        if (!ssl_ctx) {
            error_log("Could not create SSL/TLS context: %s",
                      ERR_error_string(ERR_get_error(), nullptr));
        }
        SSL_CTX_set_options(ssl_ctx,
                            SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
                            SSL_OP_NO_COMPRESSION |
                            SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        if (SSL_CTX_set1_curves_list(ssl_ctx, "P-256") != 1) {
            error_log("SSL_CTX_set1_curves_list failed: %s",
                      ERR_error_string(ERR_get_error(), nullptr));
        }
#else  /* !(OPENSSL_VERSION_NUMBER >= 0x30000000L) */
        {
            EC_KEY *ecdh;
            ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
            if (!ecdh) {
                error_log("EC_KEY_new_by_curv_name failed: %s",
                    ERR_error_string(ERR_get_error(), nullptr));
            }
            SSL_CTX_set_tmp_ecdh(ssl_ctx, ecdh);
            EC_KEY_free(ecdh);
        }
#endif /* !(OPENSSL_VERSION_NUMBER >= 0x30000000L) */

        if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM) != 1) {
            error_log("Could not read private key file %s", key_file);
        }
        if (SSL_CTX_use_certificate_chain_file(ssl_ctx, cert_file) != 1) {
            error_log("Could not read certificate file %s", cert_file);
        }

        next_proto_list[0] = NGHTTP2_PROTO_VERSION_ID_LEN;
        memcpy(&next_proto_list[1], NGHTTP2_PROTO_VERSION_ID,
               NGHTTP2_PROTO_VERSION_ID_LEN);
        next_proto_list_len = 1 + NGHTTP2_PROTO_VERSION_ID_LEN;

#ifndef OPENSSL_NO_NEXTPROTONEG
        SSL_CTX_set_next_protos_advertised_cb(ssl_ctx, next_proto_cb, nullptr);
#endif /* !OPENSSL_NO_NEXTPROTONEG */

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
        SSL_CTX_set_alpn_select_cb(ssl_ctx, alpn_select_proto_cb, nullptr);
#endif /* OPENSSL_VERSION_NUMBER >= 0x10002000L */

        return ssl_ctx;
    }

    SSL* create_ssl(SSL_CTX* ssl_ctx) {
        SSL* ssl;
        ssl = SSL_new(ssl_ctx);
        if (!ssl) {
            error_log("Could not create SSL/TLS session object: %s",
                      ERR_error_string(ERR_get_error(), nullptr));
        }
        return ssl;
    }

    void remove_stream(http2_session_data* session_data, int32_t stream_id) {
        if (!session_data) {
            return;
        }


        auto&streams = session_data->streams;
        for (auto it = streams.begin(); it != streams.end(); ++it) {
            if ((*it)->stream_id == stream_id) {
                delete (*it)->http_response;
                //        streams.erase(it);
                return;
            }
        }
    }

    int session_send(http2_session_data* session_data) {
        int rv;
        rv = nghttp2_session_send(session_data->session);
        if (rv != 0) {
            error_log("Fatal error: %s, %d", nghttp2_strerror(rv), __LINE__);
            return -1;
        }
        return 0;
    }

    int session_recv(http2_session_data* session_data) {
        ssize_t readlen;
        struct evbuffer* input = bufferevent_get_input(session_data->bev);
        size_t datalen = evbuffer_get_length(input);
        unsigned char* data = evbuffer_pullup(input, -1);

        readlen = nghttp2_session_mem_recv(session_data->session, data, datalen);
#if DEBUG_EXTENDED_INFO
        printf("Recieved data: %.*s\n", datalen, data);
#endif
        if (readlen < 0) {
            error_log("Fatal error: %s, %d", nghttp2_strerror((int)readlen), __LINE__);
            return -1;
        }
        if (evbuffer_drain(input, (size_t)readlen) != 0) {
            error_log("Fatal error: evbuffer_drain failed");
            return -1;
        }
        if (session_send(session_data) != 0) {
            return -1;
        }
        return 0;
    }

    ssize_t send_callback(nghttp2_session* session, const uint8_t* data,
                          size_t length, int flags, void* user_data) {
        http2_session_data* session_data = (http2_session_data *)user_data;
        struct bufferevent* bev = session_data->bev;
        (void)session;
        (void)flags;

        /* Avoid excessive buffering in server side. */
        if (evbuffer_get_length(bufferevent_get_output(session_data->bev)) >=
            OUTPUT_WOULDBLOCK_THRESHOLD) {
            return NGHTTP2_ERR_WOULDBLOCK;
        }
        bufferevent_write(bev, data, length);

        return (ssize_t)length;
    }

    int on_header_callback(nghttp2_session* session,
                           const nghttp2_frame* frame, const uint8_t* name,
                           size_t namelen, const uint8_t* value,
                           size_t valuelen, uint8_t flags, void* user_data) {
        http2_stream_data* stream_data;
        const char PATH[] = ":path";
        (void)flags;
        (void)user_data;

        switch (frame->hd.type) {
            case NGHTTP2_HEADERS:
                if (frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
                    break;
                }
                stream_data =
                        static_cast<http2_stream_data *>(nghttp2_session_get_stream_user_data(session,
                            frame->hd.stream_id));
                if (!stream_data) {
                    break;
                }
                stream_data->http_request.headers.try_emplace(std::string((char *)name, namelen),
                                                              std::string((char *)value, valuelen));
                if (namelen == sizeof(PATH) - 1 && memcmp(PATH, name, namelen) == 0) {
                    size_t j;
                    for (j = 0; j < valuelen && value[j] != '?'; ++j);
                }
                break;
        }
        return 0;
    }

    int on_begin_headers_callback(nghttp2_session* session,
                                  const nghttp2_frame* frame,
                                  void* user_data) {
        http2_session_data* session_data = (http2_session_data *)user_data;
        auto stream_data = new http2_stream_data(frame->hd.stream_id);

        if (frame->hd.type != NGHTTP2_HEADERS ||
            frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
            return 0;
        }
        session_data->streams.push_back(stream_data);
        nghttp2_session_set_stream_user_data(session, frame->hd.stream_id,
                                             stream_data);
        return 0;
    }

    ssize_t raw_data_provider_callback(nghttp2_session* session, int32_t stream_id,
                                       uint8_t* buf,
                                       size_t length,
                                       uint32_t* data_flags, nghttp2_data_source* source,
                                       void* user_data) {
        (void)session;
        (void)stream_id;
        auto* data = static_cast<data::Http2Response *>(source->ptr);

        if (data->type == data::BUFFER) {
            auto buffer = data->getBuffer();
            if (buffer.has_value()) {
                if (data->read_offset >= buffer->get()->size()) {
                    *data_flags |= NGHTTP2_DATA_FLAG_EOF;
                    return 0; // Все данные отправлены
                }

                size_t data_left = buffer->get()->size() - data->read_offset;
                size_t copy_len = (data_left < length) ? data_left : length;

                memcpy(buf, buffer->get()->data() + data->read_offset, copy_len);
                data->read_offset += copy_len;

                if (data->read_offset >= buffer->get()->size()) {
                    *data_flags |= NGHTTP2_DATA_FLAG_EOF;
                }
                return static_cast<ssize_t>(copy_len);
            }
        }
        else {
            const int fd = data->getFD();
            if (fd < 0) {
                error_log("File descriptor corrupted: %s", data->path.c_str());
            }
            ssize_t r;

            while ((r = read(fd, buf, length)) == -1 && errno == EINTR) {
            }

            if (r == -1) {
                return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
            }

            if (r == 0) {
                *data_flags |= NGHTTP2_DATA_FLAG_EOF;
            }

            return r;
        }
    }

    int sd_response(http2_session_data* session_data, int32_t stream_id, data::Http2Response* http_response) {
        int rv;
        nghttp2_data_provider prd;
        prd.source.ptr = http_response;
        prd.read_callback = raw_data_provider_callback;

        auto headers = http_response->getHeaders();
        rv = nghttp2_submit_response(session_data->session, stream_id, headers.data(), headers.size(), &prd);
        if (rv != 0) {
            error_log("Fatal error: %s, %d", nghttp2_strerror(rv), __LINE__);
            return -1;
        }
        return 0;
    }

    int on_request_recv(http2_session_data* session_data,
                        http2_stream_data* stream_data) {
        int rv;
        auto match = session_data->endpoint_handler->match(
            session_data->req_str.getType(stream_data->http_request.headers.at(":method")),
            stream_data->http_request.headers.at(":path"));
        if (match.has_value()) {
            auto* res = new data::Http2Response(stream_data->stream_id);
            match.value()(stream_data->http_request, *res);
            stream_data->http_response = res;
            rv = sd_response(session_data, stream_data->stream_id, res);
        }
        else {
            auto res = session_data->getErrorPage(stream_data->stream_id);
            stream_data->http_response = res;
            rv = sd_response(session_data, stream_data->stream_id, res);
        }
        if (rv != 0) {
            error_log("Error: %s", nghttp2_strerror(rv));
            error_log("Error code: %d", rv);
            return NGHTTP2_ERR_CALLBACK_FAILURE;
        }

        return 0;
    }

    int on_frame_recv_callback(nghttp2_session* session,
                               const nghttp2_frame* frame, void* user_data) {
        auto* session_data = static_cast<http2_session_data *>(user_data);
        http2_stream_data* stream_data;
        switch (frame->hd.type) {
            case NGHTTP2_DATA:
            case NGHTTP2_HEADERS:
#if DEBUG_EXTENDED_INFO
                printf("Recieved HEAdERS frame\n");
#endif
                /* Check that the client request has finished */
                if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
                    stream_data =
                            static_cast<http2_stream_data *>(nghttp2_session_get_stream_user_data(session,
                                frame->hd.stream_id));
                    /* For DATA and HEADERS frame, this callback may be called after
                       on_stream_close_callback. Check that stream still alive. */
                    if (!stream_data) {
                        return 0;
                    }
                    return on_request_recv(session_data, stream_data);
                }
                break;
            case NGHTTP2_GOAWAY: {
                SSL* ssl = bufferevent_openssl_get_ssl(session_data->bev);
#if DEBUG_EXTENDED_INFO
                printf("Recieved GOAWAY frame, SSL state\tSSL_in_init: %d, SSL_in_before: %d, SSL_is_init_finished: %d, SSL_in_connect_init: %d, SSL_in_accept_init: %d, SSL_get_state: %d\n",
                    SSL_in_init(ssl), SSL_in_before(ssl), SSL_is_init_finished(ssl), SSL_in_connect_init(ssl), SSL_in_accept_init(ssl), SSL_get_state(ssl));
#endif
                break;
            }
            default:
                break;
        }
        return 0;
    }

    int on_stream_close_callback(nghttp2_session* session, int32_t stream_id,
                                 uint32_t error_code, void* user_data) {
        http2_session_data* session_data = (http2_session_data *)user_data;
        http2_stream_data* stream_data;
        (void)error_code;

        stream_data = static_cast<http2_stream_data *>(nghttp2_session_get_stream_user_data(session, stream_id));
        if (!stream_data) {
            return 0;
        }
        remove_stream(session_data, stream_id);
        return 0;
    }

    void initialize_nghttp2_session(http2_session_data* session_data) {
        nghttp2_session_callbacks* callbacks;

        nghttp2_session_callbacks_new(&callbacks);

        nghttp2_session_callbacks_set_send_callback(callbacks, send_callback);

        nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks,
                                                             on_frame_recv_callback);

        nghttp2_session_callbacks_set_on_stream_close_callback(
            callbacks, on_stream_close_callback);

        nghttp2_session_callbacks_set_on_header_callback(callbacks,
                                                         on_header_callback);

        nghttp2_session_callbacks_set_on_begin_headers_callback(
            callbacks, on_begin_headers_callback);

        nghttp2_session_server_new(&session_data->session, callbacks, session_data);

        nghttp2_session_callbacks_del(callbacks);
    }

    int send_server_connection_header(http2_session_data* session_data) {
        nghttp2_settings_entry iv[1] = {
            {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100}
        };
        int rv;

        rv = nghttp2_submit_settings(session_data->session, NGHTTP2_FLAG_NONE, iv,
                                     ARRLEN(iv));
        if (rv != 0) {
            error_log("Fatal error: %s, %d", nghttp2_strerror(rv), __LINE__);
            return -1;
        }
        return 0;
    }

    void readcb(struct bufferevent* bev, void* ptr) {
        http2_session_data* session_data = (http2_session_data *)ptr;
        (void)bev;
        if (session_recv(session_data) != 0) {
            delete session_data;
            session_data = nullptr;
        }
        struct timeval timeout = {10, 0};
        evtimer_add(session_data->timeout_event, &timeout);
    }

    void writecb(struct bufferevent* bev, void* ptr) {
        http2_session_data* session_data = (http2_session_data *)ptr;
        SSL* ssl = bufferevent_openssl_get_ssl(bev);
        if (evbuffer_get_length(bufferevent_get_output(bev)) > 0) {
            return;
        }
        if (nghttp2_session_want_read(session_data->session) == 0 &&
            nghttp2_session_want_write(session_data->session) == 0) {
            delete session_data;
            session_data = nullptr;
            return;
        }
        if (session_send(session_data) != 0) {
            delete session_data;
            session_data = nullptr;
            return;
        }
        timeval timeout = {10, 0};
        evtimer_add(session_data->timeout_event, &timeout);
    }

    void timeout_cb(evutil_socket_t fd, short events, void* ptr) {
        http2_session_data* session_data = (http2_session_data *)ptr;
        session_data->events = events;
        delete session_data;
        session_data = nullptr;
    }

    void eventcb(struct bufferevent* bev, short events, void* ptr) {
        if (events & BEV_EVENT_CONNECTED) {
            auto* session_data = static_cast<http_session_data *>(ptr);
            const unsigned char* alpn = nullptr;
            unsigned int alpnlen = 0;
            SSL* ssl;
            (void)bev;

            ssl = bufferevent_openssl_get_ssl(bev);
            fprintf(stdout, "%s connected\n", (session_data->client_addr));

#if DEBUG_EXTENDED_INFO
            printf("eventcb\tSSL_in_init: %d, SSL_in_before: %d, SSL_is_init_finished: %d, SSL_in_connect_init: %d, SSL_in_accept_init: %d, SSL_get_state: %d\n",
                SSL_in_init(ssl), SSL_in_before(ssl), SSL_is_init_finished(ssl), SSL_in_connect_init(ssl), SSL_in_accept_init(ssl), SSL_get_state(ssl));
            if (ssl) {
                // Log SSL session information
                const SSL_SESSION *session = SSL_get_session(ssl);
                if (session) {
                    unsigned int session_id_length;
                    const unsigned char *session_id = SSL_SESSION_get_id(session, &session_id_length);
                    printf("SSL Session ID: ");
                    for (unsigned int i = 0; i < session_id_length; ++i) {
                        printf("%02x", session_id[i]);
                    }
                    printf("\n");
                }
            }
#endif

#ifndef OPENSSL_NO_NEXTPROTONEG
            SSL_get0_next_proto_negotiated(ssl, &alpn, &alpnlen);
#endif /* !OPENSSL_NO_NEXTPROTONEG */
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
            if (alpn == nullptr) {
                SSL_get0_alpn_selected(ssl, &alpn, &alpnlen);
            }
#endif /* OPENSSL_VERSION_NUMBER >= 0x10002000L */

            if (alpn != nullptr && alpnlen >= 2 && memcmp("h2", alpn, 2) == 0) {
                fprintf(stdout, "ALPN: ");
                for (size_t i = 0; i < alpnlen; ++i) {
                    fputc(alpn[i], stdout);
                }
                fprintf(stdout, "\n");
                http2_session_data* new_session_data = new http2_session_data(
                    bev, session_data->app_ctx, session_data->endpoint_handler, session_data->client_addr);
                initialize_nghttp2_session(new_session_data);

                delete session_data;
                session_data = new_session_data;
                bufferevent_setcb(new_session_data->bev, readcb, writecb, eventcb, new_session_data);
            }
            else if (alpn != nullptr && alpnlen >= 2 && memcmp("h3", alpn, 2) == 0) {
                delete session_data;
                session_data = nullptr;
                fprintf(stdout, "%s h3 is not implemented yet\n", alpn);
                bufferevent_free(bev);
                return;
            }
            else {
                delete session_data;
                session_data = nullptr;
                // Fallback to HTTP/1.1 or handle any other protocol
                info_log("Using HTTP/1.1 or an unsupported protocol\n");
                bufferevent_free(bev);
                return;
                // Сделаем вид что в h3 и h1.1 я чищу SSL
            }
            if (send_server_connection_header((http2_session_data *)session_data) != 0 ||
                session_send((http2_session_data *)session_data) != 0) {
                delete (http2_session_data *)session_data;
                session_data = nullptr;
                return;
            }

            return;
        }
        auto* session_data = static_cast<http2_session_data *>(ptr);
        session_data->events = events;
        if (events & BEV_EVENT_EOF) {
            fprintf(stdout, "%s EOF\n", session_data->client_addr);
            bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
            delete session_data;
            session_data = nullptr;
        }
        else if (events & BEV_EVENT_ERROR) {
            fprintf(stdout, "%s network error\n", session_data->client_addr);
            bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
            delete session_data;
            session_data = nullptr;
        }
    }

    void acceptcb(evconnlistener* listener, int fd,
                  sockaddr* addr, int addrlen, void* arg) {
        auto* server_dt = (server_data *)arg;
        auto* session_data = new http_session_data(server_dt->app_ctx, server_dt->endpoint_handler, addr,
                                                   addrlen);
        int val = 1;
        SSL* ssl = create_ssl(server_dt->app_ctx->ssl_ctx);
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(val));

        bufferevent* bev = bufferevent_openssl_socket_new(
            server_dt->app_ctx->evbase, fd, ssl, BUFFEREVENT_SSL_ACCEPTING,
            BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
        bufferevent_enable(bev, EV_READ | EV_WRITE);

        ssl = bufferevent_openssl_get_ssl(bev);

        bufferevent_setcb(bev, readcb, writecb, eventcb, session_data);


#if DEBUG_EXTENDED_INFO
             printf("acceptcb\tSSL_in_init: %d, SSL_in_before: %d, SSL_is_init_finished: %d, SSL_in_connect_init: %d, SSL_in_accept_init: %d, SSL_get_state: %d\n",
                 SSL_in_init(ssl), SSL_in_before(ssl), SSL_is_init_finished(ssl), SSL_in_connect_init(ssl), SSL_in_accept_init(ssl), SSL_get_state(ssl));
             char client_ip[INET6_ADDRSTRLEN];
             int client_port;
             if (addr->sa_family == AF_INET) {
                 struct sockaddr_in *s = (struct sockaddr_in *)addr;
                 inet_ntop(AF_INET, &s->sin_addr, client_ip, sizeof client_ip);
                 client_port = ntohs(s->sin_port);
             }
             else { // AF_INET6
                 struct sockaddr_in6 *s = (struct sockaddr_in6 *)addr;
                 inet_ntop(AF_INET6, &s->sin6_addr, client_ip, sizeof client_ip);
                 client_port = ntohs(s->sin6_port);
             }

             // Log the connection details
             printf("New connection accepted: FD=%d, IP=%s, Port=%d\n", fd, client_ip, client_port);
#endif
    }
}
