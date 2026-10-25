#include "sonic_jni_registry.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    int first, second, third;
    JNINativeMethod methods[] = {{"run", "()V", &first}, {"run", "(I)V", &second}};
    sonic_reset_natives();
    assert(sonic_register_natives(NULL, (jclass)"ClassA", methods, 2) == JNI_OK);
    assert(sonic_find_native("ClassA", "run", "()V") == &first);
    assert(sonic_find_native("ClassA", "run", "(I)V") == &second);
    assert(sonic_find_native("ClassB", "run", "()V") == NULL);
    JNINativeMethod other = {"run", "()V", &third};
    assert(sonic_register_natives(NULL, (jclass)"ClassB", &other, 1) == JNI_OK);
    assert(sonic_find_native("ClassB", "run", "()V") == &third);
    assert(sonic_register_natives(NULL, (jclass)"ClassA", methods, 2) == JNI_ERR);
    JNINativeMethod bad[] = {{"valid", "()V", &first}, {"invalid", "()V", NULL}};
    assert(sonic_register_natives(NULL, (jclass)"ClassA", bad, 2) == JNI_ERR);
    assert(sonic_find_native("ClassA", "valid", "()V") == NULL);
    assert(sonic_register_natives(NULL, NULL, methods, 2) == JNI_ERR);
    assert(sonic_register_natives(NULL, (jclass)"ClassA", methods, -1) == JNI_ERR);
    assert(sonic_register_natives(NULL, (jclass)"ClassA", methods, 129) == JNI_ERR);
    sonic_reset_natives();
    assert(sonic_find_native("ClassA", "run", "()V") == NULL);
    puts("JNI registry: passed class/signature dispatch, invalid batches and reset checks");
}
