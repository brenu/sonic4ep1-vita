#include "sonic_framebuffer.h"
#include "sonic_canvas.h"
#include "sonic_java.h"
#include "sonic_jni_registry.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/dialog.h"
#include <falso_jni/FalsoJNI.h>
#include <stdlib.h>
#include <string.h>

enum { CANVAS_WIDTH = 960, CANVAS_HEIGHT = 544, CANVAS_PIXELS = 960 * 544 };
static jintArray pixel_array;
static jint *pixels;
static uint16_t *rgb565;
static GLuint texture;
static unsigned frames;

int sonic_framebuffer_owns(jobject object) { return pixel_array && object == pixel_array; }

void sonic_framebuffer_attach(void) {
    if (pixel_array) return;
    void (*set_pixels)(JNIEnv *, jobject, jint, jint, jintArray, jboolean) = sonic_find_native(
        "com/ideaworks3d/marmalade/LoaderView", "setPixelsNative", "(II[IZ)V");
    if (!set_pixels) fatal_error("Sonic: setPixelsNative was not registered");
    pixel_array = (*jni).NewIntArray(&jni, CANVAS_PIXELS);
    if (!pixel_array) fatal_error("Sonic: framebuffer JNI array allocation failed");
    pixels = (*jni).GetIntArrayElements(&jni, pixel_array, NULL);
    rgb565 = malloc(CANVAS_PIXELS * sizeof(*rgb565));
    if (!pixels || !rgb565) fatal_error("Sonic: software framebuffer allocation failed");
    memset(pixels, 0, CANVAS_PIXELS * sizeof(*pixels));
    l_info("Registering 960x544 ARGB surface via setPixelsNative");
    set_pixels(&jni, SONIC_OBJECT_VIEW, CANVAS_WIDTH, CANVAS_HEIGHT, pixel_array, JNI_TRUE);
}

void sonic_framebuffer_draw(int use_gl) {
    if (!sonic_canvas_should_draw(pixel_array != NULL, use_gl)) return;
    gl_init();
    sonic_canvas_convert((const uint32_t *)pixels, rgb565, CANVAS_PIXELS);

    // Only the pre-GLES Android Canvas path comes here. Preserve the matrix,
    // program, client pointers and render states touched by our blit. Avoid
    // the pinned driver's attribute-stack path; use explicit state snapshots.
    GLint program, matrix_mode, active, client_active, binding, buffer, unpack, viewport[4];
    GLint vertex_size, vertex_type, vertex_stride, tex_size, tex_type, tex_stride;
    void *vertex_pointer, *tex_pointer;
    GLfloat color[4];
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_MATRIX_MODE, &matrix_mode);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE, &client_active);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &unpack);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetFloatv(GL_CURRENT_COLOR, color);
    glActiveTexture(GL_TEXTURE0); glClientActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    glGetIntegerv(GL_VERTEX_ARRAY_SIZE, &vertex_size);
    glGetIntegerv(GL_VERTEX_ARRAY_TYPE, &vertex_type);
    glGetIntegerv(GL_VERTEX_ARRAY_STRIDE, &vertex_stride);
    glGetIntegerv(GL_TEXTURE_COORD_ARRAY_SIZE, &tex_size);
    glGetIntegerv(GL_TEXTURE_COORD_ARRAY_TYPE, &tex_type);
    glGetIntegerv(GL_TEXTURE_COORD_ARRAY_STRIDE, &tex_stride);
    glGetPointerv(GL_VERTEX_ARRAY_POINTER, &vertex_pointer);
    glGetPointerv(GL_TEXTURE_COORD_ARRAY_POINTER, &tex_pointer);
    GLenum caps[] = {GL_DEPTH_TEST, GL_BLEND, GL_ALPHA_TEST, GL_CULL_FACE, GL_SCISSOR_TEST, GL_LIGHTING, GL_FOG};
    GLboolean enabled[sizeof(caps) / sizeof(caps[0])];
    for (unsigned i = 0; i < sizeof(caps) / sizeof(caps[0]); ++i) {
        enabled[i] = glIsEnabled(caps[i]); glDisable(caps[i]);
    }
    GLboolean old_texture = glIsEnabled(GL_TEXTURE_2D);
    GLboolean old_vertex = glIsEnabled(GL_VERTEX_ARRAY), old_coords = glIsEnabled(GL_TEXTURE_COORD_ARRAY);
    GLboolean old_color = glIsEnabled(GL_COLOR_ARRAY), old_normal = glIsEnabled(GL_NORMAL_ARRAY);

    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // This vitaGL revision uses tightly packed rows and only supports the
    // row-length pixel-store selector. Alignment setters raise INVALID_ENUM.
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    if (!texture) {
        glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, CANVAS_WIDTH, CANVAS_HEIGHT, 0,
                     GL_RGB, GL_UNSIGNED_SHORT_5_6_5, rgb565);
    } else {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT,
                        GL_RGB, GL_UNSIGNED_SHORT_5_6_5, rgb565);
    }
    glViewport(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrthof(0, CANVAS_WIDTH, CANVAS_HEIGHT, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glEnable(GL_TEXTURE_2D); glColor4f(1, 1, 1, 1);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY); glDisableClientState(GL_NORMAL_ARRAY);
    static const GLfloat vertices[] = {0,0, 960,0, 960,544, 0,0, 960,544, 0,544};
    static const GLfloat coords[] = {0,0, 1,0, 1,1, 0,0, 1,1, 0,1};
    glVertexPointer(2, GL_FLOAT, 0, vertices); glTexCoordPointer(2, GL_FLOAT, 0, coords);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(matrix_mode);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glVertexPointer(vertex_size, vertex_type, vertex_stride, vertex_pointer);
    glTexCoordPointer(tex_size, tex_type, tex_stride, tex_pointer);
    if (!old_vertex) glDisableClientState(GL_VERTEX_ARRAY);
    if (!old_coords) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    if (old_color) glEnableClientState(GL_COLOR_ARRAY);
    if (old_normal) glEnableClientState(GL_NORMAL_ARRAY);
    if (!old_texture) glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, binding);
    glColor4f(color[0], color[1], color[2], color[3]);
    for (unsigned i = 0; i < sizeof(caps) / sizeof(caps[0]); ++i)
        if (enabled[i]) glEnable(caps[i]);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, unpack);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glUseProgram(program);
    glActiveTexture(active); glClientActiveTexture(client_active);
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) l_warn("Software Canvas GL error: 0x%x", (unsigned)error);
    gl_swap();
    if (++frames == 1) l_info("Software Canvas frame presented: first=%08x center=%08x",
                             (unsigned)pixels[0], (unsigned)pixels[CANVAS_PIXELS / 2]);
}
