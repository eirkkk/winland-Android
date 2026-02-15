#ifndef EGL_DISPLAY_H
#define EGL_DISPLAY_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <android/native_window.h>

#ifdef __cplusplus
extern "C" {
#endif

// هيكل عرض EGL
struct egl_display {
    EGLDisplay display;
    EGLContext context;
    EGLSurface surface;
    EGLConfig config;
    ANativeWindow *window;
    
    // الأبعاد
    int width;
    int height;
    
    // الحالة
    int initialized;
};

// دوال EGL
int egl_display_init(int width, int height);
void egl_display_terminate(void);
int egl_display_set_window(ANativeWindow *window);
int egl_display_make_current(void);
int egl_display_swap_buffers(void);
void egl_display_get_size(int *width, int *height);

// دوال العرض
void egl_clear_screen(float r, float g, float b, float a);
void egl_display_present_buffer(uint32_t texture_id, int width, int height);
uint32_t egl_create_texture_from_buffer(void *buffer, int width, int height);
void egl_destroy_texture(uint32_t texture_id);

// إعدادات OpenGL ES
int egl_setup_shaders(void);
void egl_use_shader_program(void);

#ifdef __cplusplus
}
#endif

#endif // EGL_DISPLAY_H
