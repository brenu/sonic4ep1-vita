#include "sonic_os.h"
#include "sonic_os_queue.h"
#include "sonic_jni_registry.h"
#include "sonic_java.h"
#include "utils/dialog.h"
#include "utils/logger.h"
#include <falso_jni/FalsoJNI.h>
#include <stdatomic.h>

static SonicOsQueue queue;
static void (*native_tick)(JNIEnv *, jobject);
static atomic_uint notifications;
static pthread_mutex_t controller = PTHREAD_MUTEX_INITIALIZER;
static int started, stopping;

static void run_tick(void *unused) {
    (void)unused;
    // FalsoJNI has no real VM thread state; attach obtains the active table.
    JNIEnv *env = NULL;
    if ((*jvm).AttachCurrentThread(&jvm, &env, NULL) != JNI_OK || !env)
        fatal_error("Sonic: OS dispatcher JNI attach failed");
    static unsigned ticks;
    unsigned tick = ++ticks;
    if (tick <= 4) l_info("OS dispatcher tick %u begin", tick);
    native_tick(env, SONIC_OBJECT_THREAD);
    if (tick <= 4) l_info("OS dispatcher tick %u complete", tick);
}

void sonic_os_init(void) {
    native_tick = sonic_find_native("com/ideaworks3d/marmalade/LoaderThread",
                                    "runOnOSTickNative", "()V");
    if (!native_tick) fatal_error("Sonic: runOnOSTickNative was not registered");
    int result = sonic_os_queue_start(&queue, run_tick, NULL);
    if (result) fatal_error("Sonic: OS dispatcher creation failed: %d", result);
    pthread_mutex_lock(&controller);
    started = 1;
    pthread_mutex_unlock(&controller);
    l_info("OS dispatcher ready: separate thread, queued notifications");
}

void sonic_os_signal(void) {
    pthread_mutex_lock(&controller);
    if (!started || stopping) { pthread_mutex_unlock(&controller); return; }
    int result = sonic_os_queue_post(&queue);
    pthread_mutex_unlock(&controller);
    if (result) fatal_error("Sonic: OS signal post failed: %d", result);
    unsigned posted = atomic_fetch_add(&notifications, 1) + 1;
    if (posted <= 4) l_info("OS signal queued: %u", posted);
}

int sonic_os_shutdown(void) {
    pthread_mutex_lock(&controller);
    if (!started || stopping) { pthread_mutex_unlock(&controller); return 1; }
    stopping = 1;
    pthread_mutex_unlock(&controller);
    /* No native work may be started after runNative teardown. A task already
     * executing is joined; queued stale notifications are discarded. */
    int result = sonic_os_queue_cancel(&queue);
    pthread_mutex_lock(&controller);
    if (!result) started = 0;
    pthread_mutex_unlock(&controller);
    if (result) l_warn("OS dispatcher shutdown failed: %d", result);
    else l_info("OS dispatcher shutdown: worker joined, stale notifications discarded");
    return !result;
}
