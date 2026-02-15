#include "debug_overlay.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <android/log.h>
#include <sys/resource.h>
#include <malloc.h>

#define LOG_TAG "DebugOverlay"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define MAX_LOG_LINES 100
#define LOG_LINE_LENGTH 256

// المتغير العام
static struct debug_overlay g_overlay = {0};
static char g_log_buffer[MAX_LOG_LINES][LOG_LINE_LENGTH];
static int g_log_line_count = 0;
static int g_log_current_line = 0;

// شادرات OpenGL ES 2.0
static const char* vertex_shader =
    "attribute vec2 a_position;\n"
    "attribute vec2 a_texCoord;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "    v_texCoord = a_texCoord;\n"
    "}\n";

static const char* fragment_shader =
    "precision mediump float;\n"
    "varying vec2 v_texCoord;\n"
    "uniform sampler2D u_texture;\n"
    "uniform vec4 u_color;\n"
    "void main() {\n"
    "    vec4 texColor = texture2D(u_texture, v_texCoord);\n"
    "    gl_FragColor = u_color * texColor;\n"
    "}\n";

// تجميع الشادر
static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        LOGE("Shader compilation failed: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

// تهيئة طبقة التصحيح
int debug_overlay_init(void) {
    if (g_overlay.initialized) {
        return 0;
    }
    
    memset(&g_overlay, 0, sizeof(g_overlay));
    
    // إعدادات افتراضية
    g_overlay.enabled = 0;
    g_overlay.show_info = DEBUG_INFO_FPS | DEBUG_INFO_MEMORY;
    g_overlay.x = 10;
    g_overlay.y = 10;
    g_overlay.width = 300;
    g_overlay.height = 200;
    g_overlay.font_size = 14;
    
    // ألوان
    g_overlay.bg_color[0] = 0.0f;
    g_overlay.bg_color[1] = 0.0f;
    g_overlay.bg_color[2] = 0.0f;
    g_overlay.bg_color[3] = 0.7f;
    
    g_overlay.text_color[0] = 1.0f;
    g_overlay.text_color[1] = 1.0f;
    g_overlay.text_color[2] = 1.0f;
    g_overlay.text_color[3] = 1.0f;
    
    g_overlay.accent_color[0] = 0.3f;
    g_overlay.accent_color[1] = 0.7f;
    g_overlay.accent_color[2] = 1.0f;
    g_overlay.accent_color[3] = 1.0f;
    
    // تجميع الشادرات
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader);
    
    if (!vs || !fs) {
        LOGE("Failed to compile shaders");
        return -1;
    }
    
    g_overlay.shader_program = glCreateProgram();
    glAttachShader(g_overlay.shader_program, vs);
    glAttachShader(g_overlay.shader_program, fs);
    glLinkProgram(g_overlay.shader_program);
    
    GLint linked;
    glGetProgramiv(g_overlay.shader_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(g_overlay.shader_program, sizeof(log), NULL, log);
        LOGE("Program linking failed: %s", log);
        glDeleteProgram(g_overlay.shader_program);
        return -1;
    }
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    // إنشاء VBO
    glGenBuffers(1, &g_overlay.vbo);
    
    g_overlay.initialized = 1;
    LOGI("Debug overlay initialized");
    return 0;
}

// إنهاء طبقة التصحيح
void debug_overlay_terminate(void) {
    if (!g_overlay.initialized) return;
    
    if (g_overlay.shader_program) {
        glDeleteProgram(g_overlay.shader_program);
    }
    if (g_overlay.vbo) {
        glDeleteBuffers(1, &g_overlay.vbo);
    }
    if (g_overlay.font_texture) {
        glDeleteTextures(1, &g_overlay.font_texture);
    }
    
    memset(&g_overlay, 0, sizeof(g_overlay));
    LOGI("Debug overlay terminated");
}

// تفعيل/تعطيل
void debug_overlay_enable(bool enable) {
    g_overlay.enabled = enable;
    LOGI("Debug overlay %s", enable ? "enabled" : "disabled");
}

// التحقق من التفعيل
bool debug_overlay_is_enabled(void) {
    return g_overlay.enabled;
}

// تعيين الموقع
void debug_overlay_set_position(int x, int y) {
    g_overlay.x = x;
    g_overlay.y = y;
}

