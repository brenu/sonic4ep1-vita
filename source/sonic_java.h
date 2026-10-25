#ifndef SONIC_JAVA_H
#define SONIC_JAVA_H
#include <stddef.h>
#include <stdint.h>
#include <falso_jni/jni.h>
#define SONIC_OBJECT_THREAD ((jobject)(uintptr_t)0x534f0001)
#define SONIC_OBJECT_VIEW ((jobject)(uintptr_t)0x534f0002)
#define SONIC_OBJECT_MEDIA ((jobject)(uintptr_t)0x534f0003)
#define SONIC_OBJECT_KEYBOARD ((jobject)(uintptr_t)0x534f0004)
#define SONIC_OBJECT_ACTIVITY ((jobject)(uintptr_t)0x534f0005)
#define SONIC_OBJECT_SOUND ((jobject)(uintptr_t)0x534f0006)
#define SONIC_OBJECT_FLURRY ((jobject)(uintptr_t)0x534f0007)
#define SONIC_OBJECT_EXPANSION ((jobject)(uintptr_t)0x534f0008)
#define SONIC_OBJECT_DIALOG ((jobject)(uintptr_t)0x534f0009)

typedef struct {
    int id;
    const char *class_name, *name, *signature;
    int field;
} SonicJavaDescriptor;
extern const SonicJavaDescriptor sonic_java_descriptors[];
extern const size_t sonic_java_descriptor_count;
const SonicJavaDescriptor *sonic_java_find(const char *class_name, const char *name,
                                         const char *signature, int field);
const SonicJavaDescriptor *sonic_java_by_id(int id);
jobject sonic_java_construct(const char *class_name, int method_id);
void sonic_java_install(struct JNINativeInterface *table);
#endif
