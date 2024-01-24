//
// Created by Kirill Zhukov on 08.01.2024.
//

#ifndef STATUSCODEHANDLER_H
#define STATUSCODEHANDLER_H

#include <string>
#include <unordered_map>

namespace unit::server::http1 {
    class StatusCodeHandler {
    public:
        StatusCodeHandler(bool status_error);
        std::string getMessage(int statusCode);
        void setStatusError(bool statusError);
    private:
        std::unordered_map<int, std::string> statusCodeMessages;
        bool status_error = false;
    };
} // unit::server::http1

#endif //STATUSCODEHANDLER_H
