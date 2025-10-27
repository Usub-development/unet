#include "server/server.h"
#include <cstdio>
#include <iostream>
#include <numeric>
#include <thread>

// #include "Coroutines/Coroutine.h"
#include "Protocols/HTTP/Message.h"
// #include "utils/globals.h"


std::optional<std::string> getMultiHeader(std::string &&headerName, const std::unordered_multimap<std::string, std::string> &headers) {
    auto it = headers.find(headerName);
    if (it != headers.end())
        return it->second;
    else
        return std::nullopt;
}

std::string getFileExtension(const std::string &fileName) {
    size_t dotPos = fileName.find_last_of('.');
    if (dotPos != std::string::npos) {
        return fileName.substr(dotPos + 1);
    }
    return "";// Return empty string if no extension found
}

uint64_t request_cntr{};

bool headerMiddle(const usub::server::protocols::http::Request &request, usub::server::protocols::http::Response &response) {
    std::cout << "header middleware reached" << std::endl;
    return true;
}

bool globalMiddle(const usub::server::protocols::http::Request &request, usub::server::protocols::http::Response &response) {
    request_cntr++;
    //    std::cout<<"global middleware reached"<<std::endl;
    return true;
}


bool bodyMiddle(const usub::server::protocols::http::Request &request, usub::server::protocols::http::Response &response) {
    //    std::cout<<"body middleware reached"<<std::endl;
    return true;
}

bool responseMiddle(const usub::server::protocols::http::Request &request, usub::server::protocols::http::Response &response) {
    // std::cout << "request middleware reached " << request_cntr << std::endl;
    return true;
}
usub::uvent::task::Awaitable<int> test() {
    co_return 1;
}

usub::uvent::task::Awaitable<void> test1() {
    std::cout << co_await test() << std::endl;
    co_return;
}

usub::uvent::task::Awaitable<void> handlerFunction(usub::server::protocols::http::Request &request, usub::server::protocols::http::Response &response) {

    auto headers = request.getHeaders();
    for (const auto &[name, values]: headers) {
        std::cout << "Header: " << name << "\n";
        for (const auto &val: values) {
            std::cout << "  Value: " << val << "\n";
        }
    }
    std::cout << "Matched :" << request.getURL() << std::endl;
    for (auto &[k, v]: request.uri_params) {
        std::cout << "param[" << k << "] = " << v << '\n';
    }
    std::cout << "Query Params:\n";
    for (const auto &[key, values]: request.getQueryParams()) {
        std::cout << "key: " << key << '\n';
        for (const auto &value: values) {
            std::cout << "  value: " << value << '\n';
        }
    }
    response.setStatus(200)
            .setMessage("OK")
            .addHeader("Content-Type", "text/html")
            .setBody("Hello World! How are you \n");
    co_await test1();
    co_return;
}

#include "../include/Components/Compression/gzip.h"// FOR TESTING
#include <csignal>
#include <stdexcept>
#include <unistd.h>

void handle_alarm(int sig) {
    std::cerr << "Program timed out after 200 seconds. Exiting...\n";
    exit(0);// Exit cleanly, writing gmon.out
}

int main() {
#ifdef UVENT_DEBUG
    spdlog::set_level(spdlog::level::trace);
#endif
    usub::server::component::Gzip gzip = usub::server::component::Gzip();
    // usub::server::protocols::http::HTTPEndpointHandler router;
    //    usub::server::protocols::http::RadixRouter router;
    //
    //    router.addHandler({"GET"}, "/files/*", handlerFunction, {});
    //    router.addHandler({"GET"}, "/deposit/\\*", handlerFunction, {});
    //    router.addHandler({"GET"}, "/deposit/*", handlerFunction, {});
    //

    const usub::server::protocols::http::param_constraint uuid{
            R"([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})",
            "Must be a valid UUID"};
    //
    //    router.addHandler({"GET"},
    //                      "/deposit/update/{id}/",//  last / is trailing
    //                      handlerFunction,
    //                      {{"id", &uuid}});
    //
    //
    //    router.addPlainStringHandler({"GET"}, "/ping", handlerFunction);
    // router.addHandler({"*"}, "/deposit/*", handlerFunction);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, nullptr);
    signal(SIGALRM, handle_alarm);

    usub::server::Server server_no_ssl("../config/https.toml");
    server_no_ssl.addMiddleware(usub::server::protocols::http::MiddlewarePhase::HEADER, globalMiddle);

    // usub::server::Server server("/root/projects/serverMoving/confSSL.toml");
    // server_no_ssl.handle("*", R"(/.*)", handlerFunction).addMiddleware(usub::server::protocols::http::MiddlewarePhase::HEADER, headerMiddle).addMiddleware(usub::server::protocols::http::MiddlewarePhase::RESPONSE, responseMiddle);

    server_no_ssl.handle({"*"}, R"(/hello)", handlerFunction)
            .addMiddleware(usub::server::protocols::http::MiddlewarePhase::HEADER, headerMiddle)
            .addMiddleware(usub::server::protocols::http::MiddlewarePhase::RESPONSE, responseMiddle);
    server_no_ssl.handle({"GET"},
                         "/deposit/update/{id}/",//  last / is trailing
                         handlerFunction)
            .addMiddleware(usub::server::protocols::http::MiddlewarePhase::HEADER, headerMiddle)
            .addMiddleware(usub::server::protocols::http::MiddlewarePhase::RESPONSE, responseMiddle);

    //     Start a thread to stop the server after 110 seconds
    //     std::thread stop_thread([&server_no_ssl]() {
    //         std::this_thread::sleep_for(std::chrono::seconds(110));
    //         std::cerr << "Stopping server after 110 seconds...\n";
    //         server_no_ssl.stop();
    //     });
    //     stop_thread.detach();

    // std::cout << router.dump();
    server_no_ssl.run();

    return 0;
}
