#include "buffer_manager.h"
#include "wayland_compositor.h"
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "BufferManager"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// قائمة المخازن المؤقتة
static struct wl_list g_buffers;
static int g_initialized = 0;

// استدعاء عند تدمير المخزن المؤقت
static void buffer_destroy_notify(struct wl_listener *listener, void *data) {
    struct winland_buffer *buffer = wl_container_of(listener, buffer, destroy_listener);
    buffer->destroyed = 1;
    buffer->resource = NULL;
    winland_buffer_destroy(buffer);
}

// تهيئة مدير المخازن المؤقتة
int buffer_manager_init(struct winland_compositor *compositor) {
    if (g_initialized) return 0;
    
    wl_list_init(&g_buffers);
    g_initialized = 1;
    
    LOGI("Buffer manager initialized");
    return 0;
}

// إنهاء مدير المخازن المؤقتة
void buffer_manager_terminate(struct winland_compositor *compositor) {
    if (!g_initialized) return;
    
    // تدمير جميع المخازن المؤقتة
    struct winland_buffer *buffer, *tmp;
    wl_list_for_each_safe(buffer, tmp, &g_buffers, link) {
        winland_buffer_destroy(buffer);
    }
    
    g_initialized = 0;
    LOGI("Buffer manager terminated");
}

// إنشاء مخزن مؤقت جديد
struct winland_buffer* winland_buffer_create(
    struct wl_resource *resource,
    struct wl_shm_buffer *shm_buffer
) {
    struct winland_buffer *buffer = calloc(1, sizeof(*buffer));
    if (!buffer) {
        LOGE("Failed to allocate buffer");
        return NULL;
    }
    
    buffer->resource = resource;
    buffer->shm_buffer = shm_buffer;
    buffer->texture_id = 0;
    buffer->texture_uploaded = 0;
    buffer->busy = 0;
    buffer->destroyed = 0;
    
    if (shm_buffer) {
        buffer->width = wl_shm_buffer_get_width(shm_buffer);
        buffer->height = wl_shm_buffer_get_height(shm_buffer);
        buffer->stride = wl_shm_buffer_get_stride(shm_buffer);
        buffer->format = wl_shm_buffer_get_format(shm_buffer);
        
        wl_shm_buffer_begin_access(shm_buffer);
        buffer->data = wl_shm_buffer_get_data(shm_buffer);
        wl_shm_buffer_end_access(shm_buffer);
    }
    
    // إعداد الاستماع للتدمير
    buffer->destroy_listener.notify = buffer_destroy_notify;
    wl_resource_add_destroy_listener(resource, &buffer->destroy_listener);
    
    // إضافة إلى القائمة
    wl_list_insert(&g_buffers, &buffer->link);
    
    LOGI("Buffer created: %p (%dx%d)", buffer, buffer->width, buffer->height);
    
    return buffer;
}

// تدمير مخزن مؤقت
void winland_buffer_destroy(struct winland_buffer *buffer) {
    if (!buffer) return;
    
    LOGI("Destroying buffer: %p", buffer);
    
    // إزالة من القائمة
    wl_list_remove(&buffer->link);
    wl_list_remove(&buffer->destroy_listener.link);
    
    // حذف النسيج إذا موجود
    if (buffer->texture_id) {
        glDeleteTextures(1, &buffer->texture_id);
    }
    
    free(buffer);
}

// تحميل المخزن المؤقت إلى نسيج OpenGL
int winland_buffer_upload_to_texture(struct winland_buffer *buffer) {
    if (!buffer || !buffer->data) return -1;
    
    if (buffer->texture_uploaded && buffer->texture_id) {
        return 0; // تم التحميل مسبقاً
    }
    
    // إنشاء نسيج جديد إذا لزم الأمر
    if (!buffer->texture_id) {
        glGenTextures(1, &buffer->texture_id);
        if (!buffer->texture_id) {
            LOGE("Failed to generate texture");
            return -1;
        }
    }
    
    glBindTexture(GL_TEXTURE_2D, buffer->texture_id);
    
    // إعداد معلمات النسيج
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // تحويل التنسيق
    GLenum gl_format = GL_RGBA;
    GLenum gl_type = GL_UNSIGNED_BYTE;
    
    switch (buffer->format) {
        case WL_SHM_FORMAT_ARGB8888:
        case WL_SHM_FORMAT_XRGB8888:
            gl_format = GL_RGBA;
            break;
        case WL_SHM_FORMAT_RGB565:
            gl_format = GL_RGB;
            gl_type = GL_UNSIGNED_SHORT_5_6_5;
            break;
        default:
            LOGI("Unsupported buffer format: %d", buffer->format);
            glBindTexture(GL_TEXTURE_2D, 0);
            return -1;
    }
    
    // تحميل البيانات
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        gl_format,
        buffer->width,
        buffer->height,
        0,
        gl_format,
        gl_type,
        buffer->data
    );
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    buffer->texture_uploaded = 1;
    
    LOGI("Buffer uploaded to texture: %d", buffer->texture_id);
    return 0;
}

// الحصول على معرف النسيج
GLuint winland_buffer_get_texture(struct winland_buffer *buffer) {
    if (!buffer) return 0;
    
    if (!buffer->texture_uploaded) {
        winland_buffer_upload_to_texture(buffer);
    }
    
    return buffer->texture_id;
}

// الحصول على بيانات المخزن المؤقت
void* winland_buffer_get_data(struct winland_buffer *buffer) {
    if (!buffer) return NULL;
    return buffer->data;
}

// الحصول على عرض المخزن المؤقت
int32_t winland_buffer_get_width(struct winland_buffer *buffer) {
    if (!buffer) return 0;
    return buffer->width;
}

// الحصول على ارتفاع المخزن المؤقت
int32_t winland_buffer_get_height(struct winland_buffer *buffer) {
    if (!buffer) return 0;
    return buffer->height;
}

// الحصول على خطوة المخزن المؤقت
int32_t winland_buffer_get_stride(struct winland_buffer *buffer) {
    if (!buffer) return 0;
    return buffer->stride;
}

// إضافة مرجع
void winland_buffer_reference(struct winland_buffer *buffer) {
    if (!buffer) return;
    buffer->busy++;
}

// إزالة مرجع
void winland_buffer_unreference(struct winland_buffer *buffer) {
    if (!buffer) return;
    buffer->busy--;
    
    if (buffer->busy <= 0 && buffer->destroyed) {
        winland_buffer_destroy(buffer);
    }
}
