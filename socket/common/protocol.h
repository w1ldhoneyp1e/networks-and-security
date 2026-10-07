#ifndef SOCKET_COMMON_PROTOCOL_H
#define SOCKET_COMMON_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

enum { PROTOCOL_MAX_NAME_LENGTH = 255 };

int ProtocolSendMessage(int fd, const char *name, int32_t number);
int ProtocolRecvMessage(int fd, char *name, size_t nameSize, int32_t *number);

#endif
