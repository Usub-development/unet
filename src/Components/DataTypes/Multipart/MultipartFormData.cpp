#include "Components/DataTypes/Multipart/MultipartFormData.h"
#include "unet/mime/multipart/form_data/generic.hpp"
#include <iostream>
#include <optional>
#include <sstream>
#include <string>


std::expected<void, std::string> MultipartFormData::parse(std::string input) {
    using namespace usub::unet::mime::multipart;

    FormData parser(this->boundary_);
    auto result = parser.parse(std::move(input));
    if (!result) {
        return std::unexpected(result.error());
    }

    // convert parsed (new) Part -> old ::Part
    for (auto &kv: parser.parts_by_name()) {
        const std::string &fieldName = kv.first;
        for (auto &newPart: kv.second) {
            ::Part oldPart;
            oldPart.content_type = std::move(newPart.content_type);
            oldPart.disposition = std::move(newPart.disposition);
            oldPart.data = std::move(newPart.data);
            oldPart.headers = std::move(newPart.headers);

            parts_by_name_[fieldName].push_back(std::move(oldPart));
        }
    }

    return {};
}


void MultipartFormData::printParsedMultipart(const MultipartFormData &form) {
    int index = 0;
    for (const auto &[name, parts]: form.parts_by_name_) {
        for (const auto &part: parts) {
            std::cout << "===== Part #" << index++ << " =====\n";
            std::cout << "Field Name: " << name << "\n";

            std::cout << "Content-Disposition:\n";
            for (const auto &[k, v]: part.disposition) {
                std::cout << "  " << k << " = \"" << v << "\"\n";
            }

            std::cout << "Content-Type: " << part.content_type << "\n";

            if (!part.headers.empty()) {
                std::cout << "Headers:\n";
                for (const auto &[header, values]: part.headers) {
                    std::cout << "  " << header << ": ";
                    for (size_t i = 0; i < values.size(); ++i) {
                        std::cout << values[i];
                        if (i + 1 < values.size()) std::cout << ", ";
                    }
                    std::cout << "\n";
                }
            }

            std::cout << "Body:\n"
                      << part.data << "\n";
            std::cout << "====================\n\n";
        }
    }
}

std::string &MultipartFormData::getPart(const std::string &name) {
    auto it = parts_by_name_.find(name);
    if (it != parts_by_name_.end() && !it->second.empty()) {
        return it->second.front().data;
    }

    throw std::out_of_range("No part with key: " + name);
}

std::string MultipartFormData::getPart(const std::string &name) const {
    auto it = parts_by_name_.find(name);
    if (it != parts_by_name_.end() && !it->second.empty()) {
        return it->second.front().data;
    }
    throw std::out_of_range("No part with key: " + name);
}

std::vector<Part> &MultipartFormData::operator[](std::string_view name) {
    return parts_by_name_[std::string(name)];
}

const std::vector<Part> &MultipartFormData::operator[](std::string_view name) const {
    static const std::vector<Part> empty;
    auto it = parts_by_name_.find(std::string(name));
    if (it != parts_by_name_.end()) {
        return it->second;
    }
    return empty;
}

bool MultipartFormData::contains(const std::string &name) const {
    auto it = parts_by_name_.find(name);
    return it != parts_by_name_.end() && !it->second.empty();
}

size_t MultipartFormData::count(const std::string &name) const {
    auto it = parts_by_name_.find(name);
    if (it != parts_by_name_.end()) {
        return it->second.size();
    }
    return 0;
}

size_t MultipartFormData::size() const {
    return parts_by_name_.size();
}

std::vector<Part> &MultipartFormData::at(const std::string &name) {
    return parts_by_name_.at(name);// throws std::out_of_range if not found
}
const std::vector<Part> &MultipartFormData::at(const std::string &name) const {
    return parts_by_name_.at(name);
}

void MultipartFormData::addPart(const Part &part) {
    auto itName = part.disposition.find("name");
    if (itName == part.disposition.end()) {
        throw std::invalid_argument("[MultipartFormData] Part is missing 'name' parameter.");
    }

    parts_by_name_[itName->second].push_back(part);
}

void MultipartFormData::addPart(Part &&part) {
    auto itName = part.disposition.find("name");
    if (itName == part.disposition.end()) {
        throw std::invalid_argument("[MultipartFormData] Part is missing 'name' parameter.");
    }

    parts_by_name_[itName->second].push_back(std::move(part));
}

void MultipartFormData::clear() {
    for (auto &[_, parts]: parts_by_name_) {
        for (auto &part: parts) {

            part.disposition.clear();
            part.content_type.clear();
            part.data.clear();
            part.headers.clear();
        }
        parts.clear();// clear vector of Part objects
    }
    parts_by_name_.clear();// clear the whole map
}

void MultipartFormData::print() const {
    std::cout << "MultipartFormData {\n";

    for (const auto &[fieldName, partsVec]: parts_by_name_) {
        for (const auto &part: partsVec) {
            std::cout << "----- part \"" << fieldName << "\" -----\n";

            /* --- Content-Disposition --- */
            std::cout << "Content-Disposition: form-data";
            for (const auto &[k, v]: part.disposition)
                std::cout << "; " << k << "=\"" << v << "\"";
            std::cout << '\n';

            /* --- Content-Type (если был) --- */
            if (!part.content_type.empty())
                std::cout << "Content-Type: " << part.content_type << '\n';

            /* --- useful debug info --- */
            std::cout << "Data size: " << part.data.size() << " bytes\n";
        }
    }
    std::cout << "}\n";
}

std::string MultipartFormData::string() const {
    std::ostringstream out;
    const std::string delim = "--" + boundary_;

    for (const auto &[fieldName, partsVec]: parts_by_name_) {
        for (const auto &part: partsVec) {
            out << delim << "\r\n";
            // TODO: fix quoted string
            out << "Content-Disposition: form-data";
            for (const auto &[k, v]: part.disposition)
                out << "; " << k << "=\"" << v << "\"";
            out << "\r\n";

            if (!part.content_type.empty())
                out << "Content-Type: " << part.content_type << "\r\n";

            out << "\r\n";
            out << part.data << "\r\n";
        }
    }

    out << delim << "--\r\n";
    return out.str();
}

MultipartFormData &MultipartFormData::setBoundary(std::string boundary) {

    this->boundary_ = boundary;
    return *this;
}
