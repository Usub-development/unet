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

	bool connected = co_await client.connect(url);
	if (!connected) {
		std::cerr << "Failed to connect to " << url << "\n";
		co_return;
	}

	// Build a simple HTTP/1.1 GET request
	auto &req = client.getRequest();
	req.getRequestMethod() = "GET";
	req.setUri(path);
	// Minimal headers: Host and Connection
	req.getHeaders().addHeader<usub::server::protocols::http::Request>(std::string("Host"), std::string("example.com"));
	req.getHeaders().addHeader<usub::server::protocols::http::Request>(std::string("Connection"), std::string("close"));

	auto *resp = co_await client.send();
	if (!resp) {
		std::cerr << "No response received\n";
		co_return;
	}

	// Print raw HTTP response (status line, headers, and body)
	std::cout << "\n===== Raw HTTP Response =====\n";
	std::cout << resp->string() << std::endl;
	std::cout << "===== End Response =====\n";

	co_return;
}

int main() {
	ignore_sigpipe();

	// Adjust the URL/port if your server listens elsewhere
	const std::string url = "https://example.com"; // matches config/https.toml non-SSL listener
	const std::string path = "/hello";               // endpoint from SimpleServer example

	// Create event loop with a single thread and schedule the client coroutine
	auto uvent = std::make_shared<usub::Uvent>(1);
	usub::uvent::system::co_spawn(run_client(url, path));

	// Run the loop until all tasks complete
	uvent->run();
	return 0;
}

