//
// Created by Kirill Zhukov on 16.01.2024.
//

#ifndef HTTP1PARSER_H
#define HTTP1PARSER_H

#include <iostream>
#include <cstring>
#include <data/HttpRequest.h>

#include "event2/bufferevent.h"
#include "event2/buffer.h"
#include "logging/logger.h"
#include "data/types.h"
#include "request/requestTypesStr.h"

namespace unit::server::http {
    class Http1Parser {
    public:
        Http1Parser(evbuffer* in);

        ~Http1Parser();

    public:
        int parse_request();

        data::HttpRequest getRequest();
    public:
        uint8_t* body;
        char *version;
        char *uri;
        const char* hostname;
        const char *scheme;
        request::type request_type;
        std::unordered_map<std::string, std::string> headers;
    private:
        int header_is_valid(char* line);
    private:
        evbuffer* in;
        uint8_t* raw_data;
        request::RequestTypeStr request_type_str{};
    };
} // http::server::unit

#endif //HTTP1PARSER_H
