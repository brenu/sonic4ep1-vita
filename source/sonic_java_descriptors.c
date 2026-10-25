#include "sonic_java.h"
#include <string.h>
#include "sonic_java_descriptors.inc"

const SonicJavaDescriptor *sonic_java_find(const char *class_name, const char *name,
                                         const char *signature, int field) {
    if (!class_name || !name || !signature) return NULL;
    for (size_t i = 0; i < sonic_java_descriptor_count; ++i) {
        const SonicJavaDescriptor *d = &sonic_java_descriptors[i];
        if (d->field == field && !strcmp(d->class_name, class_name) &&
            !strcmp(d->name, name) && !strcmp(d->signature, signature)) return d;
    }
    return NULL;
}

const SonicJavaDescriptor *sonic_java_by_id(int id) {
    for (size_t i = 0; i < sonic_java_descriptor_count; ++i)
        if (sonic_java_descriptors[i].id == id) return &sonic_java_descriptors[i];
    return NULL;
}

jobject sonic_java_construct(const char *class_name, int method_id) {
    const SonicJavaDescriptor *d = sonic_java_by_id(method_id);
    if (!class_name || !d || d->field || strcmp(class_name, d->class_name) ||
        strcmp(d->name, "<init>") || strcmp(d->signature, "()V")) return NULL;
    if (!strcmp(class_name, "s3eFlurry")) return SONIC_OBJECT_FLURRY;
    // Expansion's instance downloader/UI fields remain null for installed
    // files. Its static download state is independent of object construction.
    if (!strcmp(class_name, "s3eAPKExpansion")) return SONIC_OBJECT_EXPANSION;
    if (!strcmp(class_name, "s3eDialog")) return SONIC_OBJECT_DIALOG;
    return NULL;
}
