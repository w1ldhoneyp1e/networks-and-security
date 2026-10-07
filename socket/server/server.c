#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "control.h"
#include "queue.h"
#include "stats.h"
#include "worker.h"

#include "../common/log.h"
#include "../common/parse.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

enum {
    DEFAULT_PORT = 5050,
    DEFAULT_THREAD_COUNT = 8,
    MAX_THREAD_COUNT = 64,
    CLIENT_TIMEOUT_SECONDS = 10,
    MAX_EVENTS = 16
};

static const char SERVER_NAME[] = "Server of Kirill Yashmetov";
static struct ServerControl *signalControl = NULL;

static void HandleSigint(int signalNumber)
{
    (void)signalNumber;
    if (signalControl != NULL) {
        ServerControlRequestStop(signalControl);
    }
}

static void PrintUsage(const char *program)
{
    fprintf(stderr, "Usage: %s [port (>1023)] [thread_count (1-64)]\n", program);
}

static int SetNonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        return -1;
    }
    return 0;
}

static int CreateListenSocket(uint16_t port)
{
    int fd;
    int enabled = 1;
    struct sockaddr_in address;

    printf("[server] creating TCP welcome socket\n");
    fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return -1;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) != 0) {
        (void)close(fd);
        return -1;
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    printf("[server] binding to 0.0.0.0:%u\n", (unsigned int)port);
    if (bind(fd, (const struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(fd, SOMAXCONN) != 0 || SetNonblocking(fd) != 0) {
        (void)close(fd);
        return -1;
    }
    printf("[server] listening for connections\n");
    return fd;
}

static void AcceptConnections(int listenFd, struct JobQueue *queue, struct Statistics *stats,
                              const struct ServerControl *control)
{
    for (;;) {
        struct ClientJob job;
        int pushed;

        if (control->stopRequested) {
            return;
        }
        job.fd = accept4(listenFd, NULL, NULL, SOCK_CLOEXEC);
        if (job.fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            if (errno == EINTR) {
                continue;
            }
            LogErrno("server", "accept failed");
            return;
        }
        if (clock_gettime(CLOCK_MONOTONIC, &job.acceptedAt) != 0) {
            LogErrno("server", "clock_gettime failed");
            (void)close(job.fd);
            continue;
        }
        printf("[server] accepted client socket %d\n", job.fd);
        pushed = QueuePush(queue, job);
        if (pushed == 0) {
            StatsConnectionOpened(stats);
        } else {
            fprintf(stderr, "[server] client queue %s; closing socket %d\n",
                    pushed > 0 ? "is full" : "is closed", job.fd);
            (void)close(job.fd);
        }
    }
}

int main(int argc, char *argv[])
{
    int port = DEFAULT_PORT;
    int threadCount = DEFAULT_THREAD_COUNT;
    int listenFd = -1;
    int epollFd = -1;
    int wakeupFd = -1;
    struct epoll_event event;
    struct epoll_event events[MAX_EVENTS];
    struct sigaction action;
    struct JobQueue queue;
    struct Statistics stats;
    struct ServerControl control;
    struct WorkerContext context;
    pthread_t workers[MAX_THREAD_COUNT];
    int workersCreated = 0;
    int exitCode = EXIT_FAILURE;

    if (argc > 1 && ParseIntRange(argv[1], 1024, 65535, &port) != 0) {
        PrintUsage(argv[0]);
        return EXIT_FAILURE;
    }
    if (argc > 2 && ParseIntRange(argv[2], 1, MAX_THREAD_COUNT, &threadCount) != 0) {
        PrintUsage(argv[0]);
        return EXIT_FAILURE;
    }
    if (argc > 3) {
        PrintUsage(argv[0]);
        return EXIT_FAILURE;
    }

    (void)setvbuf(stdout, NULL, _IOLBF, 0);
    (void)signal(SIGPIPE, SIG_IGN);
    if (QueueInit(&queue) != 0) {
        fprintf(stderr, "[server] unable to initialize synchronization primitives\n");
        return EXIT_FAILURE;
    }
    if (StatsInit(&stats) != 0) {
        fprintf(stderr, "[server] unable to initialize synchronization primitives\n");
        QueueDestroy(&queue);
        return EXIT_FAILURE;
    }
    ServerControlInit(&control, -1);
    signalControl = &control;
    memset(&action, 0, sizeof(action));
    action.sa_handler = HandleSigint;
    (void)sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) != 0) {
        LogErrno("server", "sigaction failed");
        StatsDestroy(&stats);
        QueueDestroy(&queue);
        return EXIT_FAILURE;
    }

    wakeupFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wakeupFd < 0) {
        LogErrno("server", "eventfd failed");
        goto cleanup;
    }
    control.wakeupFd = wakeupFd;
    context.queue = &queue;
    context.stats = &stats;
    context.control = &control;
    context.serverName = SERVER_NAME;
    context.serverNumber = 50;
    context.clientTimeoutSeconds = CLIENT_TIMEOUT_SECONDS;
    for (int index = 0; index < threadCount; ++index) {
        int result = pthread_create(&workers[index], NULL, WorkerMain, &context);

        if (result != 0) {
            errno = result;
            LogErrno("server", "pthread_create failed");
            ServerControlRequestStop(&control);
            goto cleanup;
        }
        ++workersCreated;
    }

    listenFd = CreateListenSocket((uint16_t)port);
    if (listenFd < 0) {
        LogErrno("server", "unable to initialize welcome socket");
        goto cleanup;
    }
    epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (epollFd < 0) {
        LogErrno("server", "epoll_create1 failed");
        goto cleanup;
    }
    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN;
    event.data.fd = listenFd;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, listenFd, &event) != 0) {
        LogErrno("server", "cannot add welcome socket to epoll");
        goto cleanup;
    }
    event.data.fd = wakeupFd;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, wakeupFd, &event) != 0) {
        LogErrno("server", "cannot add wakeup fd to epoll");
        goto cleanup;
    }

    printf("[server] ready: port=%d workers=%d (Ctrl+C for graceful shutdown)\n", port,
           threadCount);
    while (!control.stopRequested) {
        int ready = epoll_wait(epollFd, events, MAX_EVENTS, -1);

        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            LogErrno("server", "epoll_wait failed");
            ServerControlRequestStop(&control);
            break;
        }
        for (int index = 0; index < ready; ++index) {
            if (events[index].data.fd == wakeupFd) {
                uint64_t ignored;

                while (read(wakeupFd, &ignored, sizeof(ignored)) > 0) {
                }
            } else if (events[index].data.fd == listenFd && !control.stopRequested) {
                AcceptConnections(listenFd, &queue, &stats, &control);
            }
        }
    }
    exitCode = EXIT_SUCCESS;

cleanup:
    printf("[server] stopping: no new client connections will be accepted\n");
    if (listenFd >= 0) {
        (void)close(listenFd);
    }
    QueueClose(&queue);
    for (int index = 0; index < workersCreated; ++index) {
        (void)pthread_join(workers[index], NULL);
    }
    if (epollFd >= 0) {
        (void)close(epollFd);
    }
    if (wakeupFd >= 0) {
        control.wakeupFd = -1;
        (void)close(wakeupFd);
    }
    StatsDestroy(&stats);
    QueueDestroy(&queue);
    signalControl = NULL;
    printf("[server] resources released; shutdown complete\n");
    return exitCode;
}
