#pragma once

#include "../common/dns_packet.hpp"
#include "../common/udp_transport.hpp"

#include <vector>

class IterativeResolver {
public:
    IterativeResolver(bool debug, DnsServer initial_server);
    std::vector<std::string> Resolve(const DnsQuestion& question);

private:
    void Debug(const std::string& text) const;

    bool debug_;
    DnsServer initial_server_;
    std::uint16_t next_id_ = 0x4242;
};
