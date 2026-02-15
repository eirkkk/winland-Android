#include "dmabuf_handler.h"
#include "egl_display.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <android/log.h>
#include <errno.h>

#define LOG_TAG "DmaBufHandler"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// المتغير العام
static struct dmabuf_handler g_handler = {0};

// تنسيقات dmabuf المدعومة
static const uint32_t supported_formats[] = {
    DRM_FORMAT_ARGB8888,
    DRM_FORMAT_XRGB8888,
    DRM_FORMAT_ABGR8888,
    DRM_FORMAT_XBGR8888,
    DRM_FORMAT_RGB565,
    DRM_FORMAT_BGR565,
    0
};

// الحصول على معرض EGL
static EGLDisplay get_egl_display(void) {
    // This should be implemented to get the current EGL display
    // For now, return the default display
    return eglGetCurrentDisplay();
}

// تهيئة معالج dmabuf
int dmabuf_handler_init(void) {
    if (g_handler.initialized) {
        return 0;
    }
    
    EGLDisplay display = get_egl_display();
    if (display == EGL_NO_DISPLAY) {
        LOGE("No EGL display available");
        return -1;
    }
    
    // التحقق من التمددات
    const char* extensions = eglQueryString(display, EGL_EXTENSIONS);
    if (!extensions) {
        LOGE("Failed to query EGL extensions");
        return -1;
    }
    
    g_handler.has_image_dma_buf_import = 
        strstr(extensions, "EGL_EXT_image_dma_buf_import") != NULL;
    g_handler.has_image_dma_buf_import_modifiers = 
        strstr(extensions, "EGL_EXT_image_dma_buf_import_modifiers") != NULL;
    
    if (!g_handler.has_image_dma_buf_import) {
        LOGE("EGL_EXT_image_dma_buf_import not supported");
        return -1;
    }
    
    LOGI("DMA-BUF import supported");
    if (g_handler.has_image_dma_buf_import_modifiers) {
        LOGI("DMA-BUF import with modifiers supported");
    }
    
    // تحميل دوال التمددات
    g_handler.eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)
        eglGetProcAddress("eglCreateImageKHR");
    g_handler.eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)
        eglGetProcAddress("eglDestroyImageKHR");
    g_handler.glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
        eglGetProcAddress("glEGLImageTargetTexture2DOES");
    
    if (!g_handler.eglCreateImageKHR || !g_handler.eglDestroyImageKHR ||
        !g_handler.glEGLImageTargetTexture2DOES) {
        LOGE("Failed to load DMA-BUF extension functions");
        return -1;
    }
    
    g_handler.initialized = 1;
    LOGI("DMA-BUF handler initialized");
    return 0;
}

// إنهاء معالج dmabuf
void dmabuf_handler_terminate(void) {
    memset(&g_handler, 0, sizeof(g_handler));
    LOGI("DMA-BUF handler terminated");
}

// التحقق من الدعم
bool dmabuf_is_supported(void) {
    return g_handler.initialized && g_handler.has_image_dma_buf_import;
}

// استيراد مخزن مؤقت dmabuf
struct dmabuf_import* dmabuf_import_buffer(
    int fd,
    uint32_t width,
    uint32_t height,
    uint32_t format,
    uint32_t stride,
    uint32_t offset
) {
    if (!g_handler.initialized) {
        LOGE("DMA-BUF handler not initialized");
        return NULL;
    }
    
    struct dmabuf_import* import = calloc(1, sizeof(*import));
    if (!import) {
        LOGE("Failed to allocate dmabuf_import");
        return NULL;
    }
    
    import->fd = dup(fd);
    import->width = width;
    import->height = height;
    import->format = format;
    import->stride = stride;
    import->offset = offset;
    import->modifier = DRM_FORMAT_MOD_INVALID;
    import->egl_image = EGL_NO_IMAGE_KHR;
    import->texture_id = 0;
    import->imported = 0;
    
    EGLDisplay display = get_egl_display();
    
    // إنشاء EGL image من dmabuf
    EGLint attribs[] = {
        EGL_WIDTH, width,
        EGL_HEIGHT, height,
        EGL_LINUX_DRM_FOURCC_EXT, format,
        EGL_DMA_BUF_PLANE0_FD_EXT, import->fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, offset,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
        EGL_NONE
    };
    
    import->egl_image = g_handler.eglCreateImageKHR(
        display,
        EGL_NO_CONTEXT,
        EGL_LINUX_DMA_BUF_EXT,
        NULL,
        attribs
    );
    
    if (import->egl_image == EGL_NO_IMAGE_KHR) {
        LOGE("Failed to create EGL image from DMA-BUF");
        close(import->fd);
        free(import);
        return NULL;
    }
    
    import->imported = 1;
    LOGI("DMA-BUF imported: %dx%d, format=0x%x", width, height, format);
    
    return import;
}

