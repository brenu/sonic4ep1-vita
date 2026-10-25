#ifndef SONIC_PLATFORM_H
#define SONIC_PLATFORM_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
    uint32_t type, bsize;
    uint64_t blocks, bfree, bavail, files, ffree;
    uint32_t fsid[2], namelen, frsize, flags, spare[4];
} SonicAndroidStatfs;
typedef struct {
    char sysname[65], nodename[65], release[65], version[65], machine[65], domainname[65];
} SonicAndroidUtsname;
_Static_assert(sizeof(SonicAndroidStatfs) == 88, "Android ARM statfs size");
_Static_assert(offsetof(SonicAndroidStatfs, blocks) == 8, "Android statfs blocks");
_Static_assert(offsetof(SonicAndroidStatfs, bavail) == 24, "Android statfs available");
_Static_assert(offsetof(SonicAndroidStatfs, frsize) == 60, "Android statfs fragment size");
_Static_assert(sizeof(SonicAndroidUtsname) == 390, "Android utsname size");
_Static_assert(offsetof(SonicAndroidUtsname, machine) == 260, "Android utsname machine");
void sonic_statfs_data(SonicAndroidStatfs *out, uint64_t total, uint64_t available, uint32_t cluster);
int sonic_statfs(const char *path, SonicAndroidStatfs *out);
int sonic_uname(SonicAndroidUtsname *out);
int sonic_nice(int increment);
#endif
