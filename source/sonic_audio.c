#include "sonic_audio.h"
#include "sonic_pcm.h"
#include "sonic_jni_registry.h"
#include "sonic_java.h"
#include "utils/logger.h"
#include "utils/dialog.h"
#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_ImplBridge.h>
#include <psp2/audioout.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#ifndef SONIC_PCM_OUTPUT_GAIN_PERCENT
#define SONIC_PCM_OUTPUT_GAIN_PERCENT 100
#endif

static pthread_mutex_t control = PTHREAD_MUTEX_INITIALIZER;
static pthread_t worker;
static pthread_key_t worker_key;
static int key_ready, worker_created;
static atomic_int port = -1, configured_volume;
static int configured, configured_stereo, configured_rate;
static SonicPcmState pcm;
static jshortArray sample_array;
static jshort *sample_data;
static int16_t *queue, *output;
static void (*generate_audio)(JNIEnv *, jobject, jshortArray, jint);
static atomic_int stop_requested, playback_error, worker_finished;

static int in_worker(void) { return key_ready && pthread_getspecific(worker_key) != NULL; }

static void report_error(const char *operation, int code) {
    l_error("PCM %s failed: 0x%x", operation, (unsigned)code);
    atomic_store(&playback_error, code ? code : -1);
}

static int set_volume_locked(int volume) {
    configured_volume = volume;
    if (port < 0) return 0;
    int values[2] = {sonic_pcm_volume(volume), sonic_pcm_volume(volume)};
    return sceAudioOutSetVolume(port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, values);
}

static void *playback(void *unused) {
    (void)unused;
    JNIEnv *env = NULL;
    unsigned blocks = 0;
#ifdef SONIC_AUDIO_PROFILE
    unsigned profile_blocks = 0, near_clip_samples = 0, silent_blocks = 0;
    uint64_t callback_total_us = 0, output_total_us = 0;
    uint32_t callback_max_us = 0, output_max_us = 0, source_peak = 0;
#endif
    int key_result = pthread_setspecific(worker_key, (void *)(uintptr_t)1);
    if (key_result) { report_error("thread identity", key_result); goto finished; }
    if ((*jvm).AttachCurrentThread(&jvm, &env, NULL) != JNI_OK || !env) {
        report_error("JNI attach", -1); goto finished;
    }
    size_t available = 0;
    while (!atomic_load(&stop_requested)) {
        size_t required = sonic_pcm_required(&pcm, SONIC_AUDIO_FRAMES);
        if (!required || required > SONIC_SOURCE_FRAMES || available > required) {
            report_error("resampler bounds", -2); break;
        }
        size_t wanted = required - available;
        // Native generateAudio may intentionally leave the destination alone
        // before its mixer callback is ready. Never replay stale samples.
        memset(sample_data, 0, SONIC_SOURCE_FRAMES * pcm.channels * sizeof(*sample_data));
#ifdef SONIC_AUDIO_PROFILE
        uint64_t callback_start = sceKernelGetProcessTimeWide();
#endif
        generate_audio(env, SONIC_OBJECT_SOUND, sample_array, (jint)wanted);
#ifdef SONIC_AUDIO_PROFILE
        uint32_t callback_us = (uint32_t)(sceKernelGetProcessTimeWide() - callback_start);
        callback_total_us += callback_us;
        if (callback_us > callback_max_us) callback_max_us = callback_us;
        int block_has_signal = 0;
        for (size_t i = 0; i < wanted * pcm.channels; ++i) {
            int32_t value = sample_data[i];
            uint32_t magnitude = (uint32_t)(value < 0 ? -value : value);
            if (magnitude > source_peak) source_peak = magnitude;
            if (magnitude >= 32760) ++near_clip_samples;
            if (value) block_has_signal = 1;
        }
        if (!block_has_signal) ++silent_blocks;
#endif
        if (atomic_load(&stop_requested)) break;
        memcpy(queue + available * pcm.channels, sample_data, wanted * pcm.channels * sizeof(*queue));
        available += wanted;
        size_t consumed;
        if (!sonic_pcm_render(&pcm, queue, available, output, SONIC_AUDIO_FRAMES, &consumed)) {
            report_error("resampling", -3); break;
        }
#if SONIC_PCM_OUTPUT_GAIN_PERCENT < 100
        for (size_t i = 0; i < SONIC_AUDIO_FRAMES * 2; ++i)
            output[i] = (int16_t)((int32_t)output[i] * SONIC_PCM_OUTPUT_GAIN_PERCENT / 100);
#endif
        available -= consumed;
        memmove(queue, queue + consumed * pcm.channels, available * pcm.channels * sizeof(*queue));
#ifdef SONIC_AUDIO_PROFILE
        uint64_t output_start = sceKernelGetProcessTimeWide();
#endif
        int result = sceAudioOutOutput(port, output);
#ifdef SONIC_AUDIO_PROFILE
        uint32_t output_us = (uint32_t)(sceKernelGetProcessTimeWide() - output_start);
        output_total_us += output_us;
        if (output_us > output_max_us) output_max_us = output_us;
#endif
        if (result < 0) { report_error("output", result); break; }
        if (++blocks == 1) l_info("PCM first block submitted: input=%u Hz/%u channels output=48000 Hz/stereo",
                                pcm.rate, pcm.channels);
#ifdef SONIC_AUDIO_PROFILE
        if (++profile_blocks == 50) {
            l_info("PCM_PROFILE blocks=%u callback_avg_us=%u callback_max_us=%u output_avg_us=%u output_max_us=%u source_peak=%u near_clip_samples=%u silent_blocks=%u",
                   profile_blocks, (unsigned)(callback_total_us / profile_blocks),
                   callback_max_us, (unsigned)(output_total_us / profile_blocks),
                   output_max_us, source_peak, near_clip_samples, silent_blocks);
            profile_blocks = 0; callback_total_us = 0; output_total_us = 0;
            callback_max_us = 0; output_max_us = 0; source_peak = 0;
            near_clip_samples = 0; silent_blocks = 0;
        }
#endif
    }
    (*jvm).DetachCurrentThread(&jvm);
finished:
    atomic_store(&stop_requested, 1);
    if (port >= 0) {
        sceAudioOutOutput(port, NULL);
        sceAudioOutReleasePort(port);
        port = -1;
    }
    l_info("PCM worker stopped after %u blocks", blocks);
    atomic_store(&worker_finished, 1);
    return NULL;
}

