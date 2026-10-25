/* Game-specific JNI bridge for the supplied Sonic 4 Episode I revision.
 * Cached but unfinished callbacks stop explicitly if invoked.
 */
#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_Impl.h>
#include "sonic_java.h"
#include "utils/dialog.h"
#include "utils/logger.h"
#include "utils/glutil.h"
#include "sonic_graphics_policy.h"
#include "sonic_framebuffer.h"
#include "sonic_audio.h"
#include "sonic_media.h"
#include "sonic_input.h"
#include "sonic_perf.h"
#include "sonic_os.h"
#include "sonic_expansion.h"
#include "sonic_dialog.h"
#include <psp2/power.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/stat.h>
#include <string.h>
#include <stdatomic.h>

static void (*original_delete_global)(JNIEnv *, jobject);
static char clipboard[2048];
static SonicGraphicsState graphics_state;

static const SonicJavaDescriptor *descriptor(jmethodID id) {
    const SonicJavaDescriptor *d = sonic_java_by_id((int)(uintptr_t)id);
    if (!d || d->field) fatal_error("Sonic JNI: invalid method ID %p", id);
    return d;
}

static void unfinished(const SonicJavaDescriptor *d) {
    fatal_error("Sonic WIP: callback %s.%s%s is not implemented.",
                d->class_name, d->name, d->signature);
}

static jmethodID get_method(JNIEnv *env, jclass clazz, const char *name, const char *sig) {
    (void)env;
    const SonicJavaDescriptor *d = sonic_java_find((const char *)clazz, name, sig, 0);
    if (!d) fatal_error("Sonic JNI: unknown method %s.%s%s", clazz ? (char *)clazz : "NULL",
                        name ? name : "NULL", sig ? sig : "NULL");
    l_debug("JNI method cached: %s.%s%s", d->class_name, name, sig);
    return (jmethodID)(uintptr_t)d->id;
}

static jfieldID get_field(JNIEnv *env, jclass clazz, const char *name, const char *sig) {
    (void)env;
    const SonicJavaDescriptor *d = sonic_java_find((const char *)clazz, name, sig, 1);
    if (!d) fatal_error("Sonic JNI: unknown field %s.%s:%s", clazz ? (char *)clazz : "NULL",
                        name ? name : "NULL", sig ? sig : "NULL");
    l_debug("JNI field cached: %s.%s:%s", d->class_name, name, sig);
    return (jfieldID)(uintptr_t)d->id;
}

static jobject construct_object(jclass clazz, jmethodID method) {
    jobject obj = sonic_java_construct((const char *)clazz, (int)(uintptr_t)method);
    if (!obj) fatal_error("Sonic JNI: unsupported constructor class=%s method=%p",
                          clazz ? (const char *)clazz : "NULL", method);
    if (obj == SONIC_OBJECT_FLURRY) l_info("Constructed s3eFlurry bridge: analytics disabled");
    if (obj == SONIC_OBJECT_EXPANSION) l_info("Constructed s3eAPKExpansion bridge: installed OBB mode");
    if (obj == SONIC_OBJECT_DIALOG) l_info("Constructed s3eDialog bridge: user-selected modal dialogs");
    return obj;
}

static jobject new_object(JNIEnv *env, jclass clazz, jmethodID method, ...) {
    (void)env; return construct_object(clazz, method);
}
static jobject new_object_v(JNIEnv *env, jclass clazz, jmethodID method, va_list args) {
    (void)env; (void)args; return construct_object(clazz, method);
}
static jobject new_object_a(JNIEnv *env, jclass clazz, jmethodID method, const jvalue *args) {
    (void)env; (void)args; return construct_object(clazz, method);
}

