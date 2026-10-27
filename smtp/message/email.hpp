#pragma once

#include <string>

struct EmailMessage {
    std::string from;
    std::string to;
    std::string subject;
    std::string body;
};

EmailMessage LoadEmailMessage(const char* from, const char* to, const char* subject,
                              const char* message_file);
std::string EncodeEmailData(const EmailMessage& message);
