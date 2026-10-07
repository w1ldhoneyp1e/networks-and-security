#ifndef SOCKET_SERVER_STATS_H
#define SOCKET_SERVER_STATS_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

struct Statistics {
    unsigned long long activeConnections;
    unsigned long long processedRequests;
    unsigned long long failedConnections;
    uint64_t totalProcessingNs;
    pthread_mutex_t mutex;
};

int StatsInit(struct Statistics *stats);
void StatsConnectionOpened(struct Statistics *stats);
void StatsConnectionFinished(struct Statistics *stats, bool gotRequest, bool failed,
                             uint64_t elapsedNs);
void StatsDestroy(struct Statistics *stats);

#endif
