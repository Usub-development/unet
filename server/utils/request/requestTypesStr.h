//
// Created by Kirill Zhukov on 20.11.2023.
//

#ifndef REQUESTTYPESSTR_H
#define REQUESTTYPESSTR_H

#include <iostream>
#include <unordered_map>
#include "data/types.h"

namespace unit::server::request {
    class RequestTypeStr {
    public:
        RequestTypeStr();
        [[nodiscard]] type getType(const std::string &type) const;
    private:
        std::unordered_map<std::string, type> types;
    };
}
#endif //REQUESTTYPESSTR_H
