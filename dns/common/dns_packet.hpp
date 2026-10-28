#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class RecordType : std::uint16_t {
    A = 1,
    NS = 2,
    CNAME = 5,
    MX = 15,
    TXT = 16,
    AAAA = 28,
    DNSKEY = 48,
};

struct DnsQuestion {
    std::string name;
    RecordType type;
};

struct DnsRecord {
    std::string name;
    std::uint16_t type;
    std::uint16_t record_class;
    std::uint32_t ttl;
    std::size_t rdata_offset;
    std::vector<std::uint8_t> rdata;
};

struct DnsMessage {
    std::uint16_t id;
    std::uint16_t flags;
    std::uint8_t rcode;
    std::vector<DnsRecord> answers;
    std::vector<DnsRecord> authorities;
    std::vector<DnsRecord> additionals;
    std::vector<std::uint8_t> wire;
};

RecordType ParseRecordType(const std::string& value);
std::string RecordTypeName(std::uint16_t type);
std::string NormalizeDomainName(const std::string& name);
bool SameDomainName(const std::string& left, const std::string& right);

std::vector<std::uint8_t> BuildDnsQuery(std::uint16_t id, const DnsQuestion& question);
DnsMessage ParseDnsMessage(const std::vector<std::uint8_t>& wire);
std::string DecodeRecordName(const DnsMessage& message, const DnsRecord& record);
std::string FormatRecordData(const DnsMessage& message, const DnsRecord& record);
