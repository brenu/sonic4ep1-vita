#ifndef SONIC_TOUCH_POLICY_H
#define SONIC_TOUCH_POLICY_H
#define SONIC_TOUCH_SLOTS 6
typedef struct { int id, x, y; } SonicTouchPoint;
typedef struct { int id, action, x, y; } SonicTouchEvent;
typedef struct { SonicTouchPoint slots[SONIC_TOUCH_SLOTS]; } SonicTouchState;

static inline int sonic_touch_scale(int raw, int minimum, int maximum, int pixels) {
    if (pixels <= 0 || maximum <= minimum) return 0;
    if (raw < minimum) raw = minimum;
    if (raw > maximum) raw = maximum;
    return (int)((long long)(raw - minimum) * pixels / ((long long)maximum - minimum + 1));
}
static inline void sonic_touch_reset(SonicTouchState *state) {
    for (int i = 0; i < SONIC_TOUCH_SLOTS; ++i)
        state->slots[i] = (SonicTouchPoint){-1, 0, 0};
}
/* Events capacity is twice the slot count: release all old, then press all new. */
static inline int sonic_touch_update(SonicTouchState *state, const SonicTouchPoint *points,
                                     int count, SonicTouchEvent *events) {
    if (count < 0 || count > SONIC_TOUCH_SLOTS || (count && !points)) return -1;
    for (int i = 0; i < count; ++i) {
        /* Physical reports have uint8 IDs; 256 is our title-start pulse. */
        if (points[i].id < 0 || points[i].id > 256) return -1;
        for (int j = 0; j < i; ++j) if (points[j].id == points[i].id) return -1;
    }
    int used = 0;
    for (int slot = 0; slot < SONIC_TOUCH_SLOTS; ++slot) {
        SonicTouchPoint *old = &state->slots[slot];
        if (old->id < 0) continue;
        int found = 0;
        for (int i = 0; i < count; ++i) if (points[i].id == old->id) found = 1;
        if (!found) {
            events[used++] = (SonicTouchEvent){slot, 5, old->x, old->y};
            old->id = -1;
        }
    }
    for (int i = 0; i < count; ++i) {
        int slot = -1, action = 6;
        for (int j = 0; j < SONIC_TOUCH_SLOTS; ++j)
            if (state->slots[j].id == points[i].id) slot = j;
        if (slot < 0) {
            for (int j = 0; j < SONIC_TOUCH_SLOTS; ++j)
                if (state->slots[j].id < 0) { slot = j; break; }
            action = 4;
        }
        if (slot < 0) return -1; /* Unreachable after validated count/release. */
        if (action == 4 || state->slots[slot].x != points[i].x ||
                state->slots[slot].y != points[i].y)
            events[used++] = (SonicTouchEvent){slot, action, points[i].x, points[i].y};
        state->slots[slot] = points[i];
    }
    return used;
}
#endif
