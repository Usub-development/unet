#pragma once

#include <cstring>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "Components/Headers/ETag.h"
#include "Components/Headers/HTTPDate.h"
#include "Protocols/HTTP/header_lookup.h"
#include "utils/HTTPUtils/HTTPUtils.h"
#include "utils/ParseError.h"
#include "utils/crc32.h"
#include "utils/header_utils.h"
#include "utils/string_utils.h"
#include "utils/utils.h"
#if defined(USE_EXPERIMENTAL_FEATURES) && USE_EXPERIMENTAL_FEATURES == 1
#error "Experimental features not implemented yet"
#endif
#if defined(USE_DEPRECATED_FEATURES) && USE_DEPRECATED_FEATURES == 1
#error "Deprecated features not implemented yet"
#endif
#if defined(ONLY_BASELINE) && ONLY_BASELINE == 0
#error "Non baseline features not implemented yet"
#endif

namespace usub::server {

    namespace component {

        enum struct ACCEPT_RANGES : uint8_t {
            BYTES,
            NONE,
            OMIT
        };

        enum struct SECFETCHDEST : uint8_t {
            AUDIO,
            AUDIOWORKLET,
            DOCUMENT,
            EMBED,
            EMPTY,
            FENCEDFRAME,
            FONT,
            FRAME,
            IFRAME,
            IMAGE,
            MANIFEST,
            OBJECT,
            PAINTWORKLET,
            REPORT,
            SCRIPT,
            SERVICEWORKER,
            SHAREDWORKER,
            STYLE,
            TRACK,
            VIDEO,
            WEBIDENTITY,
            WORKER,
            XSLT,
            OMIT
        };

        enum struct SECFETCHMODE : uint8_t {
            CORS,
            NAVIGATE,
            NO_CORS,
            SAME_ORIGIN,
            WEBSOCKET,
            OMIT
        };

        enum struct SECFETCHSITE : uint8_t {
            CROSS_SITE,
            SAME_ORIGIN,
            SAME_SITE,
            NONE,
            OMIT
        };

        struct Keep_Alive {
            ssize_t timeout{-1};
            ssize_t max{-1};
        };

    }// namespace component

    namespace protocols::http {

        class Request;
        class Response;
        class General;

        class Headers /* : public component::Headers */ {
        private:
            std::unordered_map<usub::server::component::HeaderEnum, std::vector<std::string>> known_headers_map_;
            std::unordered_map<std::string, std::vector<std::string>, usub::utils::CaseInsensitiveHash, usub::utils::CaseInsensitiveEqual> unknown_headers_map_;

        public:
            Headers() = default;
            ~Headers() = default;

            Headers &clear();
            Headers &erase(std::string_view key_view);

            template<bool IgnoreCase = false>
            Headers &erase(std::string_view key_view, std::string_view value_view) {
                auto lookup = HTTPHeaderLookup::lookupHeader(key_view.data(), key_view.size());
                if (lookup) [[likely]] {
                    auto it = known_headers_map_.find(lookup->id);
                    if (it != known_headers_map_.end()) {
                        auto &values = it->second;
                        values.erase(std::remove_if(values.begin(), values.end(),
                                                    [&value_view](const std::string &value) {
                                                        if constexpr (IgnoreCase) {
                                                            return usub::utils::icmp(value, value_view);
                                                        } else {
                                                            return value == value_view;
                                                        }
                                                    }),
                                     values.end());
                        if (values.empty()) {
                            known_headers_map_.erase(it);
                        }
                    }
                } else {
                    auto &values = unknown_headers_map_[usub::utils::toLower(std::string(key_view))];
                    values.erase(std::remove_if(values.begin(), values.end(),
                                                [&value_view](const std::string &value) {
                                                    if constexpr (IgnoreCase) {
                                                        return usub::utils::icmp(value, value_view);
                                                    } else {
                                                        return value == value_view;
                                                    }
                                                }),
                                 values.end());
                    if (values.empty()) {
                        unknown_headers_map_.erase(usub::utils::toLower(std::string(key_view)));
                    }
                }
                return *this;
            }

            const size_t size() const;
            // const std::string string() const;

            inline std::vector<std::string> split(const std::string &s, char delimiter) {
                std::vector<std::string> tokens;
                std::string token;
                std::istringstream tokenStream(s);
                while (std::getline(tokenStream, token, delimiter)) {
                    tokens.emplace_back(usub::utils::trim_copy(token));
                }
                return tokens;
            }

            std::vector<std::string> &operator[](std::string_view key);
            std::vector<std::string> &operator[](usub::server::component::HeaderEnum key);

            bool containsValue(usub::server::component::HeaderEnum key, std::string_view token, bool ignore_case = true) const;
            bool containsValue(std::string_view key, std::string_view token, bool ignore_case) const;

            bool contains(usub::server::component::HeaderEnum key) const;
            bool contains(std::string_view key) const;
            const std::vector<std::string> &at(std::string_view key) const;
            const std::vector<std::string> &at(usub::server::component::HeaderEnum key) const;

