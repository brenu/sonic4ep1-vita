#ifndef SONIC_STACKS_H
#define SONIC_STACKS_H
#include <stddef.h>
#include <so_util/so_util.h>
int sonic_stack_scope(int (*entry)(void));
void *sonic_stack_allocate(size_t bytes);
void sonic_stacks_patch(so_module *module);
#endif
