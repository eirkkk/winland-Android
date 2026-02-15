#include "notification_bridge.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <android/log.h>
#include <errno.h>

#define LOG_TAG "NotificationBridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define MAX_NOTIFICATIONS 100
#define DBUS_SOCKET_PATH "/data/data/com.winland.server/files/dbus/notification"

// المتغير العام
static struct notification_bridge g_bridge = {0};
static pthread_t g_dbus_thread;
static pthread_mutex_t g_bridge_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t g_next_notification_id = 1;

// دالة خيط D-Bus
static void* dbus_listener_thread(void* arg);

// تهيئة جسر الإشعارات
int notification_bridge_init(void) {
    if (g_bridge.initialized) {
        return 0;
    }
    
    memset(&g_bridge, 0, sizeof(g_bridge));
    
    // تخصيص قائمة الإشعارات
    g_bridge.notification_capacity = MAX_NOTIFICATIONS;
    g_bridge.active_notifications = calloc(MAX_NOTIFICATIONS, sizeof(struct linux_notification));
    if (!g_bridge.active_notifications) {
        LOGE("Failed to allocate notifications array");
        return -1;
    }
    
    strcpy(g_bridge.dbus_path, DBUS_SOCKET_PATH);
    
    g_bridge.initialized = 1;
    LOGI("Notification bridge initialized");
    return 0;
}

// إنهاء جسر الإشعارات
void notification_bridge_terminate(void) {
    if (!g_bridge.initialized) return;
    
    notification_bridge_stop_dbus_listener();
    
    pthread_mutex_lock(&g_bridge_mutex);
    
    if (g_bridge.active_notifications) {
        free(g_bridge.active_notifications);
        g_bridge.active_notifications = NULL;
    }
    
    g_bridge.notification_count = 0;
    g_bridge.initialized = 0;
    
    pthread_mutex_unlock(&g_bridge_mutex);
    
    LOGI("Notification bridge terminated");
}

// بدء مستمع D-Bus
int notification_bridge_start_dbus_listener(void) {
    if (!g_bridge.initialized) {
        LOGE("Bridge not initialized");
        return -1;
    }
    
    // إنشاء مجلد المقبس
    char dir[256];
    strncpy(dir, g_bridge.dbus_path, sizeof(dir));
    char* last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir(dir, 0755);
    }
    
    // إزالة المقبس القديم
    unlink(g_bridge.dbus_path);
    
    // إنشاء مقبس Unix
    g_bridge.dbus_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_bridge.dbus_fd < 0) {
        LOGE("Failed to create socket: %s", strerror(errno));
        return -1;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, g_bridge.dbus_path, sizeof(addr.sun_path) - 1);
    
    if (bind(g_bridge.dbus_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOGE("Failed to bind socket: %s", strerror(errno));
        close(g_bridge.dbus_fd);
        g_bridge.dbus_fd = -1;
        return -1;
    }
    
    if (listen(g_bridge.dbus_fd, 5) < 0) {
        LOGE("Failed to listen on socket: %s", strerror(errno));
        close(g_bridge.dbus_fd);
        g_bridge.dbus_fd = -1;
        return -1;
    }
    
    // إنشاء الخيط
    if (pthread_create(&g_dbus_thread, NULL, dbus_listener_thread, NULL) != 0) {
        LOGE("Failed to create D-Bus thread");
        close(g_bridge.dbus_fd);
        g_bridge.dbus_fd = -1;
        return -1;
    }
    
    LOGI("D-Bus listener started on %s", g_bridge.dbus_path);
    return 0;
}

// إيقاف مستمع D-Bus
void notification_bridge_stop_dbus_listener(void) {
    if (g_bridge.dbus_fd >= 0) {
        close(g_bridge.dbus_fd);
        g_bridge.dbus_fd = -1;
    }
    
    pthread_join(g_dbus_thread, NULL);
    
    LOGI("D-Bus listener stopped");
}

// دالة خيط D-Bus
static void* dbus_listener_thread(void* arg) {
    LOGI("D-Bus listener thread started");
    
    fd_set read_fds;
    struct timeval tv;
    
    while (g_bridge.dbus_fd >= 0) {
        FD_ZERO(&read_fds);
        FD_SET(g_bridge.dbus_fd, &read_fds);
        
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms timeout
        
        int ret = select(g_bridge.dbus_fd + 1, &read_fds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            LOGE("Select error: %s", strerror(errno));
            break;
        }
        
        if (ret > 0 && FD_ISSET(g_bridge.dbus_fd, &read_fds)) {
            // قبول اتصال جديد
            struct sockaddr_un client_addr;
            socklen_t addr_len = sizeof(client_addr);
            
            int client_fd = accept(g_bridge.dbus_fd, (struct sockaddr*)&client_addr, &addr_len);
            if (client_fd < 0) {
                LOGE("Failed to accept connection: %s", strerror(errno));
                continue;
            }
            
            // قراءة الرسالة
            char buffer[4096];
            ssize_t len = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (len > 0) {
                buffer[len] = '\0';
                notification_bridge_parse_dbus_message(buffer, len);
            }
            
            close(client_fd);
        }
    }
    
    LOGI("D-Bus listener thread stopped");
    return NULL;
}

