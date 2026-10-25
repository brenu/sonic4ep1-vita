#ifndef SONIC_OS_QUEUE_H
#define SONIC_OS_QUEUE_H
#include <pthread.h>
#include <stdint.h>
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t wake;
    pthread_t worker;
    uint64_t pending;
    int stopping;
    void (*tick)(void *);
    void *context;
} SonicOsQueue;
int sonic_os_queue_start(SonicOsQueue *queue, void (*tick)(void *), void *context);
int sonic_os_queue_post(SonicOsQueue *queue);
int sonic_os_queue_stop(SonicOsQueue *queue);
int sonic_os_queue_cancel(SonicOsQueue *queue);
#endif
