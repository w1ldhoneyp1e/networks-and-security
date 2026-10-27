#include "socket_connection.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
constexpr std::size_t kMaxReplyLineLength = 1000;

[[noreturn]] void FailErrno(const std::string& action) {
    throw std::runtime_error(action + ": " + std::strerror(errno));
}
}  // namespace

Socket::Socket(int fd) : fd_(fd) {}
Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        Close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}
Socket::~Socket() { Close(); }
int Socket::Get() const { return fd_; }
void Socket::Close() {
    if (fd_ >= 0) {
        (void)close(fd_);
        fd_ = -1;
    }
}

Socket ConnectToServer(const char* host, const char* port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* results = nullptr;
    const int resolve_result = getaddrinfo(host, port, &hints, &results);
    if (resolve_result != 0) {
        throw std::runtime_error("cannot resolve " + std::string(host) + ":" + port + ": " +
                                 gai_strerror(resolve_result));
    }

    Socket connected;
    int last_error = 0;
    for (addrinfo* address = results; address != nullptr; address = address->ai_next) {
        const int fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (fd < 0) {
            last_error = errno;
            continue;
        }
        if (connect(fd, address->ai_addr, address->ai_addrlen) == 0) {
            connected = Socket(fd);
            break;
        }
        last_error = errno;
        (void)close(fd);
    }
    freeaddrinfo(results);
    if (connected.Get() < 0) {
        errno = last_error;
        FailErrno("cannot connect to " + std::string(host) + ":" + port);
    }
    return connected;
}

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

std::string ReadLine(int fd) {
    std::string line;
    line.reserve(128);
    for (;;) {
        char character = '\0';
        const ssize_t result = read(fd, &character, 1);
        if (result == 1) {
            line.push_back(character);
            if (line.size() > kMaxReplyLineLength) {
                throw std::runtime_error("SMTP reply line is too long");
            }
            if (line.size() >= 2 && line.compare(line.size() - 2, 2, "\r\n") == 0) {
                line.resize(line.size() - 2);
                return line;
            }
        } else if (result == 0) {
            throw std::runtime_error("server closed the connection while sending a reply");
        } else if (errno != EINTR) {
            FailErrno("read failed");
        }
    }
}
