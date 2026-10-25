#include "sonic_input.h"
#include "sonic_touch_policy.h"
#include "sonic_pad_policy.h"
#include "sonic_title_policy.h"
#include "sonic_jni_registry.h"
#include "sonic_java.h"
#include "utils/logger.h"
#include "utils/dialog.h"
#include <falso_jni/FalsoJNI.h>
#include <psp2/touch.h>
#include <psp2/ctrl.h>
#include <stdint.h>
#include <stdatomic.h>
#include <string.h>

static void (*motion)(JNIEnv *, jobject, jint, jint, jint, jint);
static jboolean (*key)(JNIEnv *, jobject, jint, jint, jint);
static SceTouchPanelInfo panel;
static SonicTouchState touch_state;
static SonicTitlePad title_pad;
static atomic_int title_screen;
static uint32_t previous_buttons;
static int stick_x, stick_y;
static int ready, polling;

void sonic_input_media_track(const char *path, int64_t offset, int64_t length, int channel) {
    if (channel != 0) return;
    int title = path && !strcmp(path, DATA_PATH "game.apk") &&
                offset == 9698184 && length == 259971;
    atomic_store(&title_screen, title);
    if (title) l_info("Title input: any gameplay button can tap to start");
}

void sonic_input_init(void) {
    motion = sonic_find_native("com/ideaworks3d/marmalade/LoaderThread",
                              "onMotionEvent", "(IIII)V");
    key = sonic_find_native("com/ideaworks3d/marmalade/LoaderKeyboard",
                           "onKeyEventNative", "(III)Z");
    if (!motion || !key) fatal_error("Sonic: native input registration missing");
    if (sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &panel) < 0 ||
        panel.maxDispX <= panel.minDispX || panel.maxDispY <= panel.minDispY ||
        sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START) < 0)
        fatal_error("Sonic: front touchscreen initialization failed");
    sonic_touch_reset(&touch_state);
    if (sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG) < 0)
        fatal_error("Sonic: analog controller sampling initialization failed");
    ready = 1;
    l_info("Sonic input ready: front panel [%d,%d]-[%d,%d] -> 960x544, six touch slots",
           panel.minDispX, panel.minDispY, panel.maxDispX, panel.maxDispY);
    l_info("Sonic controls: Dpad/left stick=direction, Cross=select/jump, Start/Circle=pause/back");
}

void sonic_input_poll(void) {
    if (!ready || polling) return;
    polling = 1;
    SceCtrlData pad = {0};
    int pad_valid = sceCtrlPeekBufferPositive(0, &pad, 1) > 0;
    const uint32_t start_mask = SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT |
        SCE_CTRL_CROSS | SCE_CTRL_CIRCLE | SCE_CTRL_SQUARE | SCE_CTRL_TRIANGLE |
        SCE_CTRL_START | SCE_CTRL_SELECT | SCE_CTRL_LTRIGGER | SCE_CTRL_RTRIGGER;
    SceTouchData sample = {0};
    int result = sceTouchPeek(SCE_TOUCH_PORT_FRONT, &sample, 1);
    int valid_touch = result > 0 && sample.reportNum <= SONIC_TOUCH_SLOTS;
    int in_title = atomic_load(&title_screen);
    SonicTitleFrame title = sonic_title_step(&title_pad, in_title,
        pad_valid ? pad.buttons : title_pad.previous, start_mask, valid_touch,
        valid_touch && sample.reportNum < SONIC_TOUCH_SLOTS);
    if (title.triggered) l_info("Title input: button press queued one center-screen tap");
    if (valid_touch) {
        SonicTouchPoint points[SONIC_TOUCH_SLOTS];
        SonicTouchEvent events[SONIC_TOUCH_SLOTS * 2];
        for (unsigned i = 0; i < sample.reportNum; ++i) {
            points[i] = (SonicTouchPoint){
                sample.report[i].id,
                sonic_touch_scale(sample.report[i].x, panel.minDispX, panel.maxDispX, 960),
                sonic_touch_scale(sample.report[i].y, panel.minDispY, panel.maxDispY, 544)
            };
        }
        unsigned report_count = sample.reportNum;
        if (title.touch_down)
            points[report_count++] = (SonicTouchPoint){256, 480, 272};
        int count = sonic_touch_update(&touch_state, points, report_count, events);
        static unsigned logged;
        for (int i = 0; i < count; ++i) {
            SonicTouchEvent *event = &events[i];
            if (event->action != 6 && logged++ < 16)
                l_info("Sonic touch: id=%d action=%d x=%d y=%d",
                        event->id, event->action, event->x, event->y);
            motion(&jni, SONIC_OBJECT_THREAD, event->id, event->action, event->x, event->y);
        }
    }
    if (pad_valid) {
        static const struct { uint32_t button; int code, unicode; } mapping[] = {
            {SCE_CTRL_UP,19,0}, {SCE_CTRL_DOWN,20,0},
            {SCE_CTRL_LEFT,21,0}, {SCE_CTRL_RIGHT,22,0},
            {SCE_CTRL_CROSS,23,0}, {SCE_CTRL_CIRCLE | SCE_CTRL_START,4,0}
        };
        int x = sonic_pad_axis(pad.lx, stick_x);
        int y = sonic_pad_axis(pad.ly, stick_y);
        static unsigned stick_logs;
        if ((x != stick_x || y != stick_y) && stick_logs++ < 16)
            l_info("Sonic left stick: raw=%u,%u direction=%d,%d", pad.lx, pad.ly, x, y);
        stick_x = x; stick_y = y;
        uint32_t effective = sonic_pad_merge(title.game_buttons, x, y,
            SCE_CTRL_LEFT, SCE_CTRL_RIGHT, SCE_CTRL_UP, SCE_CTRL_DOWN);
        if (title.block_controls) effective = 0;
        for (unsigned i = 0; i < sizeof(mapping)/sizeof(mapping[0]); ++i) {
            int down = sonic_pad_binding_edge(previous_buttons, effective, mapping[i].button);
            if (down < 0) continue;
            jboolean accepted = key(&jni, SONIC_OBJECT_KEYBOARD, mapping[i].code,
                                   down ? mapping[i].unicode : 0, down);
            static unsigned pause_logs;
            if (mapping[i].code == 4 && pause_logs++ < 8)
                l_info("Sonic pause/back: down=%d buttons=0x%x accepted=%d",
                        down, (unsigned)((down ? effective : previous_buttons) & mapping[i].button), accepted);
        }
        previous_buttons = effective;
    }
    polling = 0;
}

void sonic_input_shutdown(void) {
    if (!ready) return;
    ready = 0;
    sonic_touch_reset(&touch_state);
    title_pad = (SonicTitlePad){0};
    previous_buttons = 0;
    atomic_store(&title_screen, 0);
    int result = sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_STOP);
    l_info("Sonic input shutdown: front sampling stop=0x%x", result);
}