            template<typename T, typename ValueType>
            std::expected<void, usub::server::utils::error::ParseError> addHeader(std::string &&key, ValueType &&value) {
                static constexpr uint64_t u_bytes =
                        0b00000000'00000000'00000000'01110011'01100101'01110100'01111001'01100010;// "bytes" in little-endian
                static constexpr uint64_t u_none =
                        0b00000000'00000000'00000000'00000000'01100101'01101110'01101111'01101110;// "none" in little-endian
                static constexpr uint64_t u_true =
                        0b00000000'00000000'00000000'00000000'01100101'01110101'01110010'01110100;// "true" in little-endian
                static constexpr uint64_t u_timeout =
                        0b01110100'01101001'01101101'01100101'01101111'01110101'01110100'00111101;
                static constexpr uint64_t u_max =
                        0b00000000'00000000'00000000'00000000'01101101'01100001'01111000'00111101;

                if (key.empty() || value.empty()) [[unlikely]] {
                    if (key.empty()) {
                        return usub::server::utils::error::warn("Key is empty");
                    } else if (value.empty()) {
                        return usub::server::utils::error::warn("Value is empty");
                    }
                }
                if constexpr (std::is_same_v<std::decay_t<ValueType>, std::string>) {
                    // We moved the lower logic to the callee and hash so we can change code in one place
                    // key = usub::utils::toLower(key);
                    // value = usub::utils::trim(value);


                    // TODO: Implement strict mode where fields cannot repeat at all.
                    // The damned RFC states that a sender MUST NOT generate repeated fields,
                    // but a recipient MAY combine repeated fields if the header is defined as comma-separable.
                    //
                    // This behavior introduces ambiguity: how can a recipient combine something
                    // that is forbidden to be sent in the first place?
                    //
                    // In strict mode, we will reject any repeated headers outright, regardless of their type,
                    // to enforce clarity, predictability, and alignment with sender-side compliance rules.
                    //
                    // IF you encounter a case in this codebase where a header is accepted/rejected and you believe
                    // it should behave differently, please submit a change proposal with a clear rationale.
                    // Reference relevant RFC sections (e.g., RFC 9110 §5.2, §5.6.1, RFC 9111 §5.1 or other relevant sections)
                    // and provide examples of how this change would improve the code's clarity, maintainability,
                    // and real-world examples if applicable.

                    // TODO: In place SplitAndTrim, too much copies being passed around already for now we will leave it as is, because too much junk code already
                    // This code had to be refactored multiple times, current implementation is not the best but it works, in the better future we will
                    // have a better solution for this, like actually parsing the string in place and not copying it around to and out of the vector
                    // or better yet, save a request as a string and use string_view to parse it, but for now we will leave it as is
                    auto lookup = HTTPHeaderLookup::lookupHeader(key.data(), key.size());
                    if (lookup) {
                        switch (lookup->id) {
                            case usub::server::component::HeaderEnum::Accept: {
                                std::vector<std::string> values = split(value, ',');
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Accept];
                                insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                break;
                            }
                            case usub::server::component::HeaderEnum::Accept_CH: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Accept_CH];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("Accept-CH is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Accept_Encoding: {
                                std::vector<std::string> values = split(value, ',');
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Accept_Encoding];
                                insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                break;
                            }
                            case usub::server::component::HeaderEnum::Accept_Language: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Accept_Language];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("Accept-Language is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Accept_Patch: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Accept_Patch];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("Accept-Patch is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Accept_Post: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Accept_Post];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("Accept-Post is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Accept_Ranges: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    uint64_t input_value{};
                                    // Not needed in not strict mode since rfc only defines bytes and none but user is allowed to define other values

                                    // if (value.size() < 4) [[unlikely]] {
                                    //     return usub::server::utils::error::warn("Accept-Ranges value is too short, expected at least 4 bytes");
                                    // }
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Accept_Post];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Accept-Ranges is already set");
                                    }
                                    // Once again we are not strict checking the value since rfc only defines bytes and none but user is allowed to define other values

                                    // std::memcpy(&input_value, value.data(), 5);
                                    // if (input_value == u_bytes) [[likely]] {
                                    //     insert_place.push_back(std::move(value));
                                    // } else if (input_value & 0xFF'FF'FF'00'FF'FF'FF'FF == u_none) {
                                    //     insert_place.push_back(std::move(value));
                                    // } else [[unlikely]] {
                                    //     return usub::server::utils::error::warn("Accept Ranges only accepts 'bytes' or 'none' as value");
                                    // }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Accept-Ranges is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Access_Control_Allow_Credentials: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    uint64_t input_value{};
                                    if (value.size() < 4) [[unlikely]] {
                                        return usub::server::utils::error::warn("Access-Control-Allow-Credentials value is too short, expected at least 4 bytes");
                                    }
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Access_Control_Allow_Credentials];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Access-Control-Allow-Credentials is already set");
                                    }

                                    std::memcpy(&input_value, value.data(), 4);
                                    if (input_value == u_true) [[likely]] {
                                        insert_place.push_back(std::move(value));
                                    } else {
                                        return usub::server::utils::error::warn("Access-Control-Allow-Credentials only accepts 'true' as value");
                                    }
                                } else {
                                    return usub::server::utils::error::warn("Access-Control-Allow-Credentials is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Access_Control_Allow_Headers: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    // TODO: Trust programmer to not send invalid headers, if they do, we might need to check for wildcards later
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Access_Control_Allow_Headers];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("Access-Control-Allow-Headers is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Access_Control_Allow_Methods: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    // TODO: Trust programmer to not send invalid headers, if they do, we might need to check for wildcards later
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Access_Control_Allow_Methods];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("Access-Control-Allow-Methods is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Access_Control_Allow_Origin: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Access_Control_Allow_Origin];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Access-Control-Allow-Origin is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Access-Control-Allow-Origin is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Access_Control_Expose_Headers: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Access_Control_Expose_Headers];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("Access-Control-Expose-Headers is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Access_Control_Max_Age: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Access_Control_Max_Age];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Access-Control-Max-Age is already set");
                                    }

                                    // Save only the first value, since the RFC says to use the first value if multiple values are present
                                    auto comma_pos = value.find(',');

                                    if (comma_pos != std::string::npos) {
                                        value.erase(comma_pos);
                                    }
                                    usub::utils::trim(value);

                                    if (!usub::utils::isPositiveIntegerString(value)) [[unlikely]] {
                                        return usub::server::utils::error::warn("Access-Control-Max-Age value is not a positive Integer");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Access-Control-Max-Age is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Access_Control_Request_Headers: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Access_Control_Request_Headers];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("Access-Control-Request-Headers is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Access_Control_Request_Method: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    if (!this->known_headers_map_[usub::server::component::HeaderEnum::Access_Control_Request_Method].empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Access-Control-Request-Method is already set");
                                    }
                                    this->known_headers_map_[usub::server::component::HeaderEnum::Access_Control_Request_Method].push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Access-Control-Request-Method is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Age: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Age];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Age is already set");
                                    }
                                    // Save only the first value, since the RFC says to use the first value if multiple values are present
                                    auto comma_pos = value.find(',');

                                    if (comma_pos != std::string::npos) {
                                        value.erase(comma_pos);
                                    }
                                    usub::utils::trim(value);
                                    if (!usub::utils::isPositiveIntegerString(value)) [[unlikely]] {
                                        return usub::server::utils::error::warn("Age value is not a positive number");
                                    }
                                    this->known_headers_map_[usub::server::component::HeaderEnum::Age].push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Age is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Allow: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Allow];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("Allow is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Alt_Svc: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Alt_Svc];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("Alt-Svc is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Alt_Used: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Alt_Used];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Alt-Used is already set");
                                    }
                                    // TODO: Maybe check if it's a valid <host>:<port> pair
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Alt-Used is a Request only header");
                                }
                                break;
                            }
