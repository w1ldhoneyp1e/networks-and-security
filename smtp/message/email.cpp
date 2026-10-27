#include "email.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace {
void RequireHeaderValue(std::string_view value, std::string_view name) {
    if (value.empty() || value.find_first_of("\r\n") != std::string_view::npos) {
        throw std::runtime_error(std::string(name) + " must not be empty or contain a newline");
    }
}

std::string ReadFile(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("cannot open message file: " + std::string(path));
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    if (!file.good() && !file.eof()) {
        throw std::runtime_error("cannot read message file: " + std::string(path));
    }
    return contents.str();
}
}  // namespace

EmailMessage LoadEmailMessage(const char* from, const char* to, const char* subject,
                              const char* message_file) {
    RequireHeaderValue(from, "sender address");
    RequireHeaderValue(to, "recipient address");
    RequireHeaderValue(subject, "subject");
    return {from, to, subject, ReadFile(message_file)};
}

std::string EncodeEmailData(const EmailMessage& message) {
    const std::string source = "From: " + message.from + "\nTo: " + message.to +
                               "\nSubject: " + message.subject + "\n\n" + message.body;
    std::string encoded;
    std::string line;
    for (std::size_t index = 0; index <= source.size(); ++index) {
        if (index == source.size() || source[index] == '\n' || source[index] == '\r') {
            if (!line.empty() && line.front() == '.') {
                encoded.push_back('.');
            }
            encoded += line;
            encoded += "\r\n";
            line.clear();
            if (index < source.size() && source[index] == '\r' && index + 1 < source.size() &&
                source[index + 1] == '\n') {
                ++index;
            }
        } else {
            line.push_back(source[index]);
        }
    }
    return encoded + ".\r\n";
}
