#ifndef DISPLAY_FILTER_H
#define DISPLAY_FILTER_H

#include <stdint.h>
#include <stdbool.h>
#include <GLES2/gl2.h>

#ifdef __cplusplus
extern "C" {
#endif

// أنواع المرشحات
typedef enum {
    FILTER_NONE = 0,
    FILTER_NIGHT_MODE,          // وضع ليلي (تقليل الضوء الأزرق)
    FILTER_READING_MODE,        // وضع القراءة
    FILTER_COLOR_BLIND,         // وضع عمى الألوان
    FILTER_HIGH_CONTRAST,       // تباين عالٍ
    FILTER_GRAYSCALE,           // تدرج رمادي
    FILTER_INVERT,              // عكس الألوان
    FILTER_SEPIA,               // بني داكن
    FILTER_CUSTOM               // مخصص
} filter_type_t;

// أنواع عمى الألوان
typedef enum {
    COLOR_BLIND_NONE,
    COLOR_BLIND_DEUTERANOPIA,   // أخضر
    COLOR_BLIND_PROTANOPIA,     // أحمر
    COLOR_BLIND_TRITANOPIA      // أزرق
} color_blind_type_t;

// إعدادات المرشح
struct filter_config {
    filter_type_t type;
    bool enabled;
    float intensity;            // 0.0 - 1.0
    
    // وضع ليلي
    struct {
        float color_temperature; // 1000K - 6500K
        float blue_light_reduction; // 0.0 - 1.0
        float dimming;          // 0.0 - 1.0
        bool auto_schedule;
        int start_hour;         // 0-23
        int end_hour;           // 0-23
    } night_mode;
    
    // وضع القراءة
    struct {
        float sepia_intensity;
        float contrast;
        float sharpness;
    } reading_mode;
    
    // عمى الألوان
    struct {
        color_blind_type_t type;
        float severity;
    } color_blind;
    
    // تباين عالٍ
    struct {
        float contrast;
        float brightness;
    } high_contrast;
    
    // مخصص
    struct {
        float matrix[16];       // مصفوفة تحويل الألوان 4x4
    } custom;
};

// هيكل مدير المرشحات
struct display_filter {
    int initialized;
    
    // الإعدادات
    struct filter_config config;
    
    // OpenGL
    GLuint shader_program;
    GLuint vertex_shader;
    GLuint fragment_shader;
    GLuint fbo;
    GLuint texture;
    
    // الحالة
    bool active;
    float current_intensity;
    
    // الجدولة
    bool auto_enabled;
};

// دوال التهيئة
int display_filter_init(void);
void display_filter_terminate(void);

// التكوين
void display_filter_set_config(const struct filter_config* config);
const struct filter_config* display_filter_get_config(void);

// تفعيل/تعطيل
void display_filter_enable(bool enable);
bool display_filter_is_enabled(void);
void display_filter_set_intensity(float intensity);
float display_filter_get_intensity(void);

// أنواع المرشحات
void display_filter_set_type(filter_type_t type);
filter_type_t display_filter_get_type(void);
const char* display_filter_type_to_string(filter_type_t type);

// وضع ليلي
void display_filter_set_night_mode(bool enable);
void display_filter_set_color_temperature(float kelvin);
void display_filter_set_blue_light_reduction(float reduction);
void display_filter_set_night_schedule(bool auto_schedule, int start_hour, int end_hour);
bool display_filter_is_night_mode_active(void);

// وضع القراءة
void display_filter_set_reading_mode(bool enable);
void display_filter_set_sepia(float intensity);
void display_filter_set_sharpness(float sharpness);

// عمى الألوان
void display_filter_set_color_blind_mode(bool enable, color_blind_type_t type);
void display_filter_set_color_blind_severity(float severity);

// تباين عالٍ
void display_filter_set_high_contrast(bool enable);
void display_filter_set_contrast(float contrast);
void display_filter_set_brightness(float brightness);

// الرسم
void display_filter_apply(GLuint input_texture, GLuint output_framebuffer, int width, int height);
void display_filter_render_pass(GLuint texture, int width, int height);

// الجدولة
void display_filter_check_schedule(void);
void display_filter_enable_auto(bool enable);

// DPI/تحجيم
void display_filter_set_dpi_scaling(float scale);
void display_filter_enable_font_smoothing(bool enable);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_FILTER_H
