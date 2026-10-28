#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct DnsServer {
    std::string host;
    std::uint16_t port = 53;
};

std::vector<std::uint8_t> ExchangeUdp(const DnsServer& server, const std::vector<std::uint8_t>& request);
