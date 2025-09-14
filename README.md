# Webserver

**Fast and versatile web framework in modern C++**

Webserver is a lightweight, high-performance framework built on top of [Uvent](https://github.com/Usub-development/uvent).  
It provides everything you need to handle HTTP/1.0 and HTTP/1.1 efficiently, with planned support for HTTP/2 and HTTP/3.

---

## Features
- 🚀 High-performance async event loop (via Uvent)
- 📦 RFC-compliant HTTP parser
- 🔌 Middleware and routing system (regex & radix)
- 🧩 Modular and extensible design
- 🔒 TLS/SSL support (optional, OpenSSL)

---

## Quick Start

Minimal server:

```cpp
#include "server/server.h"
#include "Protocols/HTTP/Message.h"

using namespace usub::server;

void handler(protocols::http::Request &req, protocols::http::Response &res) {
    res.setStatus(200).setMessage("OK").setBody("Hello World!\n");
}

int main() {
    Server server("../config/config.toml");
    server.handle({"GET"}, "/hello", handler);
    server.run();
}
````

Run:

```bash
curl http://127.0.0.1:8111/hello
```

---

## Documentation

Full documentation:
* [Getting Started Guide](https://usub-development.github.io/webserver/getting-started/)
* [Installation](https://usub-development.github.io/webserver/installation/)
* [Middleware](https://usub-development.github.io/webserver/middlewares/)
* [Request & Response](https://usub-development.github.io/webserver/request-response/)

---

## Roadmap

* ✅ HTTP/1.0 / 1.1
* 🚧 Extended tests & docs
* 📅 Planned: HTTP/2, HTTP/3, WebSocket upgrades, streaming

See [Roadmap](https://usub-development.github.io/webserver/roadmap/).

---

## Contributing

We welcome contributions! Please see the [Contributing Guide](https://usub-development.github.io/webserver/contributing/).
Coding style is documented [here](https://usub-development.github.io/webserver/contributing/#coding-style).

---

## License

MIT

```

---

Would you like me to also add **badges** (CMake, C++23, Docs link, License) at the top, so it looks more professional at first glance?
```
