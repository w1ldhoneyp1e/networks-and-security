#include "web_server.hpp"

#include "../common/io.hpp"
#include "../http/request.hpp"
#include "../http/response.hpp"
#include "static_files.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include <sys/socket.h>
#include <unistd.h>

namespace {

class SocketGuard {
public:
    explicit SocketGuard(int fd) : fd_(fd) {}
    SocketGuard(const SocketGuard&) = delete;
    SocketGuard& operator=(const SocketGuard&) = delete;
    SocketGuard(SocketGuard&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
    SocketGuard& operator=(SocketGuard&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) (void)close(fd_);
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }
    ~SocketGuard() {
        if (fd_ >= 0) (void)close(fd_);
    }
    int Get() const { return fd_; }

private:
    int fd_;
};

[[noreturn]] void FailErrno(const char* action) {
    throw std::runtime_error(std::string(action) + ": " + std::strerror(errno));
}

HttpResponse ErrorResponse(int status, const std::string& reason, const std::string& body) {
    return {status, reason, "text/plain; charset=utf-8", body + "\n"};
}

HttpResponse HandleRequest(const HttpRequest& request, const std::string& document_root) {
    if (request.method != "GET") return ErrorResponse(405, "Method Not Allowed", "Only GET is supported");
    const std::optional<StaticFile> file = ReadStaticFile(document_root, request.target);
    if (!file) return ErrorResponse(404, "Not Found", "File Not Found");
    return {200, "OK", file->content_type, file->body};
}

void HandleClient(int fd, const std::string& document_root) {
    SocketGuard client(fd);
    try {
        const HttpRequest request = ParseHttpRequest(ReadHttpHeaders(client.Get()));
        const HttpResponse response = HandleRequest(request, document_root);
        std::cout << request.method << " " << request.target << " -> " << response.status << '\n';
        WriteAll(client.Get(), SerializeHttpResponse(response));
    } catch (const std::exception& error) {
        std::cout << "malformed request -> 400 (" << error.what() << ")\n";
        try {
            WriteAll(client.Get(), SerializeHttpResponse(ErrorResponse(400, "Bad Request", "Bad Request")));
        } catch (const std::exception&) {
            // The peer may have closed the socket; SocketGuard will still release it.
        }
    }
}

SocketGuard CreateListener(std::uint16_t requested_port, std::uint16_t* actual_port) {
    SocketGuard listener(socket(AF_INET, SOCK_STREAM, 0));
    if (listener.Get() < 0) FailErrno("socket failed");
    const int enabled = 1;
    if (setsockopt(listener.Get(), SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) != 0) {
        FailErrno("setsockopt failed");
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(requested_port);
    if (bind(listener.Get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) FailErrno("bind failed");
    if (listen(listener.Get(), SOMAXCONN) != 0) FailErrno("listen failed");
    socklen_t address_size = sizeof(address);
    if (getsockname(listener.Get(), reinterpret_cast<sockaddr*>(&address), &address_size) != 0) {
        FailErrno("getsockname failed");
    }
    *actual_port = ntohs(address.sin_port);
    return listener;
}

}  // namespace

void RunWebServer(std::uint16_t port, const std::string& document_root) {
    std::signal(SIGPIPE, SIG_IGN);
    std::uint16_t actual_port = 0;
    SocketGuard listener = CreateListener(port, &actual_port);
    std::cout << "Listening on port " << actual_port << ", document root: " << document_root << std::endl;
    for (;;) {
        const int client = accept(listener.Get(), nullptr, nullptr);
        if (client >= 0) {
            HandleClient(client, document_root);
        } else if (errno != EINTR) {
            std::cerr << "accept failed: " << std::strerror(errno) << '\n';
        }
    }
}