#if defined(USE_EXPERIMENTAL_FEATURES) && USE_EXPERIMENTAL_FEATURES == 1
                            case crc32("attribution-reporting-eligible", 29): {
                                this->headers_.Attribution_Reporting_Eligible = value;
                                break;
                            }
                            case crc32("attribution-reporting-register-source", 36): {
                                this->headers_.Attribution_Reporting_Register_Source = value;
                                break;
                            }
                            case crc32("attribution-reporting-register-trigger", 37): {
                                this->headers_.Attribution_Reporting_Register_Trigger = value;
                                break;
                            }
#endif
                            case usub::server::component::HeaderEnum::Authorization: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Authorization];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Authorization is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Authorization is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Clear_Site_Data: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Clear_Site_Data];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("Clear-Site-Data is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Connection: {
                                std::vector<std::string> values = split(value, ',');
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Connection];
                                insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                break;
                            }
                            case usub::server::component::HeaderEnum::Content_Digest: {
                                std::vector<std::string> values = split(value, ',');
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Content_Digest];
                                insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                break;
                            }
                            case usub::server::component::HeaderEnum::Content_Disposition: {
                                std::vector<std::string> values = split(value, ',');
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Content_Disposition];
                                insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                break;
                            }

#if defined(USE_DEPRECATED_FEATURES) && USE_DEPRECATED_FEATURES == 1
                            case crc32("content-dpr", 11): {
                                this->headers_.Content_DPR = std::stold(value);
                                break;
                            }
#endif

                            case usub::server::component::HeaderEnum::Content_Encoding: {
                                std::vector<std::string> values = split(value, ',');
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Content_Encoding];
                                insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                break;
                            }
                            case usub::server::component::HeaderEnum::Content_Language: {
                                std::vector<std::string> values = split(value, ',');
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Content_Language];
                                insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                break;
                            }
                            case usub::server::component::HeaderEnum::Content_Length: {
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Content_Length];
                                usub::utils::trim(value);
                                if (!insert_place.empty()) [[unlikely]] {
                                    return usub::server::utils::error::crit("Content-Length is already set");
                                }
                                if (!usub::utils::isPositiveIntegerString(value)) [[unlikely]] {
                                    return usub::server::utils::error::crit("Content-Length value is not a positive integer");
                                }
                                insert_place.push_back(std::move(value));
                                break;
                            }
                            case usub::server::component::HeaderEnum::Content_Location: {
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Content_Location];
                                if (!insert_place.empty()) [[unlikely]] {
                                    return usub::server::utils::error::crit("Content-Location is already set");
                                }
                                insert_place.push_back(std::move(value));
                                break;
                            }
                            case usub::server::component::HeaderEnum::Content_Range: {
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Content_Range];
                                if (!insert_place.empty()) [[unlikely]] {
                                    return usub::server::utils::error::crit("Content-Range is already set");
                                }
                                insert_place.push_back(std::move(value));
                                break;
                            }
                            case usub::server::component::HeaderEnum::Content_Security_Policy: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Content_Security_Policy];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Content-Security-Policy is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Content-Security-Policy is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Content_Security_Policy_Report_Only: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Content_Security_Policy_Report_Only];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Content-Security-Policy-Report-Only is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Content-Security-Policy-Report-Only is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Content_Type: {
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Content_Type];
                                if (!insert_place.empty()) [[unlikely]] {
                                    return usub::server::utils::error::crit("Content-Type is already set");
                                }
                                insert_place.push_back(std::move(value));
                                break;
                            }
                            case usub::server::component::HeaderEnum::Cookie: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Cookie];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Cookie is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Cookie is a Request only header");
                                }
                                break;
                            }
#if defined(USE_EXPERIMENTAL_FEATURES) && USE_EXPERIMENTAL_FEATURES == 1
                            case crc32("critical-ch", 11): {
                                this->headers_.Critical_CH = value;
                                break;
                            }
#endif
                            case usub::server::component::HeaderEnum::Cross_Origin_Embedder_Policy: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Cross_Origin_Embedder_Policy];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Cross-Origin-Embedder-Policy is already set");
                                    }
                                    if (value == "require-corp") {
                                        insert_place.push_back(std::move(value));
                                    } else if (value == "unsafe-none") {
                                        insert_place.push_back(std::move(value));
                                    } else if (value == "credentialless") {
                                        insert_place.push_back(std::move(value));
                                    } else {
                                        return usub::server::utils::error::warn("Cross-Origin-Embedder-Policy only accepts 'require-corp' or 'unsafe-none' as value");
                                    }
                                } else {
                                    return usub::server::utils::error::warn("Cross-Origin-Embedder-Policy is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Cross_Origin_Opener_Policy: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Cross_Origin_Opener_Policy];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Cross-Origin-Opener-Policy is already set");
                                    }
                                    if (value == "same-origin") {
                                        insert_place.push_back(std::move(value));
                                    } else if (value == "unsafe-none") {
                                        insert_place.push_back(std::move(value));
                                    } else if (value == "same-origin-allow-popups") {
                                        insert_place.push_back(std::move(value));
                                    } else if (value == "noopener-allow-popups") {
                                        insert_place.push_back(std::move(value));
                                    } else [[unlikely]] {
                                        return usub::server::utils::error::warn("Cross-Origin-Opener-Policy only accepts 'same-origin', 'unsafe-none', 'noopener-allow-popups' or 'same-origin-allow-popups' as value");
                                    }
                                } else {
                                    return usub::server::utils::error::warn("Cross-Origin-Opener-Policy is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Cross_Origin_Resource_Policy: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Cross_Origin_Resource_Policy];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Cross-Origin-Resource-Policy is already set");
                                    }
                                    if (value == "same-origin") {
                                        insert_place.push_back(std::move(value));
                                    } else if (value == "same-site") {
                                        insert_place.push_back(std::move(value));
                                    } else if (value == "cross-origin") {
                                        insert_place.push_back(std::move(value));
                                    } else {
                                        return usub::server::utils::error::warn("Cross-Origin-Resource-Policy only accepts 'same-origin', 'same-site' or 'cross-origin' as value");
                                    }
                                } else {
                                    return usub::server::utils::error::warn("Cross-Origin-Resource-Policy is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Date: {
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Date];
                                if (!insert_place.empty()) [[unlikely]] {
                                    return usub::server::utils::error::crit("Date is already set");
                                }
                                insert_place.push_back(std::move(value));
                                break;
                            }
