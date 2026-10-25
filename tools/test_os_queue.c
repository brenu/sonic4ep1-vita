#define _POSIX_C_SOURCE 200809L
#include "sonic_os_queue.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    SonicOsQueue queue;
    pthread_mutex_t lock;
    pthread_cond_t changed;
    pthread_t sender;
    unsigned entered, completed;
    int release;
} Test;

static void tick(void *arg) {
    Test *t = arg;
    assert(!pthread_equal(pthread_self(), t->sender));
    pthread_mutex_lock(&t->lock);
    if (++t->entered == 1) {
        // A callback must be able to post again without deadlocking or
        // executing recursively. Joining oneself must also be rejected.
        assert(sonic_os_queue_post(&t->queue) == 0);
        assert(sonic_os_queue_stop(&t->queue) == EDEADLK);
        pthread_cond_signal(&t->changed);
        while (!t->release) pthread_cond_wait(&t->changed, &t->lock);
    }
    ++t->completed;
    pthread_mutex_unlock(&t->lock);
}

static void *producer(void *arg) {
    Test *t = arg;
    for (unsigned i = 0; i < 250; ++i) assert(!sonic_os_queue_post(&t->queue));
    return NULL;
}

static void *cancel_pending(void *arg) {
    Test *t = arg;
    return (void *)(intptr_t)sonic_os_queue_cancel(&t->queue);
}

int main(void) {
    Test t = {.lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER,
              .sender = pthread_self()};
    assert(!sonic_os_queue_start(&t.queue, tick, &t));
    assert(!sonic_os_queue_post(&t.queue));
    pthread_mutex_lock(&t.lock);
    struct timespec limit;
    assert(!clock_gettime(CLOCK_REALTIME, &limit)); limit.tv_sec += 5;
    while (!t.entered) assert(!pthread_cond_timedwait(&t.changed, &t.lock, &limit));
    pthread_mutex_unlock(&t.lock);
    // The first tick is deliberately blocked; producers must still finish.
    pthread_t producers[8];
    for (unsigned i = 0; i < 8; ++i) assert(!pthread_create(&producers[i], NULL, producer, &t));
    for (unsigned i = 0; i < 8; ++i) assert(!pthread_join(producers[i], NULL));
    pthread_mutex_lock(&t.lock);
    assert(t.entered == 1 && t.completed == 0);
    t.release = 1; pthread_cond_signal(&t.changed);
    pthread_mutex_unlock(&t.lock);
    assert(!sonic_os_queue_stop(&t.queue));
    assert(t.entered == 2002 && t.completed == 2002);
    pthread_cond_destroy(&t.changed); pthread_mutex_destroy(&t.lock);

    Test cancel = {.lock = PTHREAD_MUTEX_INITIALIZER,
                   .changed = PTHREAD_COND_INITIALIZER, .sender = pthread_self()};
    assert(!sonic_os_queue_start(&cancel.queue, tick, &cancel));
    assert(!sonic_os_queue_post(&cancel.queue));
    pthread_mutex_lock(&cancel.lock);
    assert(!clock_gettime(CLOCK_REALTIME, &limit)); limit.tv_sec += 5;
    while (!cancel.entered)
        assert(!pthread_cond_timedwait(&cancel.changed, &cancel.lock, &limit));
    pthread_mutex_unlock(&cancel.lock);
    for (int i = 0; i < 100; ++i) assert(!sonic_os_queue_post(&cancel.queue));
    pthread_t stopper;
    assert(!pthread_create(&stopper, NULL, cancel_pending, &cancel));
    /* In-flight job is held so the queue mutex stays valid while observing
     * cancellation; after releasing it, no queued stale callback may run. */
    pthread_mutex_lock(&cancel.queue.mutex);
    while (!cancel.queue.stopping)
        assert(!pthread_cond_timedwait(&cancel.queue.wake, &cancel.queue.mutex, &limit));
    assert(cancel.queue.pending == 0);
    pthread_mutex_unlock(&cancel.queue.mutex);
    assert(sonic_os_queue_post(&cancel.queue) == ECANCELED);
    pthread_mutex_lock(&cancel.lock);
    cancel.release = 1; pthread_cond_signal(&cancel.changed);
    pthread_mutex_unlock(&cancel.lock);
    void *cancel_result;
    assert(!pthread_join(stopper, &cancel_result) && !cancel_result);
    assert(cancel.entered == 1 && cancel.completed == 1);
    pthread_cond_destroy(&cancel.changed); pthread_mutex_destroy(&cancel.lock);
    puts("OS queue: concurrent/reentrant dispatch, drain, cancellation of pending work and rejection of late posts passed");
}
