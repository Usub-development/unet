//
// Created by Kirill Zhukov on 10.11.2023.
//

#ifndef TCPSOCKETTEST_HTTPREQUEST_H
#define TCPSOCKETTEST_HTTPREQUEST_H
#include <iostream>
#include <vector>
#include <unordered_map>

namespace unit::server {
    namespace data {
        class HttpRequest {
        public:
            HttpRequest(bool isFinished, const std::vector<unsigned char> &data, int32_t stream_id);

            explicit HttpRequest(int32_t stream_id);

            [[nodiscard]] int32_t getStreamId() const;

            [[nodiscard]] const std::vector<unsigned char> &getData();

            int32_t getStreamId();

        public:
            std::unordered_map<std::string, std::string> headers{};
            std::vector<unsigned char> data;
        private:
            const int32_t stream_id;
        };
    }; // data
}; // unit::server

#endif //TCPSOCKETTEST_HTTPREQUEST_H
