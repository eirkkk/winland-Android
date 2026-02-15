#include "display_filter.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <android/log.h>

#define LOG_TAG "DisplayFilter"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// شادر المرشح (Fragment Shader)
static const char* filter_fragment_shader =
    "precision mediump float;\n"
    "varying vec2 v_texCoord;\n"
    "uniform sampler2D u_texture;\n"
    "uniform mat4 u_colorMatrix;\n"
    "uniform float u_intensity;\n"
    "\n"
    "void main() {\n"
    "    vec4 color = texture2D(u_texture, v_texCoord);\n"
    "    vec4 filtered = u_colorMatrix * color;\n"
    "    gl_FragColor = mix(color, filtered, u_intensity);\n"
    "}\n";

// مصفوفات التحويل
static const float night_mode_matrix[] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.9f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.7f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

static const float grayscale_matrix[] = {
    0.299f, 0.587f, 0.114f, 0.0f,
    0.299f, 0.587f, 0.114f, 0.0f,
    0.299f, 0.587f, 0.114f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

static const float sepia_matrix[] = {
    0.393f, 0.769f, 0.189f, 0.0f,
    0.349f, 0.686f, 0.168f, 0.0f,
    0.272f, 0.534f, 0.131f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

static const float invert_matrix[] = {
    -1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, -1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, -1.0f, 0.0f,
    1.0f, 1.0f, 1.0f, 1.0f
};

// المتغير العام
static struct display_filter g_filter = {0};

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

// تهيئة مدير المرشحات
int display_filter_init(void) {
    if (g_filter.initialized) {
        return 0;
    }
    
    memset(&g_filter, 0, sizeof(g_filter));
    
    // الإعدادات الافتراضية
    g_filter.config.type = FILTER_NONE;
    g_filter.config.enabled = false;
    g_filter.config.intensity = 0.5f;
    
    g_filter.config.night_mode.color_temperature = 3000.0f;
    g_filter.config.night_mode.blue_light_reduction = 0.5f;
    g_filter.config.night_mode.dimming = 0.2f;
    g_filter.config.night_mode.auto_schedule = false;
    g_filter.config.night_mode.start_hour = 22;
    g_filter.config.night_mode.end_hour = 6;
    
    g_filter.config.reading_mode.sepia_intensity = 0.3f;
    g_filter.config.reading_mode.contrast = 1.1f;
    g_filter.config.reading_mode.sharpness = 1.2f;
    
    g_filter.config.color_blind.type = COLOR_BLIND_NONE;
    g_filter.config.color_blind.severity = 1.0f;
    
    g_filter.config.high_contrast.contrast = 1.5f;
    g_filter.config.high_contrast.brightness = 1.1f;
    
    // تجميع الشادرات
    g_filter.vertex_shader = compile_shader(GL_VERTEX_SHADER,
        "attribute vec2 a_position;\n"
        "attribute vec2 a_texCoord;\n"
        "varying vec2 v_texCoord;\n"
        "void main() {\n"
        "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
        "    v_texCoord = a_texCoord;\n"
        "}\n");
    
    g_filter.fragment_shader = compile_shader(GL_FRAGMENT_SHADER, filter_fragment_shader);
    
    if (!g_filter.vertex_shader || !g_filter.fragment_shader) {
        LOGE("Failed to compile shaders");
        return -1;
    }
    
    g_filter.shader_program = glCreateProgram();
    glAttachShader(g_filter.shader_program, g_filter.vertex_shader);
    glAttachShader(g_filter.shader_program, g_filter.fragment_shader);
    glLinkProgram(g_filter.shader_program);
    
    GLint linked;
    glGetProgramiv(g_filter.shader_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(g_filter.shader_program, sizeof(log), NULL, log);
        LOGE("Program linking failed: %s", log);
        return -1;
    }
    
    g_filter.initialized = 1;
    LOGI("Display filter initialized");
    return 0;
}

// إنهاء مدير المرشحات
void display_filter_terminate(void) {
    if (!g_filter.initialized) return;
    
    if (g_filter.shader_program) {
        glDeleteProgram(g_filter.shader_program);
    }
    if (g_filter.vertex_shader) {
        glDeleteShader(g_filter.vertex_shader);
    }
    if (g_filter.fragment_shader) {
        glDeleteShader(g_filter.fragment_shader);
    }
    if (g_filter.fbo) {
        glDeleteFramebuffers(1, &g_filter.fbo);
    }
    if (g_filter.texture) {
        glDeleteTextures(1, &g_filter.texture);
    }
    
    memset(&g_filter, 0, sizeof(g_filter));
    LOGI("Display filter terminated");
}

// تعيين الإعدادات
void display_filter_set_config(const struct filter_config* config) {
    if (!config) return;
    memcpy(&g_filter.config, config, sizeof(*config));
}

// الحصول على الإعدادات
const struct filter_config* display_filter_get_config(void) {
    return &g_filter.config;
}

// تفعيل/تعطيل
void display_filter_enable(bool enable) {
    g_filter.config.enabled = enable;
    g_filter.active = enable;
    LOGI("Display filter %s", enable ? "enabled" : "disabled");
}

// التحقق من التفعيل
bool display_filter_is_enabled(void) {
    return g_filter.config.enabled;
}

// تعيين الشدة
void display_filter_set_intensity(float intensity) {
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;
    g_filter.config.intensity = intensity;
}

// الحصول على الشدة
float display_filter_get_intensity(void) {
    return g_filter.config.intensity;
}

// تعيين النوع
void display_filter_set_type(filter_type_t type) {
    g_filter.config.type = type;
    LOGI("Filter type set to: %s", display_filter_type_to_string(type));
}

// الحصول على النوع
filter_type_t display_filter_get_type(void) {
    return g_filter.config.type;
}

// تحويل النوع إلى نص
const char* display_filter_type_to_string(filter_type_t type) {
    switch (type) {
        case FILTER_NONE: return "None";
        case FILTER_NIGHT_MODE: return "Night Mode";
        case FILTER_READING_MODE: return "Reading Mode";
        case FILTER_COLOR_BLIND: return "Color Blind";
        case FILTER_HIGH_CONTRAST: return "High Contrast";
        case FILTER_GRAYSCALE: return "Grayscale";
        case FILTER_INVERT: return "Invert";
        case FILTER_SEPIA: return "Sepia";
        case FILTER_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

// تعيين وضع ليلي
void display_filter_set_night_mode(bool enable) {
    if (enable) {
        g_filter.config.type = FILTER_NIGHT_MODE;
        g_filter.config.enabled = true;
    } else if (g_filter.config.type == FILTER_NIGHT_MODE) {
        g_filter.config.enabled = false;
    }
    LOGI("Night mode %s", enable ? "enabled" : "disabled");
}

// تعيين درجة حرارة اللون
void display_filter_set_color_temperature(float kelvin) {
    g_filter.config.night_mode.color_temperature = kelvin;
    LOGI("Color temperature set to: %.0fK", kelvin);
}

// تعيين تقليل الضوء الأزرق
void display_filter_set_blue_light_reduction(float reduction) {
    g_filter.config.night_mode.blue_light_reduction = reduction;
}

// تعيين جدولة وضع ليلي
void display_filter_set_night_schedule(bool auto_schedule, int start_hour, int end_hour) {
    g_filter.config.night_mode.auto_schedule = auto_schedule;
    g_filter.config.night_mode.start_hour = start_hour;
    g_filter.config.night_mode.end_hour = end_hour;
    LOGI("Night schedule: %s (%02d:00 - %02d:00)",
         auto_schedule ? "auto" : "manual", start_hour, end_hour);
}

// التحقق من تفعيل وضع ليلي
bool display_filter_is_night_mode_active(void) {
    if (!g_filter.config.night_mode.auto_schedule) {
        return g_filter.config.enabled && g_filter.config.type == FILTER_NIGHT_MODE;
    }
    
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    int hour = tm_info->tm_hour;
    
    int start = g_filter.config.night_mode.start_hour;
    int end = g_filter.config.night_mode.end_hour;
    
    if (start <= end) {
        return hour >= start && hour < end;
    } else {
        // عبر منتصف الليل
        return hour >= start || hour < end;
    }
}

// تعيين وضع القراءة
void display_filter_set_reading_mode(bool enable) {
    if (enable) {
        g_filter.config.type = FILTER_READING_MODE;
        g_filter.config.enabled = true;
    } else if (g_filter.config.type == FILTER_READING_MODE) {
        g_filter.config.enabled = false;
    }
    LOGI("Reading mode %s", enable ? "enabled" : "disabled");
}

// تعيين Sepia
void display_filter_set_sepia(float intensity) {
    g_filter.config.reading_mode.sepia_intensity = intensity;
}

// تعيين الحدة
void display_filter_set_sharpness(float sharpness) {
    g_filter.config.reading_mode.sharpness = sharpness;
}

// تعيين وضع عمى الألوان
void display_filter_set_color_blind_mode(bool enable, color_blind_type_t type) {
    if (enable) {
        g_filter.config.type = FILTER_COLOR_BLIND;
        g_filter.config.color_blind.type = type;
        g_filter.config.enabled = true;
    } else if (g_filter.config.type == FILTER_COLOR_BLIND) {
        g_filter.config.enabled = false;
    }
    LOGI("Color blind mode %s (type: %d)", enable ? "enabled" : "disabled", type);
}

// تعيين شدة عمى الألوان
void display_filter_set_color_blind_severity(float severity) {
    g_filter.config.color_blind.severity = severity;
}

// تعيين تباين عالٍ
void display_filter_set_high_contrast(bool enable) {
    if (enable) {
        g_filter.config.type = FILTER_HIGH_CONTRAST;
        g_filter.config.enabled = true;
    } else if (g_filter.config.type == FILTER_HIGH_CONTRAST) {
        g_filter.config.enabled = false;
    }
    LOGI("High contrast %s", enable ? "enabled" : "disabled");
}

// تعيين التباين
void display_filter_set_contrast(float contrast) {
    g_filter.config.high_contrast.contrast = contrast;
}

// تعيين السطوع
void display_filter_set_brightness(float brightness) {
    g_filter.config.high_contrast.brightness = brightness;
}

// الحصول على مصفوفة التحويل
static const float* get_color_matrix(filter_type_t type) {
    switch (type) {
        case FILTER_NIGHT_MODE:
        case FILTER_READING_MODE:
            return sepia_matrix;
        case FILTER_GRAYSCALE:
            return grayscale_matrix;
        case FILTER_INVERT:
            return invert_matrix;
        case FILTER_SEPIA:
            return sepia_matrix;
        default:
            return NULL;
    }
}

// تطبيق المرشح
void display_filter_apply(GLuint input_texture, GLuint output_framebuffer, int width, int height) {
    if (!g_filter.initialized || !g_filter.config.enabled) return;
    
    const float* matrix = get_color_matrix(g_filter.config.type);
    if (!matrix) return;
    
    glBindFramebuffer(GL_FRAMEBUFFER, output_framebuffer);
    glViewport(0, 0, width, height);
    
    glUseProgram(g_filter.shader_program);
    
    GLint matrix_loc = glGetUniformLocation(g_filter.shader_program, "u_colorMatrix");
    GLint intensity_loc = glGetUniformLocation(g_filter.shader_program, "u_intensity");
    GLint texture_loc = glGetUniformLocation(g_filter.shader_program, "u_texture");
    
    glUniformMatrix4fv(matrix_loc, 1, GL_FALSE, matrix);
    glUniform1f(intensity_loc, g_filter.config.intensity);
    glUniform1i(texture_loc, 0);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, input_texture);
    
    // رسم المستطيل الكامل
    GLfloat vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
    };
    
    GLint position_loc = glGetAttribLocation(g_filter.shader_program, "a_position");
    GLint texCoord_loc = glGetAttribLocation(g_filter.shader_program, "a_texCoord");
    
    glVertexAttribPointer(position_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices);
    glVertexAttribPointer(texCoord_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices + 2);
    
    glEnableVertexAttribArray(position_loc);
    glEnableVertexAttribArray(texCoord_loc);
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glDisableVertexAttribArray(position_loc);
    glDisableVertexAttribArray(texCoord_loc);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// التحقق من الجدولة
void display_filter_check_schedule(void) {
    if (!g_filter.config.night_mode.auto_schedule) return;
    
    bool should_enable = display_filter_is_night_mode_active();
    
    if (should_enable && !g_filter.config.enabled) {
        display_filter_set_night_mode(true);
        LOGI("Night mode auto-enabled");
    } else if (!should_enable && g_filter.config.enabled && g_filter.config.type == FILTER_NIGHT_MODE) {
        display_filter_enable(false);
        LOGI("Night mode auto-disabled");
    }
}

// تفعيل الجدولة التلقائية
void display_filter_enable_auto(bool enable) {
    g_filter.auto_enabled = enable;
}

// تعيين تحجيم DPI
void display_filter_set_dpi_scaling(float scale) {
    // TODO: Implement DPI scaling
    LOGI("DPI scaling set to: %.2f", scale);
}

// تفعيل تنعيم الخط
void display_filter_enable_font_smoothing(bool enable) {
    // TODO: Implement font smoothing
    LOGI("Font smoothing %s", enable ? "enabled" : "disabled");
}
