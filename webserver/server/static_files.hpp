#pragma once

#include <optional>
#include <string>

struct StaticFile {
    std::string content_type;
    std::string body;
};

std::optional<StaticFile> ReadStaticFile(const std::string& document_root, const std::string& target);
