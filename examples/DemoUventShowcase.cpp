#include <csignal>
#include <cstring>
#include <iostream>
#include <atomic>
#include <chrono>

#include "uvent/Uvent.h"
#include "uvent/system/SystemContext.h"
#include "uvent/utils/timer/TimerWheel.h"
#include "server/server.h"
#include "Protocols/HTTP/Message.h"

using namespace std::chrono_literals;
using usub::uvent::task::Awaitable;

// ============================================================================
// Coroutine generators (co_yield demo)
// ============================================================================

Awaitable<int> generator()
{
    for (int i = 1; i <= 3; ++i)
    {
        std::cout << "[generator] yield " << i << "\n";
        co_yield i;
    }
    co_return 0;
}

Awaitable<void> consumer()
{
    auto g = generator();
    while (true)
    {
        int v = co_await g;
        std::cout << "[consumer] got " << v << "\n";
        if (g.get_promise()->get_coroutine_handle().done())
            break;
    }
    co_return;
}

// ============================================================================
// Background and timed tasks
// ============================================================================

Awaitable<void> metrics_task()
{
    for (;;)
    {
        std::cout << "[metrics] tick\n";
        co_await usub::uvent::system::this_coroutine::sleep_for(250ms);
    }
}

Awaitable<void> background_task()
{
    for (;;)
    {
        std::cout << "[background] work iteration\n";
        co_await usub::uvent::system::this_coroutine::sleep_for(500ms);
    }
}

Awaitable<void> pinned_boot_task(int tid)
{
    std::cout << "[boot] started on thread " << tid << "\n";
    co_await usub::uvent::system::this_coroutine::sleep_for(10ms);
    co_return;
}

// ============================================================================
// Timers using TimerWheel
// ============================================================================

struct Payload
{
    int v;
};

void schedule_one_shot(uint64_t ms)
{
    auto* t = new usub::uvent::utils::Timer(ms, usub::uvent::utils::TimerType::TIMEOUT);
    t->addFunction(
        [](std::any& a)
        {
            auto& p = std::any_cast<Payload&>(a);
            std::cout << "[timer] one-shot fired, v=" << p.v << "\n";
        },
        Payload{42}
    );
    usub::uvent::system::spawn_timer(t);
}

void schedule_periodic(uint64_t ms)
{
    auto* t = new usub::uvent::utils::Timer(ms, usub::uvent::utils::TimerType::INTERVAL);
    t->addFunction([](std::any&) { std::cout << "[timer] periodic tick\n"; }, 0);
    usub::uvent::system::spawn_timer(t);
}

// ============================================================================
// HTTP server demo
// ============================================================================

using Req = usub::server::protocols::http::Request;
using Resp = usub::server::protocols::http::Response;
std::atomic<uint64_t> g_req_count{0};

bool mw_global(const Req&, Resp&)
{
    g_req_count.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool mw_header(const Req& req, Resp&)
{
    std::cout << "[mw/header] headers=" << req.getHeaders().size() << "\n";
    return true;
}

bool mw_response(const Req&, Resp&) { return true; }

// Handler demonstrating co_yield + sleep_for
Awaitable<void> handle_hello(Req&, Resp& res)
{
    std::cout << "[handler] starting co_yield demo\n";
    co_await consumer();
    std::cout << "[handler] sleeping for 1s inside coroutine\n";
    co_await usub::uvent::system::this_coroutine::sleep_for(1s);
    res.setStatus(200)
       .setMessage("OK")
       .addHeader("Content-Type", "text/plain")
       .setBody("Hello from uvent (co_yield + 1s sleep)\n");
    co_return;
}

// Handler with simulated work delay
Awaitable<void> handle_work(Req&, Resp& res)
{
    std::cout << "[handler] /work: simulating async work (500ms)\n";
    co_await usub::uvent::system::this_coroutine::sleep_for(500ms);
    res.setStatus(200)
       .setMessage("OK")
       .addHeader("Content-Type", "text/plain")
       .setBody("Finished simulated async work after 500ms\n");
    co_return;
}

// Handler showing query parsing and async pause
Awaitable<void> handle_echo(Req& req, Resp& res)
{
    std::string out = "URL: " + req.getURL() + "\n";
    for (auto& [k, vals] : req.getQueryParams())
    {
        out += "param " + k + ":\n";
        for (auto& v : vals) out += "  - " + v + "\n";
    }

    std::cout << "[handler] echo sleeping for 200ms\n";
    co_await usub::uvent::system::this_coroutine::sleep_for(200ms);

    res.setStatus(200)
       .setMessage("OK")
       .addHeader("Content-Type", "text/plain")
       .setBody(std::move(out));
    co_return;
}

// Stats handler with small async wait
Awaitable<void> handle_stats(Req&, Resp& res)
{
    auto n = g_req_count.load(std::memory_order_relaxed);
    std::cout << "[handler] stats: async wait 100ms\n";
    co_await usub::uvent::system::this_coroutine::sleep_for(100ms);
    res.setStatus(200)
       .setMessage("OK")
       .addHeader("Content-Type", "application/json")
       .setBody("{\"requests\":" + std::to_string(n) + "}\n");
    co_return;
}

// ============================================================================
// Main
// ============================================================================

static void ignore_sigpipe()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, nullptr);
}

int main()
{
    ignore_sigpipe();

    usub::server::Server server("../config/https.toml");

    server.addMiddleware(usub::server::protocols::http::MiddlewarePhase::HEADER, mw_global);

    server.handle({"GET"}, "/hello", handle_hello)
          .addMiddleware(usub::server::protocols::http::MiddlewarePhase::HEADER, mw_header)
          .addMiddleware(usub::server::protocols::http::MiddlewarePhase::RESPONSE, mw_response);

    server.handle({"GET"}, "/stats", handle_stats);
    server.handle({"GET"}, "/echo", handle_echo);
    server.handle({"GET"}, "/work", handle_work);

    usub::uvent::system::co_spawn_static(pinned_boot_task(0), 0);

    schedule_one_shot(1500);
    schedule_periodic(1000);

    usub::uvent::system::co_spawn(metrics_task());
    usub::uvent::system::co_spawn(background_task());

    // Timed server stop after 20 seconds
    {
        auto* t = new usub::uvent::utils::Timer(20'000, usub::uvent::utils::TimerType::TIMEOUT);
        t->addFunction(
            [](std::any& a)
            {
                auto* srv = std::any_cast<usub::server::Server*>(a);
                std::cout << "[main] stopping server after 20s\n";
                srv->stop();
            },
            &server
        );
        usub::uvent::system::spawn_timer(t);
    }

    std::cout << "[main] running server...\n";
    server.run();
    std::cout << "[main] exited\n";
    return 0;
}