#if defined(ONLY_BASELINE) && ONLY_BASELINE == 0
                            case crc32("dictionary-id"): {
                                break;
                            }
                            case crc32("device-memory", 13): {
                                this->headers_.Device_Memory = std::stold(value);
                                break;
                            }
#endif

#if defined(USE_DEPRECATED_FEATURES) && USE_DEPRECATED_FEATURES == 1
                            case crc32("dnt", 3): {
                                this->headers_.DNT = value;// TODO: Enum or bool
                                break;
                            }
#endif

#if defined(USE_EXPERIMENTAL_FEATURES) && USE_EXPERIMENTAL_FEATURES == 1
                            case crc32("downlink", 8): {
                                this->headers_.Downlink = std::stold(value);
                                break;
                            }
#endif

#if defined(USE_DEPRECATED_FEATURES) && USE_DEPRECATED_FEATURES == 1
                            case crc32("dpr", 3): {
                                this->headers_.DPR = std::stold(value);
                                break;
                            }
#endif

#if defined(USE_EXPERIMENTAL_FEATURES) && USE_EXPERIMENTAL_FEATURES == 1
                            case crc32("early-data", 10): {
                                this->headers_.Early_Data = (value == "1");
                                break;
                            }
                            case crc32("ect", 3): {
                                this->headers_.ECT = value;// TODO: Enum
                                break;
                            }
#endif

                            case usub::server::component::HeaderEnum::Etag: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Etag];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Etag is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("ETag is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Expect: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Expect];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Expect is already set");
                                    }
                                    if (value != "100-continue") [[unlikely]] {
                                        return usub::server::utils::error::warn("Expect value is not '100-continue'");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Expect is a Request only header");
                                }
                                break;
                            }
#if defined(USE_DEPRECATED_FEATURES) && USE_DEPRECATED_FEATURES == 1
                            case crc32("expect-ct", 9): {
                                this->headers_.Expect_CT = value;// TODO: Separate class
                                break;
                            }
#endif

                            case usub::server::component::HeaderEnum::Expires: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Expires];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Expires is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Expires is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Forwarded: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Forwarded];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("Forwarded is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::From: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::From];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("From is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("From is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Host: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Host];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Host is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Host is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::If_Match: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::If_Match];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("If-Match is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::If_Modified_Since: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::If_Modified_Since];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("If-Modified-Since is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::If_None_Match: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::If_None_Match];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("If-None-Match is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::If_Range: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::If_Range];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("If-Range is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::If_Unmodified_Since: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::If_Unmodified_Since];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("If-Unmodified-Since is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Keep_Alive: {
                                std::vector<std::string> values = split(value, ',');
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Keep_Alive];
                                insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                break;
                            }
                            case usub::server::component::HeaderEnum::Last_Modified: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Last_Modified];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Last-Modified is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Last-Modified is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Link: {
                                std::vector<std::string> values = split(value, ',');
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Link];
                                insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                break;
                            }
                            case usub::server::component::HeaderEnum::Location: {
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Location];
                                if (!insert_place.empty()) [[unlikely]] {
                                    return usub::server::utils::error::warn("Location is already set");
                                }
                                insert_place.push_back(std::move(value));
                                break;
                            }
                            case usub::server::component::HeaderEnum::Max_Forwards: {
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Max_Forwards];
                                if (!insert_place.empty()) [[unlikely]] {
                                    return usub::server::utils::error::crit("Max-Forwards is already set");
                                }
                                if (!usub::utils::isPositiveIntegerString(value)) [[unlikely]] {
                                    return usub::server::utils::error::warn("Max-Forwards value is not a positive integer");
                                }
                                insert_place.push_back(std::move(value));
                                break;
                            }
#if defined(USE_EXPERIMENTAL_FEATURES) && USE_EXPERIMENTAL_FEATURES == 1
                            case crc32("nel", 3): {
                                this->headers_.NEL = value;// TODO: JSON parsing
                                break;
                            }
                            case crc32("no-vary-search", 14): {
                                this->headers_.No_Vary_Search.append(value);// TODO: Research
                                break;
                            }
                            case crc32("observe-browsing-topics", 22): {
                                this->headers_.Observe_Browsing_Topics = value;// TODO: Research
                                break;
                            }
#endif

                            case usub::server::component::HeaderEnum::Origin: {
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Origin];
                                if (!insert_place.empty()) [[unlikely]] {
                                    return usub::server::utils::error::crit("Origin is already set");
                                }
                                insert_place.push_back(std::move(value));
                                break;
                            }
#if defined(USE_EXPERIMENTAL_FEATURES) && USE_EXPERIMENTAL_FEATURES == 1
                            case crc32("origin-agent-cluster", 20): {
                                this->headers_.Origin_Agent_Cluster = (value == "?1");
                                break;
                            }
                            case crc32("permissions-policy", 18): {
                                this->headers_.Permissions_Policy = value;// TODO: Separate class
                                break;
                            }
#endif

#if defined(USE_DEPRECATED_FEATURES) && USE_DEPRECATED_FEATURES == 1
                            case crc32("pragma", 6): {
                                this->headers_.Pragma = (value == "no-cache");// Only one allowed value
                                break;
                            }
