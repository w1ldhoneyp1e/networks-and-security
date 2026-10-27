#include "smtp_client.hpp"

#include "../common/socket_connection.hpp"
#include "../message/email.hpp"

#include <exception>
#include <iostream>
#include <string>

namespace {
void PrintUsage(const char* program) {
    std::cerr << "Usage: " << program
              << " <server> <port> <from> <to> <subject> <message-file>\n"
              << "Example: " << program
              << " 127.0.0.1 2525 alice@example.test bob@example.test 'Hello' message.txt\n";
}

bool IsValidPort(const char* value) {
    try {
        const std::string text(value);
        std::size_t used = 0;
        const unsigned long parsed = std::stoul(text, &used);
        return used == text.size() && parsed >= 1 && parsed <= 65535;
    } catch (const std::exception&) {
        return false;
    }
}
}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 7 || !IsValidPort(argc > 2 ? argv[2] : "")) {
        PrintUsage(argv[0]);
        return 1;
    }
    try {
        const EmailMessage message = LoadEmailMessage(argv[3], argv[4], argv[5], argv[6]);
        Socket socket = ConnectToServer(argv[1], argv[2]);
        SendEmail(socket, message);
        std::cout << "Message accepted by the SMTP server.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "smtp_client: " << error.what() << '\n';
        return 1;
    }
}
