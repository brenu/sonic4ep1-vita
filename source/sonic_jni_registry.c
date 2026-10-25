#include "sonic_jni_registry.h"
#include <stddef.h>
#include <string.h>

#define MAX_NATIVES 128
typedef struct {
    char class_name[128], name[80], signature[256];
    void *function;
} NativeEntry;
static NativeEntry entries[MAX_NATIVES];
static size_t used;

void sonic_reset_natives(void) {
    memset(entries, 0, sizeof(entries));
    used = 0;
}

void *sonic_find_native(const char *class_name, const char *name, const char *signature) {
    if (!class_name || !name || !signature) return NULL;
    for (size_t i = 0; i < used; ++i)
        if (!strcmp(entries[i].class_name, class_name) && !strcmp(entries[i].name, name)
                && !strcmp(entries[i].signature, signature))
            return entries[i].function;
    return NULL;
}

jint sonic_register_natives(JNIEnv *env, jclass clazz,
                           const JNINativeMethod *methods, jint count) {
    (void)env;
    const char *class_name = (const char *)clazz;
    if (!class_name || count < 0 || (count && !methods)
            || (size_t)count > MAX_NATIVES - used
            || strlen(class_name) >= sizeof(entries[0].class_name)) return JNI_ERR;
    /* Validate the entire batch before committing any entries. */
    for (jint i = 0; i < count; ++i) {
        if (!methods[i].name || !methods[i].signature || !methods[i].fnPtr
                || strlen(methods[i].name) >= sizeof(entries[0].name)
                || strlen(methods[i].signature) >= sizeof(entries[0].signature)
                || sonic_find_native(class_name, methods[i].name, methods[i].signature)) return JNI_ERR;
        for (jint k = 0; k < i; ++k)
            if (!strcmp(methods[k].name, methods[i].name)
                    && !strcmp(methods[k].signature, methods[i].signature)) return JNI_ERR;
    }
    for (jint i = 0; i < count; ++i) {
        NativeEntry *entry = &entries[used++];
        strcpy(entry->class_name, class_name);
        strcpy(entry->name, methods[i].name);
        strcpy(entry->signature, methods[i].signature);
        entry->function = methods[i].fnPtr;
    }
    return JNI_OK;
}
