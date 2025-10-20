#pragma once

#include <string>

namespace usub::server::component {
    class ETag {
    private:
        std::string etag_{};
        bool weak_{false};

    public:
        ETag() = default;
        ~ETag() = default;

        ETag(const ETag &other);
        ETag(ETag &&other) noexcept;
        explicit ETag(const std::string &etag);
        explicit ETag(const std::string_view &etag);

        ETag &operator=(const ETag &other);
        ETag &operator=(ETag &&other) noexcept;
        ETag &operator=(const std::string_view &etag);

        bool operator==(const ETag &other) const;
        bool operator!=(const ETag &other) const;
        bool operator<(const ETag &other) const;
        bool operator>(const ETag &other) const;
        bool operator<=(const ETag &other) const;
        bool operator>=(const ETag &other) const;

        bool operator==(const std::string_view &other) const;
        bool operator!=(const std::string_view &other) const;
        bool operator<(const std::string_view &other) const;
        bool operator>(const std::string_view &other) const;
        bool operator<=(const std::string_view &other) const;
        bool operator>=(const std::string_view &other) const;

        ETag &clear();

        bool isWeak() const;

        bool parse(std::string_view etag);
        std::string string() const;
    };
}// namespace usub::server::component