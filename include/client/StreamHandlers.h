#pragma once

#include <cstdint>
#include <cstring>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <uvent/Uvent.h>
#include <uvent/net/Socket.h>
#include <uvent/tasks/Awaitable.h>
#include <uvent/utils/buffer/DynamicBuffer.h>

namespace usub::client {

    static constexpr std::size_t DEFAULT_BUFFER_SIZE = 64 * 1024;

    enum class HttpClientError : uint8_t {
        Ok,
        NoHost,
        UrlParseFailed,
        ConnectFailed,
        TlsUnavailable,
        TlsCtxCreateFailed,
        TlsSslCreateFailed,
        TlsBioCreateFailed,
        TlsHandshakeFailed,
        WriteFailed,
        ReadFailed,
        ProtocolError,
        BodyTooLarge,
        UnexpectedEof,
        InternalError,
    };

    struct HttpError {
        HttpClientError code{HttpClientError::InternalError};
        std::string message;
    };

    template<class T>
    using HttpExpected = std::expected<T, HttpError>;

    inline HttpError make_err(HttpClientError c, std::string msg) {
        return HttpError{c, std::move(msg)};
    }

    struct HttpStreamHandlerInterface {
        virtual ~HttpStreamHandlerInterface() = default;

        virtual usub::uvent::task::Awaitable<HttpExpected<void>> initialize() = 0;
        virtual usub::uvent::task::Awaitable<HttpExpected<std::string>> async_read(std::size_t max_bytes) = 0;
        virtual usub::uvent::task::Awaitable<HttpExpected<std::size_t>> async_write(const uint8_t *data, std::size_t len) = 0;

        virtual void shutdown() = 0;
        virtual int get_fd() = 0;
    };

    class PlainHttpStreamHandler final : public HttpStreamHandlerInterface {
    private:
        usub::uvent::net::TCPClientSocket socket_;
        bool initialized_{false};

    public:
        explicit PlainHttpStreamHandler(usub::uvent::net::TCPClientSocket sock)
            : socket_(std::move(sock)), initialized_(true) {}

        PlainHttpStreamHandler() = default;

        usub::uvent::task::Awaitable<HttpExpected<void>> initialize() override {
            if (!initialized_) co_return std::unexpected(make_err(HttpClientError::InternalError, "plain stream not initialized"));
            co_return {};
        }

        usub::uvent::task::Awaitable<HttpExpected<std::string>> async_read(std::size_t max_bytes) override {
            usub::uvent::utils::DynamicBuffer buf;
            buf.reserve(max_bytes);
            buf.clear();

            const ssize_t rd = co_await socket_.async_read(buf, max_bytes);
            if (rd < 0) co_return std::unexpected(make_err(HttpClientError::ReadFailed, "plain read failed"));
            if (rd == 0) co_return std::string{};

            const auto n = static_cast<std::size_t>(rd);
            co_return std::string(reinterpret_cast<const char *>(buf.data()), n);
        }

        usub::uvent::task::Awaitable<HttpExpected<std::size_t>> async_write(const uint8_t *data, std::size_t len) override {
            if (!data && len != 0)
                co_return std::unexpected(make_err(HttpClientError::InternalError, "plain write: null data"));

            const ssize_t wr = co_await socket_.async_write(const_cast<uint8_t *>(data), len);
            if (wr <= 0) co_return std::unexpected(make_err(HttpClientError::WriteFailed, "plain write failed"));
            co_return static_cast<std::size_t>(wr);
        }

        void shutdown() override { socket_.shutdown(); }

        int get_fd() override {
            auto *h = socket_.get_raw_header();
            return h ? static_cast<int>(h->fd) : -1;
        }

        usub::uvent::net::TCPClientSocket &get_socket() { return socket_; }
    };

#ifdef USE_OPEN_SSL

#include <openssl/err.h>
#include <openssl/ssl.h>

    inline std::string openssl_last_error_string() {
        unsigned long e = ERR_get_error();
        if (e == 0) return "unknown OpenSSL error";
        char buf[256];
        ERR_error_string_n(e, buf, sizeof(buf));
        return {buf};
    }

    class TlsHttpStreamHandler final : public HttpStreamHandlerInterface {
    private:
        usub::uvent::net::TCPClientSocket socket_;
        std::string host_;
        std::string port_;

        SSL_CTX *ctx_{nullptr};
        SSL *ssl_{nullptr};

        bool shutdown_called_{false};

        static constexpr std::size_t CHUNK = 16 * 1024;

