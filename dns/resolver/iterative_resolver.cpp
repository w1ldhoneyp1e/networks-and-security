#include "iterative_resolver.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class DnsResponseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

std::vector<DnsServer> RootServers(std::uint16_t port) {
    return {{"198.41.0.4", port}, {"199.9.14.201", port}, {"192.33.4.12", port},
            {"199.7.91.13", port}, {"192.203.230.10", port}, {"192.5.5.241", port},
            {"192.112.36.4", port}, {"198.97.190.53", port}, {"192.36.148.17", port},
            {"192.58.128.30", port}, {"193.0.14.129", port}, {"199.7.83.42", port},
            {"202.12.27.33", port}};
}

std::string RcodeMessage(std::uint8_t rcode) {
    if (rcode == 3) return "host not found (NXDOMAIN)";
    if (rcode == 2) return "DNS server failure (SERVFAIL)";
    if (rcode == 5) return "DNS server refused the query (REFUSED)";
    return "DNS server returned error code " + std::to_string(rcode);
}

bool IsAddressRecord(const DnsRecord& record) {
    return record.type == static_cast<std::uint16_t>(RecordType::A) ||
           record.type == static_cast<std::uint16_t>(RecordType::AAAA);
}

}  // namespace

IterativeResolver::IterativeResolver(bool debug, DnsServer initial_server)
    : debug_(debug), initial_server_(std::move(initial_server)) {}

void IterativeResolver::Debug(const std::string& text) const {
    if (debug_) std::cerr << "[debug] " << text << '\n';
}

std::vector<std::string> IterativeResolver::Resolve(const DnsQuestion& original_question) {
    DnsQuestion question = original_question;
    std::vector<DnsServer> servers = initial_server_.host.empty() ? RootServers(initial_server_.port)
                                                                    : std::vector<DnsServer>{initial_server_};
    constexpr unsigned int kMaxSteps = 32;
    for (unsigned int step = 0; step < kMaxSteps; ++step) {
        bool exchanged = false;
        for (const DnsServer& server : servers) {
            try {
                Debug("query " + question.name + " " + RecordTypeName(static_cast<std::uint16_t>(question.type)) +
                      " at " + server.host + ":" + std::to_string(server.port));
                const std::uint16_t id = next_id_++;
                const DnsMessage message = ParseDnsMessage(ExchangeUdp(server, BuildDnsQuery(id, question)));
                if (message.id != id) throw DnsResponseError("DNS response has an unexpected transaction ID");
                if ((message.flags & 0x0200U) != 0) throw DnsResponseError("truncated UDP DNS response; TCP fallback is not implemented");
                if (message.rcode != 0) throw DnsResponseError(RcodeMessage(message.rcode));
                exchanged = true;

                std::vector<std::string> results;
                for (const DnsRecord& answer : message.answers) {
                    if (answer.record_class == 1 && answer.type == static_cast<std::uint16_t>(question.type) &&
                        SameDomainName(answer.name, question.name)) {
                        results.push_back(FormatRecordData(message, answer));
                    }
                }
                if (!results.empty()) return results;

                for (const DnsRecord& answer : message.answers) {
                    if (answer.record_class == 1 && answer.type == static_cast<std::uint16_t>(RecordType::CNAME) &&
                        SameDomainName(answer.name, question.name)) {
                        question.name = DecodeRecordName(message, answer);
                        Debug("following CNAME to " + question.name);
                        servers = initial_server_.host.empty() ? RootServers(initial_server_.port)
                                                               : std::vector<DnsServer>{initial_server_};
                        goto next_step;
                    }
                }

                std::vector<std::string> name_servers;
                for (const DnsRecord& authority : message.authorities) {
                    if (authority.record_class == 1 && authority.type == static_cast<std::uint16_t>(RecordType::NS)) {
                        name_servers.push_back(DecodeRecordName(message, authority));
                    }
                }
                std::vector<DnsServer> referral_servers;
                for (const std::string& name_server : name_servers) {
                    for (const DnsRecord& additional : message.additionals) {
                        if (additional.record_class == 1 && IsAddressRecord(additional) &&
                            SameDomainName(additional.name, name_server)) {
                            referral_servers.push_back({FormatRecordData(message, additional), server.port});
                        }
                    }
                }
                if (!referral_servers.empty()) {
                    Debug("received referral to " + referral_servers.front().host);
                    servers = std::move(referral_servers);
                    goto next_step;
                }
                throw std::runtime_error("DNS response has neither an answer nor usable referral glue");
            } catch (const DnsResponseError&) {
                throw;
            } catch (const std::runtime_error& error) {
                Debug("server " + server.host + " failed: " + error.what());
                if (servers.size() == 1) throw;
            }
        }
        if (!exchanged) throw std::runtime_error("all DNS servers were unavailable");
        throw std::runtime_error("DNS resolution stopped without an answer");
next_step:
        continue;
    }
    throw std::runtime_error("DNS resolution exceeded the maximum number of delegation steps");
}
