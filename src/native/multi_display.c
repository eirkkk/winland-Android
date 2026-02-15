#include "multi_display.h"
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "MultiDisplay"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// المتغير العام
static struct multi_display_manager g_manager = {0};

// تهيئة مدير الشاشات المتعددة
int multi_display_init(void) {
    if (g_manager.initialized) {
        return 0;
    }
    
    memset(&g_manager, 0, sizeof(g_manager));
    
    // إعداد الشاشة الداخلية (الهاتف)
    struct display_info* internal = &g_manager.displays[0];
    internal->id = 0;
    strcpy(internal->name, "Internal Display");
    internal->port = DISPLAY_PORT_INTERNAL;
    internal->width = 1080;
    internal->height = 2400;
    internal->physical_width_mm = 68;
    internal->physical_height_mm = 151;
    internal->refresh_rate = 60;
    internal->dpi_x = 403;
    internal->dpi_y = 403;
    internal->scale_factor = 2.0f;
    internal->supports_hdr = false;
    internal->connected = true;
    internal->primary = true;
    internal->enabled = true;
    
    g_manager.display_count = 1;
    g_manager.primary_display = 0;
    g_manager.config.mode = DISPLAY_MODE_SINGLE;
    g_manager.dex_mode_active = false;
    
    g_manager.initialized = 1;
    LOGI("Multi-display manager initialized");
    return 0;
}

// إنهاء مدير الشاشات المتعددة
void multi_display_terminate(void) {
    if (!g_manager.initialized) return;
    
    g_manager.initialized = 0;
    LOGI("Multi-display manager terminated");
}

// اكتشاف الشاشات
int multi_display_detect_displays(void) {
    if (!g_manager.initialized) return -1;
    
    // TODO: Implement actual display detection via JNI
    // For now, just check for HDMI/USB-C
    
    LOGI("Detecting displays...");
    
    // محاكاة اكتشاف شاشة خارجية
    // في التطبيق الحقيقي، هذا سيكون عبر JNI
    
    return g_manager.display_count;
}

// الحصول على عدد الشاشات
int multi_display_get_count(void) {
    return g_manager.display_count;
}

// الحصول على شاشة
struct display_info* multi_display_get(uint32_t id) {
    if (!g_manager.initialized) return NULL;
    
    for (int i = 0; i < g_manager.display_count; i++) {
        if (g_manager.displays[i].id == id) {
            return &g_manager.displays[i];
        }
    }
    
    return NULL;
}

// الحصول على الشاشة الأساسية
struct display_info* multi_display_get_primary(void) {
    if (!g_manager.initialized || g_manager.primary_display >= g_manager.display_count) {
        return NULL;
    }
    
    return &g_manager.displays[g_manager.primary_display];
}

// إضافة شاشة
int multi_display_add(struct display_info* display) {
    if (!g_manager.initialized || !display) return -1;
    
    if (g_manager.display_count >= 4) {
        LOGE("Maximum displays reached");
        return -1;
    }
    
    // تعيين معرف فريد
    display->id = g_manager.display_count;
    
    memcpy(&g_manager.displays[g_manager.display_count], display, sizeof(*display));
    g_manager.display_count++;
    
    LOGI("Display added: %s (%dx%d)", display->name, display->width, display->height);
    
    if (g_manager.on_display_connected) {
        g_manager.on_display_connected(display);
    }
    
    return display->id;
}

// إزالة شاشة
void multi_display_remove(uint32_t id) {
    if (!g_manager.initialized) return;
    
    for (int i = 0; i < g_manager.display_count; i++) {
        if (g_manager.displays[i].id == id) {
            // إزالة بنقل العناصر
            for (int j = i; j < g_manager.display_count - 1; j++) {
                memcpy(&g_manager.displays[j], &g_manager.displays[j + 1], sizeof(struct display_info));
            }
            g_manager.display_count--;
            
            LOGI("Display removed: %u", id);
            
            if (g_manager.on_display_disconnected) {
                g_manager.on_display_disconnected(id);
            }
            
            return;
        }
    }
}

