#pragma once

/**
 * @file Client.h
 * @brief Modern HTTP client library with support for both plain HTTP and HTTPS
 *
 * This is the main entry point for the HTTP client. It provides:
 * - Templated client supporting different stream handlers (Plain, TLS)
 * - Request/Response builders with fluent API (like cpr or requests library)
 * - Support for async/await with coroutines
 * - HTTPS support with OpenSSL (when USE_OPEN_SSL is defined)
 *
 * Example usage (Plain HTTP):
 *   auto client = usub::client::HttpClientPlain();
 *   auto resp = co_await client.send(usub::client::requests::Get("http://example.com"));
 *   std::cout << "Status: " << resp.status << std::endl;
 *   std::cout << "Body: " << resp.body << std::endl;
 *
 * Example usage (HTTPS):
 *   auto client = usub::client::HttpClientTls();
 *   auto resp = co_await client.send(usub::client::requests::Post("https://api.example.com/endpoint")
 *       .with_header("Content-Type", "application/json")
 *       .with_body(R"({"key": "value"})"));
 *
 * Example with convenience methods:
 *   auto resp = co_await client.get("http://example.com");
 *   auto resp = co_await client.post("http://example.com", "request body");
 */

#include "HttpClient.h"
#include "Protocols/HTTP/Message.h"
#include "StreamHandlers.h"


namespace usub::client {
    // All public types are exported from the included headers
}// namespace usub::client