#endif
#if defined(USE_EXPERIMENTAL_FEATURES) && USE_EXPERIMENTAL_FEATURES == 1
                            case crc32("prefer", 20): {
                                break;
                            }
                            case crc32("preference-applied", 20): {
                                break;
                            }
#endif
                            case usub::server::component::HeaderEnum::Priority: {
                                std::vector<std::string> values = split(value, ',');
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Priority];
                                insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                break;
                            }
                            case usub::server::component::HeaderEnum::Proxy_Authenticate: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Proxy_Authenticate];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Proxy-Authenticate is already set");// seems its not fully comma separated list so we regect multiples
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Proxy-Authenticate is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Proxy_Authorization: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Proxy_Authorization];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Proxy-Authorization is already set");// seems its not fully comma separated list so we regect multiples
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Proxy-Authorization is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Range: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Range];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("Range is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Referer: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Referer];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Referer is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Referer is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Referrer_Policy: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Referrer_Policy];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Referrer-Policy is already set");
                                    }
                                    static const std::unordered_set<std::string> valid_values = {
                                            "no-referrer",
                                            "no-referrer-when-downgrade",
                                            "origin",
                                            "origin-when-cross-origin",
                                            "same-origin",
                                            "strict-origin",
                                            "strict-origin-when-cross-origin",
                                            "unsafe-url"};
                                    if (valid_values.find(value) != valid_values.end()) [[likely]] {
                                        insert_place.push_back(std::move(value));
                                    } else {
                                        return usub::server::utils::error::warn("Referrer-Policy only accepts 'no-referrer', 'no-referrer-when-downgrade', 'origin', 'origin-when-cross-origin', 'same-origin', 'strict-origin', 'strict-origin-when-cross-origin' or 'unsafe-url' as value");
                                    }
                                } else {
                                    return usub::server::utils::error::warn("Referrer-Policy is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Refresh: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Refresh];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Refresh is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Refresh is a Response only header");
                                }
                                break;
                            }
#if defined(USE_DEPRECATED_FEATURES) && USE_DEPRECATED_FEATURES == 1
                            case crc32("report-to", 9): {
                                this->headers_.Report_To = value;// TODO: JSON parsing
                                break;
                            }
#endif

#if defined(USE_EXPERIMENTAL_FEATURES) && USE_EXPERIMENTAL_FEATURES == 1
                            case crc32("reporting-endpoints", 19): {
                                this->headers_.Reporting_Endpoints.append(value);// TODO: Research
                                break;
                            }
#endif

                            case usub::server::component::HeaderEnum::Repr_Digest: {
                                std::vector<std::string> values = split(value, ',');
                                auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Repr_Digest];
                                insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                break;
                            }
                            case usub::server::component::HeaderEnum::Retry_After: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Retry_After];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Retry-After is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Retry-After is a Response only header");
                                }
                                break;
                            }
#if defined(USE_EXPERIMENTAL_FEATURES) && USE_EXPERIMENTAL_FEATURES == 1
                            case crc32("rtt", 3): {
                                this->headers_.RTT = std::stold(value);
                                break;
                            }
                            case crc32("save-data", 9): {
                                this->headers_.Save_Data = (value == "on");
                                break;
                            }
                            case crc32("sec-browsing-topics", 19): {
                                this->headers_.Sec_Browsing_Topics = value;// Placeholder for now
                                break;
                            }
                            case crc32("sec-ch-prefers-color-scheme", 26): {
                                this->headers_.Sec_CH_Prefers_Color_Scheme = value;// TODO: Research
                                break;
                            }
                            case crc32("sec-ch-prefers-reduced-motion", 28): {
                                this->headers_.Sec_CH_Prefers_Reduced_Motion = value;// TODO: Research
                                break;
                            }
                            case crc32("sec-ch-prefers-reduced-transparency", 35): {
                                this->headers_.Sec_CH_Prefers_Reduced_Transparency = value;// TODO: Research
                                break;
                            }
                            case crc32("sec-ch-ua", 9): {
                                this->headers_.Sec_CH_UA.append(value);// TODO: Research
                                break;
                            }
                            case crc32("sec-ch-ua-arch", 14): {
                                this->headers_.Sec_CH_UA_Arch = value;// TODO: Research
                                break;
                            }
                            case crc32("sec-ch-ua-bitness", 17): {
                                this->headers_.Sec_CH_UA_Bitness = std::stol(value);
                                break;
                            }
#if defined(USE_DEPRECATED_FEATURES) && USE_DEPRECATED_FEATURES == 1
                            case crc32("sec-ch-ua-full-version", 22): {
                                this->headers_.Sec_CH_UA_Full_Version = value;// TODO: Research
                                break;
                            }
#endif
#endif

#if defined(USE_EXPERIMENTAL_FEATURES) && USE_EXPERIMENTAL_FEATURES == 1
                            case crc32("sec-ch-ua-full-version-list", 26): {
                                this->headers_.Sec_CH_UA_Full_Version_List.append(value);// TODO: Research
                                break;
                            }
                            case crc32("sec-ch-ua-mobile", 16): {
                                this->headers_.Sec_CH_UA_Mobile = (value == "?1");
                                break;
                            }
                            case crc32("sec-ch-ua-model", 15): {
                                this->headers_.Sec_CH_UA_Model = value;// TODO: Research
                                break;
                            }
                            case crc32("sec-ch-ua-platform", 18): {
                                this->headers_.Sec_CH_UA_Platform = value;// TODO: Research
                                break;
                            }
                            case crc32("sec-ch-ua-platform-version", 26): {
                                this->headers_.Sec_CH_UA_Platform_Version = value;// TODO: Research
                                break;
                            }
