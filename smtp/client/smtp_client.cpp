#include "smtp_client.hpp"

#include "../common/smtp_reply.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void SendCommand(int fd, std::string_view command) {
    if (command.empty() || command.find_first_of("\r\n") != std::string_view::npos) {
        throw std::runtime_error("SMTP command must not be empty or contain a newline");
    }
    std::cout << "C: " << command << '\n';
    WriteAll(fd, std::string(command) + "\r\n");
}
}  // namespace

void SendEmail(Socket& socket, const EmailMessage& message) {
    const int fd = socket.Get();
    ExpectSmtpReply(fd, {220});
    SendCommand(fd, "EHLO localhost");
    ExpectSmtpReply(fd, {250});
    SendCommand(fd, "MAIL FROM:<" + message.from + ">");
    ExpectSmtpReply(fd, {250});
    SendCommand(fd, "RCPT TO:<" + message.to + ">");
    ExpectSmtpReply(fd, {250, 251});
    SendCommand(fd, "DATA");
    ExpectSmtpReply(fd, {354});
    std::cout << "C: <message data>\n";
    WriteAll(fd, EncodeEmailData(message));
    ExpectSmtpReply(fd, {250});
    SendCommand(fd, "QUIT");
    ExpectSmtpReply(fd, {221});
}
