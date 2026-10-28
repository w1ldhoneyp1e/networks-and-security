#include "io.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>

#include <unistd.h>

namespace {

constexpr std::size_t kMaxHeaderSize = 8192;

[[noreturn]] void FailErrno(const char* action) {
    throw std::runtime_error(std::string(action) + ": " + std::strerror(errno));
}

}  // namespace

void WriteAll(int fd, std::string_view data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t result = write(fd, data.data() + sent, data.size() - sent);
        if (result > 0) {
            sent += static_cast<std::size_t>(result);
        } else if (result < 0 && errno == EINTR) {
            continue;
        } else {
            FailErrno("write failed");
        }
    }
}

std::string ReadHttpHeaders(int fd) {
    std::string request;
    request.reserve(1024);
    while (request.find("\r\n\r\n") == std::string::npos) {
        char buffer[1024];
        const ssize_t result = read(fd, buffer, sizeof(buffer));
        if (result > 0) {
            request.append(buffer, static_cast<std::size_t>(result));
            if (request.size() > kMaxHeaderSize) {
                throw std::runtime_error("HTTP request headers are too large");
            }
        } else if (result == 0) {
            throw std::runtime_error("client closed connection before completing HTTP headers");
        } else if (errno != EINTR) {
            FailErrno("read failed");
        }
    }
    return request;
}
