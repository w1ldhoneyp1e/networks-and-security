#ifndef SOCKET_SERVER_QUEUE_H
#define SOCKET_SERVER_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>
#include <time.h>

enum { SERVER_QUEUE_CAPACITY = 256 };

struct ClientJob {
    int fd;
    struct timespec acceptedAt;
};

struct JobQueue {
    struct ClientJob jobs[SERVER_QUEUE_CAPACITY];
    size_t head;
    size_t tail;
    size_t count;
    bool closed;
    pthread_mutex_t mutex;
    pthread_cond_t notEmpty;
};

int QueueInit(struct JobQueue *queue);
void QueueClose(struct JobQueue *queue);
int QueuePush(struct JobQueue *queue, struct ClientJob job);
bool QueuePop(struct JobQueue *queue, struct ClientJob *job);
void QueueDestroy(struct JobQueue *queue);

#endif
