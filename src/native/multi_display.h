#ifndef MULTI_DISPLAY_H
#define MULTI_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <EGL/egl.h>

#ifdef __cplusplus
extern "C" {
#endif

// أنواع المنافذ
typedef enum {
    DISPLAY_PORT_INTERNAL,      // شاشة الهاتف
    DISPLAY_PORT_HDMI,          // HDMI
    DISPLAY_PORT_USB_C,         // USB-C DP Alt Mode
    DISPLAY_PORT_WIRELESS,      // لاسلكي (Miracast/Chromecast)
    DISPLAY_PORT_VIRTUAL        // افتراضي (VNC)
} display_port_t;

// أوضاع العرض
typedef enum {
    DISPLAY_MODE_MIRROR,        // نسخة طبق الأصل
    DISPLAY_MODE_EXTEND,        // تمديد
    DISPLAY_MODE_SINGLE         // شاشة واحدة فقط
} display_mode_t;

// معلومات العرض
struct display_info {
    uint32_t id;
    char name[64];
    display_port_t port;
    
    // الأبعاد
    int32_t width;
    int32_t height;
    int32_t physical_width_mm;  // العرض الفعلي بالملليمتر
    int32_t physical_height_mm; // الارتفاع الفعلي بالملليمتر
    
    // معدل التحديث
    int32_t refresh_rate;
    
    // DPI
    float dpi_x;
    float dpi_y;
    float scale_factor;
    
    // HDR
    bool supports_hdr;
    uint32_t max_luminance;
    
    // الحالة
    bool connected;
    bool primary;
    bool enabled;
    
    // EGL
    EGLDisplay egl_display;
    EGLSurface egl_surface;
    EGLConfig egl_config;
};

// هيكل إعدادات العرض
struct display_config {
    display_mode_t mode;
    
    // إعدادات كل شاشة
    struct {
        uint32_t display_id;
        int32_t x, y;
        int32_t width, height;
        float scale;
        bool enabled;
        int32_t refresh_rate;
    } displays[4];
    int display_count;
    
    // وضع Samsung DeX
    bool dex_mode;
    bool phone_as_touchpad;
};

// هيكل مدير الشاشات المتعددة
struct multi_display_manager {
    int initialized;
    
    // الشاشات
    struct display_info displays[4];
    int display_count;
    int primary_display;
    
    // الإعدادات
    struct display_config config;
    
    // الحالة
    bool dex_mode_active;
    
    // معاودات الاتصال
    void (*on_display_connected)(struct display_info* display);
    void (*on_display_disconnected)(uint32_t display_id);
    void (*on_display_mode_changed)(display_mode_t mode);
};

// دوال التهيئة
int multi_display_init(void);
void multi_display_terminate(void);

// اكتشاف الشاشات
int multi_display_detect_displays(void);
int multi_display_get_count(void);
struct display_info* multi_display_get(uint32_t id);
struct display_info* multi_display_get_primary(void);

// إدارة الشاشات
int multi_display_add(struct display_info* display);
void multi_display_remove(uint32_t id);
void multi_display_enable(uint32_t id, bool enable);
void multi_display_set_primary(uint32_t id);

// الأوضاع
void multi_display_set_mode(display_mode_t mode);
display_mode_t multi_display_get_mode(void);
void multi_display_set_dex_mode(bool enable);
bool multi_display_is_dex_mode(void);

// التكوين
void multi_display_set_config(const struct display_config* config);
const struct display_config* multi_display_get_config(void);
int multi_display_apply_config(void);

// التحجيم
void multi_display_set_scale(uint32_t display_id, float scale);
float multi_display_get_scale(uint32_t display_id);
void multi_display_auto_scale(uint32_t display_id);

// الدقة الديناميكية
void multi_display_set_resolution(uint32_t display_id, int32_t width, int32_t height);
void multi_display_get_resolution(uint32_t display_id, int32_t* width, int32_t* height);
void multi_display_auto_resolution(uint32_t display_id);

// HDR
void multi_display_enable_hdr(uint32_t display_id, bool enable);
bool multi_display_supports_hdr(uint32_t display_id);

// وضع اللمس كـ touchpad
void multi_display_set_phone_as_touchpad(bool enable);
bool multi_display_is_phone_as_touchpad(void);

// ترتيب الشاشات
void multi_display_arrange_horizontal(void);
void multi_display_arrange_vertical(void);
void multi_display_arrange_custom(int32_t* positions_x, int32_t* positions_y, int count);

// معاودات الاتصال
void multi_display_set_callbacks(
    void (*on_display_connected)(struct display_info* display),
    void (*on_display_disconnected)(uint32_t display_id),
    void (*on_display_mode_changed)(display_mode_t mode)
);

#ifdef __cplusplus
}
#endif

#endif // MULTI_DISPLAY_H
