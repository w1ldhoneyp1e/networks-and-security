#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>

static int SendAll(int fd, const void *buffer, size_t length)
{
    const unsigned char *data = buffer;
    size_t sent = 0;

    while (sent < length) {
        ssize_t result = send(fd, data + sent, length - sent, MSG_NOSIGNAL);
        if (result > 0) {
            sent += (size_t)result;
        } else if (result < 0 && errno == EINTR) {
            continue;
        } else {
            return -1;
        }
    }
    return 0;
}

static int RecvAll(int fd, void *buffer, size_t length)
{
    unsigned char *data = buffer;
    size_t received = 0;

    while (received < length) {
        ssize_t result = recv(fd, data + received, length - received, 0);
        if (result > 0) {
            received += (size_t)result;
        } else if (result == 0) {
            errno = ECONNRESET;
            return -1;
        } else if (errno == EINTR) {
            continue;
        } else {
            return -1;
        }
    }
    return 0;
}

int ProtocolSendMessage(int fd, const char *name, int32_t number)
{
    size_t nameLength = strlen(name);
    uint32_t networkLength;
    uint32_t networkNumber;

    if (nameLength == 0 || nameLength > PROTOCOL_MAX_NAME_LENGTH) {
        errno = EINVAL;
        return -1;
    }
    networkLength = htonl((uint32_t)nameLength);
    networkNumber = htonl((uint32_t)number);
    return SendAll(fd, &networkLength, sizeof(networkLength)) == 0 &&
                   SendAll(fd, name, nameLength) == 0 &&
                   SendAll(fd, &networkNumber, sizeof(networkNumber)) == 0
               ? 0
               : -1;
}

int ProtocolRecvMessage(int fd, char *name, size_t nameSize, int32_t *number)
{
    uint32_t networkLength;
    uint32_t networkNumber;
    uint32_t nameLength;

    if (RecvAll(fd, &networkLength, sizeof(networkLength)) != 0) {
        return -1;
    }
    nameLength = ntohl(networkLength);
    if (nameLength == 0 || nameLength > PROTOCOL_MAX_NAME_LENGTH || nameLength >= nameSize) {
        errno = EPROTO;
        return -1;
    }
    if (RecvAll(fd, name, nameLength) != 0 ||
        RecvAll(fd, &networkNumber, sizeof(networkNumber)) != 0) {
        return -1;
    }
    name[nameLength] = '\0';
    *number = (int32_t)ntohl(networkNumber);
    return 0;
}
