#define _POSIX_C_SOURCE 200809L

#include "worker.h"

#include "../common/log.h"
#include "../common/protocol.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

enum {
    MIN_NUMBER = 1,
    MAX_NUMBER = 100
};

static uint64_t ElapsedNanoseconds(const struct timespec *start, const struct timespec *end)
{
    int64_t seconds = (int64_t)end->tv_sec - (int64_t)start->tv_sec;
    int64_t nanoseconds = (int64_t)end->tv_nsec - (int64_t)start->tv_nsec;

    return (uint64_t)(seconds * 1000000000LL + nanoseconds);
}

static int SetJobTimeouts(int fd, const struct timespec *acceptedAt, int timeoutSeconds)
{
    struct timespec now;
    uint64_t elapsedUs;
    uint64_t remainingUs;
    struct timeval receiveTimeout;
    struct timeval sendTimeout = {timeoutSeconds, 0};

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return -1;
    }
    elapsedUs = ElapsedNanoseconds(acceptedAt, &now) / 1000U;
    if (elapsedUs >= (uint64_t)timeoutSeconds * 1000000U) {
        errno = ETIMEDOUT;
        return -1;
    }
    remainingUs = (uint64_t)timeoutSeconds * 1000000U - elapsedUs;
    receiveTimeout.tv_sec = (time_t)(remainingUs / 1000000U);
    receiveTimeout.tv_usec = (suseconds_t)(remainingUs % 1000000U);
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &receiveTimeout, sizeof(receiveTimeout)) != 0 ||
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &sendTimeout, sizeof(sendTimeout)) != 0) {
        return -1;
    }
    return 0;
}

static bool HandleClient(const struct ClientJob *job, const struct WorkerContext *context,
                         bool *failed)
{
    char clientName[PROTOCOL_MAX_NAME_LENGTH + 1];
    int32_t clientNumber;

    *failed = false;
    if (SetJobTimeouts(job->fd, &job->acceptedAt, context->clientTimeoutSeconds) != 0) {
        LogErrno("server", "client timed out before processing");
        *failed = true;
        return false;
    }
    printf("[server] receiving message from client socket %d\n", job->fd);
    if (ProtocolRecvMessage(job->fd, clientName, sizeof(clientName), &clientNumber) != 0) {
        LogErrno("server", "receive failed (client may have timed out or disconnected)");
        *failed = true;
        return false;
    }
    printf("[server] client name: %s\n"
           "[server] server name: %s\n"
           "[server] client number: %d\n"
           "[server] server number: %d\n"
           "[server] sum: %d\n",
           clientName, context->serverName, clientNumber, context->serverNumber,
           clientNumber + context->serverNumber);

    if (clientNumber < MIN_NUMBER || clientNumber > MAX_NUMBER) {
        printf("[server] received out-of-range number; graceful shutdown requested\n");
        ServerControlRequestStop(context->control);
        return true;
    }
    printf("[server] sending response to client socket %d\n", job->fd);
    if (ProtocolSendMessage(job->fd, context->serverName, context->serverNumber) != 0) {
        LogErrno("server", "send failed");
        *failed = true;
    }
    return true;
}

void *WorkerMain(void *argument)
{
    struct WorkerContext *context = argument;
    struct ClientJob job;

    while (QueuePop(context->queue, &job)) {
        struct timespec startedAt;
        struct timespec finishedAt;
        bool failed;
        bool gotRequest;

        (void)clock_gettime(CLOCK_MONOTONIC, &startedAt);
        gotRequest = HandleClient(&job, context, &failed);
        (void)clock_gettime(CLOCK_MONOTONIC, &finishedAt);
        printf("[server] closing client socket %d\n", job.fd);
        (void)close(job.fd);
        StatsConnectionFinished(context->stats, gotRequest, failed,
                                ElapsedNanoseconds(&startedAt, &finishedAt));
    }
    return NULL;
}
