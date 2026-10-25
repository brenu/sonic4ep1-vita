#include "utils/init.h"
#include "utils/glutil.h"
#include "utils/dialog.h"
#include "sonic_jni_registry.h"
#include "sonic_java.h"
#include "utils/logger.h"
#include "reimpl/mem.h"
#include "sonic_framebuffer.h"
#include "sonic_os.h"
#include "reimpl/io.h"
#include "sonic_tls.h"
#include "sonic_stacks.h"
#include "sonic_input.h"
#include "sonic_audio.h"
#include "sonic_media.h"
#include <errno.h>
#include <kubridge.h>
#include <string.h>
#include <psp2/io/stat.h>

#include <psp2/kernel/threadmgr.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

#ifndef NDK_PORT
#include "reimpl/controls.h"
#else
#include <falso_ndk/FalsoNDK.h>
#endif

// s3eMemoryHeapCreate uses valloc (ELF 0x6ec10 -> 0x70168), so its
// configured heaps share newlib's arena. Only executable code uses mmap.
// Reserve the configured ~208 MiB plus 64 MiB for host/runtime allocations.
int _newlib_heap_size_user = 272 * 1024 * 1024;
const SceUInt32 sceUserMainThreadStackSize = 8 * 1024 * 1024;

#ifdef USE_SCELIBC_IO
int sceLibcHeapSize = 4 * 1024 * 1024;
#endif

so_module so_mod;
static struct JNINativeInterface sonic_jni_table;

static void memory_checkpoint(void) {
    unsigned char *memory = mmap(NULL, 6 * 4096, 7, 0x21, -1, 0);
    if (memory == MAP_FAILED) fatal_error("Sonic memory checkpoint: mmap failed (errno %d)", errno);
    if (mprotect_soloader(memory, 4096, 0) || mprotect_soloader(memory + 5 * 4096, 4096, 0))
        fatal_error("Sonic memory checkpoint: guard-page setup failed (errno %d)", errno);
    volatile uint32_t *middle = (volatile uint32_t *)(memory + 4096);
    *middle = 0x534f4e49;
    // Execute real ARM instructions on two separate pages after guard setup
    // and an RX transition. API success/data retention alone did not detect
    // the probe 09 instruction-permission failure on another payload page.
    uint32_t *code = (uint32_t *)(memory + 2 * 4096);
    uint32_t *far_code = (uint32_t *)(memory + 4 * 4096);
    code[0] = 0xe3a0002a; code[1] = 0xe12fff1e; // mov r0,#42; bx lr
    far_code[0] = 0xe3a0002b; far_code[1] = 0xe12fff1e;
    if (mprotect_soloader(code, 4096, 5))
        fatal_error("Sonic memory checkpoint: executable RX setup failed");
    kuKernelFlushCaches(code, 4096);
    kuKernelFlushCaches(far_code, 4096);
    __sync_synchronize();
    l_info("Executable mmap checkpoint: calling RX page");
    if (((int (*)(void))code)() != 42)
        fatal_error("Sonic memory checkpoint: generated RX code returned wrong value");
    l_info("Executable mmap checkpoint: calling separate RWX page");
    if (((int (*)(void))far_code)() != 43)
        fatal_error("Sonic memory checkpoint: neighboring executable page returned wrong value");
    if (mprotect_soloader(memory + 4096, 4096, 5) || *middle != 0x534f4e49)
        fatal_error("Sonic memory checkpoint: read/execute transition failed");
    if (mprotect_soloader(memory + 4096, 4096, 3) || munmap(memory, 6 * 4096))
        fatal_error("Sonic memory checkpoint: restore/unmap failed (errno %d)", errno);
    // No deliberate guard fault; guard enforcement itself is not tested.
    l_success("Sonic memory checkpoint passed: RX/RWX code execution, data retention and unmap");
}

