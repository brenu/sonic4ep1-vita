#include "sonic_dynlink.h"
#include "utils/init.h"
#include "utils/dialog.h"
#include "utils/logger.h"
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

extern so_module so_mod;
typedef struct {
    const char *name;
    uintptr_t address;
    so_module module;
    atomic_int state; // 0=not loaded, 1=loading, 2=ready; mappings stay resident.
} SonicExtension;
static SonicExtension extensions[] = {
    {.name="libs3eDialog.so", .address=0x99000000},
    {.name="libs3eFlurry.so", .address=0x99200000},
    {.name="libs3eAPKExpansion.so", .address=0x99400000},
};
static int platform_handle;
static int global_handle;
static char last_error[192];
static int error_pending;

static void dynamic_error(const char *message, const char *name) {
    snprintf(last_error, sizeof(last_error), "%s: %s", message, name ? name : "NULL");
    error_pending = 1;
    l_warn("libdl: %s", last_error);
}

char *sonic_dlerror(void) {
    if (!error_pending) return NULL;
    error_pending = 0;
    return last_error;
}

void *sonic_dlopen(const char *path, int flags) {
    // The observed loader uses RTLD_NOW (2). Honor supported NOW/LAZY/GLOBAL
    // bits; never claim success for an unknown library or unsupported mode.
    if (flags & ~(3 | 0x100)) { dynamic_error("Unsupported dlopen flags", path); return NULL; }
    int kind = sonic_library_kind(path);
    l_info("dlopen(%s, 0x%x)", path ? path : "NULL", flags);
    if (kind < 0) { dynamic_error("Unknown library", path); return NULL; }
    if (kind == 0) return path ? &platform_handle : &global_handle;
    if (kind == 1) return &so_mod;
    SonicExtension *ext = &extensions[kind - 2];
    int expected = 0;
    if (!atomic_compare_exchange_strong(&ext->state, &expected, 1)) {
        if (expected == 2) return &ext->module;
        dynamic_error("Library initialization in progress", ext->name); return NULL;
    }
    char filename[256];
    snprintf(filename, sizeof(filename), DATA_PATH "%s", ext->name);
    int result = so_file_load(&ext->module, filename, ext->address);
    if (result < 0) fatal_error("Sonic: cannot load extension %s (0x%x)", filename, result);
    // These Android binaries have DT_NEEDED but no DT_SONAME. The loader's
    // dependency search requires an explicit name for each resident module.
    ext->module.soname = (char *)ext->name;
    so_relocate(&ext->module);
    resolve_imports(&ext->module);
    so_patch_module(&ext->module);
    so_flush_caches(&ext->module);
    so_initialize(&ext->module);
    atomic_store(&ext->state, 2);
    l_success("Extension loaded and initialized: %s", ext->name);
    // Marmalade itself finds/invokes RegisterExt; do not call it twice here.
    return &ext->module;
}

static void *lookup_module(so_module *module, const char *symbol) {
    return (void *)so_symbol(module, symbol);
}

void *sonic_dlsym(void *handle, const char *symbol) {
    if (!symbol) { dynamic_error("Null symbol", NULL); return NULL; }
    int global = !handle || handle == &global_handle || (uintptr_t)handle == UINTPTR_MAX || (uintptr_t)handle == UINTPTR_MAX - 1;
    int valid = global || handle == &platform_handle || handle == &so_mod;
    void *result = NULL;
    if (handle == &so_mod || global) result = lookup_module(&so_mod, symbol);
    for (unsigned i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i) {
        if (atomic_load(&extensions[i].state) != 2) continue;
        if (handle == &extensions[i].module) valid = 1;
        if (!result && (global || handle == &extensions[i].module))
            result = lookup_module(&extensions[i].module, symbol);
    }
    if (!valid) { dynamic_error("Invalid library handle", symbol); return NULL; }
    if (!result && handle != &platform_handle) result = lookup_module(&so_mod, symbol);
    if (!result) result = sonic_builtin_symbol(symbol);
    if (!result) dynamic_error("Unknown symbol", symbol);
    else l_debug("dlsym(%p, %s) = %p", handle, symbol, result);
    return result;
}

int sonic_dlclose(void *handle) {
    if (handle == &platform_handle || handle == &global_handle || handle == &so_mod) return 0;
    for (unsigned i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i)
        if (atomic_load(&extensions[i].state) == 2 && handle == &extensions[i].module) return 0;
    dynamic_error("Invalid dlclose handle", NULL); return -1;
}
