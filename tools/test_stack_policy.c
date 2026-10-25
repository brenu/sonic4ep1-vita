#include "sonic_stack_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    SonicStackArena arena = {0x100000, 1024, 0};
    size_t rounded = 999;
    uintptr_t a = sonic_stack_take(&arena, 1, &rounded);
    assert(a == 0x100040 && rounded == 16 && arena.used == 144);
    uintptr_t b = sonic_stack_take(&arena, 129, &rounded);
    assert(!(b & 15) && rounded == 144);
    assert(b - SONIC_STACK_GUARD >= a + 16 + SONIC_STACK_GUARD);
    size_t previous = arena.used;
    assert(!sonic_stack_take(&arena, 1000, &rounded) && arena.used == previous);
    assert(!sonic_stack_take(&arena, SIZE_MAX, &rounded) && arena.used == previous);
    assert(!sonic_stack_take(&arena, 0, &rounded));
    SonicStackArena exact = {0x200000, 256, 0};
    assert(sonic_stack_take(&exact, 128, &rounded) == 0x200040 && exact.used == 256);
    assert(!sonic_stack_take(&exact, 1, &rounded));
    SonicStackArena overflow = {UINTPTR_MAX - 15, 32, 0};
    assert(!sonic_stack_take(&overflow, 1, &rounded));
    arena.base = 0x100001;
    assert(!sonic_stack_take(&arena, 1, &rounded));
    puts("Registered stack arena: aligned disjoint guarded slices, bounds and exhaustion passed");
}
