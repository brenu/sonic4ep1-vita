#ifndef SONIC_JNI_REGISTRY_H
#define SONIC_JNI_REGISTRY_H
#include <falso_jni/jni.h>

/* FalsoJNI FindClass returns a class-name string in this pinned revision. */
jint sonic_register_natives(JNIEnv *env, jclass clazz,
                           const JNINativeMethod *methods, jint count);
void *sonic_find_native(const char *class_name, const char *name, const char *signature);
void sonic_reset_natives(void);
#endif
