#ifndef USUB_UTILS_H
#define USUB_UTILS_H

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

namespace usub::utils {

    bool isPositiveIntegerString(const std::string &str);
    std::vector<unsigned char> toLower(const std::vector<unsigned char> &str);
    std::string toLower(const std::string &str);
    std::string deleteSpaces(const std::string &str);
    std::string deleteSpaces(std::string_view str);
    bool operator==(const std::vector<unsigned char> &vec, const std::string &str);
    bool operator==(const std::string &str, const std::vector<unsigned char> &vec);


}// namespace usub::utils

#endif//USUB_UTILS_H
