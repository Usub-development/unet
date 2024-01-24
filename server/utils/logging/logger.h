//
// Created by Kirill Zhukov on 01.12.2023.
//

#ifndef LOGGER_H
#define LOGGER_H

#include <cstdarg>
#include <vector>

static inline void error_log(const char *fmt, ...) {
    time_t t = time(nullptr);
    char time_buf[100];
    strftime(time_buf, sizeof time_buf, "%D %T", gmtime(&t));
    std::va_list args1;
    va_start(args1, fmt);
    std::va_list args2;
    va_copy(args2, args1);
    std::vector<char> buf(1 + std::vsnprintf(nullptr, 0, fmt, args1));
    va_end(args1);
    std::vsnprintf(buf.data(), buf.size(), fmt, args2);
    va_end(args2);
    std::printf("%s [error]: %s\n", time_buf, buf.data());
}
static inline void info_log(const char *fmt, ...) {
    time_t t = time(nullptr);
    char time_buf[100];
    strftime(time_buf, sizeof time_buf, "%D %T", gmtime(&t));
    std::va_list args1;
    va_start(args1, fmt);
    std::va_list args2;
    va_copy(args2, args1);
    std::vector<char> buf(1 + std::vsnprintf(nullptr, 0, fmt, args1));
    va_end(args1);
    std::vsnprintf(buf.data(), buf.size(), fmt, args2);
    va_end(args2);
    std::printf("%s [info]: %s\n", time_buf, buf.data());
}

#endif //LOGGER_H
