//
// Created by Kirill Zhukov on 02.12.2023.
//

#ifndef LOOP_H
#define LOOP_H

#include "openssl/ssl.h"

#include <iostream>
#include <utility>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <list>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif /* HAVE_NETDB_H */
#include <csignal>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif /* HAVE_FCNTL_H */
#include <cctype>
#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */
#include <netinet/tcp.h>
#ifndef __sgi
#  include <err.h>
#endif
#include <cstring>
#include <cerrno>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/conf.h>

#include <event.h>
#include <event2/event.h>
#include <event2/bufferevent_ssl.h>
#include <event2/listener.h>
#include <nghttp2/nghttp2.h>
#include "handler/BasicEndpointHandler.h"
#include "request/requestTypesStr.h"
#include "logging/logger.h"
#if DEBUG_EXTENDED_INFO
#include <arpa/inet.h>
#endif

namespace unit::server::event_loop {
#define OUTPUT_WOULDBLOCK_THRESHOLD (1 << 16)

#define ARRLEN(x) (sizeof(x) / sizeof(x[0]))

#define MAKE_NV(NAME, VALUE)                                                   \
  {                                                                            \
    (uint8_t *)NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, sizeof(VALUE) - 1,    \
        NGHTTP2_NV_FLAG_NONE                                                   \
  }
    class app_context;
    typedef app_context app_context;

    typedef struct server_data {
        server_data(app_context *app_ctx, std::shared_ptr<regex::basic::BasicEndpointHandler> &basic_endpoint_handler);

        app_context *app_ctx;
        std::shared_ptr<regex::basic::BasicEndpointHandler> endpoint_handler;
    } server_data;

    class http2_stream_data {
    public:
        explicit http2_stream_data(int32_t stream_id);

        ~http2_stream_data();

        data::HttpRequest http_request;
        data::Http2Response *http_response;
        int32_t stream_id;
        int fd;
    };

    class http_session_data {
    public:
        http_session_data(app_context *app_ctx,
            std::shared_ptr<regex::basic::BasicEndpointHandler> &basic_endpoint_handler, sockaddr *addr, int addrlen);

        http_session_data(app_context *app_ctx,
            std::shared_ptr<regex::basic::BasicEndpointHandler> &basic_endpoint_handler, char *client_addr);

        http_session_data();

        virtual ~http_session_data();

        std::shared_ptr<regex::basic::BasicEndpointHandler> endpoint_handler;
        app_context *app_ctx;
        char *client_addr;
        short events;
    };

    class http2_session_data : public http_session_data {
    public:
        http2_session_data(bufferevent *bev, app_context *app_ctx, std::shared_ptr<regex::basic::BasicEndpointHandler> &basic_endpoint_handler,
            char *client_addr, std::string err_path = "");

        ~http2_session_data() override;

        data::Http2Response *getErrorPage(int32_t stream_id);
        void freeSSL();
        std::list<http2_stream_data *> streams;
        std::string error_path;
        request::RequestTypeStr req_str{};
        bufferevent *bev;
        nghttp2_session *session;

        //test stuff
        struct event *timeout_event;
    };

    class app_context {
    public:
        app_context(SSL_CTX *ssl_ctx, event_base *event_base);

        SSL_CTX *ssl_ctx;
        event_base *evbase;
    };

    static unsigned char next_proto_list[256];
    static size_t next_proto_list_len;

#ifndef OPENSSL_NO_NEXTPROTONEG
    extern int next_proto_cb(SSL *ssl, const unsigned char **data,
        unsigned int *len, void *arg);
#endif /* !OPENSSL_NO_NEXTPROTONEG */

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
    extern int alpn_select_proto_cb(SSL *ssl, const unsigned char **out,
        unsigned char *outlen, const unsigned char *in,
        unsigned int inlen, void *arg);
#endif /* OPENSSL_VERSION_NUMBER >= 0x10002000L */

    /* Create SSL_CTX. */
    extern SSL_CTX *create_ssl_ctx(const char *key_file, const char *cert_file);

    /* Create SSL object */
    extern SSL *create_ssl(SSL_CTX *ssl_ctx);

    extern void remove_stream(http2_session_data *session_data, int32_t stream_id);

    /* Serialize the frame and send (or buffer) the data to
       bufferevent. */
    extern int session_send(http2_session_data *session_data);

    /* Read the data in the bufferevent and feed them into nghttp2 library
       function. Invocation of nghttp2_session_mem_recv() may make
       additional pending frames, so call session_send() at the end of the
       function. */
    extern int session_recv(http2_session_data *session_data);

    extern ssize_t send_callback(nghttp2_session *session, const uint8_t *data,
        size_t length, int flags, void *user_data);

    /* nghttp2_on_header_callback: Called when nghttp2 library emits
       single header name/value pair. */
    extern int on_header_callback(nghttp2_session *session,
        const nghttp2_frame *frame, const uint8_t *name,
        size_t namelen, const uint8_t *value,
        size_t valuelen, uint8_t flags, void *user_data);

    extern int on_begin_headers_callback(nghttp2_session *session,
        const nghttp2_frame *frame,
        void *user_data);

    extern ssize_t raw_data_provider_callback(nghttp2_session *session, int32_t stream_id,
        uint8_t *buf,
        size_t length,
        uint32_t *data_flags, nghttp2_data_source *source,
        void *user_data);

    extern int sd_response(http2_session_data *session_data, int32_t stream_id, data::Http2Response *http_response);

    extern int on_request_recv(http2_session_data *session_data,
        http2_stream_data *stream_data);

    extern int on_frame_recv_callback(nghttp2_session *session,
        const nghttp2_frame *frame, void *user_data);

    extern int on_stream_close_callback(nghttp2_session *session, int32_t stream_id,
        uint32_t error_code, void *user_data);

    extern void initialize_nghttp2_session(http2_session_data *session_data);

    /* Send HTTP/2 client connection header, which includes 24 bytes
       magic octets and SETTINGS frame */
    extern int send_server_connection_header(http2_session_data *session_data);

    /* readcb for bufferevent after client connection header was
       checked. */
    extern void readcb(struct bufferevent *bev, void *ptr);

    /* writecb for bufferevent. To greaceful shutdown after sending or
       receiving GOAWAY, we check the some conditions on the nghttp2
       library and output buffer of bufferevent. If it indicates we have
       no business to this session, tear down the connection. If the
       connection is not going to shutdown, we call session_send() to
       process pending data in the output buffer. This is necessary
       because we have a threshold on the buffer size to avoid too much
       buffering. See send_callback(). */
    extern void writecb(struct bufferevent *bev, void *ptr);

    /* eventcb for bufferevent */
    extern void eventcb(struct bufferevent *bev, short events, void *ptr);

    /* callback for evconnlistener */
    extern void acceptcb(evconnlistener *listener, int fd,
        sockaddr *addr, int addrlen, void *arg);

    /* callback for timeout */
    extern void timeout_cb(evutil_socket_t fd, short event, void *ptr);

    SSL* initializeSSL(SSL_CTX * SSL_ctx, int fd);

    int doHandshake(SSL* ssl);
}


#endif //LOOP_H

