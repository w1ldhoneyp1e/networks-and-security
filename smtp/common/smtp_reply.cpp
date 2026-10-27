#include "smtp_reply.hpp"

#include "socket_connection.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
int ParseReplyCode(const std::string& line) {
    if (line.size() < 4 || (line[3] != ' ' && line[3] != '-') || line[0] < '0' || line[0] > '9' ||
        line[1] < '0' || line[1] > '9' || line[2] < '0' || line[2] > '9') {
        throw std::runtime_error("malformed SMTP reply: " + line);
    }
    return (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');
}
}  // namespace

int ReadSmtpReply(int fd) {
    const std::string first_line = ReadLine(fd);
    const int code = ParseReplyCode(first_line);
    std::cout << "S: " << first_line << '\n';
    if (first_line[3] == ' ') {
        return code;
    }
    for (;;) {
        const std::string line = ReadLine(fd);
        std::cout << "S: " << line << '\n';
        if (ParseReplyCode(line) != code) {
            throw std::runtime_error("inconsistent SMTP reply code in a multiline reply");
        }
        if (line[3] == ' ') {
            return code;
        }
    }
}

void ExpectSmtpReply(int fd, std::initializer_list<int> expected) {
    const int actual = ReadSmtpReply(fd);
    for (const int expected_code : expected) {
        if (actual == expected_code) {
            return;
        }
    }
    std::ostringstream message;
    message << "unexpected SMTP reply " << actual << ", expected ";
    for (auto iterator = expected.begin(); iterator != expected.end(); ++iterator) {
        if (iterator != expected.begin()) {
            message << " or ";
        }
        message << *iterator;
    }
    throw std::runtime_error(message.str());
}
