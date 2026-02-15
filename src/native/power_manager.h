#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// حالات الطاقة
typedef enum {
    POWER_STATE_ACTIVE,         // نشط - أداء كامل
    POWER_STATE_BALANCED,       // متوازن - أداء/بطارية
    POWER_STATE_POWERSAVE,      // توفير طاقة
    POWER_STATE_SUSPEND,        // تعليق
    POWER_STATE_HIBERNATE       // سبات
} power_state_t;

// محفزات تغيير الحالة
typedef enum {
    POWER_TRIGGER_SCREEN_OFF,
    POWER_TRIGGER_SCREEN_ON,
    POWER_TRIGGER_APP_BACKGROUND,
    POWER_TRIGGER_APP_FOREGROUND,
    POWER_TRIGGER_BATTERY_LOW,
    POWER_TRIGGER_BATTERY_CRITICAL,
    POWER_TRIGGER_CHARGING,
    POWER_TRIGGER_IDLE_TIMEOUT,
    POWER_TRIGGER_USER_ACTIVE
} power_trigger_t;

// إعدادات إدارة الطاقة
struct power_config {
    // الحدود
    int battery_low_threshold;      // نسبة البطارية المنخفضة (20%)
    int battery_critical_threshold; // نسبة البطارية الحرجة (10%)
    
    // المهلة
    int idle_timeout_ms;            // مهلة الخمول (5 دقائق)
    int suspend_timeout_ms;         // مهلة التعليق (15 دقيقة)
    
    // السلوك
    bool reduce_fps_on_battery;     // تقليل FPS عند البطارية
    bool disable_animations;        // تعطيل الحركات
    bool reduce_resolution;         // تقليل الدقة
    bool pause_rendering_background; // إيقاف الرندر في الخلفية
    
    // FPS
    int fps_active;
    int fps_balanced;
    int fps_powersave;
};

// هيكل إحصائيات الطاقة
struct power_stats {
    int battery_level;              // نسبة البطارية (0-100)
    bool is_charging;               // هل يتم الشحن
    bool is_ac_power;               // هل متصل بالكهرباء
    
    float cpu_usage;                // استخدام المعالج
    float gpu_usage;                // استخدام GPU
    float memory_usage;             // استخدام الذاكرة
    
    uint64_t uptime_ms;             // وقت التشغيل
    uint64_t idle_time_ms;          // وقت الخمول
    
    float estimated_battery_life;   // عمر البطارية المتوقع (ساعات)
};

// هيكل مدير الطاقة
struct power_manager {
    int initialized;
    power_state_t current_state;
    
    // الإعدادات
    struct power_config config;
    
    // الإحصائيات
    struct power_stats stats;
    
    // الحالة
    bool screen_on;
    bool app_in_foreground;
    uint64_t last_activity_ms;
    uint64_t last_frame_time_ms;
    
    // FPS
    int target_fps;
    int current_fps;
    
    // معاودات الاتصال
    void (*on_state_change)(power_state_t old_state, power_state_t new_state);
    void (*on_battery_low)(int level);
    void (*on_idle)(void);
};

// دوال التهيئة
int power_manager_init(void);
void power_manager_terminate(void);

// التكوين
void power_manager_set_config(const struct power_config* config);
const struct power_config* power_manager_get_config(void);

// تغيير الحالة
void power_manager_set_state(power_state_t state);
power_state_t power_manager_get_state(void);
const char* power_manager_state_to_string(power_state_t state);

// معالجة المحفزات
void power_manager_handle_trigger(power_trigger_t trigger);
void power_manager_on_screen_state(bool on);
void power_manager_on_app_state(bool foreground);
void power_manager_on_battery_level(int level);
void power_manager_on_charging_state(bool charging);

// النشاط
void power_manager_report_activity(void);
bool power_manager_is_idle(void);
uint64_t power_manager_get_idle_time_ms(void);

// FPS
void power_manager_set_target_fps(int fps);
int power_manager_get_target_fps(void);
bool power_manager_should_render_frame(void);

// الإحصائيات
void power_manager_update_stats(void);
const struct power_stats* power_manager_get_stats(void);
float power_manager_estimate_battery_life(void);

// وضع توفير الطاقة الذكي
void power_manager_enable_smart_powersave(bool enable);
bool power_manager_is_smart_powersave_enabled(void);

// تعليق/استئناف
void power_manager_suspend(void);
void power_manager_resume(void);
bool power_manager_is_suspended(void);

// معاودات الاتصال
void power_manager_set_callbacks(
    void (*on_state_change)(power_state_t old_state, power_state_t new_state),
    void (*on_battery_low)(int level),
    void (*on_idle)(void)
);

#ifdef __cplusplus
}
#endif

#endif // POWER_MANAGER_H