// تفعيل/تعطيل شاشة
void multi_display_enable(uint32_t id, bool enable) {
    struct display_info* display = multi_display_get(id);
    if (!display) return;
    
    display->enabled = enable;
    LOGI("Display %u %s", id, enable ? "enabled" : "disabled");
}

// تعيين الشاشة الأساسية
void multi_display_set_primary(uint32_t id) {
    struct display_info* display = multi_display_get(id);
    if (!display) return;
    
    // إلغاء تعيين القديمة
    for (int i = 0; i < g_manager.display_count; i++) {
        g_manager.displays[i].primary = false;
    }
    
    // تعيين الجديدة
    display->primary = true;
    g_manager.primary_display = id;
    
    LOGI("Primary display set to: %u", id);
}

// تعيين الوضع
void multi_display_set_mode(display_mode_t mode) {
    if (!g_manager.initialized) return;
    
    g_manager.config.mode = mode;
    
    switch (mode) {
        case DISPLAY_MODE_MIRROR:
            LOGI("Display mode: Mirror");
            break;
        case DISPLAY_MODE_EXTEND:
            LOGI("Display mode: Extend");
            break;
        case DISPLAY_MODE_SINGLE:
            LOGI("Display mode: Single");
            break;
    }
    
    multi_display_apply_config();
    
    if (g_manager.on_display_mode_changed) {
        g_manager.on_display_mode_changed(mode);
    }
}

// الحصول على الوضع
display_mode_t multi_display_get_mode(void) {
    return g_manager.config.mode;
}

// تعيين وضع DeX
void multi_display_set_dex_mode(bool enable) {
    if (!g_manager.initialized) return;
    
    g_manager.dex_mode_active = enable;
    g_manager.config.dex_mode = enable;
    
    if (enable) {
        LOGI("Samsung DeX mode activated");
        
        // تفعيل الشاشة الخارجية كأساسية
        for (int i = 0; i < g_manager.display_count; i++) {
            if (g_manager.displays[i].port == DISPLAY_PORT_HDMI ||
                g_manager.displays[i].port == DISPLAY_PORT_USB_C) {
                multi_display_set_primary(g_manager.displays[i].id);
                
                // تعيين دقة سطح المكتب
                multi_display_set_resolution(g_manager.displays[i].id, 1920, 1080);
                break;
            }
        }
    } else {
        LOGI("Samsung DeX mode deactivated");
        
        // إعادة الشاشة الداخلية كأساسية
        multi_display_set_primary(0);
    }
}

// التحقق من وضع DeX
bool multi_display_is_dex_mode(void) {
    return g_manager.dex_mode_active;
}

// تعيين التكوين
void multi_display_set_config(const struct display_config* config) {
    if (!config) return;
    memcpy(&g_manager.config, config, sizeof(*config));
}

// الحصول على التكوين
const struct display_config* multi_display_get_config(void) {
    return &g_manager.config;
}

// تطبيق التكوين
int multi_display_apply_config(void) {
    if (!g_manager.initialized) return -1;
    
    LOGI("Applying display configuration...");
    
    // TODO: Apply configuration via JNI to Android display manager
    
    return 0;
}

// تعيين التحجيم
void multi_display_set_scale(uint32_t display_id, float scale) {
    struct display_info* display = multi_display_get(display_id);
    if (!display) return;
    
    display->scale_factor = scale;
    LOGI("Display %u scale set to: %.2f", display_id, scale);
}

// الحصول على التحجيم
float multi_display_get_scale(uint32_t display_id) {
    struct display_info* display = multi_display_get(display_id);
    if (!display) return 1.0f;
    
    return display->scale_factor;
}

// تحجيم تلقائي
void multi_display_auto_scale(uint32_t display_id) {
    struct display_info* display = multi_display_get(display_id);
    if (!display) return;
    
    // حساب التحجيم بناءً على DPI
    float scale = display->dpi_x / 160.0f;  // 160 DPI is baseline
    
    // تقريب إلى أقرب 0.25
    scale = roundf(scale * 4.0f) / 4.0f;
    
    // تحديد الحدود
    if (scale < 1.0f) scale = 1.0f;
    if (scale > 4.0f) scale = 4.0f;
    
    multi_display_set_scale(display_id, scale);
}

