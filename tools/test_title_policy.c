#include "sonic_title_policy.h"
#include "sonic_touch_policy.h"
#include <assert.h>
#include <stdio.h>
enum { START=1, CROSS=2, OTHER=4, MASK=7 };
int main(void) {
    SonicTitlePad s={0};
    SonicTitleFrame f=sonic_title_step(&s,0,START,MASK,1,1);
    assert(!f.triggered && f.game_buttons==START && !f.touch_down);
    sonic_title_step(&s,0,0,MASK,1,1);
    f=sonic_title_step(&s,1,START,MASK,1,1);
    assert(f.triggered && f.touch_down && f.game_buttons==0);
    /* Context can change on DOWN; still issue UP and consume held Start. */
    f=sonic_title_step(&s,0,START,MASK,1,1);
    assert(!f.touch_down && !f.triggered && f.game_buttons==0 && s.pulse==0 && f.block_controls);
    f=sonic_title_step(&s,0,START,MASK,1,1);
    assert(f.game_buttons==0);
    f=sonic_title_step(&s,0,0,MASK,1,1);assert(f.game_buttons==0);
    f=sonic_title_step(&s,0,START,MASK,1,1);
    assert(f.game_buttons==START && !f.touch_down);
    s=(SonicTitlePad){0};
    f=sonic_title_step(&s,1,CROSS,MASK,0,1);assert(s.pulse==2 && !f.touch_down);
    f=sonic_title_step(&s,1,CROSS,MASK,1,0);assert(s.pulse==2 && !f.touch_down);
    f=sonic_title_step(&s,0,CROSS,MASK,1,1);
    assert(s.pulse==0 && !f.touch_down && f.game_buttons==0);
    sonic_title_step(&s,0,0,MASK,1,1);
    f=sonic_title_step(&s,1,OTHER,MASK,1,1);assert(f.touch_down);
    f=sonic_title_step(&s,1,OTHER,MASK,0,1);assert(s.pulse==1 && !f.touch_down);
    f=sonic_title_step(&s,1,OTHER,MASK,1,1);assert(s.pulse==0 && !f.touch_down);
    f=sonic_title_step(&s,1,OTHER,MASK,1,1);assert(!f.triggered && !f.touch_down);
    /* Synthetic and real finger IDs coexist; logical UP is emitted once. */
    SonicTouchState t;sonic_touch_reset(&t);SonicTouchEvent e[12];
    SonicTouchPoint p[2]={{255,100,200},{256,480,272}};
    assert(sonic_touch_update(&t,p,2,e)==2 && e[0].id!=e[1].id);
    assert(sonic_touch_update(&t,p,1,e)==1 && e[0].action==5 && e[0].x==480);
    assert(sonic_touch_update(&t,p,1,e)==0);
    p[0].id=257;assert(sonic_touch_update(&t,p,1,e)==-1);
    s=(SonicTitlePad){0};
    f=sonic_title_step(&s,1,8,MASK,1,1);
    assert(!f.triggered && !f.touch_down); /* System-button bit not in mask. */
    puts("Title input: one-shot press/release, held-button consumption, context changes, busy/missing touch samples and synthetic finger isolation passed");
}