// تحليل رسالة D-Bus
int notification_bridge_parse_dbus_message(const char* data, size_t len) {
    // TODO: Implement proper D-Bus message parsing
    // For now, use a simple text protocol
    
    struct linux_notification notification;
    memset(&notification, 0, sizeof(notification));
    
    // تنسيق بسيط: APP_NAME|TITLE|BODY|ICON|PRIORITY
    char* copy = strdup(data);
    char* token = strtok(copy, "|");
    int field = 0;
    
    while (token && field < 5) {
        switch (field) {
            case 0:
                strncpy(notification.app_name, token, sizeof(notification.app_name) - 1);
                break;
            case 1:
                strncpy(notification.title, token, sizeof(notification.title) - 1);
                break;
            case 2:
                strncpy(notification.body, token, sizeof(notification.body) - 1);
                break;
            case 3:
                strncpy(notification.icon_name, token, sizeof(notification.icon_name) - 1);
                break;
            case 4:
                notification.priority = atoi(token);
                break;
        }
        token = strtok(NULL, "|");
        field++;
    }
    
    free(copy);
    
    // إضافة الإشعار
    uint32_t id = notification_bridge_add(&notification);
    LOGI("Notification added: %s - %s (ID: %u)", notification.app_name, notification.title, id);
    
    // إرسال إلى Android
    if (g_bridge.on_linux_notification) {
        struct linux_notification* notif = notification_bridge_get(id);
        if (notif) {
            g_bridge.on_linux_notification(notif);
        }
    }
    
    return 0;
}

// إضافة إشعار
uint32_t notification_bridge_add(struct linux_notification* notification) {
    if (!notification) return 0;
    
    pthread_mutex_lock(&g_bridge_mutex);
    
    // البحث عن خانة فارغة
    int slot = -1;
    for (int i = 0; i < g_bridge.notification_capacity; i++) {
        if (g_bridge.active_notifications[i].id == 0) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        // إزالة الأقدم
        slot = 0;
        for (int i = 1; i < g_bridge.notification_capacity; i++) {
            if (g_bridge.active_notifications[i].id < g_bridge.active_notifications[slot].id) {
                slot = i;
            }
        }
    }
    
    uint32_t id = g_next_notification_id++;
    notification->id = id;
    memcpy(&g_bridge.active_notifications[slot], notification, sizeof(*notification));
    g_bridge.notification_count++;
    
    pthread_mutex_unlock(&g_bridge_mutex);
    
    return id;
}

// إزالة إشعار
void notification_bridge_remove(uint32_t id) {
    if (id == 0) return;
    
    pthread_mutex_lock(&g_bridge_mutex);
    
    for (int i = 0; i < g_bridge.notification_capacity; i++) {
        if (g_bridge.active_notifications[i].id == id) {
            memset(&g_bridge.active_notifications[i], 0, sizeof(struct linux_notification));
            g_bridge.notification_count--;
            break;
        }
    }
    
    pthread_mutex_unlock(&g_bridge_mutex);
    
    // إغلاق على Android
    notification_bridge_close_on_android(id);
}

// الحصول على إشعار
struct linux_notification* notification_bridge_get(uint32_t id) {
    if (id == 0) return NULL;
    
    pthread_mutex_lock(&g_bridge_mutex);
    
    for (int i = 0; i < g_bridge.notification_capacity; i++) {
        if (g_bridge.active_notifications[i].id == id) {
            pthread_mutex_unlock(&g_bridge_mutex);
            return &g_bridge.active_notifications[i];
        }
    }
    
    pthread_mutex_unlock(&g_bridge_mutex);
    return NULL;
}

// مسح جميع الإشعارات
void notification_bridge_clear_all(void) {
    pthread_mutex_lock(&g_bridge_mutex);
    
    for (int i = 0; i < g_bridge.notification_capacity; i++) {
        if (g_bridge.active_notifications[i].id != 0) {
            notification_bridge_close_on_android(g_bridge.active_notifications[i].id);
        }
    }
    
    memset(g_bridge.active_notifications, 0, 
           sizeof(struct linux_notification) * g_bridge.notification_capacity);
    g_bridge.notification_count = 0;
    
    pthread_mutex_unlock(&g_bridge_mutex);
    
    LOGI("All notifications cleared");
}

// إرسال إلى Android
void notification_bridge_send_to_android(struct linux_notification* notification) {
    if (!notification) return;
    
    // TODO: Implement JNI call to Android
    LOGI("Sending notification to Android: %s - %s", notification->app_name, notification->title);
}

// إغلاق على Android
void notification_bridge_close_on_android(uint32_t notification_id) {
    // TODO: Implement JNI call to cancel notification
    LOGI("Closing notification on Android: %u", notification_id);
}

// تحديث التقدم
void notification_bridge_update_progress(uint32_t notification_id, int32_t progress) {
    struct linux_notification* notif = notification_bridge_get(notification_id);
    if (!notif) return;
    
    notif->progress = progress;
    notif->has_progress = true;
    
    // TODO: Update notification on Android
    LOGI("Updating notification progress: %u = %d%%", notification_id, progress);
}

// تنفيذ إجراء
void notification_bridge_trigger_action(uint32_t notification_id, int action_index) {
    struct linux_notification* notif = notification_bridge_get(notification_id);
    if (!notif || action_index >= notif->action_count) return;
    
    LOGI("Triggering action: %s", notif->actions[action_index]);
    
    if (g_bridge.on_android_action) {
        g_bridge.on_android_action(notification_id, notif->actions[action_index]);
    }
}

// تعيين معاودات الاتصال
void notification_bridge_set_callbacks(
    void (*on_linux_notification)(struct linux_notification* notification),
    void (*on_android_action)(uint32_t notification_id, const char* action)
) {
    g_bridge.on_linux_notification = on_linux_notification;
    g_bridge.on_android_action = on_android_action;
}
