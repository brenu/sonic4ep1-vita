#ifndef SONIC_STACK_POLICY_H
#define SONIC_STACK_POLICY_H
#include <stddef.h>
#include <stdint.h>
enum { SONIC_STACK_GUARD = 64 };
typedef struct { uintptr_t base; size_t size, used; } SonicStackArena;
static inline uintptr_t sonic_stack_take(SonicStackArena *arena, size_t bytes, size_t *rounded) {
    if (!arena || !rounded || !bytes || bytes > SIZE_MAX - 15 || (arena->base & 15) ||
        arena->size > UINTPTR_MAX - arena->base || arena->used > arena->size || (arena->used & 15)) return 0;
    size_t size = (bytes + 15) & ~(size_t)15;
    if (size > SIZE_MAX - 2 * SONIC_STACK_GUARD) return 0;
    size_t total = size + 2 * SONIC_STACK_GUARD;
    if (total > arena->size - arena->used) return 0;
    uintptr_t result = arena->base + arena->used + SONIC_STACK_GUARD;
    arena->used += total; *rounded = size;
    return result;
}
#endif