// استيراد مع modifier
struct dmabuf_import* dmabuf_import_buffer_with_modifier(
    int fd,
    uint32_t width,
    uint32_t height,
    uint32_t format,
    uint64_t modifier,
    uint32_t stride,
    uint32_t offset
) {
    if (!g_handler.initialized) {
        LOGE("DMA-BUF handler not initialized");
        return NULL;
    }
    
    if (!g_handler.has_image_dma_buf_import_modifiers) {
        LOGE("DMA-BUF import with modifiers not supported");
        return NULL;
    }
    
    struct dmabuf_import* import = calloc(1, sizeof(*import));
    if (!import) {
        LOGE("Failed to allocate dmabuf_import");
        return NULL;
    }
    
    import->fd = dup(fd);
    import->width = width;
    import->height = height;
    import->format = format;
    import->stride = stride;
    import->offset = offset;
    import->modifier = modifier;
    import->egl_image = EGL_NO_IMAGE_KHR;
    import->texture_id = 0;
    import->imported = 0;
    
    EGLDisplay display = get_egl_display();
    
    // إنشاء EGL image مع modifier
    EGLint attribs[] = {
        EGL_WIDTH, width,
        EGL_HEIGHT, height,
        EGL_LINUX_DRM_FOURCC_EXT, format,
        EGL_DMA_BUF_PLANE0_FD_EXT, import->fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, offset,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, (EGLint)(modifier & 0xFFFFFFFF),
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, (EGLint)(modifier >> 32),
        EGL_NONE
    };
    
    import->egl_image = g_handler.eglCreateImageKHR(
        display,
        EGL_NO_CONTEXT,
        EGL_LINUX_DMA_BUF_EXT,
        NULL,
        attribs
    );
    
    if (import->egl_image == EGL_NO_IMAGE_KHR) {
        LOGE("Failed to create EGL image from DMA-BUF with modifier");
        close(import->fd);
        free(import);
        return NULL;
    }
    
    import->imported = 1;
    LOGI("DMA-BUF imported with modifier: %dx%d, format=0x%x, modifier=0x%lx",
         width, height, format, modifier);
    
    return import;
}

// تدمير استيراد
void dmabuf_import_destroy(struct dmabuf_import* import) {
    if (!import) return;
    
    if (import->texture_id) {
        dmabuf_destroy_texture(import);
    }
    
    if (import->egl_image != EGL_NO_IMAGE_KHR) {
        g_handler.eglDestroyImageKHR(get_egl_display(), import->egl_image);
    }
    
    if (import->fd >= 0) {
        close(import->fd);
    }
    
    free(import);
    LOGI("DMA-BUF import destroyed");
}

// إنشاء نسيج من dmabuf
GLuint dmabuf_create_texture(struct dmabuf_import* import) {
    if (!import || !import->imported) return 0;
    
    if (import->texture_id) {
        return import->texture_id;
    }
    
    glGenTextures(1, &import->texture_id);
    if (!import->texture_id) {
        LOGE("Failed to generate texture");
        return 0;
    }
    
    glBindTexture(GL_TEXTURE_2D, import->texture_id);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // ربط EGL image بالنسيج
    g_handler.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, import->egl_image);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    LOGI("Texture created from DMA-BUF: %d", import->texture_id);
    return import->texture_id;
}

// تدمير نسيج
void dmabuf_destroy_texture(struct dmabuf_import* import) {
    if (!import || !import->texture_id) return;
    
    glDeleteTextures(1, &import->texture_id);
    import->texture_id = 0;
    
    LOGI("Texture destroyed");
}

// تحديث النسيج
int dmabuf_update_texture(struct dmabuf_import* import) {
    if (!import || !import->texture_id) return -1;
    
    // DMA-BUF textures are automatically updated when the buffer content changes
    // No explicit update needed
    return 0;
}

// تحويل التنسيق إلى نص
const char* dmabuf_format_to_string(uint32_t format) {
    switch (format) {
        case DRM_FORMAT_ARGB8888: return "ARGB8888";
        case DRM_FORMAT_XRGB8888: return "XRGB8888";
        case DRM_FORMAT_ABGR8888: return "ABGR8888";
        case DRM_FORMAT_XBGR8888: return "XBGR8888";
        case DRM_FORMAT_RGB565: return "RGB565";
        case DRM_FORMAT_BGR565: return "BGR565";
        default: return "UNKNOWN";
    }
}

// التحقق من دعم التنسيق
bool dmabuf_format_is_supported(uint32_t format) {
    for (int i = 0; supported_formats[i] != 0; i++) {
        if (supported_formats[i] == format) {
            return true;
        }
    }
    return false;
}

// تصدير من نسيج
int dmabuf_export_from_texture(GLuint texture_id, uint32_t width, uint32_t height) {
    // This requires EGL_MESA_image_dma_buf_export extension
    // Implementation depends on driver support
    LOGE("DMA-BUF export not yet implemented");
    return -1;
}

// تصدير من EGL image
int dmabuf_export_from_egl_image(EGLImageKHR image, uint32_t width, uint32_t height) {
    // This requires EGL_MESA_image_dma_buf_export extension
    LOGE("DMA-BUF export not yet implemented");
    return -1;
}
