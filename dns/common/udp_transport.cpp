#include "udp_transport.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace {

class FileDescriptor {
public:
    explicit FileDescriptor(int fd) : fd_(fd) {}
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    ~FileDescriptor() {
        if (fd_ >= 0) (void)close(fd_);
    }
    int Get() const { return fd_; }

private:
    int fd_;
};

[[noreturn]] void FailErrno(const std::string& action) {
    throw std::runtime_error(action + ": " + std::strerror(errno));
}

}  // namespace

std::vector<std::uint8_t> ExchangeUdp(const DnsServer& server, const std::vector<std::uint8_t>& request) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* addresses = nullptr;
    const std::string service = std::to_string(server.port);
    const int resolve_result = getaddrinfo(server.host.c_str(), service.c_str(), &hints, &addresses);
    if (resolve_result != 0) {
        throw std::runtime_error("cannot resolve DNS server " + server.host + ": " + gai_strerror(resolve_result));
    }

    int last_error = 0;
    std::vector<std::uint8_t> response;
    for (addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
        FileDescriptor socket_fd(socket(address->ai_family, address->ai_socktype, address->ai_protocol));
        if (socket_fd.Get() < 0) {
            last_error = errno;
            continue;
        }
        timeval timeout{2, 0};
        if (setsockopt(socket_fd.Get(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
            last_error = errno;
            continue;
        }
        if (connect(socket_fd.Get(), address->ai_addr, address->ai_addrlen) != 0) {
            last_error = errno;
            continue;
        }
        std::size_t sent = 0;
        while (sent < request.size()) {
            const ssize_t result = write(socket_fd.Get(), request.data() + sent, request.size() - sent);
            if (result > 0) {
                sent += static_cast<std::size_t>(result);
            } else if (result < 0 && errno == EINTR) {
                continue;
            } else {
                last_error = errno;
                break;
            }
        }
        if (sent != request.size()) continue;

        std::vector<std::uint8_t> buffer(4096);
        ssize_t received;
        do {
            received = read(socket_fd.Get(), buffer.data(), buffer.size());
        } while (received < 0 && errno == EINTR);
        if (received >= 0) {
            buffer.resize(static_cast<std::size_t>(received));
            response = std::move(buffer);
            break;
        }
        last_error = errno;
    }
    freeaddrinfo(addresses);
    if (response.empty()) {
        errno = last_error;
        FailErrno("DNS UDP exchange with " + server.host + ":" + service + " failed");
    }
    return response;
}
