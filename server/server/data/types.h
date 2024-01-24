//
// Created by Kirill Zhukov on 20.11.2023.
//

#ifndef TYPES_H
#define TYPES_H

namespace unit::server {
    enum IPV : uint16_t {
        IPV4 = 4,
        IPV6 = 6
    };
}; // unit::server

namespace unit::server::protocols {
    enum http : int {
        HTTP1 = 1,
        HTTP2 = 2
#if 0
        HTTP3 = 2
#endif
    };
}; // unit::server::protocols

namespace unit::server::request {
        enum type : int {
            GET     = 1 << 0,
            POST    = 1 << 1,
            HEAD    = 1 << 2,
            PUT     = 1 << 3,
            DELETE  = 1 << 4,
            OPTIONS = 1 << 5,
            TRACE   = 1 << 6,
            CONNECT = 1 << 7,
            PATCH   = 1 << 8
        };
}; // unit::server::request

#endif //TYPES_H
