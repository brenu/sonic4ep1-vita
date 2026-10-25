#include "sonic_stacks.h"
#include "sonic_stack_policy.h"
#include "sonic_memory_policy.h"
#include "utils/dialog.h"
#include "utils/logger.h"
#include <psp2/kernel/threadmgr.h>
#include <kubridge.h>
#include <stdint.h>
#include <string.h>

enum { STACK_ARENA_SIZE = 4 * 1024 * 1024, MAX_STACKS = 32 };
static SonicStackArena arena;
static struct { unsigned char *base; size_t size; } stacks[MAX_STACKS];
static unsigned stack_count;
static uintptr_t owner;
static void *(*original_create)(uintptr_t, unsigned, void *, void *, int);

static uintptr_t identity(void) {
    uintptr_t value;
    __asm__ volatile("mrc p15, 0, %0, c13, c0, 3" : "=r"(value));
    return value;
}

static void check_guards(void) {
    for (unsigned i = 0; i < stack_count; ++i) {
        unsigned char *base = stacks[i].base;
        for (unsigned j = 0; j < SONIC_STACK_GUARD; ++j)
            if (base[-SONIC_STACK_GUARD + (int)j] != 0xa7 || base[stacks[i].size + j] != 0xa7)
                fatal_error("Sonic: registered fibre-stack guard damaged (stack %u)", i);
    }
}

void *sonic_stack_allocate(size_t bytes) {
    if (!owner || identity() != owner) fatal_error("Sonic: stack arena requested from another thread");
    check_guards();
    size_t rounded;
    uintptr_t base = stack_count < MAX_STACKS ? sonic_stack_take(&arena, bytes, &rounded) : 0;
    if (!base) fatal_error("Sonic: registered fibre-stack arena exhausted (request %u)", (unsigned)bytes);
    memset((void *)(base - SONIC_STACK_GUARD), 0xa7, SONIC_STACK_GUARD);
    memset((void *)(base + rounded), 0xa7, SONIC_STACK_GUARD);
    stacks[stack_count].base = (void *)base; stacks[stack_count].size = rounded;
    ++stack_count;
    l_info("Registered stack slice %u: base=%p size=%u", stack_count, (void *)base, (unsigned)bytes);
    return (void *)base;
}

__attribute__((noinline))
int sonic_stack_scope(int (*entry)(void)) {
    // The active native frames stay below this local array; guest stacks use
    // disjoint slices inside it. The enclosing frame lives until entry exits.
    unsigned char storage[STACK_ARENA_SIZE] __attribute__((aligned(16)));
    SceKernelThreadInfo info = {.size = sizeof(info)};
    if (sceKernelGetThreadInfo(sceKernelGetThreadId(), &info) < 0 ||
        !sonic_range_contains((uintptr_t)info.stack, info.stackSize,
                              (uintptr_t)storage, sizeof(storage)))
        fatal_error("Sonic: fibre arena is outside the registered Vita thread stack");
    owner = identity();
    if (!owner) fatal_error("Sonic: registered stack owner identity unavailable");
    arena = (SonicStackArena){(uintptr_t)storage, sizeof(storage), 0};
    memset(storage, 0, sizeof(storage));
    l_info("Registered main stack: base=%p size=%u; fibre arena=%p size=%u",
            info.stack, (unsigned)info.stackSize, storage, (unsigned)sizeof(storage));
    int result = entry();
    check_guards();
    return result;
}

static void *create_fibre(uintptr_t entry, unsigned bytes, void *arg, void *provided, int flags) {
    if (identity() == owner) {
        // A supplied stack leaves Marmalade's ownership flag clear. Its
        // normal constructor still initializes registers and fibre records.
        provided = sonic_stack_allocate(bytes);
        flags = 0;
    }
    return original_create(entry, bytes, arg, provided, flags);
}

void sonic_stacks_patch(so_module *module) {
    const uintptr_t offset = 0x5dc00;
    const uint32_t expected[] = {0xe92d4ff0, 0xe24dd00c, 0xe59fc330, 0xe1a0b003};
    if (module->text_size < offset + sizeof(expected) ||
        memcmp((const void *)(module->text_base + offset), expected, sizeof(expected)))
        fatal_error("Sonic: fibre constructor signature mismatch");
    // Only the first two instructions move: push + stack reservation, neither
    // PC-relative. Resume before the original literal load at entry + 8.
    uintptr_t target = (module->patch_head + 3) & ~(uintptr_t)3;
    if (target < module->patch_base || target > module->patch_base + module->patch_size ||
        module->patch_base + module->patch_size - target < 16)
        fatal_error("Sonic: no code arena for fibre constructor trampoline");
    uint32_t trampoline[] = {expected[0], expected[1], 0xe51ff004,
                             (uint32_t)(module->text_base + offset + 8)};
    memcpy((void *)target, trampoline, sizeof(trampoline));
    module->patch_head = target + sizeof(trampoline);
    kuKernelFlushCaches((void *)target, sizeof(trampoline));
    original_create = (void *)target;
    hook_addr(module->text_base + offset, (uintptr_t)create_fibre);
    l_info("Patched main-thread fibre constructor for registered Vita stack slices");
}
