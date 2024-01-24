//
// Created by Kirill Zhukov on 20.11.2023.
//

#include <string>
#include "requestTypesStr.h"

namespace unit::server::request {
    RequestTypeStr::RequestTypeStr() {
        this->types.emplace("GET", type::GET);
        this->types.emplace("POST", type::POST);
        this->types.emplace("HEAD", type::HEAD);
        this->types.emplace("DELETE", type::DELETE);
        this->types.emplace("CONNECT", type::CONNECT);
        this->types.emplace("OPTIONS", type::OPTIONS);
        this->types.emplace("TRACE", type::TRACE);
        this->types.emplace("PATCH", type::PATCH);
    }

    type RequestTypeStr::getType(const std::string& type) const {
        return this->types.at(type);
    }
}