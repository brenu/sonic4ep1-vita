#include "sonic_tls.h"
#include "sonic_stacks.h"
#include "utils/logger.h"
#include "utils/dialog.h"
#include <vitasdk/utils.h>
#include <psp2/kernel/threadmgr.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <malloc.h>
#include <stdlib.h>

void *__real_vitasdk_get_tls_data(SceUID thid);
static void *main_tls_slot;
static uintptr_t main_identity;
static pthread_t main_pthread;
static atomic_int ready;

static inline uintptr_t thread_identity(void) {
    uintptr_t value;
    // Read-only hardware thread-local identity, independent of the current SP.
    __asm__ volatile("mrc p15, 0, %0, c13, c0, 3" : "=r"(value));
    return value;
}

void *__wrap_vitasdk_get_tls_data(SceUID thid) {
    if (thid == 0 && atomic_load_explicit(&ready, memory_order_acquire) &&
        thread_identity() == main_identity)
        return main_tls_slot;
    return __real_vitasdk_get_tls_data(thid);
}

void sonic_tls_init(void) {
    // Materialize pthread_self's TLS metadata before the game changes SP.
    main_pthread = pthread_self();
    main_tls_slot = __real_vitasdk_get_tls_data(0);
    main_identity = thread_identity();
    if (!main_tls_slot || !main_identity)
        fatal_error("Sonic: main TLS cache unavailable (slot=%p identity=%p)",
                    main_tls_slot, (void *)main_identity);
    atomic_store_explicit(&ready, 1, memory_order_release);
    l_info("Main SDK TLS slot cached for alternate stacks: slot=%p identity=%p",
            main_tls_slot, (void *)main_identity);
}

static pthread_key_t probe_key;
static void *probe_value;
static void *probe(void *unused) {
    (void)unused;
    if (!pthread_equal(pthread_self(), main_pthread)) return NULL;
    // Exercise real kernel transitions and scheduling while SP is on a
    // reserved slice inside the registered Vita stack, rather than a brief
    // heap-stack call that can miss the platform's periodic bounds checks.
    for (unsigned i = 0; i < 5; ++i)
        if (sceKernelDelayThread(20000) < 0) return NULL;
    return pthread_getspecific(probe_key);
}

static void *probe_other_thread(void *value) {
    if (thread_identity() == main_identity || pthread_equal(pthread_self(), main_pthread) ||
        pthread_getspecific(probe_key) != NULL || pthread_setspecific(probe_key, value))
        return NULL;
    return pthread_getspecific(probe_key);
}

// Thumb entry; r4 preserves the native SP across the call, with both stacks
// 8-byte aligned. Only this tiny shim changes SP; no C locals span the change.
__attribute__((naked, noinline))
static void *call_on_stack(void *top, void *(*function)(void *), void *arg) {
    __asm__ volatile(
        "push {r4, lr}\n"
        "mov r4, sp\n"
        "mov sp, r0\n"
        "mov r0, r2\n"
        "blx r1\n"
        "mov sp, r4\n"
        "pop {r4, pc}\n");
}

void sonic_tls_checkpoint(void) {
    const size_t size = 64 * 1024;
    void *stack = sonic_stack_allocate(size);
    if (!stack || pthread_key_create(&probe_key, NULL))
        fatal_error("Sonic: alternate-stack TLS checkpoint allocation failed");
    probe_value = stack;
    if (pthread_setspecific(probe_key, probe_value))
        fatal_error("Sonic: alternate-stack TLS checkpoint setup failed");
    pthread_t other;
    void *other_result = NULL;
    void *other_value = (char *)stack + 16;
    int created = pthread_create(&other, NULL, probe_other_thread, other_value);
    if (created || pthread_join(other, &other_result) || other_result != other_value ||
        pthread_getspecific(probe_key) != probe_value)
        fatal_error("Sonic: SDK TLS cache thread-isolation checkpoint failed");
    l_info("SDK TLS cache checkpoint: second thread remains isolated");
    l_info("Registered alternate-stack checkpoint: pthread TLS and five kernel waits on a 64 KiB slice");
    void *result = call_on_stack((char *)stack + size, probe, NULL);
    if (result != probe_value)
        fatal_error("Sonic: alternate-stack TLS checkpoint value/thread mismatch");
    pthread_setspecific(probe_key, NULL);
    pthread_key_delete(probe_key);
    l_success("Registered alternate-stack checkpoint passed: TLS identity, value and kernel waits");
}
