#include "stats.h"

#include <stdio.h>
#include <string.h>

int StatsInit(struct Statistics *stats)
{
    memset(stats, 0, sizeof(*stats));
    return pthread_mutex_init(&stats->mutex, NULL);
}

void StatsConnectionOpened(struct Statistics *stats)
{
    (void)pthread_mutex_lock(&stats->mutex);
    ++stats->activeConnections;
    (void)pthread_mutex_unlock(&stats->mutex);
}

void StatsConnectionFinished(struct Statistics *stats, bool gotRequest, bool failed,
                             uint64_t elapsedNs)
{
    unsigned long long active;
    unsigned long long processed;
    unsigned long long failures;
    double averageMs = 0.0;

    (void)pthread_mutex_lock(&stats->mutex);
    if (stats->activeConnections > 0) {
        --stats->activeConnections;
    }
    if (gotRequest) {
        ++stats->processedRequests;
        stats->totalProcessingNs += elapsedNs;
    }
    if (failed) {
        ++stats->failedConnections;
    }
    active = stats->activeConnections;
    processed = stats->processedRequests;
    failures = stats->failedConnections;
    if (processed != 0) {
        averageMs = (double)stats->totalProcessingNs / (double)processed / 1000000.0;
    }
    (void)pthread_mutex_unlock(&stats->mutex);
    printf("[stats] active=%llu total=%llu failed=%llu avg_request_time=%.3f ms\n", active,
           processed, failures, averageMs);
}

void StatsDestroy(struct Statistics *stats)
{
    (void)pthread_mutex_destroy(&stats->mutex);
}