static jclass object_class(JNIEnv *env, jobject obj) {
    (void)env;
    const char *name = NULL;
    if (obj == SONIC_OBJECT_THREAD) name = "com/ideaworks3d/marmalade/LoaderThread";
    if (obj == SONIC_OBJECT_VIEW) name = "com/ideaworks3d/marmalade/LoaderView";
    if (obj == SONIC_OBJECT_MEDIA) name = "com/ideaworks3d/marmalade/LoaderThread$MediaPlayerManager";
    if (obj == SONIC_OBJECT_KEYBOARD) name = "com/ideaworks3d/marmalade/LoaderKeyboard";
    if (obj == SONIC_OBJECT_FLURRY) name = "s3eFlurry";
    if (obj == SONIC_OBJECT_EXPANSION) name = "s3eAPKExpansion";
    if (obj == SONIC_OBJECT_DIALOG) name = "s3eDialog";
    // Return the manifest's exact static pointer, so delete_global can identify
    // ownership without relying on string-literal pooling across object files.
    if (name) {
        for (size_t i = 0; i < sonic_java_descriptor_count; ++i)
            if (!strcmp(name, sonic_java_descriptors[i].class_name))
                return (jclass)sonic_java_descriptors[i].class_name;
    }
    fatal_error("Sonic JNI: unknown Java object %p", obj);
}

static void delete_global(JNIEnv *env, jobject obj) {
    if (sonic_framebuffer_owns(obj)) return; // Shared surface storage stays alive for the process.
    if (obj == SONIC_OBJECT_THREAD || obj == SONIC_OBJECT_VIEW ||
        obj == SONIC_OBJECT_MEDIA || obj == SONIC_OBJECT_KEYBOARD || obj == SONIC_OBJECT_ACTIVITY ||
        obj == SONIC_OBJECT_SOUND || obj == SONIC_OBJECT_FLURRY || obj == SONIC_OBJECT_EXPANSION ||
        obj == SONIC_OBJECT_DIALOG) return;
    // GetObjectClass returns static strings; FindClass's allocated strings
    // retain the upstream cleanup behavior.
    for (size_t i = 0; i < sonic_java_descriptor_count; ++i)
        if (obj == (jobject)sonic_java_descriptors[i].class_name) return;
    original_delete_global(env, obj);
}

static void ensure_directory(const char *path) {
    SceIoStat st;
    if (sceIoGetstat(path, &st) >= 0) {
        if (!SCE_S_ISDIR(st.st_mode)) fatal_error("Sonic: path is not a directory: %s", path);
        return;
    }
    int result = sceIoMkdir(path, 0777);
    if (result < 0) fatal_error("Sonic: cannot create %s (0x%x)", path, result);
}

static jobject java_string(const char *text) {
    jstring result = (*jni).NewStringUTF(&jni, text);
    if (!result) fatal_error("Sonic: JNI string allocation failed");
    return result;
}

static jobject sonic_java_object(jmethodID id, va_list args) {
    (void)args;
    const SonicJavaDescriptor *d = descriptor(id);
    if (!strcmp(d->name, "s3eAPKExpansionGetAbsolutePath")) {
        l_info("APK expansion directory: raw://" DATA_PATH);
        return java_string("raw://" DATA_PATH);
    }
    if (!strcmp(d->name, "s3eAPKExpansionGetMainExpansionFilename"))
        return java_string(SONIC_EXPANSION_FILENAME);
    if (!strcmp(d->name, "getPrivateExternalDir")) {
        ensure_directory(DATA_PATH "saves"); return java_string(DATA_PATH "saves");
    }
    if (!strcmp(d->name, "getRstDir")) return java_string(DATA_PATH);
    if (!strcmp(d->name, "getCacheDir")) {
        ensure_directory(DATA_PATH "cache"); return java_string(DATA_PATH "cache");
    }
    if (!strcmp(d->name, "getTmpDir")) {
        ensure_directory(DATA_PATH "tmp"); return java_string(DATA_PATH "tmp");
    }
    if (!strcmp(d->name, "getDeviceModel")) return java_string("PS Vita");
    if (!strcmp(d->name, "getLocale")) return java_string("en_US");
    if (!strcmp(d->name, "getDeviceId")) return java_string("PSVITA-S4EP10001");
    if (!strcmp(d->name, "clipboardGet")) return java_string(clipboard);
    // Match Android's null result on a device without telephony.
    if (!strcmp(d->name, "getDeviceIMSI") || !strcmp(d->name, "getDeviceNumber")) return NULL;
    unfinished(d); return NULL;
}

