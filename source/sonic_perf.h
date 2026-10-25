#ifndef SONIC_PERF_H
#define SONIC_PERF_H
#include <stdint.h>
uint64_t sonic_perf_begin(void);
void sonic_perf_end(uint64_t begin);
void sonic_perf_track(int64_t offset, int64_t length, int channel);
void sonic_perf_gl_error(void);
#endif
