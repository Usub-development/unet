//
// Created by Kirill Zhukov on 15.11.2023.
//

#ifndef CONFIGREADER_H
#define CONFIGREADER_H

#include <iostream>
#include "toml/toml.hpp"

namespace unit::server {
    namespace configuration {
        namespace error {
            class WrongConfig final : public std::exception {
            public:
                // Constructor (C++11 onward)
                explicit WrongConfig(std::string msg) : message(std::move(msg)) {}

                // Override the what() method from the base class
                [[nodiscard]] const char* what() const noexcept override;

            private:
                std::string message;
            };
        }; // error

        class ConfigReader {
        public:
            explicit ConfigReader(const std::string&config_path);

            std::string getKeyFilePath();

            std::string getPemFilePath();

            std::string getPort();

            int64_t getThreads();

            int getBacklog();

            int getProtocolVersion();

            bool is_ssl();

            std::string getIPAddr();

            int getIPV();

            bool getStatusError();

            toml::node_view<toml::node> getKey(const std::string &key);

            toml::parse_result& getResult();

        private:
            toml::parse_result res;
        };

    }; // configuration
}; // unit::server


#endif //CONFIGREADER_H
