#pragma once

#include <string>

struct HttpResponse {
    int status;
    std::string reason;
    std::string content_type;
    std::string body;
};

std::string SerializeHttpResponse(const HttpResponse& response);
