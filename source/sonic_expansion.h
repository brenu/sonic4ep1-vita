#ifndef SONIC_EXPANSION_H
#define SONIC_EXPANSION_H
#include <stdint.h>
#define SONIC_EXPANSION_FILENAME "main.6200011.com.sega.sonic4epi.obb"
#define SONIC_EXPANSION_SIZE UINT64_C(86710212)
enum { SONIC_EXPANSION_UNDEFINED = 0, SONIC_EXPANSION_NO_DOWNLOAD = -1,
       SONIC_EXPANSION_ERROR = 3 };
int sonic_expansion_initialize(int regular_file, uint64_t size);
int sonic_expansion_download_state(void);
#endif
