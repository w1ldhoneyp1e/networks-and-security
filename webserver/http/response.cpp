#include "response.hpp"

#include <string>

std::string SerializeHttpResponse(const HttpResponse& response) {
    return "HTTP/1.1 " + std::to_string(response.status) + " " + response.reason + "\r\n"
           "Content-Type: " + response.content_type + "\r\n"
           "Content-Length: " + std::to_string(response.body.size()) + "\r\n"
           "Connection: close\r\n"
           "\r\n" + response.body;
}
