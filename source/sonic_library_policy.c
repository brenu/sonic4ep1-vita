#include "sonic_dynlink.h"
#include <string.h>

int sonic_library_kind(const char *path) {
    if (!path) return 0;
    const char *leaf = strrchr(path, '/');
    leaf = leaf ? leaf + 1 : path;
    static const char *platform[] = {"libGLESv1_CM.so", "libGLESv2.so", "libEGL.so",
        "libc.so", "libm.so", "libz.so", "liblog.so", "libdl.so"};
    for (unsigned i = 0; i < sizeof(platform) / sizeof(platform[0]); ++i)
        if (!strcmp(leaf, platform[i])) return 0;
    if (!strcmp(leaf, "libs3e_android.so")) return 1;
    if (!strcmp(leaf, "libs3eDialog.so")) return 2;
    if (!strcmp(leaf, "libs3eFlurry.so")) return 3;
    if (!strcmp(leaf, "libs3eAPKExpansion.so")) return 4;
    return -1;
}
