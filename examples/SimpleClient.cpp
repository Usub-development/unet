// A minimal HTTP client example using include/client/Client.h
#include "client/Client.h"

#include "uvent/Uvent.h"
#include "uvent/system/SystemContext.h"

#include <csignal>
#include <iostream>
#include <string>

using usub::uvent::task::Awaitable;

static void ignore_sigpipe() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, nullptr);
}

// Simple coroutine that connects, sends a GET request, and prints the response
Awaitable<void> run_client(const std::string &url, const std::string &path) {
    usub::client::HttpClient client;
    std::cout << "\n===== Client =====\n";
    // use the convenience `get` which accepts a full URL
    auto full = url + (path.empty() ? std::string("/") : path);
    auto resp = co_await client.get(full);
    if (resp.getStatus() == 0) {
        std::cerr << "Request failed or no response\n";
        co_return;
    }

    std::cout << "\n===== Response Body =====\n";
    std::cout << resp.getBody() << std::endl;
    std::cout << "===== End Response =====\n";

    co_return;
}

int main() {
    ignore_sigpipe();

    // Adjust the URL/port if your server listens elsewhere
    const std::string url = "https://example.com";// matches config/https.toml non-SSL listener
    const std::string path = "";                  // endpoint from SimpleServer example

    // Create event loop with a single thread and schedule the client coroutine
    auto uvent = std::make_shared<usub::Uvent>(1);
    usub::uvent::system::co_spawn(run_client(url, path));

    // Run the loop until all tasks complete
    uvent->run();
    return 0;
}