// تعيين الحجم
void debug_overlay_set_size(int width, int height) {
    g_overlay.width = width;
    g_overlay.height = height;
}

// إظهار معلومات
void debug_overlay_show_info(debug_info_type_t info) {
    g_overlay.show_info |= info;
}

// إخفاء معلومات
void debug_overlay_hide_info(debug_info_type_t info) {
    g_overlay.show_info &= ~info;
}

// تبديل معلومات
void debug_overlay_toggle_info(debug_info_type_t info) {
    g_overlay.show_info ^= info;
}

// تحديث FPS
void debug_overlay_update_fps(float delta_time) {
    g_overlay.frame_counter++;
    g_overlay.fps_accumulator += delta_time;
    
    if (g_overlay.fps_accumulator >= 1.0f) {
        g_overlay.stats.fps = g_overlay.frame_counter / g_overlay.fps_accumulator;
        g_overlay.stats.frame_time_ms = (g_overlay.fps_accumulator / g_overlay.frame_counter) * 1000.0f;
        g_overlay.stats.frame_count = g_overlay.frame_counter;
        
        g_overlay.frame_counter = 0;
        g_overlay.fps_accumulator = 0.0f;
    }
}

// تحديث الذاكرة
void debug_overlay_update_memory(void) {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        g_overlay.stats.memory_used = usage.ru_maxrss * 1024; // Convert to bytes
    }
    
    // الحصول على حجم الكومة الأصلية
    struct mallinfo mi = mallinfo();
    g_overlay.stats.native_heap = mi.uordblks;
}

// تحديث الأسطح
void debug_overlay_update_surfaces(uint32_t count, uint32_t mapped) {
    g_overlay.stats.surface_count = count;
    g_overlay.stats.mapped_surfaces = mapped;
}

// تحديث المدخلات
void debug_overlay_update_input(uint32_t events) {
    g_overlay.stats.input_events_per_second = events;
}

// تحديث الرندر
void debug_overlay_update_render(uint32_t draw_calls, uint32_t vertices) {
    g_overlay.stats.draw_calls = draw_calls;
    g_overlay.stats.vertices = vertices;
}

// تحديث Wayland
void debug_overlay_update_wayland(uint32_t clients, uint32_t pending) {
    g_overlay.stats.connected_clients = clients;
    g_overlay.stats.pending_messages = pending;
}

