#pragma once

#include <string>
#include <string_view>

class Socket {
public:
    Socket() = default;
    explicit Socket(int fd);
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    ~Socket();

    int Get() const;

private:
    void Close();
    int fd_ = -1;
};

Socket ConnectToServer(const char* host, const char* port);
void WriteAll(int fd, std::string_view data);
std::string ReadLine(int fd);