static jint sonic_java_int(jmethodID id, va_list args) {
    (void)args;
    const SonicJavaDescriptor *d = descriptor(id);
    if (!strcmp(d->name, "audioPlay")) {
        jstring path = va_arg(args, jstring);
        int repeats = va_arg(args, int);
        jlong offset = va_arg(args, jlong), length = va_arg(args, jlong);
        int channel = va_arg(args, int);
        const char *text = path ? (*jni).GetStringUTFChars(&jni, path, NULL) : NULL;
        l_info("MediaPlayer audioPlay: path=%s repeats=%d offset=%lld length=%lld channel=%d",
                text ? text : "NULL", repeats, (long long)offset, (long long)length, channel);
        sonic_input_media_track(text, offset, length, channel);
        sonic_perf_track(offset, length, channel);
        int result = sonic_media_play(text, repeats, offset, length, channel);
        // This FalsoJNI revision's release callback lacks JNI's const qualifier.
        if (text) (*jni).ReleaseStringUTFChars(&jni, path, (char *)text);
        return result;
    }
    if (!strcmp(d->name, "audioPause")) return sonic_media_pause(va_arg(args, int));
    if (!strcmp(d->name, "audioResume")) return sonic_media_resume(va_arg(args, int));
    if (!strcmp(d->name, "audioGetStatus")) return sonic_media_status(va_arg(args, int));
    if (!strcmp(d->name, "audioGetPosition")) return sonic_media_position(va_arg(args, int));
    if (!strcmp(d->name, "audioGetDuration")) return sonic_media_duration(va_arg(args, int));
    if (!strcmp(d->name, "recordAvailable")) return 0; // No recording backend.
    if (!strcmp(d->name, "recordStop")) return 1; // Original: no recorder object.
    if (!strcmp(d->name, "s3eAPKExpansionGetDownloadState"))
        return sonic_expansion_download_state();
    // LoaderView begins with state 0 and no video view. videoPlay remains
    // unfinished and fatal, so these paths can only observe the idle state.
    if (!strcmp(d->name, "videoGetStatus") || !strcmp(d->name, "videoGetPosition")) return 0;
    if (!strcmp(d->name, "videoPause") || !strcmp(d->name, "videoResume")) return -1;
    if (!strcmp(d->name, "audioGetNumChannels")) {
        // Original MediaPlayerManager returns 16 logical stream slots. This
        // query is independent of the PCM mono/stereo format and the number
        // of BGM ports the device can allocate concurrently.
        l_info("MediaPlayerManager channel-count query: 16 logical slots");
        return 16;
    }
    if (!strcmp(d->name, "soundInit")) {
        int volume = va_arg(args, int), stereo = va_arg(args, int), rate = va_arg(args, int);
        l_info("Marmalade soundInit: volume=%d stereo=%d rate=%d", volume, stereo, rate);
        return sonic_audio_init(volume, stereo, rate);
    }
    if (!strcmp(d->name, "getBatteryLevel")) return scePowerGetBatteryLifePercent();
    // No Android-style physical keyboard is exposed by this bridge yet.
    if (!strcmp(d->name, "getKeyboardInfo")) return 0;
    if (!strcmp(d->name, "getOrientation")) return 0; // Vita's native display is landscape.
    if (!strcmp(d->name, "getNetworkType") || !strcmp(d->name, "getNetworkSubType")) return -1;
    if (!strcmp(d->name, "getDeviceDpi")) { (void)va_arg(args, int); return 220; }
    if (!strcmp(d->name, "showError")) {
        jstring title = va_arg(args, jstring), body = va_arg(args, jstring);
        int type = va_arg(args, int);
        const char *title_text = title ? (*jni).GetStringUTFChars(&jni, title, NULL) : "Marmalade error";
        const char *body_text = body ? (*jni).GetStringUTFChars(&jni, body, NULL) : "No message";
        l_error("Marmalade showError type=%d title=%s body=%s", type,
                title_text ? title_text : "NULL", body_text ? body_text : "NULL");
        // Capture the real engine assertion; do not silently press Continue.
        fatal_error("%s: %s", title_text ? title_text : "Marmalade", body_text ? body_text : "Error");
    }
    unfinished(d); return 0;
}

