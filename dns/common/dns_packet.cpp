#include "dns_packet.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace {

std::uint16_t ReadU16(const std::vector<std::uint8_t>& wire, std::size_t& offset) {
    if (offset + 2 > wire.size()) {
        throw std::runtime_error("truncated DNS packet");
    }
    const std::uint16_t value = static_cast<std::uint16_t>(wire[offset] << 8U) | wire[offset + 1];
    offset += 2;
    return value;
}

std::uint32_t ReadU32(const std::vector<std::uint8_t>& wire, std::size_t& offset) {
    if (offset + 4 > wire.size()) {
        throw std::runtime_error("truncated DNS packet");
    }
    const std::uint32_t value = (static_cast<std::uint32_t>(wire[offset]) << 24U) |
                                (static_cast<std::uint32_t>(wire[offset + 1]) << 16U) |
                                (static_cast<std::uint32_t>(wire[offset + 2]) << 8U) | wire[offset + 3];
    offset += 4;
    return value;
}

void AppendU16(std::vector<std::uint8_t>& wire, std::uint16_t value) {
    wire.push_back(static_cast<std::uint8_t>(value >> 8U));
    wire.push_back(static_cast<std::uint8_t>(value));
}

std::pair<std::string, std::size_t> DecodeName(const std::vector<std::uint8_t>& wire,
                                                std::size_t offset) {
    std::string name;
    std::size_t next_offset = offset;
    bool jumped = false;
    unsigned int jumps = 0;
    for (;;) {
        if (offset >= wire.size()) {
            throw std::runtime_error("truncated DNS name");
        }
        const std::uint8_t length = wire[offset];
        if ((length & 0xc0U) == 0xc0U) {
            if (offset + 1 >= wire.size()) {
                throw std::runtime_error("truncated DNS compression pointer");
            }
            const std::size_t target = static_cast<std::size_t>((length & 0x3fU) << 8U) | wire[offset + 1];
            if (target >= wire.size() || ++jumps > wire.size()) {
                throw std::runtime_error("invalid DNS compression pointer");
            }
            if (!jumped) {
                next_offset = offset + 2;
                jumped = true;
            }
            offset = target;
            continue;
        }
        if ((length & 0xc0U) != 0 || length > 63 || offset + 1 + length > wire.size()) {
            throw std::runtime_error("invalid DNS label");
        }
        ++offset;
        if (length == 0) {
            if (!jumped) {
                next_offset = offset;
            }
            return {name, next_offset};
        }
        if (!name.empty()) {
            name.push_back('.');
        }
        name.append(reinterpret_cast<const char*>(wire.data() + offset), length);
        offset += length;
        if (!jumped) {
            next_offset = offset;
        }
    }
}

void SkipQuestion(const std::vector<std::uint8_t>& wire, std::size_t& offset) {
    offset = DecodeName(wire, offset).second;
    (void)ReadU16(wire, offset);
    (void)ReadU16(wire, offset);
}

DnsRecord ReadRecord(const std::vector<std::uint8_t>& wire, std::size_t& offset) {
    DnsRecord record;
    const auto decoded_name = DecodeName(wire, offset);
    record.name = decoded_name.first;
    offset = decoded_name.second;
    record.type = ReadU16(wire, offset);
    record.record_class = ReadU16(wire, offset);
    record.ttl = ReadU32(wire, offset);
    const std::uint16_t data_length = ReadU16(wire, offset);
    record.rdata_offset = offset;
    if (offset + data_length > wire.size()) {
        throw std::runtime_error("truncated DNS resource record");
    }
    record.rdata.assign(wire.begin() + static_cast<std::ptrdiff_t>(offset),
                        wire.begin() + static_cast<std::ptrdiff_t>(offset + data_length));
    offset += data_length;
    return record;
}

std::string Lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

}  // namespace

RecordType ParseRecordType(const std::string& value) {
    const std::string type = Lowercase(value);
    if (type == "a") return RecordType::A;
    if (type == "aaaa") return RecordType::AAAA;
    if (type == "ns") return RecordType::NS;
    if (type == "cname") return RecordType::CNAME;
    if (type == "mx") return RecordType::MX;
    if (type == "txt") return RecordType::TXT;
    if (type == "dnskey" || type == "dnssec") return RecordType::DNSKEY;
    throw std::runtime_error("unsupported record type: " + value);
}

std::string RecordTypeName(std::uint16_t type) {
    switch (static_cast<RecordType>(type)) {
        case RecordType::A: return "A";
        case RecordType::NS: return "NS";
        case RecordType::CNAME: return "CNAME";
        case RecordType::MX: return "MX";
        case RecordType::TXT: return "TXT";
        case RecordType::AAAA: return "AAAA";
        case RecordType::DNSKEY: return "DNSKEY";
    }
    return "TYPE" + std::to_string(type);
}

