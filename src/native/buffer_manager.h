#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include <wayland-server.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#ifdef __cplusplus
extern "C" {
#endif

struct winland_compositor;

// هيكل مخزن مؤقت Winland
struct winland_buffer {
    struct wl_resource *resource;
    struct wl_shm_buffer *shm_buffer;
    
    // البيانات
    void *data;
    int32_t width;
    int32_t height;
    int32_t stride;
    uint32_t format;
    
    // النسيج OpenGL
    GLuint texture_id;
    int texture_uploaded;
    
    // الحالة
    int busy;
    int destroyed;
    
    // الارتباطات
    struct wl_listener destroy_listener;
    struct wl_list link;
};

// دوال مدير المخازن المؤقتة
int buffer_manager_init(struct winland_compositor *compositor);
void buffer_manager_terminate(struct winland_compositor *compositor);

// دوال المخزن المؤقت
struct winland_buffer* winland_buffer_create(
    struct wl_resource *resource,
    struct wl_shm_buffer *shm_buffer
);
void winland_buffer_destroy(struct winland_buffer *buffer);

// تحميل المخزن المؤقت إلى نسيج OpenGL
int winland_buffer_upload_to_texture(struct winland_buffer *buffer);
GLuint winland_buffer_get_texture(struct winland_buffer *buffer);

// الوصول إلى بيانات المخزن المؤقت
void* winland_buffer_get_data(struct winland_buffer *buffer);
int32_t winland_buffer_get_width(struct winland_buffer *buffer);
int32_t winland_buffer_get_height(struct winland_buffer *buffer);
int32_t winland_buffer_get_stride(struct winland_buffer *buffer);

// إدارة المراجع
void winland_buffer_reference(struct winland_buffer *buffer);
void winland_buffer_unreference(struct winland_buffer *buffer);

#ifdef __cplusplus
}
#endif

#endif // BUFFER_MANAGER_H
