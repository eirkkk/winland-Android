#include "power_manager.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <android/log.h>

#define LOG_TAG "PowerManager"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// المتغير العام
static struct power_manager g_manager = {0};

// الحصول على الوقت الحالي بالمللي ثانية
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// تهيئة مدير الطاقة
int power_manager_init(void) {
    if (g_manager.initialized) {
        return 0;
    }
    
    memset(&g_manager, 0, sizeof(g_manager));
    
    // الإعدادات الافتراضية
    g_manager.config.battery_low_threshold = 20;
    g_manager.config.battery_critical_threshold = 10;
    g_manager.config.idle_timeout_ms = 5 * 60 * 1000;      // 5 دقائق
    g_manager.config.suspend_timeout_ms = 15 * 60 * 1000;  // 15 دقيقة
    
    g_manager.config.reduce_fps_on_battery = true;
    g_manager.config.disable_animations = true;
    g_manager.config.reduce_resolution = false;
    g_manager.config.pause_rendering_background = true;
    
    g_manager.config.fps_active = 60;
    g_manager.config.fps_balanced = 45;
    g_manager.config.fps_powersave = 30;
    
    // الحالة الأولية
    g_manager.current_state = POWER_STATE_ACTIVE;
    g_manager.target_fps = g_manager.config.fps_active;
    g_manager.current_fps = 60;
    g_manager.screen_on = true;
    g_manager.app_in_foreground = true;
    g_manager.last_activity_ms = get_time_ms();
    g_manager.last_frame_time_ms = get_time_ms();
    
    g_manager.initialized = 1;
    LOGI("Power manager initialized");
    return 0;
}

// إنهاء مدير الطاقة
void power_manager_terminate(void) {
    if (!g_manager.initialized) return;
    
    g_manager.initialized = 0;
    LOGI("Power manager terminated");
}

// تعيين الإعدادات
void power_manager_set_config(const struct power_config* config) {
    if (!config) return;
    memcpy(&g_manager.config, config, sizeof(*config));
    
    // تحديث FPS الهدف
    switch (g_manager.current_state) {
        case POWER_STATE_ACTIVE:
            g_manager.target_fps = config->fps_active;
            break;
        case POWER_STATE_BALANCED:
            g_manager.target_fps = config->fps_balanced;
            break;
        case POWER_STATE_POWERSAVE:
            g_manager.target_fps = config->fps_powersave;
            break;
        default:
            break;
    }
}

// الحصول على الإعدادات
const struct power_config* power_manager_get_config(void) {
    return &g_manager.config;
}

// تعيين الحالة
void power_manager_set_state(power_state_t state) {
    if (!g_manager.initialized || g_manager.current_state == state) return;
    
    power_state_t old_state = g_manager.current_state;
    g_manager.current_state = state;
    
    // تحديث FPS الهدف
    switch (state) {
        case POWER_STATE_ACTIVE:
            g_manager.target_fps = g_manager.config.fps_active;
            break;
        case POWER_STATE_BALANCED:
            g_manager.target_fps = g_manager.config.fps_balanced;
            break;
        case POWER_STATE_POWERSAVE:
            g_manager.target_fps = g_manager.config.fps_powersave;
            break;
        case POWER_STATE_SUSPEND:
        case POWER_STATE_HIBERNATE:
            g_manager.target_fps = 0;
            break;
    }
    
    LOGI("Power state changed: %s -> %s",
         power_manager_state_to_string(old_state),
         power_manager_state_to_string(state));
    
    if (g_manager.on_state_change) {
        g_manager.on_state_change(old_state, state);
    }
}

// الحصول على الحالة
power_state_t power_manager_get_state(void) {
    return g_manager.current_state;
}

// تحويل الحالة إلى نص
const char* power_manager_state_to_string(power_state_t state) {
    switch (state) {
        case POWER_STATE_ACTIVE: return "active";
        case POWER_STATE_BALANCED: return "balanced";
        case POWER_STATE_POWERSAVE: return "powersave";
        case POWER_STATE_SUSPEND: return "suspend";
        case POWER_STATE_HIBERNATE: return "hibernate";
        default: return "unknown";
    }
}

