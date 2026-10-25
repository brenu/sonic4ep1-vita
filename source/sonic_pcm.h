#ifndef SONIC_PCM_H
#define SONIC_PCM_H
#include <stddef.h>
#include <stdint.h>
enum { SONIC_OUTPUT_RATE = 48000, SONIC_AUDIO_FRAMES = 1024, SONIC_SOURCE_FRAMES = 1026 };
typedef struct { unsigned rate, channels, phase; } SonicPcmState;
int sonic_pcm_configure(SonicPcmState *state, int requested_rate, int stereo);
size_t sonic_pcm_required(const SonicPcmState *state, size_t output_frames);
int sonic_pcm_render(SonicPcmState *state, const int16_t *source, size_t source_frames,
                     int16_t *stereo_output, size_t output_frames, size_t *consumed);
int sonic_pcm_volume(int percent);
#endif
