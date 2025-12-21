#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

#include "unet/http/header.hpp"
#include "unet/http/message.hpp"
#include "unet/uri/uri.hpp"

namespace usub::unet::http {

    static std::uint8_t max_method_token_size = std::numeric_limits<std::uint8_t>::max();// Arguably should be much smaller, but let's be VERY generous
    static std::uint16_t max_uri_size = std::numeric_limits<std::uint16_t>::max();       // Very generous limit for URI

    struct RequestMetadata {
        std::string method_token{};
        uri::URI uri{};
        VERSION version{};
    };

    struct Request {
        MessagePolicy policy{};
        RequestMetadata metadata{};
        usub::unet::header::Headers headers{};
        std::string body{};
    };

}// namespace usub::unet::http