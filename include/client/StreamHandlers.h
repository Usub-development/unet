#pragma once

#include <cstring>
#include <uvent/Uvent.h>
#include <uvent/net/Socket.h>
#include <uvent/net/SocketMetadata.h>
#include <uvent/tasks/Awaitable.h>
#include <uvent/utils/buffer/DynamicBuffer.h>

namespace usub::client {

    static constexpr std::size_t DEFAULT_BUFFER_SIZE = 64 * 1024;

    /**
     * @brief Base interface for HTTP stream handlers (plain or TLS)
     * 
     * Stream handlers manage the low-level I/O for reading/writing HTTP data
     * over a socket connection, with or without encryption.
     */
    struct HttpStreamHandlerInterface {
        virtual ~HttpStreamHandlerInterface() = default;

        /// Initialize the connection (e.g., TLS handshake for encrypted streams)
        virtual usub::uvent::task::Awaitable<bool> initialize() = 0;

        /// Read data from the stream
        virtual usub::uvent::task::Awaitable<ssize_t> async_read(std::string &dst, size_t max_bytes) = 0;

        /// Write data to the stream
        virtual usub::uvent::task::Awaitable<ssize_t> async_write(const uint8_t *data, size_t len) = 0;

        /// Shutdown/close the connection
        virtual void shutdown() = 0;

        /// Get file descriptor (for debugging)
        virtual int get_fd() = 0;
    };

    /**
     * @brief Plain HTTP stream handler (no encryption)
     */
    class PlainHttpStreamHandler : public HttpStreamHandlerInterface {
    private:
        usub::uvent::net::TCPClientSocket socket_;
        bool initialized_{false};

    public:
        explicit PlainHttpStreamHandler(usub::uvent::net::TCPClientSocket sock)
            : socket_(std::move(sock)), initialized_(true) {}

        PlainHttpStreamHandler() : initialized_(false) {}

        usub::uvent::task::Awaitable<bool> initialize() override {
            co_return initialized_;
        }

        usub::uvent::task::Awaitable<ssize_t> async_read(std::string &dst, size_t max_bytes) override {
            usub::uvent::utils::DynamicBuffer buffer;
            buffer.reserve(max_bytes);
            ssize_t read_sz = co_await socket_.async_read(buffer, max_bytes);
            if (read_sz > 0) {
                dst = std::string(buffer.data(), buffer.data() + buffer.size());
            }
            co_return read_sz;
        }

        usub::uvent::task::Awaitable<ssize_t> async_write(const uint8_t *data, size_t len) override {
            co_return co_await socket_.async_write(const_cast<uint8_t *>(data), len);
        }

        void shutdown() override {
            socket_.shutdown();
        }

        int get_fd() override {
            auto *h = socket_.get_raw_header();
            return h ? static_cast<int>(h->fd) : -1;
        }

        usub::uvent::net::TCPClientSocket &get_socket() {
            return socket_;
        }

        void set_socket(usub::uvent::net::TCPClientSocket sock) {
            socket_ = std::move(sock);
            initialized_ = true;
        }
    };

#ifdef USE_OPEN_SSL

#include "utils/ssl/SSLHelper.h"
#include <openssl/err.h>
#include <openssl/ssl.h>

    /**
     * @brief TLS/HTTPS stream handler with OpenSSL
     */
    class TlsHttpStreamHandler : public HttpStreamHandlerInterface {
    private:
        ::usub::uvent::net::TCPClientSocket socket_;
        std::string host_;
        std::string port_;
        SSL_CTX *ctx_{nullptr};
        SSL *ssl_{nullptr};
        bool initialized_{false};
        bool shutdown_called_{false};
        bool debug_{false};

        static constexpr size_t CHUNK_SIZE = 16 * 1024;

    public:
        explicit TlsHttpStreamHandler(::usub::uvent::net::TCPClientSocket sock,
                                      std::string host,
                                      std::string port,
                                      bool debug = false)
            : socket_(std::move(sock)), host_(std::move(host)), port_(std::move(port)), debug_(debug) {}

        ::usub::uvent::task::Awaitable<bool> initialize() override {
            // Create client context
            const SSL_METHOD *method = TLS_client_method();
            ctx_ = SSL_CTX_new(method);
            if (!ctx_) {
                co_return false;
            }

            SSL_CTX_set_default_verify_paths(ctx_);
            SSL_CTX_set_verify(ctx_, SSL_VERIFY_NONE, nullptr);
            SSL_CTX_set_options(ctx_, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION);
            SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION);

