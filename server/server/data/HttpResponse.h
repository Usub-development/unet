//
// Created by Kirill Zhukov on 15.11.2023.
//

#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <list>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <cerrno>
#include <optional>
#include <variant>
#include <memory>
#include <errno.h>
#include <cstring>
#include <event2/buffer.h>
#include <event2/http.h>
#include <event.h>

#include "nghttp2/nghttp2.h"
#include "http1/StatusCodeHandler.h"

namespace unit::server {
#define MAKE_NV(NAME, VALUE)                                                    \
{                                                                               \
    (uint8_t *)NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, sizeof(VALUE) - 1,     \
    NGHTTP2_NV_FLAG_NONE                                                        \
}

    namespace data {
        enum DATA_TYPE {
            NONE = 0,
            BUFFER = 1,
            FD = 2
        };

        class HttpResponse {
        public:
            virtual ~HttpResponse() = default;
            /**
             * Used for sending files.
             * @param file_path path to file to be sent as reponse
             */
            virtual bool writeFile(const std::string&file_path) = 0;
            /**
             * Used for writing text reponses such as JSON, XML, etc.
             * @param buf buffer to be written
             * @param length size of buffer
             */
            virtual bool writeRawData(const uint8_t* buf, size_t length) = 0;
            /**
             * Adds header to response.
             * @param name name of header
             * @param value value of header
             */
            virtual void addHeader(char *name, const char *value) = 0;
            /**
             * Allows to set several headers by single function call.
             * @param headers vector of pairs: <b>header name : header value</b>
             */
            virtual void addHeaders(const std::vector<std::pair<std::string, std::string>>&headers) = 0;
            /**
             * Set status to response. Is status code is described in RFC, there is no need to set reason manually.
             * @param status status to be set
             */
            virtual void setStatus(int status) = 0;

            /**
             * Set reason to status code (should be used for only non-RFC status codes) for HTTP/1.1.
             * @param reason reason which should be sent with status in HTTP/1.1
             */
            virtual void setReason(const char *reason) = 0;
        };

        class Http1Response final : public HttpResponse {
        public:
            Http1Response() = default;
            explicit Http1Response(evhttp_request* request);
            ~Http1Response() override;

            bool writeFile(const std::string&file_path) override;

            bool writeRawData(const uint8_t* buf, size_t length) override;

            [[nodiscard]] std::optional<std::shared_ptr<std::vector<uint8_t>>> getBuffer() const;

            [[nodiscard]] int getFD() const;

            void addHeader(char *name, const char *value) override;

            void addHeaders(const std::vector<std::pair<std::string, std::string>>&headers) override;

            void setStatus(int status) override;

            void send();

            void setReq(evhttp_request* req);

            void setReason(const char* reason) override;

            void setStatusError(bool status_error);

        public:
            DATA_TYPE type = NONE;
        private:
            http1::StatusCodeHandler status_code_handler{false};
            std::variant<std::shared_ptr<std::vector<uint8_t>>, int> data;
            std::list<std::pair<std::string, std::string>> headers;
            evbuffer* buf = nullptr;
            evhttp_request* req;
            struct stat st;
            int status{};
            const char* reason = nullptr;
        };

        class Http2Response final : public HttpResponse {
        public:
            explicit Http2Response(int32_t stream_id);
            ~Http2Response() override;

            bool writeFile(const std::string&file_path) override;

            bool writeRawData(const uint8_t* buf, size_t length) override;

            [[nodiscard]] std::optional<std::shared_ptr<std::vector<uint8_t>>> getBuffer() const;

            [[nodiscard]] int getFD() const;

            void addHeader(char *name, const char *value) override;

            void addHeaders(const std::vector<std::pair<std::string, std::string>>&headers) override;

            void setStatus(int status) override;

            std::vector<nghttp2_nv>& getHeaders();

            void setReason(const char* reason) override;

        private:
            static nghttp2_nv make_nv(const char *name, const char *value);

        public:
            DATA_TYPE type = NONE;
            size_t read_offset = 0;
            const int32_t stream_id;
            std::string path;

        private:
            std::variant<std::shared_ptr<std::vector<uint8_t>>, int> data;
            std::vector<nghttp2_nv> headers;
        };
    }; // data
}

; // unit::server

#endif //HTTPRESPONSE_H
