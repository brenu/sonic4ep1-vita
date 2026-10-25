#include "sonic_perf.h"
#include "sonic_perf_policy.h"
#include "utils/logger.h"
#include <psp2/kernel/processmgr.h>
#include <psp2/power.h>
#include <pthread.h>
#include <stdatomic.h>

void sonic_perf_vitagl_report(uint32_t elapsed_us, uint32_t fixed_draw_us,
                              uint32_t fixed_draws, uint32_t fixed_state_us,
                              uint32_t shader_draw_us, uint32_t shader_draws,
                              uint32_t gpu_queue_us) {
    l_info("VGL_PROFILE elapsed_us=%u fixed_draw_us=%u fixed_draws=%u fixed_state_us=%u shader_draw_us=%u shader_draws=%u gpu_queue_us=%u",
           elapsed_us, fixed_draw_us, fixed_draws, fixed_state_us,
           shader_draw_us, shader_draws, gpu_queue_us);
}

static pthread_mutex_t phase_lock = PTHREAD_MUTEX_INITIALIZER;
static int64_t track_offset, track_length;
static unsigned phase, seen_phase, previous_errors;
static atomic_uint gl_errors;
static SonicPerfWindow window;

void sonic_perf_track(int64_t offset, int64_t length, int channel) {
    if (channel != 0) return;
    pthread_mutex_lock(&phase_lock);
    track_offset = offset; track_length = length; ++phase;
    pthread_mutex_unlock(&phase_lock);
}
void sonic_perf_gl_error(void) { atomic_fetch_add(&gl_errors, 1); }
uint64_t sonic_perf_begin(void) { return sceKernelGetProcessTimeWide(); }
void sonic_perf_end(uint64_t begin) {
    uint64_t end = sceKernelGetProcessTimeWide();
    pthread_mutex_lock(&phase_lock);
    unsigned current_phase = phase;
    int64_t offset = track_offset, length = track_length;
    pthread_mutex_unlock(&phase_lock);
    if (current_phase != seen_phase) {
        window = (SonicPerfWindow){0}; seen_phase = current_phase;
        previous_errors = atomic_load(&gl_errors);
    }
    SonicPerfWindow sample;
    if (!sonic_perf_add(&window, begin, end, &sample)) return;
    unsigned errors = atomic_load(&gl_errors), count = sample.frames;
    unsigned fps = sonic_perf_fps10(&sample);
#ifdef SONIC_QUIET_DIAGNOSTICS
    const char *mode = "quiet";
#else
    const char *mode = "baseline";
#endif
    l_info("PERF mode=%s phase=%lld/%lld frames=%u fps=%u.%u frame_us=%u work_us=%u swap_us=%u worst_us=%u GL_errors=%u CPU=%d GPU=%d BUS=%d XBAR=%d",
        mode, (long long)offset, (long long)length, count, fps/10, fps%10,
        (unsigned)((sample.last_end - sample.start)/count),
        (unsigned)(sample.work/count), (unsigned)(sample.swap/count),
        (unsigned)sample.worst, errors - previous_errors,
        scePowerGetArmClockFrequency(), scePowerGetGpuClockFrequency(),
        scePowerGetBusClockFrequency(), scePowerGetGpuXbarClockFrequency());
    previous_errors = errors;
}
