//
// Created by Kirill Zhukov on 15.11.2023.
//

#include "HttpResponse.h"
#include <unistd.h>
#include "logging/logger.h"

unit::server::data::Http1Response::Http1Response(evhttp_request* request) : req(request) {
    this->buf = evbuffer_new();
}

unit::server::data::Http1Response::~Http1Response() {
    if (this->type == FD) {
        close(this->getFD());
    }
    if (this->reason) {
        free((void *)this->reason);
    }
}

bool unit::server::data::Http1Response::writeFile(const std::string&file_path) {
    if (this->type != NONE) {
        return false;
    }
    int fd = open(file_path.c_str(), O_RDONLY);
    if (fd < 0) {
        error_log("File not opened: %s, error: %s", file_path.c_str(), strerror(errno));
        evhttp_send_error(this->req, HTTP_NOTFOUND, "File not found");
        return false;
    }
    if (fstat(fd, &this->st) < 0) {
        close(fd);
        error_log("File corrupted: %s, file size: %d", file_path.c_str(), fstat(fd, &this->st));
        evhttp_send_error(this->req, HTTP_INTERNAL, "Internal Server Error");
        return false;
    }
    this->data = fd;
    this->type = FD;
    return true;
}

bool unit::server::data::Http1Response::writeRawData(const uint8_t* buf, size_t length) {
    if (this->type != NONE) {
        return false;
    }
    this->data = std::make_shared<std::vector<uint8_t>>(buf, buf + length);
    this->type = BUFFER;
    return true;
}

std::optional<std::shared_ptr<std::vector<uint8_t>>> unit::server::data::Http1Response::getBuffer() const {
    if (std::holds_alternative<std::shared_ptr<std::vector<uint8_t>>>(data)) {
        return std::get<std::shared_ptr<std::vector<uint8_t>>>(data);
    }
    return std::nullopt;
}

int unit::server::data::Http1Response::getFD() const {
    if (std::holds_alternative<int>(this->data)) {
        return std::get<int>(this->data);
    }
    return -1;
}

void unit::server::data::Http1Response::addHeader(char* name, const char* value) {
    evhttp_add_header(evhttp_request_get_output_headers(this->req), name, value);
}

void unit::server::data::Http1Response::addHeaders(const std::vector<std::pair<std::string, std::string>>&headers) {
    for (auto&[name, value]: headers) {
        evhttp_add_header(evhttp_request_get_output_headers(this->req), name.c_str(), value.c_str());
    }
}

void unit::server::data::Http1Response::setStatus(int status) {
    this->status = status;
    this->reason = strdup(this->status_code_handler.getMessage(status).c_str());
}

void unit::server::data::Http1Response::send() {
    if (this->type == FD) {
        const int fd = this->getFD();
        evhttp_send_reply_start(this->req, (this->status != 0) ? this->status : 200,
                                (this->reason) ? this->reason : "OK");
        evbuffer_add_file(this->buf, fd, 0, this->st.st_size);
        evhttp_send_reply_chunk(this->req, this->buf);
        evbuffer_free(this->buf);
        evhttp_send_reply_end(this->req);
        close(fd);
    }
    else if (this->type == BUFFER) {
        auto buffer = this->getBuffer();
        if (buffer.has_value()) {
            evbuffer_add(this->buf, buffer.value()->data(), buffer.value()->size());
            evhttp_send_reply(req, (this->status != 0) ? this->status : 200, (this->reason) ? this->reason : "OK",
                              this->buf);
            evbuffer_free(this->buf);
        }
    }
    else if (this->type == NONE) {
        evhttp_send_reply(this->req, (this->status != 0) ? this->status : HTTP_INTERNAL,
                          (this->reason != nullptr) ? this->reason : "Internal Server Error", nullptr);
        evbuffer_free(this->buf);
    }
}

void unit::server::data::Http1Response::setReq(evhttp_request* req) {
    this->req = req;
}

void unit::server::data::Http1Response::setReason(const char* reason) {
    this->reason = reason;
}

void unit::server::data::Http1Response::setStatusError(bool status_error) {
    this->status_code_handler.setStatusError(status_error);
}

unit::server::data::Http2Response::Http2Response(const int32_t stream_id) : stream_id(stream_id) {
}

unit::server::data::Http2Response::~Http2Response() {
    if (this->type == FD) {
        close(this->getFD());
    }
}

bool unit::server::data::Http2Response::writeFile(const std::string&file_path) {
    if (this->type != NONE) {
        return false;
    }
    int fd = open(file_path.c_str(), O_RDONLY);

    if (fd == -1) {
        const std::string data = "<html><head><title>404</title></head>"
                "<body><h1>404 Not Found</h1></body></html>";
        error_log("File not opened: %s, error: %s", file_path.c_str(), strerror(errno));
        this->writeRawData(reinterpret_cast<const uint8_t *>(data.data()), data.length());
        this->addHeader((char *)":status", (char *)"404");
        this->addHeader((char *)"Content-Type", (char *)"text/html; charset=utf-8");
        return false;
    }
    this->data = fd;
    this->type = FD;
    this->path = file_path;
    return true;
}

bool unit::server::data::Http2Response::writeRawData(const uint8_t* buf, const size_t length) {
    if (this->type != NONE) {
        return false;
    }
    this->data = std::make_shared<std::vector<uint8_t>>(buf, buf + length);
    this->type = BUFFER;
    return true;
}

std::optional<std::shared_ptr<std::vector<uint8_t>>> unit::server::data::Http2Response::getBuffer() const {
    if (std::holds_alternative<std::shared_ptr<std::vector<uint8_t>>>(data)) {
        return std::get<std::shared_ptr<std::vector<uint8_t>>>(data);
    }
    return std::nullopt;
}

int unit::server::data::Http2Response::getFD() const {
    if (std::holds_alternative<int>(this->data)) {
        return std::get<int>(this->data);
    }
    return -1;
}

void unit::server::data::Http2Response::addHeader(char* name, const char* value) {
    this->headers.push_back({
        (uint8_t *)name, (uint8_t *)value, std::strlen(name), std::strlen(value), NGHTTP2_NV_FLAG_NONE
    });
}

void unit::server::data::Http2Response::addHeaders(const std::vector<std::pair<std::string, std::string>>&headers) {
    for (auto&[name, value]: headers) {
        this->headers.push_back({
            (uint8_t *)name.c_str(), (uint8_t *)value.c_str(), std::strlen(name.c_str()), std::strlen(value.c_str()),
            NGHTTP2_NV_FLAG_NONE
        });
    }
}

void unit::server::data::Http2Response::setStatus(int status) {
    char num[3];
    snprintf(num, 3, "%d", status);
    this->headers.push_back(MAKE_NV(":status", num));
}

nghttp2_nv unit::server::data::Http2Response::make_nv(const char* name, const char* value) {
    return {
        (uint8_t *)(name),
        (uint8_t *)(value),
        strlen(name),
        strlen(value),
        NGHTTP2_NV_FLAG_NONE
    };
}

std::vector<nghttp2_nv>& unit::server::data::Http2Response::getHeaders() {
    if (this->headers.empty()) {
        this->headers.push_back(MAKE_NV(":status", "200"));
        this->headers.push_back(MAKE_NV("content-type", "text/plain"));
    }
    return this->headers;
}

void unit::server::data::Http2Response::setReason(const char* reason) {
}
