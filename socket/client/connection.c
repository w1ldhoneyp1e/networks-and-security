#define _POSIX_C_SOURCE 200112L

#include "connection.h"

#include "../common/log.h"

#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int ClientConnectToServer(const char *host, uint16_t port)
{
    char service[6];
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    int fd = -1;
    int result;

    (void)snprintf(service, sizeof(service), "%u", (unsigned int)port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    result = getaddrinfo(host, service, &hints, &addresses);
    if (result != 0) {
        fprintf(stderr, "[client] cannot resolve %s:%s: %s\n", host, service,
                gai_strerror(result));
        return -1;
    }

    for (address = addresses; address != NULL; address = address->ai_next) {
        printf("[client] creating TCP socket\n");
        fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (fd < 0) {
            continue;
        }
        printf("[client] connecting to %s:%s\n", host, service);
        if (connect(fd, address->ai_addr, address->ai_addrlen) == 0) {
            break;
        }
        LogErrno("client", "connect failed");
        (void)close(fd);
        fd = -1;
    }
    freeaddrinfo(addresses);
    return fd;
}
