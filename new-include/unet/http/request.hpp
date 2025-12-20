#pragma once

#include <cstddef>

namespace usub::unet::http {

    static std::size_t max_method_token_size = 40;// Arguably should be much smaller, but let's be generous
    static std::size_t max_uri_size = 16 * 1024;  // Very generous limit for URI

}// namespace usub::unet::http