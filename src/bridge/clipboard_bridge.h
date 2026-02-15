#ifndef CLIPBOARD_BRIDGE_H
#define CLIPBOARD_BRIDGE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// أنواع MIME للحافظة
#define CLIPBOARD_MIME_TEXT_PLAIN       "text/plain"
#define CLIPBOARD_MIME_TEXT_UTF8        "text/plain;charset=utf-8"
#define CLIPBOARD_MIME_TEXT_HTML        "text/html"
#define CLIPBOARD_MIME_IMAGE_PNG        "image/png"
#define CLIPBOARD_MIME_IMAGE_JPEG       "image/jpeg"
#define CLIPBOARD_MIME_IMAGE_BMP        "image/bmp"
#define CLIPBOARD_MIME_URI_LIST         "text/uri-list"

// هيكل جسر الحافظة
struct clipboard_bridge {
    int initialized;
    
    // البيانات المخزنة
    char *mime_type;
    uint8_t *data;
    size_t data_size;
    
    // معاودة الاتصال للتغييرات
    void (*on_clipboard_changed)(const char *mime_type);
};

// دوال جسر الحافظة
int clipboard_bridge_init(void);
void clipboard_bridge_terminate(void);

// تعيين واسترجاع البيانات
int clipboard_bridge_set_data(const char *mime_type, const uint8_t *data, size_t size);
int clipboard_bridge_get_data(const char *mime_type, uint8_t **data, size_t *size);
int clipboard_bridge_clear(void);

// أنواع MIME المتاحة
int clipboard_bridge_get_available_mime_types(char ***mime_types, int *count);
int clipboard_bridge_has_mime_type(const char *mime_type);

// النص
int clipboard_bridge_set_text(const char *text);
char* clipboard_bridge_get_text(void);

// HTML
int clipboard_bridge_set_html(const char *html);
char* clipboard_bridge_get_html(void);

// الصور
int clipboard_bridge_set_image(const uint8_t *data, size_t size, const char *format);
int clipboard_bridge_get_image(uint8_t **data, size_t *size, char **format);

// معاودة الاتصال
void clipboard_bridge_set_callback(void (*on_clipboard_changed)(const char *mime_type));

#ifdef __cplusplus
}
#endif

#endif // CLIPBOARD_BRIDGE_H
