# Middleware

Middleware allows you to intercept and process requests/responses at different stages of the HTTP pipeline.  
They are simple functions that take a `Request` and `Response` reference, returning `true` (continue) or `false` (stop processing).

---

## Middleware Phases

Middleware can be registered for specific phases of request handling:


**SETTINGS** - Middleware executed when the uri or pseudo headers become known, to set up handler specific limits
**HEADER** - Middleware executed after the headers were parsed. 
**BODY** - Middleware executed during the body processing phase in certain data types.
**RESPONSE** - Middleware executed during sending response, called only once per response.

---

## Example

```cpp
#include "server/server.h"
#include "Protocols/HTTP/Message.h"
#include <iostream>

using namespace usub::server;

bool globalMiddle(const protocols::http::Request &req, protocols::http::Response &res) {
    std::cout << "Global middleware: request count++" << std::endl;
    return true;
}

bool headerMiddle(const protocols::http::Request &req, protocols::http::Response &res) {
    std::cout << "Header middleware: URL = " << req.getURL() << std::endl;
    return true;
}

bool responseMiddle(const protocols::http::Request &req, protocols::http::Response &res) {
    std::cout << "Response middleware triggered" << std::endl;
    return true;
}

void handler(protocols::http::Request &req, protocols::http::Response &res) {
    res.setStatus(200)
       .setMessage("OK")
       .addHeader("Content-Type", "text/plain")
       .setBody("Hello with middleware!\n");
}

int main() {
    Server server("../config/config.toml");

    // Global middleware applies to all requests
    server.addMiddleware(protocols::http::MiddlewarePhase::HEADER, globalMiddle);

    // Route-specific middleware
    server.handle({"GET"}, "/hello", handler)
          .addMiddleware(protocols::http::MiddlewarePhase::HEADER, headerMiddle)
          .addMiddleware(protocols::http::MiddlewarePhase::RESPONSE, responseMiddle);

    server.run();
}
```

---

## Behavior

1. **Settings middleware** always runs first as soon as url or pseudo headers are parsed.
2. **Global middleware and Header middleware** runs once headers are parsed. In the same order first global, then route specific
3. **Body middleware** runs if the request body type is chunked.
4. **Response middleware** runs right before sending back data.
5. If any middleware returns `false`, the pipeline stops and no further handlers are executed, the response will be sent.