static jboolean sonic_java_boolean(jmethodID id, va_list args) {
    (void)args;
    const SonicJavaDescriptor *d = descriptor(id);
    if (!strcmp(d->name, "audioIsPlaying"))
        return sonic_media_status(va_arg(args, int)) == 1 ? JNI_TRUE : JNI_FALSE;
    if (!strcmp(d->name, "s3eDialogAlertBox")) return sonic_dialog_alert(args);
    if (!strcmp(d->name, "chargerIsConnected")) return scePowerIsPowerOnline() ? JNI_TRUE : JNI_FALSE;
    if (!strcmp(d->name, "vibrateAvailable")) return JNI_FALSE;
    if (!strcmp(d->name, "hasMultitouch")) return JNI_TRUE;
    if (!strcmp(d->name, "getSilentMode")) return JNI_FALSE; // No ringtone mute mode.
    if (!strcmp(d->name, "locationStart") || !strcmp(d->name, "locationStop") ||
        !strcmp(d->name, "locationGpsData") || !strcmp(d->name, "smsStart") ||
        !strcmp(d->name, "acquireMulticastLock") || !strcmp(d->name, "releaseMulticastLock")) return JNI_FALSE;
    if (!strcmp(d->name, "networkCheckStart") || !strcmp(d->name, "networkCheckStop")) return JNI_FALSE;
    unfinished(d); return JNI_FALSE;
}

