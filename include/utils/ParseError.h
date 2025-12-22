#pragma once

#include <expected>

namespace usub::server::utils::error {

    // TODO: This will later be used all throughout the program to tell if there was an error during parsing.

    enum class ErrorSeverity {
        Warning,
        Critical
    };

    struct ParseError {
        ErrorSeverity severity;
        std::string message;
    };

    inline std::expected<void, ParseError> warn(std::string msg) {
        return std::expected<void, ParseError>{std::unexpected(ParseError{ErrorSeverity::Warning, std::move(msg)})};
    }

    inline std::expected<void, ParseError> crit(std::string msg) {
        return std::expected<void, ParseError>{std::unexpected(ParseError{ErrorSeverity::Critical, std::move(msg)})};
    }


}// namespace usub::server::utils::error
