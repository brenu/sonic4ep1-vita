#ifndef SONIC_MEDIA_RANGE_H
#define SONIC_MEDIA_RANGE_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <limits.h>

static inline int sonic_media_bounds(int64_t size, int64_t offset, int64_t length,
                                     int64_t *start, int64_t *count) {
    if (size < 0) return 0;
    /* Android only uses offset for a positive-length AssetFileDescriptor. */
    if (length <= 0) { offset = 0; length = size; }
    if (offset < 0 || offset > size || length <= 0 || length > size - offset) return 0;
    *start = offset; *count = length;
    return 1;
}

static inline int sonic_media_seek_target(int64_t length, int64_t current,
                                          int64_t delta, int whence, int64_t *target) {
    int64_t base;
    if (length < 0 || current < 0 || current > length) return 0;
    if (whence == SEEK_SET) base = 0;
    else if (whence == SEEK_CUR) base = current;
    else if (whence == SEEK_END) base = length;
    else return 0;
    if ((delta > 0 && base > INT64_MAX - delta) ||
        (delta < 0 && base < INT64_MIN - delta)) return 0;
    int64_t value = base + delta;
    if (value < 0 || value > length) return 0;
    *target = value;
    return 1;
}

static inline int sonic_media_repeat(int *remaining) {
    if (*remaining <= 0) return 1;
    if (*remaining == 1) return 0;
    --*remaining;
    return 1;
}
#endif
