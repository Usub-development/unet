//
// Created by Kirill Zhukov on 20.11.2023.
//

#include "BasicEndpointHandler.h"

namespace unit::server::regex::basic {
    std::future<std::optional<std::function<void(data::HttpRequest&, data::HttpResponse&)>>> BasicEndpointHandler::
    matchAsync(const request::type&reqType, const std::string&data) {
        return std::async(std::launch::async, [this, reqType, data]() {
            return match(reqType, data); // Call the synchronous match function
        });
    }

    std::optional<std::function<void (data::HttpRequest&, data::HttpResponse&)>> BasicEndpointHandler::match(
        request::type request_type, const std::string&data) {
        if (const auto it = std::ranges::find_if(regexes, [&request_type, &data, this](const auto&tuple) {
            return std::get<0>(tuple) == request_type && equalsFunction(request_type, data, std::get<1>(tuple));
        }); it != regexes.end()) {
            return std::get<2>(*it);
        }
        return std::nullopt;
    }

    void BasicEndpointHandler::addHandler(request::type request_type, const std::regex&regular_expression,
                                          std::function<void (data::HttpRequest&,
                                                              data::HttpResponse&)> function) {
        this->regexes.emplace_back(request_type, regular_expression, function);
    }

    void BasicEndpointHandler::addModule(const std::shared_ptr<EndpointHandler> &module_handler) {
        auto vec = module_handler->getHandlers();
        this->regexes.insert(this->regexes.end(), vec.begin(), vec.end());
    }

    std::vector<std::tuple<request::type, std::regex, std::function<void(data::HttpRequest&, data::HttpResponse&)>>>
    BasicEndpointHandler::getHandlers() {
        return this->regexes;
    }

    size_t BasicEndpointHandler::getSize() const {
        return this->regexes.size();
    }

    bool BasicEndpointHandler::equalsFunction(request::type request_type, const std::string&data,
                                              const std::regex&e) {
        return std::regex_match(data, e);
    }
}; // unit::server::regex::basic
