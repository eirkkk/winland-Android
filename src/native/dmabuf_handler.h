#ifndef DMABUF_HANDLER_H
#define DMABUF_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <linux/dma-buf.h>

#ifdef __cplusplus
extern "C" {
#endif

// هيكل معالج dmabuf
struct dmabuf_handler {
    int initialized;
    
    // EGL extensions
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
    
    // دعم التمددات
    bool has_image_dma_buf_import;
    bool has_image_dma_buf_import_modifiers;
};

// هيكل مستورد dmabuf
struct dmabuf_import {
    int fd;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint64_t modifier;
    uint32_t stride;
    uint32_t offset;
    
    // EGL image
    EGLImageKHR egl_image;
    
    // OpenGL texture
    GLuint texture_id;
    
    // الحالة
    int imported;
};

// دوال المعالج
int dmabuf_handler_init(void);
void dmabuf_handler_terminate(void);
bool dmabuf_is_supported(void);

// استيراد وتصدير
struct dmabuf_import* dmabuf_import_buffer(
    int fd,
    uint32_t width,
    uint32_t height,
    uint32_t format,
    uint32_t stride,
    uint32_t offset
);

struct dmabuf_import* dmabuf_import_buffer_with_modifier(
    int fd,
    uint32_t width,
    uint32_t height,
    uint32_t format,
    uint64_t modifier,
    uint32_t stride,
    uint32_t offset
);

void dmabuf_import_destroy(struct dmabuf_import* import);

// إنشاء نسيج من dmabuf
GLuint dmabuf_create_texture(struct dmabuf_import* import);
void dmabuf_destroy_texture(struct dmabuf_import* import);

// تحديث البيانات
int dmabuf_update_texture(struct dmabuf_import* import);

// معلومات التنسيق
const char* dmabuf_format_to_string(uint32_t format);
bool dmabuf_format_is_supported(uint32_t format);

// إدارة الملفات
int dmabuf_export_from_texture(GLuint texture_id, uint32_t width, uint32_t height);
int dmabuf_export_from_egl_image(EGLImageKHR image, uint32_t width, uint32_t height);

#ifdef __cplusplus
}
#endif

#endif // DMABUF_HANDLER_H