// معالجة المحفز
void power_manager_handle_trigger(power_trigger_t trigger) {
    if (!g_manager.initialized) return;
    
    switch (trigger) {
        case POWER_TRIGGER_SCREEN_OFF:
            power_manager_on_screen_state(false);
            break;
        case POWER_TRIGGER_SCREEN_ON:
            power_manager_on_screen_state(true);
            break;
        case POWER_TRIGGER_APP_BACKGROUND:
            power_manager_on_app_state(false);
            break;
        case POWER_TRIGGER_APP_FOREGROUND:
            power_manager_on_app_state(true);
            break;
        case POWER_TRIGGER_BATTERY_LOW:
            if (g_manager.stats.battery_level <= g_manager.config.battery_critical_threshold) {
                power_manager_set_state(POWER_STATE_POWERSAVE);
            } else if (g_manager.stats.battery_level <= g_manager.config.battery_low_threshold) {
                power_manager_set_state(POWER_STATE_BALANCED);
            }
            break;
        case POWER_TRIGGER_CHARGING:
            power_manager_set_state(POWER_STATE_ACTIVE);
            break;
        case POWER_TRIGGER_IDLE_TIMEOUT:
            if (!g_manager.app_in_foreground && !g_manager.screen_on) {
                power_manager_set_state(POWER_STATE_SUSPEND);
            }
            break;
        case POWER_TRIGGER_USER_ACTIVE:
            power_manager_report_activity();
            break;
    }
}

// حالة الشاشة
void power_manager_on_screen_state(bool on) {
    g_manager.screen_on = on;
    
    if (on) {
        power_manager_report_activity();
        if (g_manager.current_state == POWER_STATE_SUSPEND) {
            power_manager_set_state(POWER_STATE_ACTIVE);
        }
    } else {
        // الشاشة مغلقة
        if (g_manager.config.pause_rendering_background) {
            LOGI("Screen off - pausing rendering");
        }
    }
}

// حالة التطبيق
void power_manager_on_app_state(bool foreground) {
    g_manager.app_in_foreground = foreground;
    
    if (foreground) {
        power_manager_report_activity();
        if (g_manager.current_state == POWER_STATE_SUSPEND) {
            power_manager_set_state(POWER_STATE_ACTIVE);
        }
    } else {
        // التطبيق في الخلفية
        if (g_manager.config.pause_rendering_background) {
            LOGI("App in background - reducing power usage");
            power_manager_set_state(POWER_STATE_POWERSAVE);
        }
    }
}

// مستوى البطارية
void power_manager_on_battery_level(int level) {
    g_manager.stats.battery_level = level;
    
    if (level <= g_manager.config.battery_critical_threshold) {
        LOGI("Battery critical: %d%%", level);
        if (g_manager.on_battery_low) {
            g_manager.on_battery_low(level);
        }
        power_manager_set_state(POWER_STATE_POWERSAVE);
    } else if (level <= g_manager.config.battery_low_threshold) {
        LOGI("Battery low: %d%%", level);
        if (g_manager.on_battery_low) {
            g_manager.on_battery_low(level);
        }
        if (g_manager.current_state == POWER_STATE_ACTIVE) {
            power_manager_set_state(POWER_STATE_BALANCED);
        }
    }
}

// حالة الشحن
void power_manager_on_charging_state(bool charging) {
    g_manager.stats.is_charging = charging;
    
    if (charging) {
        LOGI("Charging started");
        power_manager_set_state(POWER_STATE_ACTIVE);
    } else {
        LOGI("Charging stopped");
        // التحقق من مستوى البطارية
        power_manager_on_battery_level(g_manager.stats.battery_level);
    }
}