// تعيين الدقة
void multi_display_set_resolution(uint32_t display_id, int32_t width, int32_t height) {
    struct display_info* display = multi_display_get(display_id);
    if (!display) return;
    
    display->width = width;
    display->height = height;
    
    LOGI("Display %u resolution set to: %dx%d", display_id, width, height);
}

// الحصول على الدقة
void multi_display_get_resolution(uint32_t display_id, int32_t* width, int32_t* height) {
    struct display_info* display = multi_display_get(display_id);
    if (!display) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }
    
    if (width) *width = display->width;
    if (height) *height = display->height;
}

// دقة تلقائية
void multi_display_auto_resolution(uint32_t display_id) {
    struct display_info* display = multi_display_get(display_id);
    if (!display) return;
    
    // TODO: Detect optimal resolution from EDID
    // For now, use common resolutions
    
    if (display->port == DISPLAY_PORT_HDMI || display->port == DISPLAY_PORT_USB_C) {
        // شاشة خارجية - استخدم 1080p
        multi_display_set_resolution(display_id, 1920, 1080);
    }
}

// تفعيل HDR
void multi_display_enable_hdr(uint32_t display_id, bool enable) {
    struct display_info* display = multi_display_get(display_id);
    if (!display) return;
    
    if (!display->supports_hdr) {
        LOGI("Display %u does not support HDR", display_id);
        return;
    }
    
    LOGI("Display %u HDR %s", display_id, enable ? "enabled" : "disabled");
}

// التحقق من دعم HDR
bool multi_display_supports_hdr(uint32_t display_id) {
    struct display_info* display = multi_display_get(display_id);
    if (!display) return false;
    
    return display->supports_hdr;
}

// تعيين الهاتف كـ touchpad
void multi_display_set_phone_as_touchpad(bool enable) {
    g_manager.config.phone_as_touchpad = enable;
    LOGI("Phone as touchpad: %s", enable ? "enabled" : "disabled");
}

// التحقق من وضع الهاتف كـ touchpad
bool multi_display_is_phone_as_touchpad(void) {
    return g_manager.config.phone_as_touchpad;
}

// ترتيب أفقي
void multi_display_arrange_horizontal(void) {
    if (!g_manager.initialized) return;
    
    int32_t x = 0;
    for (int i = 0; i < g_manager.display_count; i++) {
        if (g_manager.displays[i].enabled) {
            g_manager.config.displays[i].x = x;
            g_manager.config.displays[i].y = 0;
            x += g_manager.displays[i].width;
        }
    }
    
    LOGI("Displays arranged horizontally");
}

// ترتيب عمودي
void multi_display_arrange_vertical(void) {
    if (!g_manager.initialized) return;
    
    int32_t y = 0;
    for (int i = 0; i < g_manager.display_count; i++) {
        if (g_manager.displays[i].enabled) {
            g_manager.config.displays[i].x = 0;
            g_manager.config.displays[i].y = y;
            y += g_manager.displays[i].height;
        }
    }
    
    LOGI("Displays arranged vertically");
}

// ترتيب مخصص
void multi_display_arrange_custom(int32_t* positions_x, int32_t* positions_y, int count) {
    if (!g_manager.initialized || !positions_x || !positions_y) return;
    
    for (int i = 0; i < count && i < g_manager.display_count; i++) {
        g_manager.config.displays[i].x = positions_x[i];
        g_manager.config.displays[i].y = positions_y[i];
    }
    
    LOGI("Displays arranged custom");
}

// تعيين معاودات الاتصال
void multi_display_set_callbacks(
    void (*on_display_connected)(struct display_info* display),
    void (*on_display_disconnected)(uint32_t display_id),
    void (*on_display_mode_changed)(display_mode_t mode)
) {
    g_manager.on_display_connected = on_display_connected;
    g_manager.on_display_disconnected = on_display_disconnected;
    g_manager.on_display_mode_changed = on_display_mode_changed;
}
