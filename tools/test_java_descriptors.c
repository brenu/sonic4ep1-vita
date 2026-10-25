#include "sonic_java.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    assert(sonic_java_descriptor_count == 105);
    for (size_t i = 0; i < sonic_java_descriptor_count; ++i) {
        const SonicJavaDescriptor *d = &sonic_java_descriptors[i];
        assert(sonic_java_by_id(d->id) == d);
        assert(sonic_java_find(d->class_name, d->name, d->signature, d->field) == d);
        assert(!sonic_java_find("wrong/class", d->name, d->signature, d->field));
        assert(!sonic_java_find(d->class_name, d->name, "wrong/signature", d->field));
        assert(!sonic_java_find(d->class_name, d->name, d->signature, !d->field));
        for (size_t k = 0; k < i; ++k) assert(d->id != sonic_java_descriptors[k].id);
    }
    assert(!sonic_java_by_id(0));
    assert(!sonic_java_by_id(-1));
    assert(!sonic_java_find(NULL, "x", "()V", 0));
    const SonicJavaDescriptor *constructor = sonic_java_find("s3eFlurry", "<init>", "()V", 0);
    assert(constructor && constructor->id == 2001);
    assert(sonic_java_construct("s3eFlurry", constructor->id) == SONIC_OBJECT_FLURRY);
    assert(!sonic_java_construct("wrong/class", constructor->id));
    assert(!sonic_java_construct(NULL, constructor->id));
    assert(!sonic_java_construct("s3eFlurry", 0));
    const SonicJavaDescriptor *ordinary = sonic_java_find("s3eFlurry", "s3eFlurryEndSession", "()V", 0);
    assert(ordinary && !sonic_java_construct("s3eFlurry", ordinary->id));
    assert(!sonic_java_find("s3eFlurry", "<init>", "(I)V", 0));
    assert(!sonic_java_find("s3eFlurry", "unknownAnalyticsCall", "()V", 0));
    const SonicJavaDescriptor *expansion = sonic_java_find("s3eAPKExpansion", "<init>", "()V", 0);
    assert(expansion && expansion->id == 2015);
    assert(sonic_java_construct("s3eAPKExpansion", expansion->id) == SONIC_OBJECT_EXPANSION);
    assert(!sonic_java_construct("s3eFlurry", expansion->id));
    assert(!sonic_java_construct("s3eAPKExpansion", constructor->id));
    assert(!sonic_java_find("s3eAPKExpansion", "<init>", "(I)V", 0));
    const SonicJavaDescriptor *dialog = sonic_java_find("s3eDialog", "<init>", "()V", 0);
    assert(dialog && dialog->id == 2022);
    assert(sonic_java_construct("s3eDialog", dialog->id) == SONIC_OBJECT_DIALOG);
    assert(!sonic_java_construct("s3eFlurry", dialog->id));
    assert(!sonic_java_construct("s3eDialog", expansion->id));
    for (int id = 1; id <= 80; ++id)
        assert(strcmp(sonic_java_by_id(id)->class_name, "s3eFlurry"));
    puts("JNI descriptors: 105 unique descriptors; three constructor classes isolated and startup IDs stable");
}
