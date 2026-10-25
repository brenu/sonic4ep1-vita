#ifndef SONIC_MEDIA_DURATION_H
#define SONIC_MEDIA_DURATION_H
#include <stdint.h>
#include <limits.h>

/* Estimate decoded sample frames from a constant-bitrate MP3 byte range. */
static inline int sonic_media_cbr_frames(int64_t bytes, int bitrate_kbps,
                                         int sample_rate, int64_t *frames) {
    if (!frames || bytes <= 0 || bitrate_kbps <= 0 || sample_rate <= 0 ||
        bytes > INT64_MAX / (8LL * sample_rate)) return 0;
    int64_t value = bytes * 8 * sample_rate / ((int64_t)bitrate_kbps * 1000);
    if (value <= 0) return 0;
    *frames = value;
    return 1;
}
#endif
