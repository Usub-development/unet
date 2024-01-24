//
// Created by Kirill Zhukov on 08.01.2024.
//

#include "StatusCodeHandler.h"

#include "logging/logger.h"


namespace unit::server::http1 {
    StatusCodeHandler::StatusCodeHandler(bool status_error) : status_error(status_error) {
        statusCodeMessages.reserve(80);
        statusCodeMessages[100] = "Continue";
        statusCodeMessages[101] = "Switching Protocols";
        statusCodeMessages[102] = "Processing";
        statusCodeMessages[103] = "Early Hints";
        statusCodeMessages[200] = "OK";
        statusCodeMessages[201] = "Created";
        statusCodeMessages[202] = "Accepted";
        statusCodeMessages[203] = "Non-Authoritative Information";
        statusCodeMessages[204] = "No Content";
        statusCodeMessages[205] = "Reset Content";
        statusCodeMessages[206] = "Partial Content";
        statusCodeMessages[207] = "Multi-Status";
        statusCodeMessages[208] = "Already Reported";
        statusCodeMessages[226] = "IM Used";
        statusCodeMessages[300] = "Multiple Choices";
        statusCodeMessages[301] = "Moved Permanently";
        statusCodeMessages[302] = "Found";
        statusCodeMessages[303] = "See Other";
        statusCodeMessages[304] = "Not Modified";
        statusCodeMessages[306] = "Switch Proxy";
        statusCodeMessages[307] = "Temporary Redirect";
        statusCodeMessages[308] = "Resume Incomplete";
        statusCodeMessages[400] = "Bad Request";
        statusCodeMessages[401] = "Unauthorized";
        statusCodeMessages[402] = "Payment Required";
        statusCodeMessages[403] = "Forbidden";
        statusCodeMessages[404] = "Not Found";
        statusCodeMessages[405] = "Method Not Allowed";
        statusCodeMessages[406] = "Not Acceptable";
        statusCodeMessages[407] = "Proxy Authentication Required";
        statusCodeMessages[408] = "Request Timeout";
        statusCodeMessages[409] = "Conflict";
        statusCodeMessages[410] = "Gone";
        statusCodeMessages[411] = "Length Required";
        statusCodeMessages[412] = "Precondition Failed";
        statusCodeMessages[413] = "Request Entity Too Large";
        statusCodeMessages[414] = "Request-URI Too Long";
        statusCodeMessages[415] = "Unsupported Media Type";
        statusCodeMessages[416] = "Requested Range Not Satisfiable";
        statusCodeMessages[417] = "Expectation Failed";
        statusCodeMessages[418] = "I'm a teapot";
        statusCodeMessages[421] = "Misdirected Request";
        statusCodeMessages[422] = "Unprocessable Entity";
        statusCodeMessages[423] = "Locked";
        statusCodeMessages[424] = "Failed Dependency";
        statusCodeMessages[426] = "Upgrade Required";
        statusCodeMessages[428] = "Precondition Required";
        statusCodeMessages[429] = "Too Many Requests";
        statusCodeMessages[431] = "Request Header Fields Too Large";
        statusCodeMessages[440] = "Login Time-out";
        statusCodeMessages[444] = "Connection Closed Without Response";
        statusCodeMessages[449] = "Retry With";
        statusCodeMessages[450] = "Blocked by Windows Parental Controls";
        statusCodeMessages[451] = "Unavailable For Legal Reasons";
        statusCodeMessages[494] = "Request Header Too Large";
        statusCodeMessages[495] = "SSL Certificate Error";
        statusCodeMessages[496] = "SSL Certificate Required";
        statusCodeMessages[497] = "HTTP Request Sent to HTTPS Port";
        statusCodeMessages[499] = "Client Closed Request";
        statusCodeMessages[500] = "Internal Server Error";
        statusCodeMessages[501] = "Not Implemented";
        statusCodeMessages[502] = "Bad Gateway";
        statusCodeMessages[503] = "Service Unavailable";
        statusCodeMessages[504] = "Gateway Timeout";
        statusCodeMessages[505] = "HTTP Version Not Supported";
        statusCodeMessages[506] = "Variant Also Negotiates";
        statusCodeMessages[507] = "Insufficient Storage";
        statusCodeMessages[508] = "Loop Detected";
        statusCodeMessages[509] = "Bandwidth Limit Exceeded";
        statusCodeMessages[510] = "Not Extended";
        statusCodeMessages[511] = "Network Authentication Required";
        statusCodeMessages[520] = "Unknown Error";
        statusCodeMessages[521] = "Web Server Is Down";
        statusCodeMessages[522] = "Connection Timed Out";
        statusCodeMessages[523] = "Origin Is Unreachable";
        statusCodeMessages[524] = "A Timeout Occurred";
        statusCodeMessages[525] = "SSL Handshake Failed";
        statusCodeMessages[526] = "Invalid SSL Certificate";
        statusCodeMessages[527] = "Railgun Listener to Origin Error";
        statusCodeMessages[530] = "Origin DNS Error";
        statusCodeMessages[598] = "Network Read Timeout Error";
    }

    std::string StatusCodeHandler::getMessage(int statusCode) {
        if (statusCodeMessages.find(statusCode) != statusCodeMessages.end()) {
            return statusCodeMessages[statusCode];
        }
        if (this->status_error) {
            error_log("Status code isn't found: %d", statusCode);
        }
        return "";
    }

    void StatusCodeHandler::setStatusError(bool statusError) {
        this->status_error = statusError;
    }
} // unit::server::http1