// رسم الخلفية
static void draw_background(void) {
    float vertices[] = {
        // المواقع
        -1.0f + (2.0f * g_overlay.x / 1920.0f), 
        1.0f - (2.0f * g_overlay.y / 1080.0f),
        
        -1.0f + (2.0f * (g_overlay.x + g_overlay.width) / 1920.0f),
        1.0f - (2.0f * g_overlay.y / 1080.0f),
        
        -1.0f + (2.0f * g_overlay.x / 1920.0f),
        1.0f - (2.0f * (g_overlay.y + g_overlay.height) / 1080.0f),
        
        -1.0f + (2.0f * (g_overlay.x + g_overlay.width) / 1920.0f),
        1.0f - (2.0f * (g_overlay.y + g_overlay.height) / 1080.0f),
    };
    
    glUseProgram(g_overlay.shader_program);
    
    GLint position_loc = glGetAttribLocation(g_overlay.shader_program, "a_position");
    glEnableVertexAttribArray(position_loc);
    glVertexAttribPointer(position_loc, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    
    GLint color_loc = glGetUniformLocation(g_overlay.shader_program, "u_color");
    glUniform4fv(color_loc, 1, g_overlay.bg_color);
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glDisableVertexAttribArray(position_loc);
}

// رسم النص (مبسط)
static void draw_text_simple(int x, int y, const char* text) {
    // TODO: Implement proper text rendering
    // For now, just log the text
    LOGI("[Overlay] %s", text);
}

// رسم الإحصائيات
void debug_overlay_render_stats(void) {
    if (!g_overlay.enabled) return;
    
    int line_height = 18;
    int y = g_overlay.y + 10;
    char buffer[256];
    
    // العنوان
    draw_text_simple(g_overlay.x + 10, y, "=== Winland Debug ===");
    y += line_height * 2;
    
    // FPS
    if (g_overlay.show_info & DEBUG_INFO_FPS) {
        snprintf(buffer, sizeof(buffer), "FPS: %.1f (%.2f ms)", 
                 g_overlay.stats.fps, g_overlay.stats.frame_time_ms);
        draw_text_simple(g_overlay.x + 10, y, buffer);
        y += line_height;
    }
    
    // الذاكرة
    if (g_overlay.show_info & DEBUG_INFO_MEMORY) {
        snprintf(buffer, sizeof(buffer), "Memory: %.1f MB (Heap: %.1f MB)",
                 g_overlay.stats.memory_used / (1024.0f * 1024.0f),
                 g_overlay.stats.native_heap / (1024.0f * 1024.0f));
        draw_text_simple(g_overlay.x + 10, y, buffer);
        y += line_height;
    }
    
    // الأسطح
    if (g_overlay.show_info & DEBUG_INFO_SURFACES) {
        snprintf(buffer, sizeof(buffer), "Surfaces: %u (Mapped: %u)",
                 g_overlay.stats.surface_count, g_overlay.stats.mapped_surfaces);
        draw_text_simple(g_overlay.x + 10, y, buffer);
        y += line_height;
    }
    
    // المدخلات
    if (g_overlay.show_info & DEBUG_INFO_INPUT) {
        snprintf(buffer, sizeof(buffer), "Input: %u events/s",
                 g_overlay.stats.input_events_per_second);
        draw_text_simple(g_overlay.x + 10, y, buffer);
        y += line_height;
    }
    
    // الرندر
    if (g_overlay.show_info & DEBUG_INFO_RENDER) {
        snprintf(buffer, sizeof(buffer), "Render: %u draw calls, %u vertices",
                 g_overlay.stats.draw_calls, g_overlay.stats.vertices);
        draw_text_simple(g_overlay.x + 10, y, buffer);
        y += line_height;
    }
    
    // Wayland
    if (g_overlay.show_info & DEBUG_INFO_WAYLAND) {
        snprintf(buffer, sizeof(buffer), "Wayland: %u clients, %u pending",
                 g_overlay.stats.connected_clients, g_overlay.stats.pending_messages);
        draw_text_simple(g_overlay.x + 10, y, buffer);
        y += line_height;
    }
}

// الرسم الرئيسي
void debug_overlay_render(void) {
    if (!g_overlay.enabled || !g_overlay.initialized) return;
    
    // حفظ الحالة
    GLint prev_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
    
    // رسم الخلفية
    draw_background();
    
    // رسم الإحصائيات
    debug_overlay_render_stats();
    
    // استعادة الحالة
    glUseProgram(prev_program);
}

// إضافة إلى السجل
void debug_overlay_log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[LOG_LINE_LENGTH];
    vsnprintf(buffer, sizeof(buffer), format, args);
    
    va_end(args);
    
    // نسخ إلى المخزن المؤقت
    strncpy(g_log_buffer[g_log_current_line], buffer, LOG_LINE_LENGTH - 1);
    g_log_buffer[g_log_current_line][LOG_LINE_LENGTH - 1] = '\0';
    
    g_log_current_line = (g_log_current_line + 1) % MAX_LOG_LINES;
    if (g_log_line_count < MAX_LOG_LINES) {
        g_log_line_count++;
    }
    
    // طباعة إلى اللوج كات
    LOGI("[Overlay] %s", buffer);
}

// مسح السجل
void debug_overlay_clear_log(void) {
    g_log_line_count = 0;
    g_log_current_line = 0;
    memset(g_log_buffer, 0, sizeof(g_log_buffer));
}

// بداية الإطار
void debug_overlay_begin_frame(void) {
    if (!g_overlay.enabled) return;
    
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    g_overlay.last_frame_time = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// نهاية الإطار
void debug_overlay_end_frame(void) {
    if (!g_overlay.enabled) return;
    
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    
    float delta_time = (now - g_overlay.last_frame_time) / 1000000000.0f;
    debug_overlay_update_fps(delta_time);
    
    // تحديث الذاكرة كل ثانية
    static float memory_update_timer = 0;
    memory_update_timer += delta_time;
    if (memory_update_timer >= 1.0f) {
        debug_overlay_update_memory();
        memory_update_timer = 0;
    }
}

// تحديد حدث
void debug_overlay_mark_event(const char* name) {
    if (!g_overlay.enabled) return;
    
    debug_overlay_log("Event: %s", name);
}
