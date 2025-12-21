#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>


namespace usub::unet::http {

    static std::uint8_t max_method_token_size = std::numeric_limits<std::uint8_t>::max();// Arguably should be much smaller, but let's be VERY generous
    static std::uint16_t max_uri_size = std::numeric_limits<std::uint16_t>::max();       // Very generous limit for URI

    class Request {
    };

}// namespace usub::unet::http