// الإبلاغ عن نشاط
void power_manager_report_activity(void) {
    g_manager.last_activity_ms = get_time_ms();
}

// التحقق من الخمول
bool power_manager_is_idle(void) {
    return power_manager_get_idle_time_ms() >= g_manager.config.idle_timeout_ms;
}

// الحصول على وقت الخمول
uint64_t power_manager_get_idle_time_ms(void) {
    return get_time_ms() - g_manager.last_activity_ms;
}

// تعيين FPS الهدف
void power_manager_set_target_fps(int fps) {
    g_manager.target_fps = fps;
}

// الحصول على FPS الهدف
int power_manager_get_target_fps(void) {
    return g_manager.target_fps;
}

// التحقق مما إذا كان يجب رسم إطار
bool power_manager_should_render_frame(void) {
    if (!g_manager.initialized) return true;
    
    // إذا كان في وضع التعليق
    if (g_manager.current_state == POWER_STATE_SUSPEND ||
        g_manager.current_state == POWER_STATE_HIBERNATE) {
        return false;
    }
    
    // إذا كان التطبيق في الخلفية والإعداد مفعل
    if (!g_manager.app_in_foreground && g_manager.config.pause_rendering_background) {
        return false;
    }
    
    // إذا كانت الشاشة مغلقة والإعداد مفعل
    if (!g_manager.screen_on && g_manager.config.pause_rendering_background) {
        return false;
    }
    
    // التحقق من FPS
    uint64_t now = get_time_ms();
    uint64_t frame_interval = 1000 / g_manager.target_fps;
    
    if (now - g_manager.last_frame_time_ms >= frame_interval) {
        g_manager.last_frame_time_ms = now;
        return true;
    }
    
    return false;
}

// تحديث الإحصائيات
void power_manager_update_stats(void) {
    // TODO: Implement actual stats collection via JNI
    // For now, use placeholder values
    
    g_manager.stats.cpu_usage = 15.0f;  // Placeholder
    g_manager.stats.gpu_usage = 25.0f;  // Placeholder
    g_manager.stats.memory_usage = 40.0f;  // Placeholder
    g_manager.stats.uptime_ms = get_time_ms();
}

// الحصول على الإحصائيات
const struct power_stats* power_manager_get_stats(void) {
    power_manager_update_stats();
    return &g_manager.stats;
}

// تقدير عمر البطارية
float power_manager_estimate_battery_life(void) {
    if (g_manager.stats.is_charging) {
        return -1.0f; // لا نهائي
    }
    
    // TODO: Implement actual estimation based on power usage
    float hours = (float)g_manager.stats.battery_level / 10.0f;
    return hours;
}

// تفعيل وضع توفير الطاقة الذكي
void power_manager_enable_smart_powersave(bool enable) {
    // TODO: Implement smart power save mode
    LOGI("Smart powersave: %s", enable ? "enabled" : "disabled");
}

// التحقق من وضع توفير الطاقة الذكي
bool power_manager_is_smart_powersave_enabled(void) {
    // TODO: Implement
    return true;
}

// تعليق
void power_manager_suspend(void) {
    power_manager_set_state(POWER_STATE_SUSPEND);
    LOGI("System suspended");
}

// استئناف
void power_manager_resume(void) {
    power_manager_set_state(POWER_STATE_ACTIVE);
    power_manager_report_activity();
    LOGI("System resumed");
}

// التحقق من التعليق
bool power_manager_is_suspended(void) {
    return g_manager.current_state == POWER_STATE_SUSPEND ||
           g_manager.current_state == POWER_STATE_HIBERNATE;
}

// تعيين معاودات الاتصال
void power_manager_set_callbacks(
    void (*on_state_change)(power_state_t old_state, power_state_t new_state),
    void (*on_battery_low)(int level),
    void (*on_idle)(void)
) {
    g_manager.on_state_change = on_state_change;
    g_manager.on_battery_low = on_battery_low;
    g_manager.on_idle = on_idle;
}
