#pragma once


#include <string>

#include "unet/http/message.hpp"

namespace usub::unet::http::parser {
    struct Error {
        enum class CODE {
            GENERIC_ERROR
        };

        CODE code;
        STATUS_CODE expected_status;
        std::string message;
        std::array<char, 256> tail;
    };
}// namespace usub::unet::http::parser