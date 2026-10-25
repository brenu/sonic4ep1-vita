#ifndef SONIC_PAD_POLICY_H
#define SONIC_PAD_POLICY_H
#include <stdint.h>

/* Vita analog axes are 0..255, centered at 128. Press at 48 units from
 * center, release at 32; the gap prevents jitter near the dead-zone edge. */
static inline int sonic_pad_axis(int raw, int previous) {
    if (raw <= 80) return -1;
    if (raw >= 176) return 1;
    if (previous < 0 && raw < 96) return -1;
    if (previous > 0 && raw > 160) return 1;
    return 0;
}
/* Several physical buttons can share one game action. Return -1 for no
 * logical edge, 1 for press, 0 for release. */
static inline int sonic_pad_binding_edge(uint32_t previous, uint32_t current, uint32_t mask) {
    int before = (previous & mask) != 0, after = (current & mask) != 0;
    return before == after ? -1 : after;
}
static inline uint32_t sonic_pad_merge(uint32_t physical, int x, int y,
                                       uint32_t left, uint32_t right,
                                       uint32_t up, uint32_t down) {
    uint32_t effective = physical;
    if (x < 0) effective |= left;
    if (x > 0) effective |= right;
    if (y < 0) effective |= up;
    if (y > 0) effective |= down;
    /* Opposed directions cancel; releasing either source then exposes the
     * remaining held direction. Non-direction buttons are unchanged. */
    if ((effective & left) && (effective & right)) effective &= ~(left | right);
    if ((effective & up) && (effective & down)) effective &= ~(up | down);
    return effective;
}
#endif
