#pragma once


#include <expected>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// #include "utils/string_utils.h"

// TODO: Remove forward declaration
#include <algorithm>
namespace usub::utils {
    struct CaseInsensitiveHash {
        std::size_t operator()(const std::string &key) const {
            std::size_t hash = 0;
            for (char c: key) {
                hash ^= std::hash<char>()(std::tolower(c)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            return hash;
        }
    };

    struct CaseInsensitiveEqual {
        bool operator()(const std::string &lhs, const std::string &rhs) const {
            return std::equal(lhs.begin(), lhs.end(),
                              rhs.begin(), rhs.end(),
                              [](char a, char b) {
                                  return std::tolower(a) == std::tolower(b);
                              });
        }
    };

    std::vector<std::string> split(const std::string &s, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }

    void trim(std::string &s) {
        // Trim leading spaces
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                    return !std::isspace(ch);
                }));
        // Trim trailing spaces
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
                    return !std::isspace(ch);
                }).base(),
                s.end());
    }

    std::string trim_copy(const std::string &s) {
        std::string result = s;
        trim(result);
        return result;
    }

    std::string toLower(const std::string &s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return result;
    }

}// namespace usub::utils

namespace usub::unet::mime::multipart {

    struct Part {
        std::string content_type;
        std::unordered_map<std::string, std::string> disposition;
        std::string data;
        std::unordered_map<std::string, std::vector<std::string>, usub::utils::CaseInsensitiveHash, usub::utils::CaseInsensitiveEqual> headers;
    };

    class FormData {
    private:
        using parts_map_t = std::unordered_map<std::string, std::vector<Part>, usub::utils::CaseInsensitiveHash, usub::utils::CaseInsensitiveEqual>;

        std::string boundary_;
        parts_map_t parts_by_name_;

    public:
        using iterator = parts_map_t::iterator;
        using const_iterator = parts_map_t::const_iterator;

        explicit FormData(std::string boundary = "");
        FormData(std::string boundary, std::string raw_data);

        std::expected<void, std::string> parse(std::string input);

        std::vector<Part> &operator[](const std::string &name);
        const std::vector<Part> &operator[](const std::string &name) const;

        std::vector<Part> &at(const std::string &name);
        const std::vector<Part> &at(const std::string &name) const;

        bool contains(const std::string &name) const;

        std::size_t size() const;
        bool empty() const;
        void clear();
        std::size_t erase(const std::string &name);

        iterator begin();
        iterator end();
        const_iterator begin() const;
        const_iterator end() const;

        const parts_map_t &parts_by_name() const;
        parts_map_t &parts_by_name();
    };

}// namespace usub::unet::mime::multipart
