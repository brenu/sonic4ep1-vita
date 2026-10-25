#include "sonic_expansion.h"
#include <stdatomic.h>

static atomic_int download_state = SONIC_EXPANSION_UNDEFINED;

int sonic_expansion_initialize(int regular_file, uint64_t size) {
    int state = regular_file && size == SONIC_EXPANSION_SIZE ?
                SONIC_EXPANSION_NO_DOWNLOAD : SONIC_EXPANSION_ERROR;
    atomic_store(&download_state, state);
    return state;
}

int sonic_expansion_download_state(void) { return atomic_load(&download_state); }
