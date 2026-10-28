#pragma once

#include <string>
#include <string_view>

void WriteAll(int fd, std::string_view data);
std::string ReadHttpHeaders(int fd);
