#pragma once

#include <unet/http/message.hpp>

namespace usub::unet::http {

    struct response {
        MessagePolicy policy{};
    };

}// namespace usub::unet::http