#endif

                            case usub::server::component::HeaderEnum::Sec_Fetch_Dest: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Sec_Fetch_Dest];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Sec-Fetch-Dest is already set");
                                    }
                                    static const std::unordered_set<std::string> validSecFetchDest = {
                                            "audio", "audioworklet", "document", "embed", "empty", "fencedframe", "font",
                                            "frame", "iframe", "image", "manifest", "object", "paintworklet", "report",
                                            "script", "serviceworker", "sharedworker", "style", "track", "video", "webidentity",
                                            "worker", "xslt"};
                                    if (validSecFetchDest.find(value) != validSecFetchDest.end()) {
                                        insert_place.push_back(std::move(value));
                                    } else {
                                        return usub::server::utils::error::warn("Invalid Sec-Fetch-Dest header value: " + value);
                                    }
                                } else {
                                    return usub::server::utils::error::warn("Sec-Fetch-Dest is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Sec_Fetch_Mode: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Sec_Fetch_Mode];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Sec-Fetch-Mode is already set");
                                    }
                                    static const std::unordered_set<std::string> validSecFetchMode = {
                                            "cors", "navigate", "no-cors", "same-origin", "websocket"};
                                    if (validSecFetchMode.find(value) != validSecFetchMode.end()) {
                                        insert_place.push_back(std::move(value));
                                    } else {
                                        return usub::server::utils::error::warn("Invalid Sec-Fetch-Mode header value: " + value);
                                    }
                                } else {
                                    return usub::server::utils::error::warn("Sec-Fetch-Mode is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Sec_Fetch_Site: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Sec_Fetch_Site];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Sec-Fetch-Site is already set");
                                    }
                                    static const std::unordered_set<std::string> validSecFetchSite = {
                                            "cross-site", "same-origin", "same-site", "none"};
                                    if (validSecFetchSite.find(value) != validSecFetchSite.end()) {
                                        insert_place.push_back(std::move(value));
                                    } else {
                                        return usub::server::utils::error::warn("Invalid Sec-Fetch-Site header value: " + value);
                                    }
                                } else {
                                    return usub::server::utils::error::warn("Sec-Fetch-Site is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Sec_Fetch_User: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Sec_Fetch_User];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Sec-Fetch-User is already set");
                                    }
                                    if (value == "?1") {
                                        insert_place.push_back(std::move(value));
                                    } else [[unlikely]] {
                                        return usub::server::utils::error::warn("Sec-Fetch-User value is not '?1'");
                                    }
                                } else {
                                    return usub::server::utils::error::warn("Sec-Fetch-User is a Request only header");
                                }
                                break;
                            }
#if defined(USE_EXPERIMENTAL_FEATURES) && USE_EXPERIMENTAL_FEATURES == 1
                            case crc32("sec-gpc", 7): {
                                this->headers_.Sec_GPC = (value == "1");
                                break;
                            }
#endif

#if defined(ONLY_BASELINE) && ONLY_BASELINE == 0
                            case crc32("sec-purpose", 11): {
                                this->headers_.Sec_Purpose = (value == "1");
                                break;
                            }
#endif

                            case usub::server::component::HeaderEnum::Sec_WebSocket_Accept: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Sec_WebSocket_Accept];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Sec-WebSocket-Accept is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Sec-WebSocket-Accept is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Sec_WebSocket_Extensions: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Sec_WebSocket_Extensions];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Sec-WebSocket-Extensions is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Sec-WebSocket-Extensions is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Sec_WebSocket_Key: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Sec_WebSocket_Key];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Sec-WebSocket-Key is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Sec-WebSocket-Key is a Request only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Sec_WebSocket_Protocol: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Sec_WebSocket_Protocol];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Sec-WebSocket-Protocol is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Sec_WebSocket_Protocol];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Sec_WebSocket_Version: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Sec_WebSocket_Version];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Sec_WebSocket_Version];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Sec-WebSocket-Version is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Server: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Server];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Server is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Server is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Server_Timing: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Server_Timing];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                } else {
                                    return usub::server::utils::error::warn("Server-Timing is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Service_Worker: {
                                if constexpr (std::is_same_v<T, Request>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Service_Worker];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Service-Worker is already set");
                                    }
                                    if (value == "script") {
                                        insert_place.push_back(std::move(value));
                                    } else [[unlikely]] {
                                        return usub::server::utils::error::warn("Service-Worker value is not 'script'");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Service-Worker is a Response only header");
                                }
                                break;
                            }
                            case usub::server::component::HeaderEnum::Service_Worker_Allowed: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Service_Worker_Allowed];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("Service-Worker-Allowed is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Service-Worker-Allowed is a Response only header");
                                }
                                break;
                            }
#if defined(ONLY_BASELINE) && ONLY_BASELINE == 0
                            case usub::server::component::HeaderEnum::Service_Worker_Navigation_Preload: {
                                this->headers_.Service_Worker_Navigation_Preload = value;// TODO: Research
                                break;
                            }
#endif
                            case usub::server::component::HeaderEnum::Set_Cookie: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Set_Cookie];
                                    // if (!insert_place.empty()) [[unlikely]] {
                                    //     return usub::server::utils::error::crit("Set-Cookie is already set");
                                    // }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("Set-Cookie is a Response only header");
                                }
                                break;
                            }
#if defined(USE_EXPERIMENTAL_FEATURES) && USE_EXPERIMENTAL_FEATURES == 1
                            case crc32("set-login", 9): {
                                this->headers_.Set_Login = value;// TODO: Enum or bool
                                break;
                            }
#endif

                            case usub::server::component::HeaderEnum::SourceMap: {
                                if constexpr (std::is_same_v<T, Response>) {
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::SourceMap];
                                    if (!insert_place.empty()) [[unlikely]] {
                                        return usub::server::utils::error::crit("SourceMap is already set");
                                    }
                                    insert_place.push_back(std::move(value));
                                } else {
                                    return usub::server::utils::error::warn("SourceMap is a Response only header");
                                }
                                break;

#if defined(USE_EXPERIMENTAL_FEATURES) && USE_EXPERIMENTAL_FEATURES == 1
                                case crc32("speculation-rules", 18): {
                                    this->headers_.Speculation_Rules.push_back(value);// TODO: Research
                                    break;
                                }
#endif

                                case usub::server::component::HeaderEnum::Strict_Transport_Security: {
                                    if constexpr (std::is_same_v<T, Response>) {
                                        auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Strict_Transport_Security];
                                        if (!insert_place.empty()) [[unlikely]] {
                                            return usub::server::utils::error::crit("Strict-Transport-Security is already set");
                                        }
                                        insert_place.push_back(std::move(value));
                                    } else {
                                        return usub::server::utils::error::warn("Strict-Transport-Security is a Response only header");
                                    }
                                    break;
                                }
