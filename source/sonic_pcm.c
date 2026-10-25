#include "sonic_pcm.h"

int sonic_pcm_configure(SonicPcmState *state, int requested_rate, int stereo) {
    if (!state || (stereo != 0 && stereo != 1)) return 0;
    unsigned rate = requested_rate ? (unsigned)requested_rate : SONIC_OUTPUT_RATE;
    switch (rate) {
        case 8000: case 11025: case 12000: case 16000: case 22050:
        case 24000: case 32000: case 44100: case 48000: break;
        default: return 0;
    }
    *state = (SonicPcmState){rate, stereo ? 2u : 1u, 0};
    return (int)rate;
}

size_t sonic_pcm_required(const SonicPcmState *state, size_t frames) {
    if (!state || !state->rate || !frames || frames > SONIC_AUDIO_FRAMES) return 0;
    return (size_t)(((uint64_t)state->phase + (frames - 1) * (uint64_t)state->rate) / SONIC_OUTPUT_RATE) + 2;
}

int sonic_pcm_render(SonicPcmState *state, const int16_t *source, size_t available,
                     int16_t *output, size_t frames, size_t *consumed) {
    if (!state || !consumed || state->phase >= SONIC_OUTPUT_RATE ||
        !state->rate || state->rate > SONIC_OUTPUT_RATE || (state->channels != 1 && state->channels != 2)) return 0;
    if (!frames) { *consumed = 0; return 1; }
    size_t required = sonic_pcm_required(state, frames);
    if (!required || !source || !output || available < required) return 0;
    uint64_t position = state->phase;
    for (size_t i = 0; i < frames; ++i) {
        size_t index = (size_t)(position / SONIC_OUTPUT_RATE);
        unsigned fraction = position % SONIC_OUTPUT_RATE;
        for (unsigned channel = 0; channel < 2; ++channel) {
            unsigned c = state->channels == 1 ? 0 : channel;
            int32_t a = source[index * state->channels + c];
            int32_t b = source[(index + 1) * state->channels + c];
            output[i * 2 + channel] = (int16_t)((a * (int32_t)(SONIC_OUTPUT_RATE - fraction) + b * (int32_t)fraction) / SONIC_OUTPUT_RATE);
        }
        position += state->rate;
    }
    *consumed = (size_t)(position / SONIC_OUTPUT_RATE);
    state->phase = position % SONIC_OUTPUT_RATE;
    return 1;
}

int sonic_pcm_volume(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return percent * 32768 / 100;
}
