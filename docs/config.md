# Server Configuration Guide

## Overview

A configuration consists of:

* A single `[server]` section with process‐wide parameters.
* An ordered list of `[[listener]]` blocks.
  **Order matters**: the first listener config is bound to the first stream handler template parameter, the second to the second stream handler, and so on.

```toml
[server]
threads = 4
backlog = 128

[[listener]]            # listener[0]
port = 25565
ssl  = false

[[listener]]            # listener[1]
port      = 8443
ssl       = true
key_file  = "key.pem"
cert_file = "cert.pem"
```

---

## Mapping config → server type

Your server type determines how many listeners you must declare and in what order they are interpreted.

### 1) Plain HTTP only

```cpp
using Server = usub::server::ServerImpl<
    protocols::http::HTTPEndpointHandler,
    usub::server::PlainHTTPStreamHandler
>;
```

* **Stream handler parameters**: `PlainHTTPStreamHandler` → **1 listener required**
* **Expected listener layout**:

  * `listener[0]` → plain TCP (must be `ssl = false`)

**Minimal config**

```toml
[server]
threads = 4
backlog = 128

[[listener]]            # binds to PlainHTTPStreamHandler
port = 8080
ssl  = false
```

### 2) Mixed (Plain + TLS)

```cpp
using MixedServerRadix = usub::server::Server<
    protocols::http::HTTPEndpointHandler,
    usub::server::PlainHTTPStreamHandler,
    usub::server::TLSHTTPStreamHandler
>;
```

* **Stream handler parameters**: `PlainHTTPStreamHandler`, `TLSHTTPStreamHandler` → **2 listeners required**
* **Expected listener layout**:

  * `listener[0]` → plain TCP (must be `ssl = false`)
  * `listener[1]` → TLS (must be `ssl = true`, with key/cert)

**Typical config**

```toml
[server]
threads = 4
backlog = 256

[[listener]]            # binds to PlainHTTPStreamHandler
port = 80
ssl  = false

[[listener]]            # binds to TLSHTTPStreamHandler
port      = 443
ssl       = true
key_file  = "/etc/ssl/private/server.key"
cert_file = "/etc/ssl/certs/server.crt"
```

> If you swap the two `[[listener]]` blocks, you change which handler handles each port, Keep the order consistent with your server type.

---

## Reference

### `[server]`

* `threads` *(int, ≥1)* — size of the worker thread pool.
* `backlog` *(int, ≥0)* — listen backlog passed to the OS.

### `[[listener]]`

* `port` *(int, 1–65535)* — TCP port to bind.
* `ssl` *(bool)* — `false` for plain TCP; `true` for TLS. 
* `key_file` *(string, required when `ssl=true`)* — path to the private key.
* `cert_file` *(string, required when `ssl=true`)* — path to the certificate chain (server cert first).

---