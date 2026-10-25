#ifndef SONIC_INPUT_H
#define SONIC_INPUT_H
#include <stdint.h>
void sonic_input_init(void);
void sonic_input_shutdown(void);
void sonic_input_poll(void);
void sonic_input_media_track(const char *path, int64_t offset, int64_t length, int channel);
#endif
