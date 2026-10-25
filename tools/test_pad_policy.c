#include "sonic_pad_policy.h"
#include <assert.h>
#include <stdio.h>

enum { LEFT=1, RIGHT=2, UP=4, DOWN=8, JUMP=16 };
static uint32_t merge(uint32_t physical, int x, int y) {
    return sonic_pad_merge(physical,x,y,LEFT,RIGHT,UP,DOWN);
}
int main(void) {
    assert(sonic_pad_axis(128,0)==0 && sonic_pad_axis(128,-1)==0);
    assert(sonic_pad_axis(80,0)==-1 && sonic_pad_axis(81,0)==0);
    assert(sonic_pad_axis(95,-1)==-1 && sonic_pad_axis(96,-1)==0);
    assert(sonic_pad_axis(176,0)==1 && sonic_pad_axis(175,0)==0);
    assert(sonic_pad_axis(161,1)==1 && sonic_pad_axis(160,1)==0);
    assert(sonic_pad_axis(255,-1)==1 && sonic_pad_axis(0,1)==-1);
    assert(merge(0,-1,0)==LEFT && merge(0,1,0)==RIGHT);
    assert(merge(0,0,-1)==UP && merge(0,0,1)==DOWN);
    assert(merge(JUMP,1,-1)==(JUMP|RIGHT|UP));
    /* Releasing one source cannot release an arrow held by the other. */
    assert(merge(LEFT,-1,0)==LEFT && merge(LEFT,0,0)==LEFT);
    assert(merge(0,-1,0)==LEFT && merge(0,0,0)==0);
    assert(merge(LEFT,1,0)==0 && merge(LEFT,0,0)==LEFT);
    assert(merge(UP,0,1)==0);
    assert(merge(LEFT|RIGHT|JUMP,0,0)==JUMP);
    uint32_t old=merge(LEFT,-1,0), now=merge(0,-1,0);
    assert((old^now)==0);
    now=merge(0,0,0); assert((old^now)==LEFT);
    /* Start/Circle aliases create only one press and final release. */
    enum { START=32, CIRCLE=64 };
    assert(sonic_pad_binding_edge(0,START,START|CIRCLE)==1);
    assert(sonic_pad_binding_edge(START,START|CIRCLE,START|CIRCLE)==-1);
    assert(sonic_pad_binding_edge(START|CIRCLE,CIRCLE,START|CIRCLE)==-1);
    assert(sonic_pad_binding_edge(CIRCLE,0,START|CIRCLE)==0);
    assert(sonic_pad_binding_edge(0,JUMP,START|CIRCLE)==-1);
    puts("Pad policy: dead-zone hysteresis, reversal/diagonals, Dpad/stick source merging, jump retention and release edges passed");
}
