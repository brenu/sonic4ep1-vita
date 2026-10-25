#ifndef SONIC_TITLE_POLICY_H
#define SONIC_TITLE_POLICY_H
#include <stdint.h>

typedef struct { uint32_t previous, blocked; int pulse; } SonicTitlePad;
typedef struct { uint32_t game_buttons; int touch_down, triggered, block_controls; } SonicTitleFrame;

/* pulse 2 waits to press; pulse 1 owes an UP, even if title context ended.
 * game_buttons is computed before advancing the pulse, suppressing controls
 * through the release frame. Held trigger buttons stay consumed until up. */
static inline SonicTitleFrame sonic_title_step(SonicTitlePad *state, int title,
        uint32_t physical, uint32_t mask, int valid_touch, int room) {
    SonicTitleFrame frame = {0};
    uint32_t pressed = physical & ~state->previous & mask;
    state->previous = physical;
    state->blocked &= physical;
    if (!title && state->pulse == 2) state->pulse = 0;
    if (title && pressed && !state->pulse) {
        state->pulse = 2;
        frame.triggered = 1;
    }
    if (title || state->pulse) state->blocked |= physical & mask;
    frame.block_controls = title || state->pulse;
    frame.game_buttons = frame.block_controls ? 0 : physical & ~state->blocked;
    if (valid_touch) {
        if (state->pulse == 2 && room) {
            frame.touch_down = 1;
            state->pulse = 1;
        } else if (state->pulse == 1) state->pulse = 0;
    }
    return frame;
}
#endif
