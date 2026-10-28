#pragma once

#include <string>

struct HttpRequest {
    std::string method;
    std::string target;
    std::string version;
};

HttpRequest ParseHttpRequest(const std::string& headers);