#if defined(USE_EXPERIMENTAL_FEATURES) && USE_EXPERIMENTAL_FEATURES == 1
                                case crc32("supports-loading-mode", 21): {
                                    this->headers_.Supports_Loading_Mode = value;// TODO: Enum
                                    break;
                                }
#endif

                                case usub::server::component::HeaderEnum::TE: {
                                    if constexpr (std::is_same_v<T, Request>) {
                                        std::vector<std::string> values = split(value, ',');
                                        auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::TE];
                                        insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                    } else {
                                        return usub::server::utils::error::warn("TE is a Request only header");
                                    }
                                    break;
                                }
#if defined(USE_DEPRECATED_FEATURES) && USE_DEPRECATED_FEATURES == 1
                                case crc32("tk", 2): {
                                    this->headers_.Tk = value;// TODO: Enum
                                    break;
                                }
#endif
#if defined(ONLY_BASELINE) && ONLY_BASELINE == 0
                                case usub::server::component::HeaderEnum::Trailer: {
                                    this->headers_.Trailer.push_back(value);// TODO: Research
                                    break;
                                }
#endif
                                case usub::server::component::HeaderEnum::Transfer_Encoding: {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Transfer_Encoding];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                    break;
                                }
                                case usub::server::component::HeaderEnum::Upgrade: {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Upgrade];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                    break;
                                }
                                case usub::server::component::HeaderEnum::Upgrade_Insecure_Requests: {
                                    if constexpr (std::is_same_v<T, Request>) {
                                        auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Upgrade_Insecure_Requests];
                                        if (!insert_place.empty()) [[unlikely]] {
                                            return usub::server::utils::error::crit("Upgrade-Insecure-Requests is already set");
                                        }
                                        if (value == "1") {
                                            insert_place.push_back(std::move(value));
                                        } else [[unlikely]] {
                                            return usub::server::utils::error::warn("Upgrade-Insecure-Requests value is not '1'");
                                        }
                                    } else {
                                        return usub::server::utils::error::warn("Upgrade-Insecure-Requests is a Request only header");
                                    }
                                    break;
                                }
                                case usub::server::component::HeaderEnum::User_Agent: {
                                    if constexpr (std::is_same_v<T, Request>) {
                                        auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::User_Agent];
                                        if (!insert_place.empty()) [[unlikely]] {
                                            return usub::server::utils::error::crit("User-Agent is already set");
                                        }
                                        insert_place.push_back(std::move(value));
                                    } else {
                                        return usub::server::utils::error::warn("User-Agent is a Request only header");
                                    }
                                    break;
                                }
                                // case usub::server::component::HeaderEnum::Vary: {
                                //     if constexpr (std::is_same_v<T, Response>) {
                                //         std::vector<std::string> values = split(value, ',');
                                //         auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Vary];
                                //         insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                //     } else {
                                //         return usub::server::utils::error::warn("Vary is a Response only header");
                                //     }
                                //     break;
                                // }
                                //case usub::server::component::HeaderEnum::Via: {
                                //    std::vector<std::string> values = split(value, ',');
                                //    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Via];
                                //    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                //    break;
                                //}
#if defined(USE_DEPRECATED_FEATURES) && USE_DEPRECATED_FEATURES == 1
                                case crc32("viewport-width", 14): {
                                    this->headers_.Viewport_Width = std::stol(value);
                                    break;
                                }
#endif

                                case usub::server::component::HeaderEnum::Want_Content_Digest: {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Want_Content_Digest];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                    break;
                                }
                                case usub::server::component::HeaderEnum::Want_Repr_Digest: {
                                    std::vector<std::string> values = split(value, ',');
                                    auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::Want_Repr_Digest];
                                    insert_place.insert(insert_place.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
                                    break;
                                }
#if defined(USE_DEPRECATED_FEATURES) && USE_DEPRECATED_FEATURES == 1
                                case crc32("warning", 7): {
                                    this->headers_.Warning = value;// TODO: Separate class
                                    break;
                                }
                                case crc32("width", 5): {
                                    this->headers_.Width = std::stol(value);
                                    break;
                                }
#endif

                                case usub::server::component::HeaderEnum::WWW_Authenticate: {
                                    if constexpr (std::is_same_v<T, Response>) {
                                        auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::WWW_Authenticate];
                                        if (!insert_place.empty()) [[unlikely]] {
                                            return usub::server::utils::error::crit("WWW-Authenticate is already set");
                                        }
                                        insert_place.push_back(std::move(value));
                                    } else {
                                        return usub::server::utils::error::warn("WWW-Authenticate is a Response only header");
                                    }
                                    break;
                                }
                                case usub::server::component::HeaderEnum::X_Content_Type_Options: {
                                    if constexpr (std::is_same_v<T, Response>) {
                                        auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::X_Content_Type_Options];
                                        if (!insert_place.empty()) [[unlikely]] {
                                            return usub::server::utils::error::crit("X-Content-Type-Options is already set");
                                        }
                                        if (value == "nosniff") {
                                            insert_place.push_back(std::move(value));
                                        } else [[unlikely]] {
                                            return usub::server::utils::error::warn("X-Content-Type-Options value is not 'nosniff'");
                                        }
                                        insert_place.push_back(std::move(value));
                                    } else {
                                        return usub::server::utils::error::warn("X-Content-Type-Options is a Response only header");
                                    }
                                    break;
                                }
#if defined(ONLY_BASELINE) && ONLY_BASELINE == 0
                                case crc32("x-dns-prefetch-control", 21): {
                                    this->headers_.X_DNS_Prefetch_Control = value;// Placeholder for now
                                    break;
                                }
                                case crc32("x-forwarded-for", 15): {
                                    this->headers_.X_Forwarded_For.append(value);// TODO: Research
                                    break;
                                }
                                case crc32("x-forwarded-host", 16): {
                                    this->headers_.X_Forwarded_Host = value;// Placeholder for now
                                    break;
                                }
                                case crc32("x-forwarded-proto", 17): {
                                    this->headers_.X_Forwarded_Proto = value;// Placeholder for now
                                    break;
                                }
