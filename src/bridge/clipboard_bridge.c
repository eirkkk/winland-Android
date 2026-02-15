#include "clipboard_bridge.h"
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "ClipboardBridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// المتغير العام
static struct clipboard_bridge g_clipboard = {0};

// تهيئة جسر الحافظة
int clipboard_bridge_init(void) {
    if (g_clipboard.initialized) {
        LOGI("Clipboard bridge already initialized");
        return 0;
    }
    
    memset(&g_clipboard, 0, sizeof(g_clipboard));
    g_clipboard.initialized = 1;
    
    LOGI("Clipboard bridge initialized");
    return 0;
}

// إنهاء جسر الحافظة
void clipboard_bridge_terminate(void) {
    if (!g_clipboard.initialized) return;
    
    clipboard_bridge_clear();
    g_clipboard.initialized = 0;
    
    LOGI("Clipboard bridge terminated");
}

// تعيين بيانات الحافظة
int clipboard_bridge_set_data(const char *mime_type, const uint8_t *data, size_t size) {
    if (!g_clipboard.initialized || !mime_type || !data) return -1;
    
    // مسح البيانات القديمة
    clipboard_bridge_clear();
    
    // نسخ نوع MIME
    g_clipboard.mime_type = strdup(mime_type);
    if (!g_clipboard.mime_type) {
        LOGE("Failed to allocate MIME type");
        return -1;
    }
    
    // نسخ البيانات
    g_clipboard.data = malloc(size);
    if (!g_clipboard.data) {
        LOGE("Failed to allocate clipboard data");
        free(g_clipboard.mime_type);
        g_clipboard.mime_type = NULL;
        return -1;
    }
    
    memcpy(g_clipboard.data, data, size);
    g_clipboard.data_size = size;
    
    LOGI("Clipboard data set: %s (%zu bytes)", mime_type, size);
    
    // استدعاء معاودة الاتصال
    if (g_clipboard.on_clipboard_changed) {
        g_clipboard.on_clipboard_changed(mime_type);
    }
    
    return 0;
}

// استرجاع بيانات الحافظة
int clipboard_bridge_get_data(const char *mime_type, uint8_t **data, size_t *size) {
    if (!g_clipboard.initialized || !mime_type || !data || !size) return -1;
    
    if (!g_clipboard.data || !g_clipboard.mime_type) {
        *data = NULL;
        *size = 0;
        return 0;
    }
    
    // التحقق من تطابق نوع MIME
    if (strcmp(g_clipboard.mime_type, mime_type) != 0) {
        *data = NULL;
        *size = 0;
        return 0;
    }
    
    // نسخ البيانات
    *data = malloc(g_clipboard.data_size);
    if (!*data) {
        LOGE("Failed to allocate data copy");
        return -1;
    }
    
    memcpy(*data, g_clipboard.data, g_clipboard.data_size);
    *size = g_clipboard.data_size;
    
    return 0;
}

// مسح الحافظة
int clipboard_bridge_clear(void) {
    if (!g_clipboard.initialized) return -1;
    
    if (g_clipboard.mime_type) {
        free(g_clipboard.mime_type);
        g_clipboard.mime_type = NULL;
    }
    
    if (g_clipboard.data) {
        free(g_clipboard.data);
        g_clipboard.data = NULL;
    }
    
    g_clipboard.data_size = 0;
    
    LOGI("Clipboard cleared");
    return 0;
}

// الحصول على أنواع MIME المتاحة
int clipboard_bridge_get_available_mime_types(char ***mime_types, int *count) {
    if (!g_clipboard.initialized || !mime_types || !count) return -1;
    
    if (!g_clipboard.mime_type) {
        *mime_types = NULL;
        *count = 0;
        return 0;
    }
    
    *mime_types = malloc(sizeof(char*));
    if (!*mime_types) {
        LOGE("Failed to allocate MIME types array");
        return -1;
    }
    
    (*mime_types)[0] = strdup(g_clipboard.mime_type);
    if (!(*mime_types)[0]) {
        free(*mime_types);
        return -1;
    }
    
    *count = 1;
    return 0;
}

// التحقق من وجود نوع MIME
int clipboard_bridge_has_mime_type(const char *mime_type) {
    if (!g_clipboard.initialized || !mime_type) return 0;
    
    if (!g_clipboard.mime_type) return 0;
    
    return strcmp(g_clipboard.mime_type, mime_type) == 0;
}

