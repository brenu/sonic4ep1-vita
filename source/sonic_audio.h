#ifndef SONIC_AUDIO_H
#define SONIC_AUDIO_H
int sonic_audio_init(int volume, int stereo, int requested_rate);
void sonic_audio_start(void);
void sonic_audio_stop(void);
void sonic_audio_volume(int volume);
void sonic_audio_check(void);
int sonic_audio_shutdown(void);
#endif
