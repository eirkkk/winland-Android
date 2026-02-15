#include "egl_display.h"
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "EGLDisplay"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// المتغير العام لعرض EGL
static struct egl_display g_egl = {0};

// شادرات OpenGL ES 2.0
static const char *vertex_shader_source =
    "attribute vec4 a_position;\n"
    "attribute vec2 a_texCoord;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "    gl_Position = a_position;\n"
    "    v_texCoord = a_texCoord;\n"
    "}\n";

static const char *fragment_shader_source =
    "precision mediump float;\n"
    "varying vec2 v_texCoord;\n"
    "uniform sampler2D s_texture;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(s_texture, v_texCoord);\n"
    "}\n";

static GLuint g_shader_program = 0;
static GLuint g_position_loc = 0;
static GLuint g_texCoord_loc = 0;
static GLuint g_texture_loc = 0;

// تجميع الشادر
static GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    if (!shader) {
        LOGE("Failed to create shader");
        return 0;
    }
    
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint info_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1) {
            char *info_log = malloc(info_len);
            glGetShaderInfoLog(shader, info_len, NULL, info_log);
            LOGE("Shader compilation failed: %s", info_log);
            free(info_log);
        }
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

// إعداد الشادرات
int egl_setup_shaders(void) {
    if (g_shader_program) return 0;
    
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    
    if (!vertex_shader || !fragment_shader) {
        LOGE("Failed to compile shaders");
        return -1;
    }
    
    g_shader_program = glCreateProgram();
    if (!g_shader_program) {
        LOGE("Failed to create shader program");
        return -1;
    }
    
    glAttachShader(g_shader_program, vertex_shader);
    glAttachShader(g_shader_program, fragment_shader);
    glLinkProgram(g_shader_program);
    
    GLint linked;
    glGetProgramiv(g_shader_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint info_len = 0;
        glGetProgramiv(g_shader_program, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1) {
            char *info_log = malloc(info_len);
            glGetProgramInfoLog(g_shader_program, info_len, NULL, info_log);
            LOGE("Shader program linking failed: %s", info_log);
            free(info_log);
        }
        glDeleteProgram(g_shader_program);
        g_shader_program = 0;
        return -1;
    }
    
    // الحصول على مواقع المتغيرات
    g_position_loc = glGetAttribLocation(g_shader_program, "a_position");
    g_texCoord_loc = glGetAttribLocation(g_shader_program, "a_texCoord");
    g_texture_loc = glGetUniformLocation(g_shader_program, "s_texture");
    
    LOGI("Shaders setup successfully");
    return 0;
}

// تهيئة عرض EGL
int egl_display_init(int width, int height) {
    if (g_egl.initialized) {
        LOGI("EGL already initialized");
        return 0;
    }
    
    LOGI("Initializing EGL display - Size: %dx%d", width, height);
    
    // الحصول على العرض الافتراضي
    g_egl.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_egl.display == EGL_NO_DISPLAY) {
        LOGE("Failed to get EGL display");
        return -1;
    }
    
    // تهيئة EGL
    EGLint major, minor;
    if (!eglInitialize(g_egl.display, &major, &minor)) {
        LOGE("Failed to initialize EGL");
        return -1;
    }
    
    LOGI("EGL version: %d.%d", major, minor);
    
    // اختيار الإعدادات
    EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_NONE
    };
    
    EGLint num_configs;
    if (!eglChooseConfig(g_egl.display, attribs, &g_egl.config, 1, &num_configs)) {
        LOGE("Failed to choose EGL config");
        return -1;
    }
    
    if (num_configs < 1) {
        LOGE("No suitable EGL config found");
        return -1;
    }
    
    // إنشاء سياق OpenGL ES 2.0
    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    
    g_egl.context = eglCreateContext(
        g_egl.display,
        g_egl.config,
        EGL_NO_CONTEXT,
        context_attribs
    );
    
    if (g_egl.context == EGL_NO_CONTEXT) {
        LOGE("Failed to create EGL context");
        return -1;
    }
    
    g_egl.width = width;
    g_egl.height = height;
    g_egl.initialized = 1;
    
    LOGI("EGL initialized successfully");
    return 0;
}

