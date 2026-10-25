#include "sonic_touch_policy.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    assert(sonic_touch_scale(0,0,1919,960)==0);
    assert(sonic_touch_scale(960,0,1919,960)==480);
    assert(sonic_touch_scale(1919,0,1919,960)==959);
    assert(sonic_touch_scale(2000,0,1919,960)==959);
    assert(sonic_touch_scale(-1,0,1919,960)==0);
    assert(sonic_touch_scale(1087,0,1087,544)==543);
    assert(sonic_touch_scale(110,10,209,100)==50);
    SonicTouchState s; sonic_touch_reset(&s);
    SonicTouchEvent e[12];
    SonicTouchPoint p[6]={{101,100,200},{8,300,400}};
    int n=sonic_touch_update(&s,p,2,e);
    assert(n==2 && e[0].id==0 && e[0].action==4 && e[1].id==1 && e[1].action==4);
    SonicTouchPoint swapped[2]={p[1],p[0]};
    assert(sonic_touch_update(&s,swapped,2,e)==0);
    swapped[0].x=301;
    assert(sonic_touch_update(&s,swapped,2,e)==1 && e[0].id==1 && e[0].action==6);
    p[0]=(SonicTouchPoint){77,500,100}; p[1]=swapped[0];
    n=sonic_touch_update(&s,p,2,e);
    assert(n==2 && e[0].id==0 && e[0].action==5 && e[0].x==100);
    assert(e[1].id==0 && e[1].action==4 && s.slots[1].id==8);
    assert(sonic_touch_update(&s,0,0,e)==2);
    for(int i=0;i<6;++i)p[i]=(SonicTouchPoint){i,10*i,20*i};
    assert(sonic_touch_update(&s,p,6,e)==6);
    for(int i=0;i<6;++i)p[i].id+=10;
    n=sonic_touch_update(&s,p,6,e);assert(n==12);
    for(int i=0;i<6;++i){assert(e[i].action==5);assert(e[i+6].action==4);}
    SonicTouchState old=s;p[1].id=p[0].id;
    assert(sonic_touch_update(&s,p,6,e)==-1 && !memcmp(&s,&old,sizeof(s)));
    assert(sonic_touch_update(&s,p,7,e)==-1);
    puts("Touch policy: scaling/clamps, stable slots, reorder, movement, release/reuse, six-finger transitions and malformed reports passed");
}