            // Create SSL object
            ssl_ = SSL_new(ctx_);
            if (!ssl_) {
                SSL_CTX_free(ctx_);
                ctx_ = nullptr;
                co_return false;
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
                co_return false;
            }

            SSL_set_bio(ssl_, rbio, wbio);
            SSL_set_connect_state(ssl_);

            if (!host_.empty()) {
                SSL_set_tlsext_host_name(ssl_, host_.c_str());
            }

            // Perform TLS handshake
            if (!co_await perform_handshake()) {
                shutdown();
                co_return false;
            }

            initialized_ = true;
            co_return true;
        }

        ::usub::uvent::task::Awaitable<ssize_t> async_read(std::string &dst, size_t max_bytes) override {
            if (!ssl_) co_return -1;

            co_await read_into_rbio(max_bytes);

            while (true) {
                int rc = SSL_read(ssl_, dst.data(), static_cast<int>(max_bytes));
                if (rc > 0) {
                    co_return rc;
                }

                int err = SSL_get_error(ssl_, rc);
                if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                    if (!co_await pump_io()) {
                        co_return -1;
                    }
                    continue;
                }
                if (err == SSL_ERROR_ZERO_RETURN) {
                    co_return 0;
                }
                co_return -1;
            }
        }

        ::usub::uvent::task::Awaitable<ssize_t> async_write(const uint8_t *data, size_t len) override {
            if (!ssl_) co_return -1;

            size_t written = 0;
            while (written < len) {
                int rc = SSL_write(ssl_, data + written, static_cast<int>(len - written));
                if (!co_await flush_write_bio()) {
                    co_return -1;
                }

                if (rc > 0) {
                    written += static_cast<size_t>(rc);
                    continue;
                }

                int err = SSL_get_error(ssl_, rc);
                if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                    if (!co_await pump_io()) {
                        co_return -1;
                    }
                    continue;
                }
                if (err == SSL_ERROR_ZERO_RETURN) {
                    co_return static_cast<ssize_t>(written);
                }
                co_return -1;
            }

            co_return static_cast<ssize_t>(written);
        }

        void shutdown() override {
            if (shutdown_called_) return;
            shutdown_called_ = true;

            if (ssl_) {
                SSL_shutdown(ssl_);
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

        ::usub::uvent::net::TCPClientSocket &get_socket() {
            return socket_;
        }

    private:
        ::usub::uvent::task::Awaitable<bool> perform_handshake() {
            while (true) {
                int rc = SSL_do_handshake(ssl_);
                if (!co_await flush_write_bio()) {
                    co_return false;
                }

                if (rc == 1) {
                    co_return true;
                }

                int err = SSL_get_error(ssl_, rc);
                if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                    if (!co_await read_into_rbio(CHUNK_SIZE)) {
                        co_return false;
                    }
                    continue;
                }
                co_return false;
            }
        }

        ::usub::uvent::task::Awaitable<bool> flush_write_bio() {
            if (!ssl_) co_return false;

            BIO *wbio = SSL_get_wbio(ssl_);
            if (!wbio) co_return true;

            std::vector<uint8_t> tmp(CHUNK_SIZE);

            while (true) {
                int pending = BIO_ctrl_pending(wbio);
                if (pending <= 0) break;

                int to_read = pending < static_cast<int>(CHUNK_SIZE) ? pending : static_cast<int>(CHUNK_SIZE);
                int n = BIO_read(wbio, tmp.data(), to_read);
                if (n <= 0) break;

                ssize_t wr = co_await socket_.async_write(tmp.data(), static_cast<size_t>(n));
                if (wr <= 0) {
                    co_return false;
                }
            }

            co_return true;
        }

        ::usub::uvent::task::Awaitable<bool> read_into_rbio(size_t max_chunk) {
            ::usub::uvent::utils::DynamicBuffer buf;
            buf.reserve(max_chunk);
            buf.clear();

            ssize_t rd = co_await socket_.async_read(buf, max_chunk);
            if (rd <= 0) {
                co_return false;
            }

            BIO *rbio = SSL_get_rbio(ssl_);
            if (!rbio) {
                co_return false;
            }

            int written = BIO_write(rbio, buf.data(), static_cast<int>(buf.size()));
            co_return written > 0;
        }

        ::usub::uvent::task::Awaitable<bool> pump_io() {
            if (!co_await read_into_rbio(CHUNK_SIZE)) {
                co_return false;
            }
            co_return co_await flush_write_bio();
        }
    };

#endif// USE_OPEN_SSL

}// namespace usub::client
