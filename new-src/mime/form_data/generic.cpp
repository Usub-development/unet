#include "unet/mime/multipart/form_data/generic.hpp"
#include <sstream>

namespace usub::unet::mime::multipart {

    std::expected<void, std::string> FormData::parse(std::string input) {
        std::istringstream stream(input);
        std::string line;

        std::string detectedBoundary;
        bool isFirstLine = true;
        bool readingData = false;

        std::optional<std::string> rawDisposition;
        std::optional<std::string> rawContentType;
        std::ostringstream dataBuffer;
        std::unordered_map<std::string, std::vector<std::string>, usub::utils::CaseInsensitiveHash, usub::utils::CaseInsensitiveEqual> extra_Headers;

        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (isFirstLine) {
                if (line.rfind("--", 0) == 0) {
                    detectedBoundary = line.substr(2);
                } else {
                    return std::unexpected("Invalid multipart input: first line must start with '--'.");
                }

                if (this->boundary_.empty()) {
                    return std::unexpected("Boundary must be provided.");
                }

                if (this->boundary_ != detectedBoundary) {
                    return std::unexpected("Boundary mismatch");
                }

                isFirstLine = false;
                continue;
            }

            if (line == "--" + detectedBoundary || line == "--" + detectedBoundary + "--") {
                if (rawDisposition.has_value()) {
                    std::unordered_map<std::string, std::string> disp;
                    for (auto &seg: usub::utils::split(rawDisposition.value(), ';')) {
                        usub::utils::trim(seg);
                        auto eq = seg.find('=');
                        if (eq == std::string::npos) {
                            continue;
                        }
                        std::string keyPart = seg.substr(0, eq);
                        std::string valPart = seg.substr(eq + 1);

                        usub::utils::trim(keyPart);
                        usub::utils::trim(valPart);

                        if (valPart.size() >= 2 &&
                            valPart.front() == '"' &&
                            valPart.back() == '"') {
                            valPart = valPart.substr(1, valPart.size() - 2);
                        }

                        disp.emplace(usub::utils::toLower(keyPart), std::move(valPart));
                    }

                    std::string ctype = rawContentType.value_or("text/plain; charset=US-ASCII");

                    std::string content = dataBuffer.str();

                    if (!content.empty() && content.back() == '\n')
                        content.pop_back();
                    if (!content.empty() && content.back() == '\r')
                        content.pop_back();

                    Part part{
                            std::move(ctype),
                            std::move(disp),
                            std::move(content),
                            std::move(extra_Headers)};

                    auto itName = part.disposition.find("name");
                    if (itName != part.disposition.end()) {
                        this->parts_by_name_[itName->second].push_back(std::move(part));
                    }

                    rawDisposition.reset();
                    rawContentType.reset();
                    dataBuffer.str("");
                    dataBuffer.clear();
                    extra_Headers.clear();
                }

                if (line == "--" + detectedBoundary + "--") {
                    break;// final boundary
                }

                readingData = false;
                continue;
            }

            if (!readingData && line.find("Content-Disposition:") == 0) {
                rawDisposition = usub::utils::trim_copy(line.substr(std::string("Content-Disposition:").length()));
            } else if (!readingData && line.find("Content-Type:") == 0) {
                rawContentType = usub::utils::trim_copy(line.substr(std::string("Content-Type:").length()));
            } else if (!readingData && line.find(":") != std::string::npos) {
                auto colonPos = line.find(":");
                std::string headerName = usub::utils::trim_copy(line.substr(0, colonPos));
                std::string headerValue = usub::utils::trim_copy(line.substr(colonPos + 1));

                if (!rawDisposition.has_value()) continue;
                extra_Headers[headerName].push_back(headerValue);
            } else if (!readingData && line.empty()) {
                readingData = true;
            } else if (readingData) {
                dataBuffer << line << '\n';
            }
        }

        return {};
    }

}// namespace usub::unet::mime::multipart
