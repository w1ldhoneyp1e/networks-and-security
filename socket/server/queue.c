#include "queue.h"

#include <string.h>

int QueueInit(struct JobQueue *queue)
{
    memset(queue, 0, sizeof(*queue));
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&queue->notEmpty, NULL) != 0) {
        (void)pthread_mutex_destroy(&queue->mutex);
        return -1;
    }
    return 0;
}

void QueueClose(struct JobQueue *queue)
{
    (void)pthread_mutex_lock(&queue->mutex);
    queue->closed = true;
    (void)pthread_cond_broadcast(&queue->notEmpty);
    (void)pthread_mutex_unlock(&queue->mutex);
}

int QueuePush(struct JobQueue *queue, struct ClientJob job)
{
    int result = 0;

    (void)pthread_mutex_lock(&queue->mutex);
    if (queue->closed) {
        result = -1;
    } else if (queue->count == SERVER_QUEUE_CAPACITY) {
        result = 1;
    } else {
        queue->jobs[queue->tail] = job;
        queue->tail = (queue->tail + 1U) % SERVER_QUEUE_CAPACITY;
        ++queue->count;
        (void)pthread_cond_signal(&queue->notEmpty);
    }
    (void)pthread_mutex_unlock(&queue->mutex);
    return result;
}

bool QueuePop(struct JobQueue *queue, struct ClientJob *job)
{
    (void)pthread_mutex_lock(&queue->mutex);
    while (queue->count == 0 && !queue->closed) {
        (void)pthread_cond_wait(&queue->notEmpty, &queue->mutex);
    }
    if (queue->count == 0) {
        (void)pthread_mutex_unlock(&queue->mutex);
        return false;
    }
    *job = queue->jobs[queue->head];
    queue->head = (queue->head + 1U) % SERVER_QUEUE_CAPACITY;
    --queue->count;
    (void)pthread_mutex_unlock(&queue->mutex);
    return true;
}

void QueueDestroy(struct JobQueue *queue)
{
    (void)pthread_cond_destroy(&queue->notEmpty);
    (void)pthread_mutex_destroy(&queue->mutex);
}