#endif

                                case usub::server::component::HeaderEnum::X_Frame_Options: {
                                    if constexpr (std::is_same_v<T, Response>) {
                                        auto &insert_place = this->known_headers_map_[usub::server::component::HeaderEnum::X_Frame_Options];
                                        if (!insert_place.empty()) [[unlikely]] {
                                            return usub::server::utils::error::crit("X-Frame-Options is already set");
                                        }
                                        if (value == "DENY" || value == "SAMEORIGIN" || value == "ALLOW-FROM") {
                                            insert_place.push_back(std::move(value));
                                        } else [[unlikely]] {
                                            return usub::server::utils::error::warn("X-Frame-Options value is not 'DENY', 'SAMEORIGIN' or 'ALLOW-FROM'");
                                        }
                                    } else {
                                        return usub::server::utils::error::warn("X-Frame-Options is a Response only header");
                                    }
                                    break;
                                }
#if defined(ONLY_BASELINE) && ONLY_BASELINE == 0
                                case crc32("x-permitted-cross-domain-policies", 32): {
                                    this->headers_.X_Permitted_Cross_Domain_Policies = value;// Placeholder for now
                                    break;
                                }
                                case crc32("x-powered-by", 12): {
                                    this->headers_.X_Powered_By = value;// Placeholder for now
                                    break;
                                }
                                case crc32("x-robots-tag", 12): {
                                    this->headers_.X_Robots_Tag.append(value);// Placeholder for now
                                    break;
                                }
#if defined(USE_DEPRECATED_FEATURES) && USE_DEPRECATED_FEATURES == 1
                                case crc32("x-xss-protection", 16): {
                                    this->headers_.X_XSS_Protection = value;// Placeholder for now
                                    break;
                                }
#endif
#endif

                                default:
                                    usub::utils::trim(value);
                                    this->unknown_headers_map_[key].push_back(value);
                                    break;
                            }
                        }
                    } else {
                        usub::utils::trim(value);
                        this->unknown_headers_map_[key].push_back(value);
                    }
                    return {};

                } else {
                    static_assert(false, "Unsupported value type for addHeader");
                    return usub::server::utils::error::crit("Unreachable error reached");
                }
            }

            // template<typename T>
            const std::string string() const {

                std::string rv{};
                rv.reserve(8192);
                for (const auto &header: this->known_headers_map_) {
                    if (header.first != usub::server::component::HeaderEnum::Set_Cookie) [[likely]] {
                        rv += std::string(usub::server::component::header_enum_to_string_lower[(size_t) header.first]) + ": ";
                        for (const auto &value: header.second) {
                            rv += value + ", ";
                        }
                        rv[rv.size() - 2] = '\r';
                        rv[rv.size() - 1] = '\n';
                    } else [[unlikely]] {
                        for (const auto &value: header.second) {
                            rv += std::string(usub::server::component::header_enum_to_string_lower[(size_t) header.first]) + ": " + value + "\r\n";
                        }
                    }
                }
                for (const auto &header: this->unknown_headers_map_) {
                    rv += header.first + ": ";
                    for (const auto &value: header.second) {
                        rv += value + ", ";
                    }
                    rv[rv.size() - 2] = '\r';
                    rv[rv.size() - 1] = '\n';
                }
                return rv;
            }
            // ========== Iterator Logic ==========

            class Iterator {
            private:
                enum class Phase { Known,
                                   Unknown,
                                   End };

                std::unordered_map<usub::server::component::HeaderEnum, std::vector<std::string>>::const_iterator known_it_;
                std::unordered_map<usub::server::component::HeaderEnum, std::vector<std::string>>::const_iterator known_end_;
                std::unordered_map<std::string, std::vector<std::string>>::const_iterator unknown_it_;
                std::unordered_map<std::string, std::vector<std::string>>::const_iterator unknown_end_;
                Phase phase_;

            public:
                Iterator(
                        std::unordered_map<usub::server::component::HeaderEnum, std::vector<std::string>>::const_iterator k_it,
                        std::unordered_map<usub::server::component::HeaderEnum, std::vector<std::string>>::const_iterator k_end,
                        std::unordered_map<std::string, std::vector<std::string>>::const_iterator u_it,
                        std::unordered_map<std::string, std::vector<std::string>>::const_iterator u_end)
                    : known_it_(k_it), known_end_(k_end), unknown_it_(u_it), unknown_end_(u_end) {
                    if (known_it_ != known_end_) {
                        phase_ = Phase::Known;
                    } else if (unknown_it_ != unknown_end_) {
                        phase_ = Phase::Unknown;
                    } else {
                        phase_ = Phase::End;
                    }
                }

                Iterator &operator++() {
                    if (phase_ == Phase::Known) {
                        ++known_it_;
                        if (known_it_ == known_end_) {
                            if (unknown_it_ != unknown_end_) {
                                phase_ = Phase::Unknown;
                            } else {
                                phase_ = Phase::End;
                            }
                        }
                    } else if (phase_ == Phase::Unknown) {
                        ++unknown_it_;
                        if (unknown_it_ == unknown_end_) {
                            phase_ = Phase::End;
                        }
                    }
                    return *this;
                }

                bool operator!=(const Iterator &other) const {
                    return phase_ != other.phase_ ||
                           known_it_ != other.known_it_ ||
                           unknown_it_ != other.unknown_it_;
                }

                std::pair<std::string_view, const std::vector<std::string> &> operator*() const {
                    if (phase_ == Phase::Known) {
                        std::size_t index = static_cast<std::size_t>(known_it_->first);
                        std::string_view name = usub::server::component::header_enum_to_string_lower[index];
                        return {name, known_it_->second};
                    } else {
                        return {unknown_it_->first, unknown_it_->second};
                    }
                }
            };

            Iterator begin() const {
                return Iterator(
                        known_headers_map_.begin(), known_headers_map_.end(),
                        unknown_headers_map_.begin(), unknown_headers_map_.end());
            }

            Iterator end() const {
                return Iterator(
                        known_headers_map_.end(), known_headers_map_.end(),
                        unknown_headers_map_.end(), unknown_headers_map_.end());
            }
        };


    }// namespace protocols::http
}// namespace usub::server
