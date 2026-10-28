#include "../server/web_server.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

void PrintUsage(const char* program) {
    std::cerr << "Usage: " << program << " [port] [document-root]\n"
              << "Example: " << program << " 8080 ./www\n";
}

std::uint16_t ParsePort(const std::string& value) {
    try {
        std::size_t used = 0;
        const unsigned long port = std::stoul(value, &used);
        if (used != value.size() || port > 65535) throw std::runtime_error("invalid");
        return static_cast<std::uint16_t>(port);
    } catch (const std::exception&) {
        throw std::runtime_error("port must be an integer from 0 to 65535");
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc > 3) {
        PrintUsage(argv[0]);
        return 1;
    }
    try {
        const std::uint16_t port = argc > 1 ? ParsePort(argv[1]) : 8080;
        const std::string document_root = argc > 2 ? argv[2] : "www";
        if (!std::filesystem::is_directory(document_root)) {
            throw std::runtime_error("document root is not a directory: " + document_root);
        }
        RunWebServer(port, document_root);
    } catch (const std::exception& error) {
        std::cerr << "webserver: " << error.what() << '\n';
        return 1;
    }
}
