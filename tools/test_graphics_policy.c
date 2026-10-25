#include "sonic_graphics_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    SonicGraphicsState state = {0};
    assert(sonic_graphics_event(&state, SONIC_GL_RESTART) == SONIC_GL_NO_ACTION);
    assert(!state.use_gl && !state.context_live);
    assert(sonic_graphics_event(&state, SONIC_GL_START) == SONIC_GL_CREATE_CONTEXT);
    assert(state.use_gl && state.context_live);
    assert(sonic_graphics_event(&state, SONIC_GL_START) == SONIC_GL_KEEP_CONTEXT);
    assert(sonic_graphics_event(&state, SONIC_GL_RESTART) == SONIC_GL_KEEP_CONTEXT);
    assert(sonic_graphics_event(&state, SONIC_GL_STOP) == SONIC_GL_NO_ACTION);
    assert(!state.use_gl && state.context_live);
    assert(sonic_graphics_event(&state, SONIC_GL_RESTART) == SONIC_GL_NO_ACTION);
    assert(sonic_graphics_event(&state, SONIC_GL_START) == SONIC_GL_KEEP_CONTEXT);
    puts("Graphics lifecycle: early reinit is inert, context created once, fixed-surface restart retains context");
}
