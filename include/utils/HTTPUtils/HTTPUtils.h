#ifndef HTTPUTILS_H
#define HTTPUTILS_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace usub::utils {
    // TODO: This should be removed/copied into classes with corresponding character limitations instead of all things being in one place
    // Also the naming of this file is not really good

    bool isTchar(char c);
    // checks if string contains only tchar characters and SP (space) or "," (comma)
    bool isTchar(std::string_view value);
    bool isVchar(char c);
    // checks if string contains only vchar characters and SP (space) or "," (comma)
    bool isVchar(std::string_view value);
    bool isObsText(char c);
    bool isVcharOrObsText(char c);
    //https://www.rfc-editor.org/rfc/rfc5646.html#section-2.1
    // checks if string contains only VCHAR, obs-text characters and SP (space) or "," (comma)
    bool isVcharOrObsText(std::string_view value);
    bool isLangtag(std::string_view value);
    //https://httpwg.org/specs/rfc9110.html#field.etag
    bool isETag(std::string_view value);

    bool isLanguageTag(std::string_view value);
    bool areLanguageTags(std::string_view value);

}// namespace usub::utils


#endif// HTTPUTILS_H