// تعيين النافذة
int egl_display_set_window(ANativeWindow *window) {
    if (!g_egl.initialized) {
        LOGE("EGL not initialized");
        return -1;
    }
    
    if (g_egl.surface != EGL_NO_SURFACE) {
        eglDestroySurface(g_egl.display, g_egl.surface);
        g_egl.surface = EGL_NO_SURFACE;
    }
    
    if (g_egl.window) {
        ANativeWindow_release(g_egl.window);
    }
    
    g_egl.window = window;
    
    if (window) {
        // تعيين تنسيق النافذة
        EGLint format;
        eglGetConfigAttrib(g_egl.display, g_egl.config, EGL_NATIVE_VISUAL_ID, &format);
        ANativeWindow_setBuffersGeometry(window, 0, 0, format);
        
        // إنشاء سطح النافذة
        g_egl.surface = eglCreateWindowSurface(
            g_egl.display,
            g_egl.config,
            window,
            NULL
        );
        
        if (g_egl.surface == EGL_NO_SURFACE) {
            LOGE("Failed to create window surface");
            return -1;
        }
        
        // جعل السياق الحالي
        if (!eglMakeCurrent(g_egl.display, g_egl.surface, g_egl.surface, g_egl.context)) {
            LOGE("Failed to make EGL context current");
            return -1;
        }
        
        // إعداد الشادرات
        if (egl_setup_shaders() != 0) {
            LOGE("Failed to setup shaders");
            return -1;
        }
        
        // تعيين منفذ العرض
        glViewport(0, 0, g_egl.width, g_egl.height);
    }
    
    LOGI("EGL window set successfully");
    return 0;
}

// جعل السياق الحالي
int egl_display_make_current(void) {
    if (!g_egl.initialized || g_egl.surface == EGL_NO_SURFACE) {
        return -1;
    }
    
    if (!eglMakeCurrent(g_egl.display, g_egl.surface, g_egl.surface, g_egl.context)) {
        LOGE("Failed to make context current");
        return -1;
    }
    
    return 0;
}

// تبديل المخازن المؤقتة
int egl_display_swap_buffers(void) {
    if (!g_egl.initialized || g_egl.surface == EGL_NO_SURFACE) {
        return -1;
    }
    
    if (!eglSwapBuffers(g_egl.display, g_egl.surface)) {
        LOGE("Failed to swap buffers");
        return -1;
    }
    
    return 0;
}

// الحصول على الأبعاد
void egl_display_get_size(int *width, int *height) {
    if (width) *width = g_egl.width;
    if (height) *height = g_egl.height;
}

// مسح الشاشة
void egl_clear_screen(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// إنشاء نسيج من مخزن مؤقت
uint32_t egl_create_texture_from_buffer(void *buffer, int width, int height) {
    if (!buffer || width <= 0 || height <= 0) return 0;
    
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        buffer
    );
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return texture;
}

// تدمير النسيج
void egl_destroy_texture(uint32_t texture_id) {
    if (texture_id) {
        GLuint tex = texture_id;
        glDeleteTextures(1, &tex);
    }
}

// عرض المخزن المؤقت
void egl_display_present_buffer(uint32_t texture_id, int width, int height) {
    if (!texture_id || !g_shader_program) return;
    
    glUseProgram(g_shader_program);
    
    // إعداد الرؤوس
    GLfloat vertices[] = {
        // المواقع          // إحداثيات النسيج
        -1.0f, -1.0f,       0.0f, 1.0f,
         1.0f, -1.0f,       1.0f, 1.0f,
        -1.0f,  1.0f,       0.0f, 0.0f,
         1.0f,  1.0f,       1.0f, 0.0f,
    };
    
    glVertexAttribPointer(g_position_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices);
    glVertexAttribPointer(g_texCoord_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices + 2);
    
    glEnableVertexAttribArray(g_position_loc);
    glEnableVertexAttribArray(g_texCoord_loc);
    
    // ربط النسيج
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glUniform1i(g_texture_loc, 0);
    
    // الرسم
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glDisableVertexAttribArray(g_position_loc);
    glDisableVertexAttribArray(g_texCoord_loc);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// إنهاء EGL
void egl_display_terminate(void) {
    if (!g_egl.initialized) return;
    
    LOGI("Terminating EGL display");
    
    if (g_shader_program) {
        glDeleteProgram(g_shader_program);
        g_shader_program = 0;
    }
    
    eglMakeCurrent(g_egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    
    if (g_egl.surface != EGL_NO_SURFACE) {
        eglDestroySurface(g_egl.display, g_egl.surface);
        g_egl.surface = EGL_NO_SURFACE;
    }
    
    if (g_egl.context != EGL_NO_CONTEXT) {
        eglDestroyContext(g_egl.display, g_egl.context);
        g_egl.context = EGL_NO_CONTEXT;
    }
    
    eglTerminate(g_egl.display);
    g_egl.display = EGL_NO_DISPLAY;
    
    if (g_egl.window) {
        ANativeWindow_release(g_egl.window);
        g_egl.window = NULL;
    }
    
    g_egl.initialized = 0;
    
    LOGI("EGL terminated");
}
