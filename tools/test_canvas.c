#include "sonic_canvas.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    assert(sonic_argb_to_rgb565(0xff000000) == 0);
    assert(sonic_argb_to_rgb565(0xffffffff) == 0xffff);
    assert(sonic_argb_to_rgb565(0xffff0000) == 0xf800);
    assert(sonic_argb_to_rgb565(0xff00ff00) == 0x07e0);
    assert(sonic_argb_to_rgb565(0xff0000ff) == 0x001f);
    assert(sonic_argb_to_rgb565(0x000000ff) == 0x001f); // RGB565 ignores alpha.
    uint32_t input[] = {0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff};
    uint16_t output[] = {0, 0, 0, 0, 0x1234};
    sonic_canvas_convert(input, output, 4);
    assert(output[0] == 0xf800 && output[1] == 0x07e0 && output[2] == 0x001f && output[3] == 0xffff);
    assert(output[4] == 0x1234);
    sonic_canvas_convert(input, output, 0);
    assert(output[4] == 0x1234);
    assert(sonic_canvas_should_draw(1, 0));
    assert(!sonic_canvas_should_draw(0, 0));
    assert(!sonic_canvas_should_draw(1, 1));
    puts("Software Canvas: RGB565 colors, alpha handling, order/bounds and GLES bypass passed");
}
