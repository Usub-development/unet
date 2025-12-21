# Quick Start

This guide shows how to build and run a simple **Webserver** instance.

## Minimal Example

Create a `config.toml`:

```toml
[server]
threads  = 4
backlog  = 128

[[listener]]
port = 25565
ssl  = false
```

For description of each param you can see: [config](config.md)

Create a `main.cpp`:

```cpp
#include "server/server.h"
#include "Protocols/HTTP/Message.h"
#include <iostream>

using namespace usub::server;

ServerHandler handlerFunction(protocols::http::Request &request,
                     protocols::http::Response &response) {
    std::cout << "Matched: " << request.getURL() << std::endl;
    response.setStatus(200)
            .setMessage("OK")
            .addHeader("Content-Type", "text/plain")
            .setBody("Hello World from Webserver!\n");
    co_return;
}

int main() {
    Server server("config.toml");

    // Register a simple route
    server.handle({"GET"}, "/hello", handlerFunction);

    // Run server loop
    server.run();
}
```

## Run

Compile and run:

```bash
g++ -std=c++23 main.cpp -Iinclude -Lbuild -lwebserver -o demo
./demo
```

## Test

Send a request using curl:

```bash
curl -v http://127.0.0.1:8111/hello
```

Expected output:

```
Hello World from Webserver!
```

---

## Next Steps

- Learn how to work with [requests/responses](request-response.md)
- Learn how to add [middleware](middlewares.md).

