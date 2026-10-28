#include "../common/dns_packet.hpp"
#include "../resolver/iterative_resolver.hpp"

#include <exception>
#include <iostream>
#include <string>

namespace {

void PrintUsage(const char* program) {
    std::cerr << "Usage: " << program << " <domain> <type> [-d] [--root IPv4] [--port port]\n"
              << "Example: " << program << " example.com A\n"
              << "         " << program << " example.com AAAA -d\n";
}

std::uint16_t ParsePort(const std::string& value) {
    try {
        std::size_t used = 0;
        const unsigned long port = std::stoul(value, &used);
        if (used != value.size() || port == 0 || port > 65535) throw std::runtime_error("invalid");
        return static_cast<std::uint16_t>(port);
    } catch (const std::exception&) {
        throw std::runtime_error("port must be an integer from 1 to 65535");
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        PrintUsage(argv[0]);
        return 1;
    }
    try {
        bool debug = false;
        DnsServer initial_server;
        for (int index = 3; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "-d") {
                debug = true;
            } else if (argument == "--root" && index + 1 < argc) {
                initial_server.host = argv[++index];
            } else if (argument == "--port" && index + 1 < argc) {
                initial_server.port = ParsePort(argv[++index]);
            } else {
                PrintUsage(argv[0]);
                return 1;
            }
        }
        const DnsQuestion question{argv[1], ParseRecordType(argv[2])};
        IterativeResolver resolver(debug, initial_server);
        const std::vector<std::string> results = resolver.Resolve(question);
        for (const std::string& result : results) std::cout << result << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "dns_resolver: " << error.what() << '\n';
        return 1;
    }
}
