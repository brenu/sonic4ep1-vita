#include "sonic_memory_policy.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    size_t rounded = 0;
    assert(!sonic_page_size(0, &rounded));
    assert(sonic_page_size(1, &rounded) && rounded == 4096);
    assert(sonic_page_size(4096, &rounded) && rounded == 4096);
    assert(sonic_page_size(4097, &rounded) && rounded == 8192);
    assert(!sonic_page_size(SIZE_MAX, &rounded));
    assert(sonic_range_contains(0x10000, 0x4000, 0x10000, 0x1000));
    assert(sonic_range_contains(0x10000, 0x4000, 0x13000, 0x1000));
    assert(!sonic_range_contains(0x10000, 0x4000, 0xf000, 0x1000));
    assert(!sonic_range_contains(0x10000, 0x4000, 0x13000, 0x2000));
    assert(!sonic_range_contains(0x10000, 0x4000, UINTPTR_MAX, 4096));
    assert(sonic_anonymous_flags(0x21));
    assert(sonic_anonymous_flags(0x22));
    assert(!sonic_anonymous_flags(0x20));
    assert(!sonic_anonymous_flags(0x23));
    assert(!sonic_anonymous_flags(0x31)); // MAP_FIXED rejected
    assert(!sonic_anonymous_flags(0x1));  // file-backed mapping rejected
    puts("Memory policy: page rounding, overflow, guard ranges and supported mmap flags passed");
}