std::string NormalizeDomainName(const std::string& name) {
    std::string normalized = Lowercase(name);
    while (!normalized.empty() && normalized.back() == '.') {
        normalized.pop_back();
    }
    return normalized;
}

bool SameDomainName(const std::string& left, const std::string& right) {
    return NormalizeDomainName(left) == NormalizeDomainName(right);
}

std::vector<std::uint8_t> BuildDnsQuery(std::uint16_t id, const DnsQuestion& question) {
    const std::string name = NormalizeDomainName(question.name);
    if (name.empty() || name.size() > 253) {
        throw std::runtime_error("invalid domain name");
    }
    std::vector<std::uint8_t> wire;
    wire.reserve(512);
    AppendU16(wire, id);
    AppendU16(wire, 0);  // Iterative request: recursion-desired flag is deliberately unset.
    AppendU16(wire, 1);
    AppendU16(wire, 0);
    AppendU16(wire, 0);
    AppendU16(wire, 0);

    std::size_t start = 0;
    while (start < name.size()) {
        const std::size_t end = name.find('.', start);
        const std::size_t length = (end == std::string::npos ? name.size() : end) - start;
        if (length == 0 || length > 63) {
            throw std::runtime_error("invalid DNS label in domain name");
        }
        wire.push_back(static_cast<std::uint8_t>(length));
        wire.insert(wire.end(), name.begin() + static_cast<std::ptrdiff_t>(start),
                    name.begin() + static_cast<std::ptrdiff_t>(start + length));
        start = end == std::string::npos ? name.size() : end + 1;
    }
    wire.push_back(0);
    AppendU16(wire, static_cast<std::uint16_t>(question.type));
    AppendU16(wire, 1);
    return wire;
}

DnsMessage ParseDnsMessage(const std::vector<std::uint8_t>& wire) {
    if (wire.size() < 12) {
        throw std::runtime_error("DNS response is shorter than its header");
    }
    std::size_t offset = 0;
    DnsMessage message;
    message.wire = wire;
    message.id = ReadU16(wire, offset);
    message.flags = ReadU16(wire, offset);
    const std::uint16_t question_count = ReadU16(wire, offset);
    const std::uint16_t answer_count = ReadU16(wire, offset);
    const std::uint16_t authority_count = ReadU16(wire, offset);
    const std::uint16_t additional_count = ReadU16(wire, offset);
    message.rcode = static_cast<std::uint8_t>(message.flags & 0x0fU);
    if ((message.flags & 0x8000U) == 0) {
        throw std::runtime_error("received a DNS packet that is not a response");
    }
    for (std::uint16_t index = 0; index < question_count; ++index) {
        SkipQuestion(wire, offset);
    }
    for (std::uint16_t index = 0; index < answer_count; ++index) message.answers.push_back(ReadRecord(wire, offset));
    for (std::uint16_t index = 0; index < authority_count; ++index) message.authorities.push_back(ReadRecord(wire, offset));
    for (std::uint16_t index = 0; index < additional_count; ++index) message.additionals.push_back(ReadRecord(wire, offset));
    return message;
}

std::string DecodeRecordName(const DnsMessage& message, const DnsRecord& record) {
    return DecodeName(message.wire, record.rdata_offset).first;
}

std::string FormatRecordData(const DnsMessage& message, const DnsRecord& record) {
    char address[INET6_ADDRSTRLEN]{};
    if (record.type == static_cast<std::uint16_t>(RecordType::A) && record.rdata.size() == 4) {
        if (inet_ntop(AF_INET, record.rdata.data(), address, sizeof(address)) == nullptr) {
            throw std::runtime_error("cannot format A record");
        }
        return address;
    }
    if (record.type == static_cast<std::uint16_t>(RecordType::AAAA) && record.rdata.size() == 16) {
        if (inet_ntop(AF_INET6, record.rdata.data(), address, sizeof(address)) == nullptr) {
            throw std::runtime_error("cannot format AAAA record");
        }
        return address;
    }
    if (record.type == static_cast<std::uint16_t>(RecordType::NS) ||
        record.type == static_cast<std::uint16_t>(RecordType::CNAME)) {
        return DecodeRecordName(message, record);
    }
    std::ostringstream formatted;
    formatted << std::hex << std::setfill('0');
    for (const std::uint8_t byte : record.rdata) formatted << std::setw(2) << static_cast<unsigned int>(byte);
    return formatted.str();
}