// Called only by the serialized control path. No worker resources are freed
// until it has exited; its JNI callback can request stop without self-joining.
static int release_locked(void) {
    atomic_store(&stop_requested, 1);
    if (worker_created) {
        // Stop must not join an in-flight native mixer callback: the game can
        // hold its mixer lock while calling Java stop. Defer cleanup until the
        // worker reports completion, rather than creating a lock inversion.
        if (!atomic_load(&worker_finished)) return 0;
        int result = pthread_join(worker, NULL);
        if (result) { report_error("thread join", result); return 0; }
        worker_created = 0;
    }
    if (port >= 0) {
        sceAudioOutOutput(port, NULL);
        sceAudioOutReleasePort(port); port = -1;
    }
    if (sample_array) { jda_free((JavaDynArray *)sample_array); sample_array = NULL; }
    sample_data = NULL;
    free(queue); queue = NULL;
    free(output); output = NULL;
    return 1;
}

static int prepare_locked(void) {
    if (!sonic_pcm_configure(&pcm, configured_rate, configured_stereo)) return 0;
    if (!key_ready) {
        int result = pthread_key_create(&worker_key, NULL);
        if (result) { report_error("thread key", result); return 0; }
        key_ready = 1;
    }
    generate_audio = sonic_find_native("com/ideaworks3d/marmalade/SoundPlayer", "generateAudio", "([SI)V");
    if (!generate_audio) { report_error("missing generateAudio registration", -4); return 0; }
    sample_array = (*jni).NewShortArray(&jni, SONIC_SOURCE_FRAMES * pcm.channels);
    sample_data = sample_array ? (*jni).GetShortArrayElements(&jni, sample_array, NULL) : NULL;
    queue = malloc(SONIC_SOURCE_FRAMES * pcm.channels * sizeof(*queue));
    output = memalign(64, SONIC_AUDIO_FRAMES * 2 * sizeof(*output));
    if (!sample_data || !queue || !output) { report_error("buffer allocation", -5); release_locked(); return 0; }
    port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, SONIC_AUDIO_FRAMES, SONIC_OUTPUT_RATE, SCE_AUDIO_OUT_MODE_STEREO);
    if (port < 0) { report_error("port open", port); release_locked(); return 0; }
    int result = set_volume_locked(configured_volume);
    if (result < 0) { report_error("volume", result); release_locked(); return 0; }
    atomic_store(&stop_requested, 0);
    l_info("PCM initialized: requested=%d actual input=%u Hz stereo=%d volume=%d output=48000 Hz pcm_gain=%d%%",
            configured_rate, pcm.rate, configured_stereo, configured_volume,
            SONIC_PCM_OUTPUT_GAIN_PERCENT);
    return (int)pcm.rate;
}

