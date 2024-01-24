//
// Created by Kirill Zhukov on 16.01.2024.
//

#include "Http1Parser.h"



namespace unit::server::http {
    Http1Parser::Http1Parser(evbuffer* in) : in(in) {
        size_t len = evbuffer_get_length(in);
        this->raw_data = evbuffer_pullup(in, len);
    }

    Http1Parser::~Http1Parser() {
        if (this->in) {
            evbuffer_free(this->in);
        }
    }

    int Http1Parser::parse_request() {
        auto line_ptr = reinterpret_cast<char*>(this->raw_data);
        char* method = strsep(&line_ptr, " ");
        if (!method) return -1;
        this->request_type = this->request_type_str.getType(method);
        this->uri = strsep(&line_ptr, " ");
        if (!this->uri) return -1;
        this->version = strsep(&line_ptr, "\r\n");
        if (!this->version) return -1;
        size_t skip = strspn(line_ptr, "\r\n");
        line_ptr[skip-1] += 2;
        while (strncmp(line_ptr, "\n\r\n", 1) != 0) {
            while (*line_ptr == '\f' || *line_ptr == ' ' || *line_ptr == '\v') {
                ++line_ptr;
            }
            char* key = strsep(&line_ptr, ":");
            while (*line_ptr == '\f' || *line_ptr == ' ' || *line_ptr == '\v') {
                ++line_ptr;
            }
            char* value = strsep(&line_ptr, "\r\n");
            printf("key:%s, value:%s\n", key, value);
            if (!line_ptr) {
                break;
            }
            skip = strspn(line_ptr, "\r\n");
            line_ptr[skip-1] += 1;
        }
        if (line_ptr) {
            while (*line_ptr == '\r' || *line_ptr == '\n' || *line_ptr == '\v') {
                ++line_ptr;
            }
            this->body = reinterpret_cast<uint8_t*>(line_ptr);
        }
        return 1;
    }

    data::HttpRequest Http1Parser::getRequest() {
        data::HttpRequest request(-1);
        request.data.assign(reinterpret_cast<const unsigned char*>(this->body),
                            reinterpret_cast<const unsigned char*>(this->body) + strlen(reinterpret_cast<const char*>(this->body)));
        request.headers = std::move(this->headers);
        return request;
    }

    int Http1Parser::header_is_valid(char* line) {
        const char *p = line;

        while ((p = strpbrk(p, "\r\n")) != NULL) {
            /* we really expect only one new line */
            p += strspn(p, "\r\n");
            /* we expect a space or tab for continuation */
            if (*p != ' ' && *p != '\t')
                return (0);
        }
        return (1);
    }
} // http::server::unit