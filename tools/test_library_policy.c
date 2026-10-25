#include "sonic_dynlink.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    assert(sonic_library_kind(NULL) == 0);
    assert(sonic_library_kind("libEGL.so") == 0);
    assert(sonic_library_kind("/system/lib/libGLESv1_CM.so") == 0);
    assert(sonic_library_kind("libs3e_android.so") == 1);
    assert(sonic_library_kind("ux0:data/sonic4ep1/libs3eDialog.so") == 2);
    assert(sonic_library_kind("libs3eFlurry.so") == 3);
    assert(sonic_library_kind("libs3eAPKExpansion.so") == 4);
    assert(sonic_library_kind("libs3eAPKExpansion.so.bak") == -1);
    assert(sonic_library_kind("libOpenSLES.so") == -1);
    assert(sonic_library_kind("") == -1);
    puts("Dynamic library policy: supported basenames and unknown libraries checked");
}
