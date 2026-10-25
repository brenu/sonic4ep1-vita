#ifndef SONIC_DYNLINK_H
#define SONIC_DYNLINK_H
/* -1: unknown, 0: native/platform veneer, 1: already loaded core, 2..4: extensions. */
int sonic_library_kind(const char *path);
void *sonic_dlopen(const char *path, int flags);
void *sonic_dlsym(void *handle, const char *symbol);
int sonic_dlclose(void *handle);
char *sonic_dlerror(void);
void *sonic_builtin_symbol(const char *name);
#endif
