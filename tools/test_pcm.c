#include "sonic_pcm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int16_t wave(size_t frame, unsigned channel) {
    return (int16_t)((int)((frame * 97 + channel * 613) % 60001) - 30000);
}

// Compare streaming chunks, including retained interpolation samples, with
// direct evaluation on an uninterrupted source timeline over several seconds.
static void continuity(int rate, int stereo) {
    SonicPcmState s;
    assert(sonic_pcm_configure(&s, rate, stereo) == rate);
    int16_t source[SONIC_SOURCE_FRAMES * 2], out[SONIC_AUDIO_FRAMES * 2];
    size_t base = 0, available = 0, generated = 0;
    uint64_t total = 0;
    for (unsigned block = 0; block < 220; ++block) {
        size_t required = sonic_pcm_required(&s, SONIC_AUDIO_FRAMES);
        assert(required <= SONIC_SOURCE_FRAMES && available <= required);
        for (size_t i = available; i < required; ++i, ++generated)
            for (unsigned c = 0; c < s.channels; ++c)
                source[i * s.channels + c] = wave(generated, c);
        available = required;
        size_t consumed;
        assert(sonic_pcm_render(&s, source, available, out, SONIC_AUDIO_FRAMES, &consumed));
        for (size_t i = 0; i < SONIC_AUDIO_FRAMES; ++i) {
            uint64_t pos = (total + i) * (unsigned)rate;
            size_t index = pos / SONIC_OUTPUT_RATE;
            unsigned fraction = pos % SONIC_OUTPUT_RATE;
            for (unsigned c = 0; c < 2; ++c) {
                unsigned channel = stereo ? c : 0;
                int32_t a = wave(index, channel), b = wave(index + 1, channel);
                int16_t expected = (a * (int32_t)(SONIC_OUTPUT_RATE - fraction) + b * (int32_t)fraction) / SONIC_OUTPUT_RATE;
                assert(out[i * 2 + c] == expected);
            }
        }
        base += consumed;
        total += SONIC_AUDIO_FRAMES;
        assert(base == total * (unsigned)rate / SONIC_OUTPUT_RATE);
        assert(s.phase == total * (unsigned)rate % SONIC_OUTPUT_RATE);
        available -= consumed;
        memmove(source, source + consumed * s.channels, available * s.channels * sizeof(*source));
    }
}

int main(void) {
    SonicPcmState s;
    assert(sonic_pcm_configure(&s, 0, 1) == 48000);
    assert(!sonic_pcm_configure(&s, -1, 0));
    assert(!sonic_pcm_configure(&s, 96000, 1));
    assert(!sonic_pcm_configure(&s, 48000, 2));
    assert(sonic_pcm_configure(&s, 24000, 0) == 24000);
    int16_t input[] = {-1000, 0, 1000, 2000};
    int16_t out[10];
    for (unsigned i = 0; i < 10; ++i) out[i] = 12345;
    size_t used = 777;
    assert(!sonic_pcm_render(&s, input, 2, out + 1, 4, &used));
    assert(used == 777 && s.phase == 0);
    for (unsigned i = 0; i < 10; ++i) assert(out[i] == 12345);
    assert(sonic_pcm_render(&s, input, 4, out + 1, 4, &used));
    assert(used == 2 && out[0] == 12345 && out[9] == 12345);
    for (unsigned i = 0; i < 4; ++i) {
        assert(out[1 + i * 2] == -1000 + (int)i * 500);
        assert(out[2 + i * 2] == out[1 + i * 2]);
    }
    assert(!sonic_pcm_required(&s, SONIC_AUDIO_FRAMES + 1));
    assert(sonic_pcm_configure(&s, 48000, 1));
    int16_t extremes[] = {-32768, 32767, 32767, -32768, 0, 0};
    assert(sonic_pcm_render(&s, extremes, 3, out + 1, 2, &used));
    assert(!memcmp(out + 1, extremes, 4 * sizeof(int16_t)));
    assert(sonic_pcm_volume(-1) == 0 && sonic_pcm_volume(50) == 16384);
    assert(sonic_pcm_volume(100) == 32768 && sonic_pcm_volume(999) == 32768);
    const int rates[] = {8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000};
    for (unsigned i = 0; i < sizeof(rates) / sizeof(*rates); ++i) {
        continuity(rates[i], 0); continuity(rates[i], 1);
    }
    puts("PCM format, interpolation, channel, bounds, continuity and volume tests passed");
}
