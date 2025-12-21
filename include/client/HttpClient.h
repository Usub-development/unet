#pragma once

#include "Protocols/HTTP/Message.h"
#include "StreamHandlers.h"

#include <chrono>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include <uvent/Uvent.h>
#include <uvent/net/Socket.h>
#include <uvent/tasks/Awaitable.h>

namespace usub::client {

    static constexpr std::size_t MAX_READ_SIZE = 64 * 1024;
    static constexpr std::size_t MAX_HEADER_SIZE = 8 * 1024;
    static constexpr std::size_t MAX_BODY_SIZE = 10 * 1024 * 1024;

    class HttpClient {
    public:
        using Resp = ::usub::server::protocols::http::Response;

        explicit HttpClient(std::chrono::milliseconds connect_timeout = std::chrono::milliseconds{0},
                            std::optional<std::size_t> max_response_bytes = std::nullopt)
            : connect_timeout_(connect_timeout), max_response_bytes_(max_response_bytes) {}

        ~HttpClient() = default;

        ::usub::uvent::task::Awaitable<HttpExpected<Resp>> get(const std::string &url) {
            co_return co_await send(url, "GET", {}, "");
        }

        ::usub::uvent::task::Awaitable<HttpExpected<Resp>> post(
                const std::string &url,
                const std::string &body,
                const std::unordered_map<std::string, std::string> &headers = {}) {
            co_return co_await send(url, "POST", headers, body);
        }

        ::usub::uvent::task::Awaitable<HttpExpected<Resp>> send(::usub::server::protocols::http::Request &req) {
            std::string host;
            std::string port = "443";

            auto &headers = req.getHeaders();
            if (headers.contains("Host")) {
                const auto &vals = headers.at("Host");
                if (!vals.empty()) {
                    const std::string &hv = vals.front();
                    const size_t colon = hv.find(':');
                    if (colon != std::string::npos) {
                        host = hv.substr(0, colon);
                        port = hv.substr(colon + 1);
                    } else {
                        host = hv;
                    }
                }
            }

            if (host.empty())
                co_return std::unexpected(make_err(HttpClientError::NoHost, "No Host header found"));

            const bool use_tls = true;
            co_return co_await send_internal(host, port, use_tls, req.string());
        }

        ::usub::uvent::task::Awaitable<HttpExpected<Resp>>
        send(const std::string &url,
             const std::string &method = "GET",
             const std::unordered_map<std::string, std::string> &headers = {},
             const std::string &body = "") {

            std::string scheme, host, port, path;
            if (!parse_url(url, scheme, host, port, path))
                co_return std::unexpected(make_err(HttpClientError::UrlParseFailed, "url parse failed"));

            if (scheme.empty()) scheme = "https";
            const bool use_tls = (scheme == "https");

            ::usub::server::protocols::http::Request req;
            req.setRequestMethod(method);
            req.setUri(path.empty() ? "/" : path);
            req.addHeader("Host", host);

            for (const auto &h: headers) req.addHeader(h.first, h.second);
            if (!body.empty()) req.setBody(body, "");

            co_return co_await send_internal(host, port, use_tls, req.string());
        }

    private:
        ::usub::uvent::task::Awaitable<HttpExpected<Resp>>
        send_internal(const std::string &host, const std::string &port, bool use_tls, const std::string &serialized) {
            Resp out;
            out.setState(::usub::server::protocols::http::RESPONSE_STATE::VERSION);

            ::usub::uvent::net::TCPClientSocket socket;

            auto ec = co_await socket.async_connect(host.c_str(), port.c_str(), this->connect_timeout_);
            if (ec.has_value()) {
                co_return std::unexpected(make_err(HttpClientError::ConnectFailed, "connect_failed"));
            }

            std::unique_ptr<HttpStreamHandlerInterface> handler;

            if (use_tls) {
#ifdef USE_OPEN_SSL
                handler = std::make_unique<TlsHttpStreamHandler>(std::move(socket), host, port);
#else
                co_return std::unexpected(make_err(HttpClientError::TlsUnavailable, "tls requested but OpenSSL disabled"));
#endif
            } else {
                handler = std::make_unique<PlainHttpStreamHandler>(std::move(socket));
            }

            auto init = co_await handler->initialize();
            if (!init) {
                handler->shutdown();
                co_return std::unexpected(init.error());
            }

            auto wr = co_await handler->async_write(reinterpret_cast<const uint8_t *>(serialized.data()), serialized.size());
            if (!wr) {
                handler->shutdown();
                co_return std::unexpected(wr.error());
            }

            std::size_t total_read = 0;

            for (;;) {
                auto chunk = co_await handler->async_read(MAX_READ_SIZE);
                if (!chunk) {
                    handler->shutdown();
                    co_return std::unexpected(chunk.error());
                }

                if (chunk->empty()) {
                    const auto st = out.getState();
                    handler->shutdown();

                    if (st == ::usub::server::protocols::http::RESPONSE_STATE::FINISHED) {
                        co_return out;
                    }
                    if (st == ::usub::server::protocols::http::RESPONSE_STATE::ERROR) {
                        co_return std::unexpected(make_err(HttpClientError::ProtocolError, "parser error before EOF"));
                    }
                    co_return std::unexpected(make_err(HttpClientError::ReadFailed, "EOF before response finished"));
                }

                total_read += chunk->size();
                if (max_response_bytes_.has_value() && total_read > *max_response_bytes_) {
                    handler->shutdown();
                    co_return std::unexpected(make_err(HttpClientError::ReadFailed, "response exceeds max_response_bytes"));
                }

                out.parse<::usub::server::protocols::http::VERSION::HTTP_1_X>(*chunk);

                const auto st = out.getState();
                if (st == ::usub::server::protocols::http::RESPONSE_STATE::ERROR) {
                    handler->shutdown();
                    co_return std::unexpected(make_err(HttpClientError::ProtocolError, "http parse error"));
                }
                if (st == ::usub::server::protocols::http::RESPONSE_STATE::FINISHED) {
                    break;
                }
            }

            handler->shutdown();
            co_return out;
        }

        static bool parse_url(const std::string &url,
                              std::string &scheme,
                              std::string &host,
                              std::string &port,
                              std::string &path) {
            scheme.clear();
            host.clear();
            port.clear();
            path.clear();

            std::string u = url;
            size_t pos = u.find("://");
            if (pos != std::string::npos) {
                scheme = u.substr(0, pos);
                u = u.substr(pos + 3);
            }

            size_t slash = u.find('/');
            std::string hostport = (slash == std::string::npos) ? u : u.substr(0, slash);
            path = (slash == std::string::npos) ? std::string("/") : u.substr(slash);

            if (hostport.empty()) return false;

            size_t colon = hostport.find(':');
            if (colon == std::string::npos) {
                host = hostport;
                if (port.empty())
                    port = (scheme == "http") ? "80" : "443";
            } else {
                host = hostport.substr(0, colon);
                port = hostport.substr(colon + 1);
                if (port.empty()) port = (scheme == "http") ? "80" : "443";
            }

            if (scheme.empty()) scheme = "https";
            if (port.empty()) port = (scheme == "http") ? "80" : "443";
            return !host.empty();
        }

    private:
        std::chrono::milliseconds connect_timeout_;
        std::optional<std::size_t> max_response_bytes_;
    };

}// namespace usub::client
