#ifndef SONIC_FRAMEBUFFER_H
#define SONIC_FRAMEBUFFER_H
#include <falso_jni/jni.h>
void sonic_framebuffer_attach(void);
void sonic_framebuffer_draw(int use_gl);
int sonic_framebuffer_owns(jobject object);
#endif
