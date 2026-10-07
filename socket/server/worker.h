#ifndef SOCKET_SERVER_WORKER_H
#define SOCKET_SERVER_WORKER_H

#include "control.h"
#include "queue.h"
#include "stats.h"

#include <stdint.h>

struct WorkerContext {
    struct JobQueue *queue;
    struct Statistics *stats;
    struct ServerControl *control;
    const char *serverName;
    int32_t serverNumber;
    int clientTimeoutSeconds;
};

void *WorkerMain(void *argument);

#endif
