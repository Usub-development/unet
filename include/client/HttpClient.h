#pragma once

#include "../Protocols/HTTP/Message.h"
#include "StreamHandlers.h"
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <uvent/Uvent.h>
#include <uvent/net/Socket.h>
#include <uvent/tasks/Awaitable.h>

namespace usub::client {

    static constexpr std::size_t MAX_READ_SIZE = 64 * 1024;
    static constexpr std::size_t MAX_HEADER_SIZE = 8 * 1024;
    static constexpr std::size_t MAX_BODY_SIZE = 10 * 1024 * 1024;// 10MB


    class HttpClient {
    public:
        HttpClient() = default;
        ~HttpClient() = default;

        ::usub::uvent::task::Awaitable<::usub::server::protocols::http::Response> get(const std::string &url) {
            co_return co_await send(url, "GET", {}, "");
        }

        ::usub::uvent::task::Awaitable<::usub::server::protocols::http::Response> post(const std::string &url, const std::string &body,
                                                                                       const std::unordered_map<std::string, std::string> &headers = {}) {
            co_return co_await send(url, "POST", headers, body);
        }

        ::usub::uvent::task::Awaitable<::usub::server::protocols::http::Response>
        send(::usub::server::protocols::http::Request &req) {
            std::string host;
            std::string port = "443";

            // Parse Host header to get host and port
            auto &headers = req.getHeaders();
            if (headers.contains("Host")) {
                const auto &vals = headers.at("Host");
                if (!vals.empty()) {
                    size_t colon = vals.front().find(':');
                    if (colon != std::string::npos) {
                        host = vals.front().substr(0, colon);
                        port = vals.front().substr(colon + 1);
                    } else {
                        host = vals.front();
                    }
                }
            }

            if (host.empty()) {
                ::usub::server::protocols::http::Response out;
                out.setStatus(0);
                out.setBody(std::string("No Host header found"));
                co_return out;
            }

            // Default to HTTPS
            bool use_tls = true;

            co_return co_await send_internal(host, port, use_tls, req.string());
        }

        ::usub::uvent::task::Awaitable<::usub::server::protocols::http::Response>
        send(const std::string &url,
             const std::string &method = "GET",
             const std::unordered_map<std::string, std::string> &headers = {},
             const std::string &body = "") {

            // Parse URL (simple)
            std::string scheme;
            std::string host;
            std::string port;
            std::string path;

            parse_url(url, scheme, host, port, path);
            if (scheme.empty()) scheme = "https";// default to https
            bool use_tls = (scheme == "https");

            // Build server Request for serialization
            ::usub::server::protocols::http::Request req;
            req.setRequestMethod(method);
            req.setUri(path.empty() ? "/" : path);
            req.addHeader("Host", host);
            for (const auto &h: headers) req.addHeader(h.first, h.second);
            if (!body.empty()) req.setBody(body, "");

            std::string serialized = req.string();

            co_return co_await send_internal(host, port, use_tls, serialized);
        }

    private:
        ::usub::uvent::task::Awaitable<::usub::server::protocols::http::Response>
        send_internal(const std::string &host, const std::string &port, bool use_tls, const std::string &serialized) {
            ::usub::server::protocols::http::Response out;
            out.setState(::usub::server::protocols::http::RESPONSE_STATE::VERSION);

            ::usub::uvent::net::TCPClientSocket socket;

            (void) socket;// silence unused-variable warnings when appropriate

            auto ec = co_await socket.async_connect(host.c_str(), port.c_str());
            if (ec.has_value()) {
                out.setStatus(0);
                out.setBody(std::string("connect_failed"));
                co_return out;
            }

            // Create appropriate handler
            std::unique_ptr<HttpStreamHandlerInterface> handler;
            if (use_tls) {
#ifdef USE_OPEN_SSL
                handler = std::make_unique<TlsHttpStreamHandler>(std::move(socket), host, port, false);
#else
                // No OpenSSL available, fallback to plain
                handler = std::make_unique<PlainHttpStreamHandler>(std::move(socket));
#endif
            } else {
                handler = std::make_unique<PlainHttpStreamHandler>(std::move(socket));
            }

            if (!co_await handler->initialize()) {
                out.setStatus(0);
                out.setBody(std::string("handler init failed"));
                handler->shutdown();
                co_return out;
            }

            ssize_t wr = co_await handler->async_write(reinterpret_cast<const uint8_t *>(serialized.data()), serialized.size());
            if (wr <= 0) {
                out.setStatus(0);
                out.setBody(std::string("write_failed"));
                handler->shutdown();
                co_return out;
            }

            // Read all available data (simple approach)
            std::string resp_data;
            resp_data.reserve(4096);
            std::string buf;
            buf.reserve(MAX_READ_SIZE);
            for (;;) {
                ssize_t rd = co_await handler->async_read(buf, MAX_READ_SIZE);
                if (rd < 0) {
                    out.setStatus(0);
                    out.setBody(std::string("read_failed"));
                    break;
                }
                out.parse<::usub::server::protocols::http::VERSION::HTTP_1_X>(buf);
                if (out.getState() == ::usub::server::protocols::http::RESPONSE_STATE::ERROR || out.getState() == ::usub::server::protocols::http::RESPONSE_STATE::FINISHED) {
                    break;
                }
                if (rd == 0) break;
            }

            // // Parse first response line and headers (simple)
            // if (!resp_data.empty()) {
            //     parse_response(resp_data, out);
            // }

            handler->shutdown();
            co_return out;
        }

        static void parse_url(const std::string &url, std::string &scheme, std::string &host, std::string &port,
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

            // host[:port][/path]
            size_t slash = u.find('/');
            std::string hostport = (slash == std::string::npos) ? u : u.substr(0, slash);
            path = (slash == std::string::npos) ? std::string("/") : u.substr(slash);

            size_t colon = hostport.find(':');
            if (colon == std::string::npos) {
                host = hostport;
                port = scheme == "http" ? "80" : "443";
            } else {
                host = hostport.substr(0, colon);
                port = hostport.substr(colon + 1);
            }
        }

        static bool parse_response(const std::string &data, ::usub::server::protocols::http::Response &out) {
            size_t pos = 0;
            size_t eol = data.find("\r\n", pos);
            if (eol == std::string::npos) return false;
            std::string status_line = data.substr(pos, eol - pos);
            pos = eol + 2;

            size_t sp1 = status_line.find(' ');
            if (sp1 == std::string::npos) return false;
            size_t sp2 = status_line.find(' ', sp1 + 1);
            std::string status_str = (sp2 == std::string::npos) ? status_line.substr(sp1 + 1) : status_line.substr(sp1 + 1, sp2 - sp1 - 1);
            try {
                out.setStatus(static_cast<uint16_t>(std::stoul(status_str)));
            } catch (...) { return false; }

            while (pos < data.size()) {
                eol = data.find("\r\n", pos);
                if (eol == std::string::npos) break;
                if (eol == pos) {
                    out.setBody(data.substr(pos + 2));
                    return true;
                }
                std::string line = data.substr(pos, eol - pos);
                pos = eol + 2;
                size_t colon = line.find(':');
                if (colon == std::string::npos) continue;
                std::string k = line.substr(0, colon);
                std::string v = line.substr(colon + 1);
                size_t s = v.find_first_not_of(" \t");
                if (s != std::string::npos) v = v.substr(s);
                size_t e = v.find_last_not_of(" \t\r\n");
                if (e != std::string::npos) v = v.substr(0, e + 1);
                out.addHeader(k, v);
            }
            return true;
        }
    };

}// namespace usub::client