int sonic_audio_init(int volume, int stereo, int requested_rate) {
    if (in_worker()) { report_error("reinitialize from mixer thread", -6); return 0; }
    pthread_mutex_lock(&control);
    if (!release_locked()) {
        l_warn("PCM reinit requested while previous mixer callback is stopping");
        pthread_mutex_unlock(&control); return 0;
    }
    atomic_store(&playback_error, 0);
    configured_volume = volume; configured_stereo = stereo; configured_rate = requested_rate;
    SonicPcmState validation;
    configured = sonic_pcm_configure(&validation, requested_rate, stereo) != 0;
    if (!configured) l_error("PCM unsupported format: rate=%d stereo=%d", requested_rate, stereo);
    int result = configured ? prepare_locked() : 0;
    pthread_mutex_unlock(&control);
    return result; // Android SoundPlayer.init returns the rate, or zero on failure.
}

void sonic_audio_start(void) {
    if (in_worker()) return;
    pthread_mutex_lock(&control);
    if (worker_created && atomic_load(&stop_requested) && !release_locked()) {
        report_error("start before stop completed", -8);
        pthread_mutex_unlock(&control); return;
    }
    if (!worker_created && configured && (port >= 0 || prepare_locked())) {
        atomic_store(&stop_requested, 0);
        atomic_store(&worker_finished, 0);
        int result = pthread_create(&worker, NULL, playback, NULL);
        if (result) report_error("thread creation", result);
        else { worker_created = 1; l_info("PCM worker started"); }
    } else if (!configured) report_error("start before valid init", -7);
    pthread_mutex_unlock(&control);
}

void sonic_audio_stop(void) {
    if (in_worker()) { atomic_store(&stop_requested, 1); return; }
    pthread_mutex_lock(&control);
    int released = release_locked();
    pthread_mutex_unlock(&control);
    l_info("PCM stop: %s", released ? "resources released" : "worker draining; cleanup deferred");
}

void sonic_audio_volume(int volume) {
    // Stop defers cleanup while a mixer callback is active, so its port
    // remains valid. Avoid reentering the controller mutex then.
    if (in_worker()) {
        atomic_store(&configured_volume, volume);
        int values[2] = {sonic_pcm_volume(volume), sonic_pcm_volume(volume)};
        int result = sceAudioOutSetVolume(port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, values);
        if (result < 0 && !atomic_load(&stop_requested)) report_error("mixer volume", result);
        l_info("PCM volume update: requested=%d effective=%d", volume, sonic_pcm_volume(volume));
        return;
    }
    pthread_mutex_lock(&control);
    int result = 0;
    configured_volume = volume;
    if (!atomic_load(&stop_requested)) result = set_volume_locked(volume);
    pthread_mutex_unlock(&control);
    if (result < 0 && !atomic_load(&stop_requested)) report_error("volume", result);
    l_info("PCM volume update: requested=%d effective=%d", volume, sonic_pcm_volume(volume));
}

void sonic_audio_check(void) {
    int error = atomic_load(&playback_error);
    if (error) fatal_error("Sonic PCM worker error 0x%x. See port.log.", (unsigned)error);
}

int sonic_audio_shutdown(void) {
    /* Unlike soundStop inside a native mixer call, runNative has now returned
     * and no caller-owned mixer lock is held. Drain deferred cleanup here. */
    if (in_worker()) return 0;
    pthread_mutex_lock(&control);
    atomic_store(&stop_requested, 1);
    configured = 0;
    uint64_t deadline = sceKernelGetProcessTimeWide() + 2000000;
    while (worker_created && !atomic_load(&worker_finished) &&
           sceKernelGetProcessTimeWide() < deadline)
        sceKernelDelayThread(1000);
    int released = release_locked();
    pthread_mutex_unlock(&control);
    if (released) l_info("PCM shutdown: worker joined and resources released");
    else l_warn("PCM shutdown: worker did not drain within 2 seconds; process exit will reclaim it");
    return released;
}
