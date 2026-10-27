#pragma once

#include <initializer_list>

int ReadSmtpReply(int fd);
void ExpectSmtpReply(int fd, std::initializer_list<int> expected);
