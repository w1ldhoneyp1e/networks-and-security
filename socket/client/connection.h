#ifndef SOCKET_CLIENT_CONNECTION_H
#define SOCKET_CLIENT_CONNECTION_H

#include <stdint.h>

int ClientConnectToServer(const char *host, uint16_t port);

#endif
