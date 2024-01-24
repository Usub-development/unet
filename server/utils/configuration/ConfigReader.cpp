//
// Created by Kirill Zhukov on 15.11.2023.
//

#include "ConfigReader.h"

#include "logging/logger.h"

unit::server::configuration::ConfigReader::ConfigReader(const std::string&config_path) {
    try {
        this->res = toml::parse_file(config_path);
    }
    catch (toml::v3::ex::parse_error&e) {
        error_log("`ConfigReader` constructor error: %s", e.what());
        exit(9);
    }
}

std::string unit::server::configuration::ConfigReader::getKeyFilePath() {
    if (this->res.contains("certs") && this->res.get_as<toml::table>("certs")->contains("key_file")) {
        return this->res["certs"]["key_file"].as_string()->get();
    }
    return "";
}

std::string unit::server::configuration::ConfigReader::getPemFilePath() {
    if (this->res.contains("certs") && this->res.get_as<toml::table>("certs")->contains("cert_file")) {
        return this->res["certs"]["cert_file"].as_string()->get();
    }
    return "";
}

std::string unit::server::configuration::ConfigReader::getPort() {
    if (!this->res.contains("net") || !this->res.get_as<toml::table>("net")->contains("port")) {
        error_log("No port was provided");
        exit(9);
    }
    return std::to_string(this->res["net"]["port"].as_integer()->get());
}

toml::node_view<toml::node> unit::server::configuration::ConfigReader::getKey(const std::string&key) {
    return this->res[key];
}

toml::parse_result& unit::server::configuration::ConfigReader::getResult() {
    return this->res;
}

int64_t unit::server::configuration::ConfigReader::getThreads() {
    if (this->res.contains("server") || this->res.get_as<toml::table>("server")->contains("threads")) {
        return this->res["server"]["threads"].as_integer()->get();
    }
    return 1;
}

int unit::server::configuration::ConfigReader::getBacklog() {
    if (!this->res.contains("server") || !this->res.get_as<toml::table>("server")->contains("backlog")) {
        return 50;
    }
    return this->res["server"]["backlog"].as_integer()->get();
}

int unit::server::configuration::ConfigReader::getProtocolVersion() {
    if (this->res.contains("server")) {
        if (this->res.get_as<toml::table>("server")->contains("http_version")) {
            return this->res["server"]["http_version"].as_integer()->get();
        }
        if (this->res.get_as<toml::table>("server")->contains("ssl")) {
            if (this->res["server"]["ssl"].as_boolean()->get())
                return 2;
            return 1;
        }
    }
    error_log("No server setting has been provided in config file");
    exit(9);
}

bool unit::server::configuration::ConfigReader::is_ssl() {
    if (this->res.get_as<toml::table>("server")->contains("ssl")) {
        return this->res["server"]["ssl"].as_boolean()->get();
    }
    return true;
}

std::string unit::server::configuration::ConfigReader::getIPAddr() {
    if (this->res.contains("server") && this->res.get_as<toml::table>("server")->contains("ip_addr")) {
        return this->res["server"]["ip_addr"].as_string()->get();
    }
    return "0.0.0.0";
}

int unit::server::configuration::ConfigReader::getIPV() {
    if (this->res.contains("server") && this->res.get_as<toml::table>("server")->contains("ipv")) {
        return this->res["server"]["ipv"].as_integer()->get();
    }
    return 4;
}

bool unit::server::configuration::ConfigReader::getStatusError() {
    if (this->res.contains("log") && this->res.get_as<toml::table>("log")->contains("status_error")) {
        return this->res["log"]["status_error"].as_boolean()->get();
    }
    return true;
}
