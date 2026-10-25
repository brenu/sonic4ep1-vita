#ifndef SONIC_MEDIA_H
#define SONIC_MEDIA_H
#include <stdint.h>
int sonic_media_play(const char *path, int repeats, int64_t offset, int64_t length, int channel);
void sonic_media_stop(int channel);
int sonic_media_pause(int channel);
int sonic_media_resume(int channel);
int sonic_media_status(int channel);
int sonic_media_position(int channel);
int sonic_media_duration(int channel);
void sonic_media_seek(int milliseconds, int channel);
void sonic_media_volume(int volume, int channel);
void sonic_media_pump(void);
void sonic_media_shutdown(void);
#endif
