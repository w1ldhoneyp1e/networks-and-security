#include "connection.h"
#include "input.h"

#include "../common/log.h"
#include "../common/parse.h"
#include "../common/protocol.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
    DEFAULT_PORT = 5050,
    MIN_NUMBER = 1,
    MAX_NUMBER = 100
};

static const char CLIENT_NAME[] = "Client of Kirill";

static void PrintUsage(const char *program)
{
    fprintf(stderr, "Usage: %s [host] [port]\n       %s --shutdown [host] [port]\n", program,
            program);
}

int main(int argc, char *argv[])
{
    const char *host = "127.0.0.1";
    uint16_t port = DEFAULT_PORT;
    bool shutdownMode = false;
    int clientNumber;
    int32_t serverNumber;
    char serverName[PROTOCOL_MAX_NAME_LENGTH + 1];
    int fd;
    int parsedPort;

    if (argc > 1 && strcmp(argv[1], "--shutdown") == 0) {
        shutdownMode = true;
        if (argc > 2) {
            host = argv[2];
        }
        if (argc > 3 && ParseIntRange(argv[3], 1024, 65535, &parsedPort) != 0) {
            PrintUsage(argv[0]);
            return EXIT_FAILURE;
        }
        if (argc > 3) {
            port = (uint16_t)parsedPort;
        }
        if (argc > 4) {
            PrintUsage(argv[0]);
            return EXIT_FAILURE;
        }
    } else {
        if (argc > 1) {
            host = argv[1];
        }
        if (argc > 2 && ParseIntRange(argv[2], 1024, 65535, &parsedPort) != 0) {
            PrintUsage(argv[0]);
            return EXIT_FAILURE;
        }
        if (argc > 2) {
            port = (uint16_t)parsedPort;
        }
        if (argc > 3) {
            PrintUsage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    clientNumber = shutdownMode ? 0 : ClientReadNumber(MIN_NUMBER, MAX_NUMBER);
    if (clientNumber < 0) {
        return EXIT_FAILURE;
    }
    fd = ClientConnectToServer(host, port);
    if (fd < 0) {
        LogErrno("client", "unable to connect to server");
        return EXIT_FAILURE;
    }
    printf("[client] sending name and number %d\n", clientNumber);
    if (ProtocolSendMessage(fd, CLIENT_NAME, (int32_t)clientNumber) != 0) {
        LogErrno("client", "send failed");
        (void)close(fd);
        return EXIT_FAILURE;
    }
    if (shutdownMode) {
        printf("[client] shutdown request sent; server will close the connection\n");
        (void)close(fd);
        return EXIT_SUCCESS;
    }

    printf("[client] waiting for server response\n");
    if (ProtocolRecvMessage(fd, serverName, sizeof(serverName), &serverNumber) != 0) {
        LogErrno("client", "receive failed");
        (void)close(fd);
        return EXIT_FAILURE;
    }
    printf("[client] received server response\n"
           "Client name: %s\n"
           "Server name: %s\n"
           "Client number: %d\n"
           "Server number: %d\n"
           "Sum: %d\n",
           CLIENT_NAME, 
           serverName,
           clientNumber,
           serverNumber,
           clientNumber + serverNumber);
    printf("[client] closing socket\n");
    if (close(fd) != 0) {
        LogErrno("client", "close failed");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