static void sonic_java_void(jmethodID id, va_list args) {
    const SonicJavaDescriptor *d = descriptor(id);
    if (!strcmp(d->name, "accelStop") || !strcmp(d->name, "compassStop") ||
        !strcmp(d->name, "smsStop")) {
        // Matching Java null-listener/null-receiver cleanup. Sensor starts
        // still stop explicitly and SMS start returns false, so no active
        // Android subscription can exist. Do not emulate active sensors.
        l_info("%s: no active Android subscription", d->name);
        return;
    }
    if (!strcmp(d->name, "audioStop")) {
        sonic_media_stop(va_arg(args, int));
        return;
    }
    if (!strcmp(d->name, "audioSetPosition") || !strcmp(d->name, "audioSetVolume")) {
        int value = va_arg(args, int), channel = va_arg(args, int);
        if (!strcmp(d->name, "audioSetPosition")) sonic_media_seek(value, channel);
        else sonic_media_volume(value, channel);
        return;
    }
    if (!strcmp(d->name, "s3eAPKExpansionInitialize")) {
        (void)va_arg(args, jstring); // Android downloader key is unused for staged files.
        SceIoStat st = {0};
        int valid = sceIoGetstat(DATA_PATH SONIC_EXPANSION_FILENAME, &st) >= 0 && SCE_S_ISREG(st.st_mode);
        int state = sonic_expansion_initialize(valid, (uint64_t)st.st_size);
        l_info("APK expansion initialize: installed=%d bytes=%llu state=%d",
                valid, (unsigned long long)st.st_size, state);
        if (state == SONIC_EXPANSION_ERROR)
            l_error("APK expansion missing or wrong size: " DATA_PATH SONIC_EXPANSION_FILENAME);
        return;
    }
    if (!strcmp(d->name, "s3eAPKExpansionStart") || !strcmp(d->name, "s3eAPKExpansionStop")) {
        l_info("%s: no downloader connection in installed OBB mode", d->name);
        return;
    }
    if (!strcmp(d->class_name, "s3eFlurry")) {
        // Only exact declared descriptors can reach here. Analytics is
        // intentionally offline; never inspect/log keys or user/event data.
        static atomic_uint skipped;
        if (atomic_fetch_add(&skipped, 1) < 4)
            l_info("Flurry analytics disabled: %s", d->name);
        return;
    }
    if (!strcmp(d->name, "runOnOSSignal")) { sonic_os_signal(); return; }
    if (!strcmp(d->name, "videoStop")) {
        // Original Java performs no cleanup or native completion notification
        // when already idle. Revisit these handlers when videoPlay is added.
        l_info("videoStop: no active video; elapsed startup=%llu ms",
                (unsigned long long)(sceKernelGetProcessTimeWide() / 1000));
        return;
    }
    if (!strcmp(d->name, "videoSetVolume")) {
        int volume = va_arg(args, int);
        l_info("videoSetVolume(%d): no active video", volume);
        return; // Original VideoRunner ignores this when m_VideoView is null.
    }
    if (!strcmp(d->name, "soundStart")) { sonic_audio_start(); return; }
    if (!strcmp(d->name, "soundStop")) { sonic_audio_stop(); return; }
    if (!strcmp(d->name, "soundSetVolume")) { sonic_audio_volume(va_arg(args, int)); return; }
    if (!strcmp(d->name, "doDraw")) { sonic_audio_check(); sonic_media_pump(); sonic_input_poll(); sonic_framebuffer_draw(graphics_state.use_gl); return; }
    if (!strcmp(d->name, "glInit")) {
        int version = va_arg(args, int);
        // LoaderGL.startGL(0) retains/defaults its context version; Android's
        // null EGL context-attribute list defaults to GLES 1.
        if (version < 0 || version > 2) fatal_error("Sonic: unsupported GLES context %d", version);
        l_info("Marmalade requested GLES %d", version);
        SonicGraphicsAction action = sonic_graphics_event(&graphics_state, SONIC_GL_START);
        if (action == SONIC_GL_CREATE_CONTEXT) gl_init();
        return;
    }
    if (!strcmp(d->name, "glReInit")) {
        SonicGraphicsAction action = sonic_graphics_event(&graphics_state, SONIC_GL_RESTART);
        l_info("Marmalade glReInit: %s", action == SONIC_GL_NO_ACTION ? "GL not enabled; no action" : "retaining fixed Vita surface");
        if (action == SONIC_GL_CREATE_CONTEXT) gl_init();
        return;
    }
    if (!strcmp(d->name, "glTerm")) {
        sonic_graphics_event(&graphics_state, SONIC_GL_STOP);
        l_info("Marmalade glTerm: surface inactive; Vita context retained");
        return;
    }
    if (!strcmp(d->name, "glSwapBuffers")) {
        sonic_audio_check();
        sonic_media_pump();
        sonic_input_poll();
        if (!graphics_state.use_gl) return; // Match LoaderView's started() check.
        static unsigned frames;
        uint64_t begin = sonic_perf_begin();
        gl_swap(); ++frames;
        sonic_perf_end(begin);
        if (frames == 1 || frames == 30) l_info("Marmalade completed swap %u", frames);
        return;
    }
    if (!strcmp(d->name, "fixOrientation")) {
        int orientation = va_arg(args, int);
        if (orientation != 0 && orientation != 2 && orientation != 4)
            fatal_error("Sonic: portrait orientation request %d is not supported", orientation);
        l_info("Using Vita landscape display (S3E orientation request %d)", orientation); return;
    }
    if (!strcmp(d->name, "backlightOn")) { sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT); return; }
    if (!strcmp(d->name, "enableRespondingToRotation")) return; // Vita display is fixed.
    if (!strcmp(d->name, "clipboardSet")) {
        jstring value = va_arg(args, jstring);
        const char *text = value ? (*jni).GetStringUTFChars(&jni, value, NULL) : "";
        if (!text || strlen(text) >= sizeof(clipboard)) fatal_error("Sonic: clipboard string too large");
        strcpy(clipboard, text);
        if (value) (*jni).ReleaseStringUTFChars(&jni, value, (char *)text);
        return;
    }
    unfinished(d);
}

#include "sonic_java_dispatch.inc"

void sonic_java_install(struct JNINativeInterface *table) {
    original_delete_global = table->DeleteGlobalRef;
    table->GetMethodID = get_method;
    table->GetFieldID = get_field;
    table->GetObjectClass = object_class;
    table->NewObject = new_object;
    table->NewObjectV = new_object_v;
    table->NewObjectA = new_object_a;
    table->DeleteGlobalRef = delete_global;
}