    public:
        explicit TlsHttpStreamHandler(usub::uvent::net::TCPClientSocket sock,
                                      std::string host,
                                      std::string port)
            : socket_(std::move(sock)), host_(std::move(host)), port_(std::move(port)) {}

        ~TlsHttpStreamHandler() override { shutdown(); }

        usub::uvent::task::Awaitable<HttpExpected<void>> initialize() override {
            const SSL_METHOD *method = TLS_client_method();
            ctx_ = SSL_CTX_new(method);
            if (!ctx_) co_return std::unexpected(make_err(HttpClientError::TlsCtxCreateFailed, "SSL_CTX_new failed"));

            (void) SSL_CTX_set_default_verify_paths(ctx_);
            SSL_CTX_set_verify(ctx_, SSL_VERIFY_NONE, nullptr);
            SSL_CTX_set_options(ctx_, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION);
            (void) SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION);

            ssl_ = SSL_new(ctx_);
            if (!ssl_) {
                SSL_CTX_free(ctx_);
                ctx_ = nullptr;
                co_return std::unexpected(make_err(HttpClientError::TlsSslCreateFailed, "SSL_new failed: " + openssl_last_error_string()));
            }

            BIO *rbio = BIO_new(BIO_s_mem());
            BIO *wbio = BIO_new(BIO_s_mem());
            if (!rbio || !wbio) {
                if (rbio) BIO_free(rbio);
                if (wbio) BIO_free(wbio);
                SSL_free(ssl_);
                SSL_CTX_free(ctx_);
                ssl_ = nullptr;
                ctx_ = nullptr;
                co_return std::unexpected(make_err(HttpClientError::TlsBioCreateFailed, "BIO_new failed"));
            }

            SSL_set_bio(ssl_, rbio, wbio);
            SSL_set_connect_state(ssl_);

            if (!host_.empty()) (void) SSL_set_tlsext_host_name(ssl_, host_.c_str());

            auto hs = co_await handshake();
            if (!hs) {
                auto err = hs.error();
                shutdown();
                co_return std::unexpected(std::move(err));
            }

            co_return {};
        }

        usub::uvent::task::Awaitable<HttpExpected<std::string>> async_read(std::size_t max_bytes) override {
            if (!ssl_) co_return std::unexpected(make_err(HttpClientError::InternalError, "tls read: ssl is null"));
            if (max_bytes == 0) co_return std::string{};

            const std::size_t want = (max_bytes > CHUNK) ? CHUNK : max_bytes;
            std::string out;
            out.resize(want);

            for (;;) {
                const int rc = SSL_read(ssl_, out.data(), static_cast<int>(want));
                if (rc > 0) {
                    out.resize(static_cast<std::size_t>(rc));
                    co_return out;
                }

                const int err = SSL_get_error(ssl_, rc);

                if (err == SSL_ERROR_WANT_READ) {
                    auto r = co_await read_into_rbio(CHUNK);
                    if (!r) co_return std::unexpected(r.error());
                    continue;
                }
                if (err == SSL_ERROR_WANT_WRITE) {
                    auto f = co_await flush_wbio();
                    if (!f) co_return std::unexpected(f.error());
                    continue;
                }
                if (err == SSL_ERROR_ZERO_RETURN) {
                    co_return std::string{};
                }

                if (err == SSL_ERROR_SYSCALL) {
                    if (rc == 0) co_return std::string{};
                    co_return std::unexpected(make_err(HttpClientError::ReadFailed, "tls read syscall failed: " + openssl_last_error_string()));
                }

                co_return std::unexpected(make_err(HttpClientError::ReadFailed, "tls read failed: " + openssl_last_error_string()));
            }
        }

        usub::uvent::task::Awaitable<HttpExpected<std::size_t>> async_write(const uint8_t *data, std::size_t len) override {
            if (!ssl_) co_return std::unexpected(make_err(HttpClientError::InternalError, "tls write: ssl is null"));
            if (!data && len != 0) co_return std::unexpected(make_err(HttpClientError::InternalError, "tls write: null data"));
            if (len == 0) co_return std::size_t{0};

            std::size_t written = 0;

            while (written < len) {
                const int rc = SSL_write(ssl_, data + written, static_cast<int>(len - written));

                auto f = co_await flush_wbio();
                if (!f) co_return std::unexpected(f.error());

                if (rc > 0) {
                    written += static_cast<std::size_t>(rc);
                    continue;
                }

                const int err = SSL_get_error(ssl_, rc);

                if (err == SSL_ERROR_WANT_READ) {
                    auto r = co_await read_into_rbio(CHUNK);
                    if (!r) co_return std::unexpected(r.error());
                    continue;
                }
                if (err == SSL_ERROR_WANT_WRITE) {
                    auto f2 = co_await flush_wbio();
                    if (!f2) co_return std::unexpected(f2.error());
                    continue;
                }
                if (err == SSL_ERROR_ZERO_RETURN) {
                    co_return written;
                }

                if (err == SSL_ERROR_SYSCALL) {
                    co_return std::unexpected(make_err(HttpClientError::WriteFailed, "tls write syscall failed: " + openssl_last_error_string()));
                }

                co_return std::unexpected(make_err(HttpClientError::WriteFailed, "tls write failed: " + openssl_last_error_string()));
            }

            co_return written;
        }

