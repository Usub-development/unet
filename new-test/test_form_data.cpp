#include <cassert>
#include <iostream>
#include <string>


#include "unet/mime/multipart/form_data/generic.hpp"

using namespace usub::unet::mime::multipart;

int main() {
    // Build a simple multipart body with two fields
    std::string boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
    std::string body;
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"field1\"\r\n";
    body += "\r\n";
    body += "value1\r\n";
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"field2\"\r\n";
    body += "Content-Type: text/plain\r\n";
    body += "\r\n";
    body += "value2\r\n";
    body += "--" + boundary + "--\r\n";

    FormData parser(boundary);
    auto result = parser.parse(body);
    if (!result) {
        std::cerr << "parse_form_data failed: " << result.error() << std::endl;
        return 2;
    }

    auto &parts = parser.parts_by_name();
    // Expect two fields
    assert(parts.count("field1") == 1);
    assert(parts.count("field2") == 1);
    assert(parts.at("field1")[0].data == "value1");
    assert(parts.at("field2")[0].data == "value2");

    std::cout << "All basic tests passed.\n";
    return 0;
}
