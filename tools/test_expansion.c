#include "sonic_expansion.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    assert(sonic_expansion_download_state() == SONIC_EXPANSION_UNDEFINED);
    assert(sonic_expansion_initialize(0, SONIC_EXPANSION_SIZE) == SONIC_EXPANSION_ERROR);
    assert(sonic_expansion_initialize(1, 0) == SONIC_EXPANSION_ERROR);
    assert(sonic_expansion_initialize(1, SONIC_EXPANSION_SIZE - 1) == SONIC_EXPANSION_ERROR);
    assert(sonic_expansion_initialize(1, SONIC_EXPANSION_SIZE + 1) == SONIC_EXPANSION_ERROR);
    assert(sonic_expansion_initialize(1, SONIC_EXPANSION_SIZE + (UINT64_C(1) << 32)) == SONIC_EXPANSION_ERROR);
    assert(sonic_expansion_initialize(1, SONIC_EXPANSION_SIZE) == SONIC_EXPANSION_NO_DOWNLOAD);
    assert(sonic_expansion_download_state() == -1);
    assert(!strcmp(SONIC_EXPANSION_FILENAME, "main.6200011.com.sega.sonic4epi.obb"));
    puts("APK expansion: initial/error/installed states, filename and 64-bit size validation passed");
}
