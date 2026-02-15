#ifndef DEBUG_OVERLAY_H
#define DEBUG_OVERLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#ifdef __cplusplus
extern "C" {
#endif

// أنواع معلومات العرض
typedef enum {
    DEBUG_INFO_NONE = 0,
    DEBUG_INFO_FPS = 1 << 0,
    DEBUG_INFO_MEMORY = 1 << 1,
    DEBUG_INFO_SURFACES = 1 << 2,
    DEBUG_INFO_INPUT = 1 << 3,
    DEBUG_INFO_RENDER = 1 << 4,
    DEBUG_INFO_WAYLAND = 1 << 5,
    DEBUG_INFO_ALL = 0xFF
} debug_info_type_t;

// هيكل الإحصائيات
struct debug_stats {
    // FPS
    float fps;
    float frame_time_ms;
    uint32_t frame_count;
    
    // الذاكرة
    size_t memory_used;
    size_t memory_total;
    size_t native_heap;
    
    // الأسطح
    uint32_t surface_count;
    uint32_t mapped_surfaces;
    
    // المدخلات
    uint32_t input_events_per_second;
    
    // الرندر
    uint32_t draw_calls;
    uint32_t vertices;
    
    // Wayland
    uint32_t connected_clients;
    uint32_t pending_messages;
};

// هيكل طبقة التصحيح
struct debug_overlay {
    int initialized;
    int enabled;
    
    // المعلومات المعروضة
    debug_info_type_t show_info;
    
    // الإحصائيات
    struct debug_stats stats;
    struct debug_stats prev_stats;
    
    // FPS calculation
    uint64_t last_frame_time;
    uint32_t frame_counter;
    float fps_accumulator;
    
    // OpenGL
    GLuint shader_program;
    GLuint vbo;
    GLuint texture;
    
    // Font rendering
    GLuint font_texture;
    int font_size;
    
    // Position
    int x, y;
    int width, height;
    
    // Colors
    float bg_color[4];
    float text_color[4];
    float accent_color[4];
};

// دوال التهيئة
int debug_overlay_init(void);
void debug_overlay_terminate(void);
void debug_overlay_enable(bool enable);
bool debug_overlay_is_enabled(void);

// إعدادات العرض
void debug_overlay_set_position(int x, int y);
void debug_overlay_set_size(int width, int height);
void debug_overlay_show_info(debug_info_type_t info);
void debug_overlay_hide_info(debug_info_type_t info);
void debug_overlay_toggle_info(debug_info_type_t info);

// تحديث الإحصائيات
void debug_overlay_update_fps(float delta_time);
void debug_overlay_update_memory(void);
void debug_overlay_update_surfaces(uint32_t count, uint32_t mapped);
void debug_overlay_update_input(uint32_t events);
void debug_overlay_update_render(uint32_t draw_calls, uint32_t vertices);
void debug_overlay_update_wayland(uint32_t clients, uint32_t pending);

// الرسم
void debug_overlay_render(void);
void debug_overlay_render_stats(void);
void debug_overlay_render_graph(void);

// المساعدة
void debug_overlay_draw_text(int x, int y, const char* text);
void debug_overlay_draw_rect(int x, int y, int w, int h, float color[4]);
void debug_overlay_draw_line(int x1, int y1, int x2, int y2, float color[4]);

// السجل
void debug_overlay_log(const char* format, ...);
void debug_overlay_clear_log(void);

// أدوات التصحيح
void debug_overlay_begin_frame(void);
void debug_overlay_end_frame(void);
void debug_overlay_mark_event(const char* name);

#ifdef __cplusplus
}
#endif

#endif // DEBUG_OVERLAY_H