        void shutdown() override {
            if (shutdown_called_) return;
            shutdown_called_ = true;

            if (ssl_) {
                (void) SSL_shutdown(ssl_);
                SSL_free(ssl_);
                ssl_ = nullptr;
            }
            if (ctx_) {
                SSL_CTX_free(ctx_);
                ctx_ = nullptr;
            }
            socket_.shutdown();
        }

        int get_fd() override {
            auto *h = socket_.get_raw_header();
            return h ? static_cast<int>(h->fd) : -1;
        }

        usub::uvent::net::TCPClientSocket &get_socket() { return socket_; }

    private:
        usub::uvent::task::Awaitable<HttpExpected<void>> handshake() {
            if (!ssl_) co_return std::unexpected(make_err(HttpClientError::InternalError, "handshake: ssl null"));

            for (;;) {
                const int rc = SSL_do_handshake(ssl_);

                auto f = co_await flush_wbio();
                if (!f) co_return std::unexpected(f.error());

                if (rc == 1) co_return {};

                const int err = SSL_get_error(ssl_, rc);
                if (err == SSL_ERROR_WANT_READ) {
                    auto r = co_await read_into_rbio(CHUNK);
                    if (!r) co_return std::unexpected(r.error());
                    continue;
                }
                if (err == SSL_ERROR_WANT_WRITE) {
                    continue;
                }

                co_return std::unexpected(make_err(HttpClientError::TlsHandshakeFailed, "TLS handshake failed: " + openssl_last_error_string()));
            }
        }

        usub::uvent::task::Awaitable<HttpExpected<void>> flush_wbio() {
            if (!ssl_) co_return std::unexpected(make_err(HttpClientError::InternalError, "flush_wbio: ssl null"));

            BIO *wbio = SSL_get_wbio(ssl_);
            if (!wbio) co_return {};

            std::vector<uint8_t> tmp(CHUNK);

            for (;;) {
                const size_t pending = BIO_ctrl_pending(wbio);
                if (pending == 0) break;

                const int to_read = (pending < CHUNK) ? static_cast<int>(pending) : static_cast<int>(CHUNK);
                const int n = BIO_read(wbio, tmp.data(), to_read);
                if (n <= 0) break;

                const ssize_t wr = co_await socket_.async_write(tmp.data(), static_cast<std::size_t>(n));
                if (wr <= 0) co_return std::unexpected(make_err(HttpClientError::WriteFailed, "tls flush_wbio socket write failed"));
            }

            co_return {};
        }

        usub::uvent::task::Awaitable<HttpExpected<void>> read_into_rbio(std::size_t max_chunk) {
            if (!ssl_) co_return std::unexpected(make_err(HttpClientError::InternalError, "read_into_rbio: ssl null"));

            usub::uvent::utils::DynamicBuffer buf;
            buf.reserve(max_chunk);
            buf.clear();

            const ssize_t rd = co_await socket_.async_read(buf, max_chunk);
            if (rd < 0) co_return std::unexpected(make_err(HttpClientError::ReadFailed, "tls read_into_rbio socket read failed"));
            if (rd == 0) co_return std::unexpected(make_err(HttpClientError::ReadFailed, "tls read_into_rbio eof"));

            BIO *rbio = SSL_get_rbio(ssl_);
            if (!rbio) co_return std::unexpected(make_err(HttpClientError::InternalError, "tls read_into_rbio: no rbio"));

            const int written = BIO_write(rbio, buf.data(), static_cast<int>(rd));
            if (written <= 0)
                co_return std::unexpected(make_err(HttpClientError::ReadFailed, "BIO_write(rbio) failed: " + openssl_last_error_string()));

            co_return {};
        }
    };

#endif// USE_OPEN_SSL

}// namespace usub::client
