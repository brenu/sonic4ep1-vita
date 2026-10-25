#include "sonic_dialog.h"
#include "sonic_dialog_policy.h"
#include "sonic_jni_registry.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include <falso_jni/FalsoJNI.h>
#include <psp2/message_dialog.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static atomic_flag active = ATOMIC_FLAG_INIT;

jboolean sonic_dialog_alert(va_list args) {
    jstring strings[5];
    const char *text[5] = {0};
    for (unsigned i = 0; i < 5; ++i) strings[i] = va_arg(args, jstring);
    int id = va_arg(args, int);
    if (atomic_flag_test_and_set(&active)) {
        l_error("s3eDialog: another dialog is already active"); return JNI_FALSE;
    }
    jboolean success = JNI_FALSE;
    for (unsigned i = 0; i < 5; ++i) {
        if (strings[i]) {
            text[i] = (*jni).GetStringUTFChars(&jni, strings[i], NULL);
            if (!text[i]) goto cleanup;
        }
    }
    l_info("s3eDialog id=%d title=%s message=%s buttons=[%s][%s][%s]", id,
            text[1] ? text[1] : "", text[0] ? text[0] : "",
            text[2] ? text[2] : "", text[3] ? text[3] : "", text[4] ? text[4] : "");
    void (*callback)(JNIEnv *, jclass, jint, jint) = sonic_find_native(
        "s3eDialog", "native_dialogCallback", "(II)V");
    if (!callback || !text[2]) {
        l_error("s3eDialog: missing native callback or primary button"); goto cleanup;
    }
    int count = sonic_dialog_button_count(text[3] != NULL, text[4] != NULL);
    char message[SCE_MSG_DIALOG_USER_MSG_SIZE];
    // Vita's one/two-button dialogs have system labels. Show the game's
    // corresponding action beside each label so the user's choice is clear.
    if (count == 1) snprintf(message, sizeof(message), "%s\n%s\n\nOK: %s",
                            text[1] ? text[1] : "", text[0] ? text[0] : "", text[2]);
    else if (count == 2) snprintf(message, sizeof(message), "%s\n%s\n\nYes: %s\nNo: %s",
                                  text[1] ? text[1] : "", text[0] ? text[0] : "", text[2], text[3]);
    else snprintf(message, sizeof(message), "%s\n%s", text[1] ? text[1] : "", text[0] ? text[0] : "");
    SceMsgDialogButtonsParam buttons = {0};
    buttons.msg1 = text[2]; buttons.msg2 = text[3]; buttons.msg3 = text[4];
    SceMsgDialogUserMessageParam user = {0};
    user.msg = message;
    user.buttonType = count == 1 ? SCE_MSG_DIALOG_BUTTON_TYPE_OK :
                      count == 2 ? SCE_MSG_DIALOG_BUTTON_TYPE_YESNO : SCE_MSG_DIALOG_BUTTON_TYPE_3BUTTONS;
    if (count == 3) user.buttonParam = &buttons;
    SceMsgDialogParam param;
    sceMsgDialogParamInit(&param); param.mode = SCE_MSG_DIALOG_MODE_USER_MSG; param.userMsgParam = &user;
    gl_init();
    int result = sceMsgDialogInit(&param);
    if (result < 0) { l_error("s3eDialog init failed: 0x%x", (unsigned)result); goto cleanup; }
    GLfloat clear[4]; GLboolean mask[4];
    GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clear); glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    glDisable(GL_SCISSOR_TEST); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); glClearColor(0, 0, 0, 1);
    while (sceMsgDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_RUNNING) {
        glClear(GL_COLOR_BUFFER_BIT); vglSwapBuffers(GL_TRUE);
    }
    SceMsgDialogResult selected = {0};
    result = sceMsgDialogGetResult(&selected);
    sceMsgDialogTerm();
    glClearColor(clear[0], clear[1], clear[2], clear[3]); glColorMask(mask[0], mask[1], mask[2], mask[3]);
    if (scissor) glEnable(GL_SCISSOR_TEST);
    if (result < 0) { l_error("s3eDialog result failed: 0x%x", (unsigned)result); goto cleanup; }
    int choice = sonic_dialog_button_index(count, selected.buttonId);
    l_info("s3eDialog user selection: id=%d button=%d", id, choice);
    callback(&jni, (jclass)"s3eDialog", choice, id);
    success = JNI_TRUE;
cleanup:
    for (unsigned i = 0; i < 5; ++i)
        if (text[i]) (*jni).ReleaseStringUTFChars(&jni, strings[i], (char *)text[i]);
    atomic_flag_clear(&active);
    return success;
}
