/*
 * Copyright (C) 2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  patch.c
 * @brief Patching some of the .so internal functions or bridging them to native
 *        for better compatibility.
 */

#include <kubridge.h>
#include <so_util/so_util.h>
#include <stdio.h>
#include <string.h>
#include <vitasdk.h>

#ifdef __cplusplus
extern "C"
{
#endif
	extern so_module so_mod;
#ifdef __cplusplus
};
#endif

#define SCE_KERNEL_MEMBLOCK_TYPE_USER_RX (0x0C20D050)

#include "utils/logger.h"
#include "utils/dialog.h"
#include "reimpl/sys.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include "sonic_stacks.h"

// Marmalade emits ARM code for the S3E payload, then flushes its instruction
// cache through an Android private SVC. Vita cannot execute that syscall.
// The original wrapper takes (start, byte_length), not (start, end).
static int sonic_code_cache_flush(void *start, uint32_t length) {
	if (length && (!start || length > UINTPTR_MAX - (uintptr_t)start))
		fatal_error("Sonic: invalid generated-code cache range %p + %u", start, length);
	__sync_synchronize();
	if (length) kuKernelFlushCaches(start, length);
	__sync_synchronize();
	static atomic_uint calls;
	unsigned call = atomic_fetch_add(&calls, 1) + 1;
	if (call <= 4)
		l_info("Marmalade code cache flush %u: start=%p length=%u", call, start, length);
	return 0;
}

static void sonic_patch_code_cache(so_module *module) {
	// Verified against the supplied version 1.5.0 ELF and probe 07 registers.
	// Guard the complete wrapper, including its syscall-number literal.
	static const uint32_t expected[] = {
		0xe52d7004, 0xe1a03000, 0xe0802001, 0xe59fc018,
		0xe1a00003, 0xe1a01002, 0xe1a0700c, 0xe3a02000,
		0xef000000, 0xe49d7004, 0xe12fff1e, 0x000f0002
	};
	const uint32_t offset = 0x2696c;
	if (module->text_size < offset + sizeof(expected) ||
		memcmp((const void *)(module->text_base + offset), expected, sizeof(expected)))
		fatal_error("Sonic: Android cache-flush wrapper signature mismatch");
	hook_addr(module->text_base + offset, (uintptr_t)sonic_code_cache_flush);
	l_info("Patched Marmalade Android cacheflush at ELF offset 0x%x", offset);
}

void __kuser_memory_barrier(void) {
	__sync_synchronize();
}

static bool atomic_block_ready;

void kuser_patch(so_module *module) {
	if (!atomic_block_ready) {
	SceKernelAllocMemBlockKernelOpt opt;
	memset(&opt, 0, sizeof(SceKernelAllocMemBlockKernelOpt));
	opt.size = sizeof(SceKernelAllocMemBlockKernelOpt);
	opt.attr = 0x1;
	opt.field_C = (SceUInt32)0x9A000000;
	if (kuKernelAllocMemBlock("atomic", SCE_KERNEL_MEMBLOCK_TYPE_USER_RX, 0x1000, &opt) < 0)
		fatal_error("Error could not allocate atomic block.");
	if (kuKernelMemProtect((void *)0x9A000000, (SceSize)0x1000, KU_KERNEL_PROT_EXEC | KU_KERNEL_PROT_READ | KU_KERNEL_PROT_WRITE) < 0)
		fatal_error("Error: could not protect the atomic helper block.");

	hook_addr(0x9A000FA0, (uintptr_t)__kuser_memory_barrier);
	hook_addr(0x9A000FC0, (uintptr_t)__atomic_cmpxchg);
	atomic_block_ready = true;
	}

	uint32_t patched_addr;
	for (uint32_t addr = module->text_base; addr + 4 <= module->text_base + module->text_size; addr += 4) {
		uint32_t *a = (uint32_t *)addr;
		if (*a == 0xFFFF0FC0) {
			l_debug("Patching 0x%x -> __kuser_cmpxchg", a);
			patched_addr = 0x9A000FC0;
			kuKernelCpuUnrestrictedMemcpy((void *)(addr), &patched_addr, sizeof(uint32_t));
		}
		else if (*a == 0xFFFF0FA0) {
			l_debug("Patching 0x%x -> __kuser_memory_barrier", a);
			patched_addr = 0x9A000FA0;
			kuKernelCpuUnrestrictedMemcpy((void *)(addr), &patched_addr, sizeof(uint32_t));
		}
	}
}

void so_patch_module(so_module *module) {
	kuser_patch(module);
}

void so_patch(void) {
	so_patch_module(&so_mod);
	sonic_patch_code_cache(&so_mod);
	sonic_stacks_patch(&so_mod);
	// Sample hook with symbol name
	// hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN6glitch2os7Printer5printEPKcz"), (uintptr_t)&hookedFunction);
	// Or with offset
	// hook_addr((uintptr_t)so_mod.text_base + 0xdeadbabe, (uintptr_t)&hookedFunction);
	// If you use SO_CONTINUE, define a so_hook before the function and assign to it
	// function_hook = hook_addr(...);
}
