#include "request.hpp"

#include <sstream>
#include <stdexcept>

HttpRequest ParseHttpRequest(const std::string& headers) {
    const std::size_t line_end = headers.find("\r\n");
    if (line_end == std::string::npos) {
        throw std::runtime_error("HTTP request has no request line");
    }
    std::istringstream line(headers.substr(0, line_end));
    HttpRequest request;
    std::string extra;
    if (!(line >> request.method >> request.target >> request.version) || (line >> extra) ||
        request.version.rfind("HTTP/", 0) != 0 || request.target.empty() || request.target.front() != '/') {
        throw std::runtime_error("malformed HTTP request line");
    }
    return request;
}
