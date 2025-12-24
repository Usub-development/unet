#pragma once

#include <algorithm>
#include <expected>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "unet/header/enum.hpp"
#include "unet/utils/error.hpp"

namespace usub::unet::header {

    struct Generic;

    struct Header {
        std::string key;
        std::string value;
    };

    class Headers {
    public:
        Headers() = default;
        ~Headers() = default;

        void addHeader(std::string_view key, std::string_view value) {// Make into std::expected, we would need to validate value
            std::string key_lower;
            key_lower.resize(key.size());
            std::transform(key.begin(), key.end(), key_lower.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            header_list_.emplace_back(Header{key_lower, std::string(value)});
            header_keys_.insert(key_lower);
        }

        void addHeader(std::string &&key, std::string &&value) {
            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            header_list_.emplace_back(Header{std::move(key), std::move(value)});
            header_keys_.insert(header_list_.back().key);
        }

        bool contains(std::string_view key) const {
            std::string key_lower;
            key_lower.resize(key.size());
            std::transform(key.begin(), key.end(), key_lower.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            return header_keys_.contains(key_lower);
        }

        // template<class Type, ENUM Header>
        // std::expected<void, usub::unet::utils::UnetError> validate(std::string_view key, std::string_view value);

    private:
        std::vector<Header> header_list_;
        std::set<std::string> header_keys_;// We can use this to quickly check for existence of headers for cases
                                           // where we don't allow duplicate headers. "Host", "Content-Length", etc.
    };

}// namespace usub::unet::header