static int sonic_main(void) {
    sonic_tls_init();
    sonic_tls_checkpoint();
    soloader_init_all();
    memory_checkpoint();

    /* Marmalade uses RegisterNatives; this FalsoJNI revision otherwise ignores it. */
    sonic_reset_natives();
    sonic_jni_table = *jni;
    sonic_jni_table.RegisterNatives = sonic_register_natives;
    sonic_java_install(&sonic_jni_table);
    jni = &sonic_jni_table;
    jint (*on_load)(JavaVM *, void *) = (void *)so_symbol(&so_mod, "JNI_OnLoad");
    if (!on_load || on_load(&jvm, NULL) != JNI_VERSION_1_2) {
        fatal_error("Sonic WIP: Marmalade JNI_OnLoad failed.");
        return 1;
    }
    const char *loader_class = "com/ideaworks3d/marmalade/LoaderThread";
    if (!sonic_find_native(loader_class, "initNative", "()V") ||
        !sonic_find_native(loader_class, "setViewNative",
                          "(Lcom/ideaworks3d/marmalade/LoaderView;)V") ||
        !sonic_find_native(loader_class, "runNative",
                          "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V")) {
        fatal_error("Sonic WIP: expected Marmalade native methods were not registered.");
        return 1;
    }
    void (*set_art)(JNIEnv *, jobject, jboolean) = sonic_find_native(
        "com/ideaworks3d/marmalade/LoaderActivity", "setART", "(Z)V");
    void (*init_native)(JNIEnv *, jobject) = sonic_find_native(loader_class, "initNative", "()V");
    void (*set_view)(JNIEnv *, jobject, jobject) = sonic_find_native(loader_class, "setViewNative",
        "(Lcom/ideaworks3d/marmalade/LoaderView;)V");
    if (!set_art) fatal_error("Sonic WIP: setART was not registered");
    l_info("Sonic diagnostic stage: setART(false)");
    set_art(&jni, SONIC_OBJECT_ACTIVITY, JNI_FALSE);
    l_info("Sonic diagnostic stage: cache 80 Java methods and two fields");
    init_native(&jni, SONIC_OBJECT_THREAD);
    l_info("Sonic diagnostic stage: setViewNative");
    set_view(&jni, SONIC_OBJECT_THREAD, SONIC_OBJECT_VIEW);
    sonic_framebuffer_attach();
    l_success("Sonic diagnostic checkpoint passed: JNI initialization and view setup");
    SceIoStat apk_stat, obb_stat;
    if (sceIoGetstat(DATA_PATH "game.apk", &apk_stat) < 0 || apk_stat.st_size != 38851237)
        fatal_error("Sonic: game.apk missing or wrong size in " DATA_PATH);
    if (sceIoGetstat(DATA_PATH "main.6200011.com.sega.sonic4epi.obb", &obb_stat) < 0 ||
        obb_stat.st_size != 86710212)
        fatal_error("Sonic: original OBB missing or wrong size in " DATA_PATH);
    stat64_bionic android_obb;
    if (stat_soloader(DATA_PATH "main.6200011.com.sega.sonic4epi.obb", &android_obb) ||
        android_obb.st_size != 86710212)
        fatal_error("Sonic: Android stat bridge OBB-size checkpoint failed");
    l_success("Android stat checkpoint: OBB size=%lld bytes at offset 48", (long long)android_obb.st_size);
    char root[] = DATA_PATH;
    if (root[strlen(root) - 1] == '/') root[strlen(root) - 1] = 0;
    jstring file_root = (*jni).NewStringUTF(&jni, root);
    jstring package_path = (*jni).NewStringUTF(&jni, DATA_PATH "game.apk");
    jstring library_dir = (*jni).NewStringUTF(&jni, root);
    if (!file_root || !package_path || !library_dir) fatal_error("Sonic: startup string allocation failed");
    void (*run_native)(JNIEnv *, jobject, jstring, jstring, jstring) = sonic_find_native(loader_class,
        "runNative", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    l_info("Entering runNative: root=%s apk=" DATA_PATH "game.apk nativeLibraries=%s", root, root);
    sonic_os_init();
    sonic_input_init();
    run_native(&jni, SONIC_OBJECT_THREAD, file_root, package_path, library_dir);
    l_info("Marmalade runNative returned: beginning normal application shutdown");
    sonic_input_shutdown();
    sonic_media_shutdown();
    int os_released = sonic_os_shutdown();
    int pcm_released = sonic_audio_shutdown();
    gl_shutdown();
    l_info("Sonic shutdown complete: OS=%d PCM=%d; returning exit status 0",
            os_released, pcm_released);
    return 0;

}

int main(void) {
    sonic_log_start();
    l_info("Sonic diagnostic build starting; shared heap 272 MiB");
    int result = sonic_stack_scope(sonic_main);
    sonic_log_finish();
    return result;
}

#ifndef NDK_PORT
void controls_handler_key(int32_t keycode, ControlsAction action) {
    // Call into the .so here
}

void controls_handler_touch(int32_t id, float x, float y, ControlsAction action) {
    // Call into the .so here
}

void controls_handler_analog(ControlsStickId which, float x, float y, ControlsAction action) {
    // Call into the .so here
}
#endif
