
# Working with Request and Response

---

## Request

The `Request` object represents an incoming HTTP request.  
You can access URL, query parameters, headers, body, and the request method.

### Get the Request URL
```cpp
std::string url = request.getURL();
std::cout << "Path: " << url << std::endl;
````

### Get the Full URL (with query string)

```cpp
std::string full = request.getFullURL();
std::cout << "Full URL: " << full << std::endl;
```

### Get Query Parameters

```cpp
for (const auto &[key, values] : request.getQueryParams()) {
    std::cout << key << ":\n";
    for (const auto &v : values) {
        std::cout << "  " << v << "\n";
    }
}
```

### Get URI Parameters

```cpp
for (auto &[k, v] : request.uri_params) {
    std::cout << "param[" << k << "] = " << v << '\n';
}
```

### Get Headers

```cpp
for (const auto &[name, values] : request.getHeaders()) {
    std::cout << "Header: " << name << "\n";
    for (const auto &val : values) {
        std::cout << "  Value: " << val << "\n";
    }
}
```

### Get Request Method

```cpp
std::string method = request.getRequestMethod();
if (method == "GET") {
    // handle GET
}
```

### Get Request Body

```cpp
std::string body = request.getBody();
std::cout << "Body: " << body << std::endl;
```

---

## Response

The `Response` object represents what will be sent back to the client.
You can set status, headers, body, and even serve files.

### Set Status and Message

```cpp
response.setStatus(200)
        .setMessage("OK");
```

### Add Headers

```cpp
response.addHeader("Content-Type", "application/json")
        .addHeader("Cache-Control", "no-store");
```

### Set Body

```cpp
response.setBody("{\"msg\": \"Hello World\"}", "application/json");
```

### Send a File

```cpp
response.setFile("index.html", "text/html");
```

### Chunked Responses

```cpp
response.setChunked();
```

---

## Example Handler

```cpp
void handler(usub::server::protocols::http::Request &request,
             usub::server::protocols::http::Response &response) {
    std::cout << "URL: " << request.getURL() << "\n";
    std::cout << "Method: " << request.getRequestMethod() << "\n";

    response.setStatus(200)
            .setMessage("OK")
            .addHeader("Content-Type", "text/plain")
            .setBody("Hello from handler!\n");
}
```

---

## Next Steps

* Learn how to add [middleware](middlewares.md).
* Explore [Quick Start](getting-started.md) to set up a basic server.