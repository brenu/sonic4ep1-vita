#ifndef SONIC_CANVAS_H
#define SONIC_CANVAS_H
#include <stddef.h>
#include <stdint.h>
/* Android Bitmap.Config.RGB_565 ignores the source ARGB alpha component. */
static inline uint16_t sonic_argb_to_rgb565(uint32_t argb) {
    return (uint16_t)(((argb >> 8) & 0xf800) | ((argb >> 5) & 0x07e0) | ((argb >> 3) & 0x001f));
}
static inline void sonic_canvas_convert(const uint32_t *source, uint16_t *dest, size_t count) {
    for (size_t i = 0; i < count; ++i) dest[i] = sonic_argb_to_rgb565(source[i]);
}
static inline int sonic_canvas_should_draw(int bitmap_ready, int use_gl) {
    return bitmap_ready && !use_gl;
}
#endif
