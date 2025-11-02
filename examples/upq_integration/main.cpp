#include "upq/PgPool.h"
#include "uvent/Uvent.h"
#include "server/server.h"
#include "Protocols/HTTP/Message.h"

#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include <iostream>

// simple structure for demonstration
struct UserRow
{
    int64_t id;
    std::string name;
};

usub::uvent::task::Awaitable<std::optional<UserRow>>
fetch_user_by_id(int64_t user_id)
{
    auto& pool = usub::pg::PgPool::instance();

    auto res = co_await pool.query_awaitable(
        "SELECT id, name FROM users WHERE id = $1;",
        user_id
    );

    if (!res.ok)
    {
        std::cout << "[db] query failed: " << res.error << std::endl;
        co_return std::nullopt;
    }

    if (res.rows.empty())
    {
        co_return std::nullopt;
    }

    const auto& row = res.rows[0];

    UserRow out{
        .id = std::stoll(std::string(row.cols[0])),
        .name = std::string(row.cols[1]),
    };

    co_return out;
}

// HTTP handler coroutine
ServerHandler get_user_handler(usub::server::protocols::http::Request& request,
                               usub::server::protocols::http::Response& response)
{
    std::string id_raw;
    if (auto it = request.uri_params.find("id"); it != request.uri_params.end())
    {
        id_raw = it->second;
    }

    int64_t user_id = 0;
    try
    {
        user_id = std::stoll(id_raw);
    }
    catch (...)
    {
        response.setStatus(400)
                .setMessage("Bad Request")
                .addHeader("Content-Type", "application/json")
                .setBody("{\"error\":\"invalid id\"}\n");
        co_return;
    }

    auto user_opt = co_await fetch_user_by_id(user_id);

    if (!user_opt.has_value())
    {
        response.setStatus(404)
                .setMessage("Not Found")
                .addHeader("Content-Type", "application/json")
                .setBody("{\"error\":\"user not found\"}\n");
        co_return;
    }

    const auto& u = *user_opt;

    // very basic JSON serialization
    std::string body;
    body.reserve(128);
    body += "{";
    body += "\"id\":";
    body += std::to_string(u.id);
    body += ",\"name\":\"";
    for (char c : u.name)
    {
        if (c == '\"') body += "\\\"";
        else if (c == '\\') body += "\\\\";
        else body += c;
    }
    body += "\"}";

    response.setStatus(200)
            .setMessage("OK")
            .addHeader("Content-Type", "application/json")
            .setBody(body);

    co_return;
}

// simple middleware examples
bool headerMiddle(const usub::server::protocols::http::Request& request,
                  usub::server::protocols::http::Response& response)
{
    std::cout << "[mw] header phase" << std::endl;
    return true;
}

bool globalMiddle(const usub::server::protocols::http::Request& request,
                  usub::server::protocols::http::Response& response)
{
    return true;
}

bool responseMiddle(const usub::server::protocols::http::Request& request,
                    usub::server::protocols::http::Response& response)
{
    response.addHeader("X-Powered-By", "uvent+upq");
    return true;
}

int main()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, nullptr);

    // initialize PostgreSQL connection pool
    {
        usub::pg::PgPool::init_global(
            "localhost", // host
            "12432", // port
            "postgres", // user
            "postgres", // db
            "password", // password
            /*max_pool_size*/ 32,
            /*queue_capacity*/ 64
        );

        std::cout << "[init] PgPool ready" << std::endl;
    }

    // create server (no SSL)
    usub::server::Server server_no_ssl("../config.toml");

    // global middleware
    server_no_ssl.addMiddleware(
        usub::server::protocols::http::MiddlewarePhase::HEADER,
        globalMiddle
    );

    // param constraint: numeric ID
    const usub::server::protocols::http::param_constraint numeric_id{
        R"(\d+)",
        "Must be numeric id"
    };

    // register GET /user/{id}
    server_no_ssl
        .handle({"GET"},
                "/user/{id}",
                get_user_handler)
        .addMiddleware(usub::server::protocols::http::MiddlewarePhase::HEADER,
                       headerMiddle)
        .addMiddleware(usub::server::protocols::http::MiddlewarePhase::RESPONSE,
                       responseMiddle);

    // register /hello route
    server_no_ssl
        .handle({"*"}, R"(/hello)",
                [](auto& req, auto& resp) -> usub::uvent::task::Awaitable<void>
                {
                    resp.setStatus(200)
                        .setMessage("OK")
                        .addHeader("Content-Type", "text/plain")
                        .setBody("Hello World\n");
                    co_return;
                })
        .addMiddleware(usub::server::protocols::http::MiddlewarePhase::HEADER,
                       headerMiddle)
        .addMiddleware(usub::server::protocols::http::MiddlewarePhase::RESPONSE,
                       responseMiddle);

    // start server (blocking)
    server_no_ssl.run();

    return 0;
}
