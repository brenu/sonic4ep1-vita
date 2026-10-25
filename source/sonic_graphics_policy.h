#ifndef SONIC_GRAPHICS_POLICY_H
#define SONIC_GRAPHICS_POLICY_H

typedef struct { int use_gl, context_live; } SonicGraphicsState;
typedef enum { SONIC_GL_START, SONIC_GL_RESTART, SONIC_GL_STOP } SonicGraphicsEvent;
typedef enum { SONIC_GL_NO_ACTION, SONIC_GL_CREATE_CONTEXT, SONIC_GL_KEEP_CONTEXT } SonicGraphicsAction;

/* LoaderThread.glReInit does nothing before m_UseGL becomes true. Vita's
 * fixed display keeps its context across surface-only stop/restart events.
 * Suspend/resume and true context loss still require separate implementation.
 */
static inline SonicGraphicsAction sonic_graphics_event(SonicGraphicsState *state, SonicGraphicsEvent event) {
    if (event == SONIC_GL_STOP) {
        state->use_gl = 0;
        return SONIC_GL_NO_ACTION;
    }
    if (event == SONIC_GL_RESTART && !state->use_gl) return SONIC_GL_NO_ACTION;
    state->use_gl = 1;
    if (state->context_live) return SONIC_GL_KEEP_CONTEXT;
    state->context_live = 1;
    return SONIC_GL_CREATE_CONTEXT;
}
#endif
