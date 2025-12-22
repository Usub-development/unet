#include <string>

namespace usub::utils {
    // TODO: Remove this duplication of other Header Utils file

    bool isToken(std::string_view value);
    bool areTokens(std::string_view value);
    bool isValidEtag(std::string_view value);
    bool areValidEtags(std::string_view value);// TODO: Optimize & test
}// namespace usub::utils