#include "sonic_media_range.h"
#include "sonic_media_duration.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    int64_t start, length, target;
    assert(sonic_media_bounds(38851237, 9698184, 259971, &start, &length));
    assert(start == 9698184 && length == 259971);
    assert(!sonic_media_bounds(10, 8, 3, &start, &length));
    assert(!sonic_media_bounds(10, -1, 3, &start, &length));
    assert(!sonic_media_bounds(INT64_MAX, INT64_MAX - 1, 3, &start, &length));
    assert(sonic_media_bounds(INT64_MAX, INT64_MAX - 1, 1, &start, &length));
    assert(sonic_media_bounds(10, -50, 0, &start, &length) && start == 0 && length == 10);
    assert(!sonic_media_bounds(0, 0, 0, &start, &length));
    assert(sonic_media_seek_target(100, 20, -10, SEEK_CUR, &target) && target == 10);
    assert(sonic_media_seek_target(100, 20, -1, SEEK_END, &target) && target == 99);
    assert(sonic_media_seek_target(100, 20, 100, SEEK_SET, &target) && target == 100);
    assert(!sonic_media_seek_target(100, 20, 101, SEEK_SET, &target));
    assert(!sonic_media_seek_target(100, 20, INT64_MIN, SEEK_CUR, &target));
    assert(!sonic_media_seek_target(INT64_MAX, INT64_MAX, 1, SEEK_CUR, &target));
    assert(!sonic_media_seek_target(100, 20, 0, 999, &target));
    int64_t frames;
    assert(sonic_media_cbr_frames(1125564, 128, 44100, &frames));
    assert(frames == 3102335); /* z1a1 is CBR; within encoder delay/padding */
    assert(sonic_media_cbr_frames(855562, 128, 44100, &frames));
    assert(frames > 0 && frames * 1000 / 44100 == 53472);
    assert(!sonic_media_cbr_frames(100, 0, 44100, &frames));
    assert(!sonic_media_cbr_frames(100, 128, 0, &frames));
    assert(!sonic_media_cbr_frames(INT64_MAX, 128, 44100, &frames));
    assert(!sonic_media_cbr_frames(100, 128, 44100, NULL));
    int repeats = 3;
    assert(sonic_media_repeat(&repeats) && repeats == 2);
    assert(sonic_media_repeat(&repeats) && repeats == 1);
    assert(!sonic_media_repeat(&repeats));
    repeats = 0;
    for (int i = 0; i < 100; ++i) assert(sonic_media_repeat(&repeats) && repeats == 0);
    repeats = INT_MIN;
    assert(sonic_media_repeat(&repeats) && repeats == INT_MIN);
    puts("Media ranges: APK slice, whole-file behavior, signed seeks, overflow and finite/infinite repeats passed");
}
