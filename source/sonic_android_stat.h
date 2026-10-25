#ifndef SONIC_ANDROID_STAT_H
#define SONIC_ANDROID_STAT_H
#include <stdint.h>
#include <stddef.h>
typedef struct { int32_t tv_sec; int32_t tv_nsec; } SonicAndroidTimespec;
// ARM EABI stat layout verified against Marmalade stat consumers, notably
// ldrd [sp,#48] for size and ldr [sp,#80] for mtime. Do not use host longs.
typedef struct __attribute__((packed)) {
    uint64_t st_dev;
    uint8_t __pad0[4];
    uint32_t __st_ino, st_mode, st_nlink, st_uid, st_gid;
    uint64_t st_rdev;
    uint8_t __pad3[8];
    int64_t st_size;
    uint32_t st_blksize, __pad4;
    uint64_t st_blocks;
    SonicAndroidTimespec st_atim, st_mtim, st_ctim;
    uint64_t st_ino;
} stat64_bionic;
_Static_assert(sizeof(stat64_bionic) == 104, "Android ARM stat size");
_Static_assert(offsetof(stat64_bionic, st_mode) == 16, "Android mode offset");
_Static_assert(offsetof(stat64_bionic, st_size) == 48, "Android size offset");
_Static_assert(offsetof(stat64_bionic, st_blocks) == 64, "Android block count offset");
_Static_assert(offsetof(stat64_bionic, st_mtim) == 80, "Android mtime offset");
#endif
