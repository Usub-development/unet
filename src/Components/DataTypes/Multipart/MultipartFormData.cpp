#include <iostream>
#include <string>
#include <sstream>
#include <optional>
#include  "Components/DataTypes/Multipart/MultipartFormData.h"


std::expected<void, std::string> MultipartFormData::parse(std::string input) {
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
			}
			else {
				return std::unexpected("Invalid multipart input: first line must start with '--'.");
			}

			if (this->boundary_.empty()) {
				return std::unexpected("Boundary must be provided in the constructor.");
			}

			if (this->boundary_ != detectedBoundary) {
				return std::unexpected(
					"Boundary mismatch"
				);
			}

			isFirstLine = false;
			continue;
		}

		if (line == "--" + detectedBoundary || line == "--" + detectedBoundary + "--") {
			if (rawDisposition.has_value()) {
                std::unordered_map<std::string,std::string> disp;
                for (auto &seg : usub::utils::split(rawDisposition.value(), ';')) {
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
					    valPart.back()  == '"')
					{
					    valPart = valPart.substr(1, valPart.size() - 2);
					}
					
					disp.emplace(
					    usub::utils::toLower(keyPart),
					    std::move(valPart)
					);
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
					std::move(extra_Headers)
				};

				auto itName = part.disposition.find("name");
                if (itName != part.disposition.end()) {
                    parts_by_name_[itName->second].push_back(std::move(part));
                }

				rawDisposition.reset();
				rawContentType.reset();
				dataBuffer.str("");
				dataBuffer.clear();
				extra_Headers.clear();
			}

			if (line == "--" + detectedBoundary + "--") {
				break; // final boundary
			}

			readingData = false;
			continue;
		}

		if (!readingData && line.find("Content-Disposition:") == 0) {
			rawDisposition = usub::utils::trim_copy(line.substr(std::string("Content-Disposition:").length()));
		}
		else if (!readingData && line.find("Content-Type:") == 0) {
			rawContentType = usub::utils::trim_copy(line.substr(std::string("Content-Type:").length()));
		}
		else if (!readingData && line.find(":") != std::string::npos) {
			auto colonPos = line.find(":");
			std::string headerName = usub::utils::trim_copy(line.substr(0, colonPos));
			std::string headerValue = usub::utils::trim_copy(line.substr(colonPos + 1));

			if (!rawDisposition.has_value()) continue;
			extra_Headers[headerName].push_back(headerValue);
		}
		else if (!readingData && line.empty()) {
			readingData = true;
		}
		else if (readingData) {
			dataBuffer << line << '\n';
		}
	}

	return {}; // success
}


void MultipartFormData::printParsedMultipart(const MultipartFormData& form) {
    int index = 0;
    for (const auto& [name, parts] : form.parts_by_name_) {
        for (const auto& part : parts) {
            std::cout << "===== Part #" << index++ << " =====\n";
            std::cout << "Field Name: " << name << "\n";

            std::cout << "Content-Disposition:\n";
            for (const auto& [k, v] : part.disposition) {
                std::cout << "  " << k << " = \"" << v << "\"\n";
            }

            std::cout << "Content-Type: " << part.content_type << "\n";

            if (!part.headers.empty()) {
                std::cout << "Headers:\n";
                for (const auto& [header, values] : part.headers) {
                    std::cout << "  " << header << ": ";
                    for (size_t i = 0; i < values.size(); ++i) {
                        std::cout << values[i];
                        if (i + 1 < values.size()) std::cout << ", ";
                    }
                    std::cout << "\n";
                }
            }

            std::cout << "Body:\n" << part.data << "\n";
            std::cout << "====================\n\n";
        }
    }
}

std::string& MultipartFormData::getPart(const std::string& name) {
	auto it = parts_by_name_.find(name);
	if (it != parts_by_name_.end() && !it->second.empty()) {
		return it->second.front().data;
	}

	throw std::out_of_range("No part with key: " + name);

}

std::string MultipartFormData::getPart(const std::string& name) const {
	auto it = parts_by_name_.find(name);
	if (it != parts_by_name_.end() && !it->second.empty()) {
		return it->second.front().data;
	}
	throw std::out_of_range("No part with key: " + name);

}

std::vector<Part>& MultipartFormData::operator[](std::string_view name) {
	return parts_by_name_[std::string(name)];  
}

const std::vector<Part>& MultipartFormData::operator[](std::string_view name) const {
	static const std::vector<Part> empty;
	auto it = parts_by_name_.find(std::string(name));
	if (it != parts_by_name_.end()) {
		return it->second;
	}
	return empty;
}

bool MultipartFormData::contains(const std::string& name) const {
	auto it = parts_by_name_.find(name);
	return it != parts_by_name_.end() && !it->second.empty();
}

size_t MultipartFormData::count(const std::string& name) const {
	auto it = parts_by_name_.find(name);
	if (it != parts_by_name_.end()) {
		return it->second.size(); 
	}
	return 0;
}

size_t MultipartFormData::size() const {
	return parts_by_name_.size();
}   

std::vector<Part>& MultipartFormData::at(const std::string& name) {
	return parts_by_name_.at(name);  // throws std::out_of_range if not found
}
const std::vector<Part>& MultipartFormData::at(const std::string& name) const {
	return parts_by_name_.at(name);
}

void MultipartFormData::addPart(const Part& part) {
	auto itName = part.disposition.find("name");
	if (itName == part.disposition.end()) {
	    throw std::invalid_argument("[MultipartFormData] Part is missing 'name' parameter.");
	}

	parts_by_name_[itName->second].push_back(part);
}

void MultipartFormData::addPart(Part&& part) {
	auto itName = part.disposition.find("name");
	if (itName == part.disposition.end()) {
	    throw std::invalid_argument("[MultipartFormData] Part is missing 'name' parameter.");
	}

	parts_by_name_[itName->second].push_back(std::move(part));
}

void MultipartFormData::clear() {
	for (auto& [_, parts] : parts_by_name_) {
		for (auto& part : parts) {

			part.disposition.clear();
			part.content_type.clear();
			part.data.clear();
			part.headers.clear();
		
		}
		parts.clear(); // clear vector of Part objects
	}
	parts_by_name_.clear(); // clear the whole map
}

void MultipartFormData::print() const
{
    std::cout << "MultipartFormData {\n";

    for (const auto &[fieldName, partsVec] : parts_by_name_) {
        for (const auto &part : partsVec) {
            std::cout << "----- part \"" << fieldName << "\" -----\n";

            /* --- Content-Disposition --- */
            std::cout << "Content-Disposition: form-data";
            for (const auto &[k, v] : part.disposition)
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

std::string MultipartFormData::string() const
{
    std::ostringstream out;
    const std::string delim = "--" + boundary_;

    for (const auto &[fieldName, partsVec] : parts_by_name_) {
        for (const auto &part : partsVec) {
            out << delim << "\r\n";
			// TODO: fix quoted string
            out << "Content-Disposition: form-data";
            for (const auto &[k, v] : part.disposition)
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

MultipartFormData& MultipartFormData::setBoundary(std::string boundary) {

	this->boundary_ = boundary;
	return *this;
}


