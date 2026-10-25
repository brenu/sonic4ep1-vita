#include "sonic_os_queue.h"
#include <errno.h>
#include <string.h>

static void *dispatch(void *arg) {
    SonicOsQueue *q = arg;
    pthread_mutex_lock(&q->mutex);
    for (;;) {
        while (!q->pending && !q->stopping) pthread_cond_wait(&q->wake, &q->mutex);
        if (!q->pending && q->stopping) break;
        --q->pending;
        pthread_mutex_unlock(&q->mutex);
        // Never hold the queue mutex across native work. A tick can itself
        // post another notification, and the original sender must return.
        q->tick(q->context);
        pthread_mutex_lock(&q->mutex);
    }
    pthread_mutex_unlock(&q->mutex);
    return NULL;
}

int sonic_os_queue_start(SonicOsQueue *q, void (*tick)(void *), void *context) {
    if (!q || !tick) return EINVAL;
    memset(q, 0, sizeof(*q)); q->tick = tick; q->context = context;
    int result = pthread_mutex_init(&q->mutex, NULL);
    if (result) return result;
    result = pthread_cond_init(&q->wake, NULL);
    if (result) { pthread_mutex_destroy(&q->mutex); return result; }
    pthread_attr_t attr;
    result = pthread_attr_init(&attr);
    if (!result) {
        result = pthread_attr_setstacksize(&attr, 1024 * 1024);
        if (!result) result = pthread_create(&q->worker, &attr, dispatch, q);
        pthread_attr_destroy(&attr);
    }
    if (result) { pthread_cond_destroy(&q->wake); pthread_mutex_destroy(&q->mutex); }
    return result;
}

int sonic_os_queue_post(SonicOsQueue *q) {
    pthread_mutex_lock(&q->mutex);
    int result = q->stopping ? ECANCELED : q->pending == UINT64_MAX ? EOVERFLOW : 0;
    if (!result) { ++q->pending; pthread_cond_signal(&q->wake); }
    pthread_mutex_unlock(&q->mutex);
    return result;
}

static int stop_queue(SonicOsQueue *q, int discard) {
    if (pthread_equal(pthread_self(), q->worker)) return EDEADLK;
    pthread_mutex_lock(&q->mutex);
    q->stopping = 1;
    if (discard) q->pending = 0;
    pthread_cond_broadcast(&q->wake);
    pthread_mutex_unlock(&q->mutex);
    int result = pthread_join(q->worker, NULL);
    if (!result) { pthread_cond_destroy(&q->wake); pthread_mutex_destroy(&q->mutex); }
    return result;
}

int sonic_os_queue_stop(SonicOsQueue *q) { return stop_queue(q, 0); }
int sonic_os_queue_cancel(SonicOsQueue *q) { return stop_queue(q, 1); }