// تعيين نص
int clipboard_bridge_set_text(const char *text) {
    if (!text) return -1;
    
    return clipboard_bridge_set_data(
        CLIPBOARD_MIME_TEXT_PLAIN,
        (const uint8_t*)text,
        strlen(text)
    );
}

// استرجاع نص
char* clipboard_bridge_get_text(void) {
    if (!g_clipboard.initialized) return NULL;
    
    if (!g_clipboard.data || !g_clipboard.mime_type) return NULL;
    
    // التحقق من أن البيانات نصية
    if (strcmp(g_clipboard.mime_type, CLIPBOARD_MIME_TEXT_PLAIN) != 0 &&
        strcmp(g_clipboard.mime_type, CLIPBOARD_MIME_TEXT_UTF8) != 0) {
        return NULL;
    }
    
    // نسخ النص
    char *text = malloc(g_clipboard.data_size + 1);
    if (!text) return NULL;
    
    memcpy(text, g_clipboard.data, g_clipboard.data_size);
    text[g_clipboard.data_size] = '\0';
    
    return text;
}

// تعيين HTML
int clipboard_bridge_set_html(const char *html) {
    if (!html) return -1;
    
    return clipboard_bridge_set_data(
        CLIPBOARD_MIME_TEXT_HTML,
        (const uint8_t*)html,
        strlen(html)
    );
}

// استرجاع HTML
char* clipboard_bridge_get_html(void) {
    if (!g_clipboard.initialized) return NULL;
    
    if (!g_clipboard.data || !g_clipboard.mime_type) return NULL;
    
    // التحقق من أن البيانات HTML
    if (strcmp(g_clipboard.mime_type, CLIPBOARD_MIME_TEXT_HTML) != 0) {
        return NULL;
    }
    
    // نسخ HTML
    char *html = malloc(g_clipboard.data_size + 1);
    if (!html) return NULL;
    
    memcpy(html, g_clipboard.data, g_clipboard.data_size);
    html[g_clipboard.data_size] = '\0';
    
    return html;
}

// تعيين صورة
int clipboard_bridge_set_image(const uint8_t *data, size_t size, const char *format) {
    if (!data || size == 0 || !format) return -1;
    
    const char *mime_type;
    if (strcmp(format, "png") == 0) {
        mime_type = CLIPBOARD_MIME_IMAGE_PNG;
    } else if (strcmp(format, "jpeg") == 0 || strcmp(format, "jpg") == 0) {
        mime_type = CLIPBOARD_MIME_IMAGE_JPEG;
    } else if (strcmp(format, "bmp") == 0) {
        mime_type = CLIPBOARD_MIME_IMAGE_BMP;
    } else {
        LOGE("Unsupported image format: %s", format);
        return -1;
    }
    
    return clipboard_bridge_set_data(mime_type, data, size);
}

// استرجاع صورة
int clipboard_bridge_get_image(uint8_t **data, size_t *size, char **format) {
    if (!g_clipboard.initialized || !data || !size || !format) return -1;
    
    if (!g_clipboard.data || !g_clipboard.mime_type) {
        *data = NULL;
        *size = 0;
        *format = NULL;
        return 0;
    }
    
    // تحديد التنسيق من نوع MIME
    if (strcmp(g_clipboard.mime_type, CLIPBOARD_MIME_IMAGE_PNG) == 0) {
        *format = strdup("png");
    } else if (strcmp(g_clipboard.mime_type, CLIPBOARD_MIME_IMAGE_JPEG) == 0) {
        *format = strdup("jpeg");
    } else if (strcmp(g_clipboard.mime_type, CLIPBOARD_MIME_IMAGE_BMP) == 0) {
        *format = strdup("bmp");
    } else {
        *data = NULL;
        *size = 0;
        *format = NULL;
        return 0;
    }
    
    // نسخ البيانات
    *data = malloc(g_clipboard.data_size);
    if (!*data) {
        free(*format);
        *format = NULL;
        return -1;
    }
    
    memcpy(*data, g_clipboard.data, g_clipboard.data_size);
    *size = g_clipboard.data_size;
    
    return 0;
}

// تعيين معاودة الاتصال
void clipboard_bridge_set_callback(void (*on_clipboard_changed)(const char *mime_type)) {
    g_clipboard.on_clipboard_changed = on_clipboard_changed;
}
