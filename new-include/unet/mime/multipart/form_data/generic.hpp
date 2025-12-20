#pragma once

#include "utils/string_utils.h"
#include <expected>
#include <string>
#include <unordered_map>
#include <vector>


namespace usub::unet::mime::multipart {

    struct Part {
        std::string content_type;
        std::unordered_map<std::string, std::string> disposition;
        std::string data;
        std::unordered_map<std::string, std::vector<std::string>, usub::utils::CaseInsensitiveHash, usub::utils::CaseInsensitiveEqual> headers;
    };

    class FormData {
    private:
        std::string boundary_;
        std::unordered_map<std::string, std::vector<Part>> parts_by_name_;

    public:
        explicit FormData(std::string boundary = "") : boundary_(boundary) {}

        std::expected<void, std::string> parse(std::string input);

        // Accessors
        const std::unordered_map<std::string, std::vector<Part>> &parts_by_name() const { return parts_by_name_; }
        std::unordered_map<std::string, std::vector<Part>> &parts_by_name() { return parts_by_name_; }
    };

}// namespace usub::unet::mime::multipart
