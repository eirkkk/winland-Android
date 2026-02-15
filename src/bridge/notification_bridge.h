#ifndef NOTIFICATION_BRIDGE_H
#define NOTIFICATION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// أولوية الإشعار
typedef enum {
    NOTIFICATION_PRIORITY_LOW = 0,
    NOTIFICATION_PRIORITY_NORMAL = 1,
    NOTIFICATION_PRIORITY_HIGH = 2,
    NOTIFICATION_PRIORITY_URGENT = 3
} notification_priority_t;

// هيكل إشعار Linux (من libnotify)
struct linux_notification {
    uint32_t id;
    char app_name[256];
    char title[512];
    char body[2048];
    char icon_name[256];
    notification_priority_t priority;
    int32_t timeout;  // -1 = default, 0 = never expire
    
    // أزرار الإجراءات
    char actions[8][64];
    int action_count;
    
    // التقدم (للإشعارات التقدم)
    int32_t progress;
    bool has_progress;
};

// هيكل جسر الإشعارات
struct notification_bridge {
    int initialized;
    
    // مقبس D-Bus
    int dbus_fd;
    char dbus_path[256];
    
    // معاودة الاتصال لإرسال إشعار إلى Android
    void (*on_linux_notification)(struct linux_notification* notification);
    
    // معاودة الاتصال لإجراء من Android
    void (*on_android_action)(uint32_t notification_id, const char* action);
    
    // قائمة الإشعارات النشطة
    struct linux_notification* active_notifications;
    int notification_count;
    int notification_capacity;
};

// دوال التهيئة
int notification_bridge_init(void);
void notification_bridge_terminate(void);

// استقبال إشعارات من Linux
int notification_bridge_start_dbus_listener(void);
void notification_bridge_stop_dbus_listener(void);
int notification_bridge_parse_dbus_message(const char* data, size_t len);

// إرسال إشعارات إلى Android
void notification_bridge_send_to_android(struct linux_notification* notification);
void notification_bridge_close_on_android(uint32_t notification_id);
void notification_bridge_update_progress(uint32_t notification_id, int32_t progress);

// إدارة الإشعارات
uint32_t notification_bridge_add(struct linux_notification* notification);
void notification_bridge_remove(uint32_t id);
struct linux_notification* notification_bridge_get(uint32_t id);
void notification_bridge_clear_all(void);

// إجراءات
void notification_bridge_trigger_action(uint32_t notification_id, int action_index);
void notification_bridge_set_callbacks(
    void (*on_linux_notification)(struct linux_notification* notification),
    void (*on_android_action)(uint32_t notification_id, const char* action)
);

#ifdef __cplusplus
}
#endif

#endif // NOTIFICATION_BRIDGE_H
