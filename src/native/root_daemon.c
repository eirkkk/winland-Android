#include "root_daemon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <android/log.h>
#include <errno.h>
#include <pthread.h>

#define LOG_TAG "RootDaemon"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// المتغير العام
static struct root_daemon g_daemon = {0};
static pthread_mutex_t g_daemon_mutex = PTHREAD_MUTEX_INITIALIZER;

// مسارات Magisk الشائعة
static const char* magisk_paths[] = {
    "/data/adb/magisk/magisk",
    "/sbin/magisk",
    "/system/bin/magisk",
    "/system/xbin/magisk",
    "/magisk/.core/bin/magisk",
    "/data/magisk/magisk",
    NULL
};

// مسارات su الشائعة
static const char* su_paths[] = {
    "/data/adb/magisk/magisk",
    "/sbin/su",
    "/system/bin/su",
    "/system/xbin/su",
    "/su/bin/su",
    "/magisk/.core/bin/su",
    "/data/local/xbin/su",
    "/data/local/bin/su",
    NULL
};

// التحقق من وجود ملف
static bool file_exists(const char* path) {
    return access(path, F_OK) == 0;
}

// التحقق من نوع الـ root
root_type_t root_detect_type(void) {
    // التحقق من Magisk
    if (root_has_magisk()) {
        return ROOT_TYPE_MAGISK;
    }
    
    // التحقق من SuperSU
    if (file_exists("/su/bin/su") || file_exists("/system/xbin/daemonsu")) {
        return ROOT_TYPE_SUPERSU;
    }
    
    // التحقق من KernelSU
    if (file_exists("/data/adb/ksud")) {
        return ROOT_TYPE_KERNELSU;
    }
    
    // التحقق من su عادي
    if (root_has_su_binary()) {
        return ROOT_TYPE_OTHER;
    }
    
    return ROOT_TYPE_NONE;
}

// التحقق من وجود Magisk
bool root_has_magisk(void) {
    for (int i = 0; magisk_paths[i] != NULL; i++) {
        if (file_exists(magisk_paths[i])) {
            strncpy(g_daemon.magisk_path, magisk_paths[i], sizeof(g_daemon.magisk_path) - 1);
            return true;
        }
    }
    return false;
}

// التحقق من وجود su binary
bool root_has_su_binary(void) {
    for (int i = 0; su_paths[i] != NULL; i++) {
        if (file_exists(su_paths[i])) {
            return true;
        }
    }
    return false;
}

// الحصول على إصدار Magisk
int root_get_magisk_version(void) {
    if (!root_has_magisk()) {
        return 0;
    }
    
    char command[512];
    snprintf(command, sizeof(command), "%s -V", g_daemon.magisk_path);
    
    FILE* fp = popen(command, "r");
    if (!fp) {
        return 0;
    }
    
    char output[32];
    if (fgets(output, sizeof(output), fp) != NULL) {
        pclose(fp);
        return atoi(output);
    }
    
    pclose(fp);
    return 0;
}

// الحصول على مسار Magisk
const char* root_get_magisk_path(void) {
    if (strlen(g_daemon.magisk_path) > 0) {
        return g_daemon.magisk_path;
    }
    
    for (int i = 0; magisk_paths[i] != NULL; i++) {
        if (file_exists(magisk_paths[i])) {
            strncpy(g_daemon.magisk_path, magisk_paths[i], 
                   sizeof(g_daemon.magisk_path) - 1);
            return g_daemon.magisk_path;
        }
    }
    
    return NULL;
}

// تهيئة الـ daemon
int root_daemon_init(void) {
    pthread_mutex_lock(&g_daemon_mutex);
    
    if (g_daemon.state != DAEMON_STATE_STOPPED) {
        pthread_mutex_unlock(&g_daemon_mutex);
        LOGI("Root daemon already initialized");
        return 0;
    }
    
    memset(&g_daemon, 0, sizeof(g_daemon));
    
    // حفظ UID/GID الأصلي
    g_daemon.original_uid = getuid();
    g_daemon.original_gid = getgid();
    
    // اكتشاف نوع الـ root
    g_daemon.root_type = root_detect_type();
    g_daemon.has_root = (g_daemon.root_type != ROOT_TYPE_NONE);
    g_daemon.has_magisk = root_has_magisk();
    g_daemon.magisk_version = root_get_magisk_version();
    
    // إعدادات افتراضية
    g_daemon.use_root_for_graphics = true;
    g_daemon.use_root_for_input = true;
    g_daemon.use_root_for_audio = true;
    g_daemon.use_root_for_usb = true;
    
    LOGI("Root daemon initialized");
    LOGI("Root type: %d", g_daemon.root_type);
    LOGI("Has root: %s", g_daemon.has_root ? "yes" : "no");
    LOGI("Has Magisk: %s", g_daemon.has_magisk ? "yes" : "no");
    if (g_daemon.has_magisk) {
        LOGI("Magisk version: %d", g_daemon.magisk_version);
        LOGI("Magisk path: %s", g_daemon.magisk_path);
    }
    
    pthread_mutex_unlock(&g_daemon_mutex);
    return 0;
}

// إنهاء الـ daemon
void root_daemon_terminate(void) {
    pthread_mutex_lock(&g_daemon_mutex);
    
    if (g_daemon.state == DAEMON_STATE_RUNNING) {
        root_daemon_stop();
    }
    
    memset(&g_daemon, 0, sizeof(g_daemon));
    
    LOGI("Root daemon terminated");
    pthread_mutex_unlock(&g_daemon_mutex);
}

// بدء الـ daemon
int root_daemon_start(void) {
    pthread_mutex_lock(&g_daemon_mutex);
    
    if (g_daemon.state == DAEMON_STATE_RUNNING) {
        pthread_mutex_unlock(&g_daemon_mutex);
        LOGI("Root daemon already running");
        return 0;
    }
    
    if (!g_daemon.has_root) {
        pthread_mutex_unlock(&g_daemon_mutex);
        LOGE("Cannot start daemon: no root access");
        return -1;
    }
    
    g_daemon.state = DAEMON_STATE_STARTING;
    
    // TODO: Implement daemon startup logic
    // This could include:
    // - Setting up the daemon socket
    // - Forking a privileged process
    // - Setting up IPC
    
    g_daemon.state = DAEMON_STATE_RUNNING;
    
    LOGI("Root daemon started");
    pthread_mutex_unlock(&g_daemon_mutex);
    return 0;
}

// إيقاف الـ daemon
void root_daemon_stop(void) {
    pthread_mutex_lock(&g_daemon_mutex);
    
    if (g_daemon.state != DAEMON_STATE_RUNNING) {
        pthread_mutex_unlock(&g_daemon_mutex);
        return;
    }
    
    // TODO: Implement daemon shutdown logic
    
    g_daemon.state = DAEMON_STATE_STOPPED;
    
    LOGI("Root daemon stopped");
    pthread_mutex_unlock(&g_daemon_mutex);
}

// رفع الصلاحيات
int root_elevate(void) {
    if (getuid() == 0) {
        return 0; // Already root
    }
    
    const char* su_path = root_get_magisk_path();
    if (!su_path) {
        su_path = "/system/xbin/su";
    }
    
    if (!file_exists(su_path)) {
        LOGE("su binary not found");
        return -1;
    }
    
    // Execute su -c to elevate
    char command[512];
    snprintf(command, sizeof(command), "%s -c id", su_path);
    
    int ret = system(command);
    return ret;
}

// تخفيض الصلاحيات
int root_drop_privileges(void) {
    if (getuid() != 0) {
        return 0; // Not root, nothing to do
    }
    
    if (setgid(g_daemon.original_gid) != 0) {
        LOGE("Failed to drop gid: %s", strerror(errno));
        return -1;
    }
    
    if (setuid(g_daemon.original_uid) != 0) {
        LOGE("Failed to drop uid: %s", strerror(errno));
        return -1;
    }
    
    LOGI("Dropped privileges to uid=%d, gid=%d", getuid(), getgid());
    return 0;
}

// تنفيذ أمر كـ root
int root_execute_command(const char* command) {
    if (!command) return -1;
    
    const char* su_path = root_get_magisk_path();
    if (!su_path) {
        su_path = "/system/xbin/su";
    }
    
    if (!file_exists(su_path)) {
        LOGE("su binary not found");
        return -1;
    }
    
    char full_command[1024];
    snprintf(full_command, sizeof(full_command), "%s -c '%s'", su_path, command);
    
    LOGI("Executing: %s", command);
    return system(full_command);
}

// تنفيذ أمر كـ root بشكل غير متزامن
int root_execute_command_async(const char* command) {
    if (!command) return -1;
    
    pid_t pid = fork();
    if (pid < 0) {
        LOGE("Fork failed: %s", strerror(errno));
        return -1;
    }
    
    if (pid == 0) {
        // Child process
        int ret = root_execute_command(command);
        _exit(ret);
    }
    
    // Parent process - don't wait
    return 0;
}

// تنفيذ أمر والحصول على الناتج
char* root_execute_command_with_output(const char* command) {
    if (!command) return NULL;
    
    const char* su_path = root_get_magisk_path();
    if (!su_path) {
        su_path = "/system/xbin/su";
    }
    
    char full_command[1024];
    snprintf(full_command, sizeof(full_command), "%s -c '%s' 2>&1", su_path, command);
    
    FILE* fp = popen(full_command, "r");
    if (!fp) {
        LOGE("Failed to execute command");
        return NULL;
    }
    
    // Read output
    char buffer[4096];
    size_t output_size = 0;
    char* output = NULL;
    
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        size_t len = strlen(buffer);
        char* new_output = realloc(output, output_size + len + 1);
        if (!new_output) {
            free(output);
            pclose(fp);
            return NULL;
        }
        output = new_output;
        memcpy(output + output_size, buffer, len + 1);
        output_size += len;
    }
    
    pclose(fp);
    return output;
}

// تثبيت وحدة Magisk
int root_magisk_install_module(const char* module_path) {
    if (!g_daemon.has_magisk) {
        LOGE("Magisk not available");
        return -1;
    }
    
    char command[512];
    snprintf(command, sizeof(command), "%s --install-module '%s'", 
             g_daemon.magisk_path, module_path);
    
    return system(command);
}

// إلغاء تثبيت وحدة Magisk
int root_magisk_uninstall_module(const char* module_name) {
    if (!g_daemon.has_magisk) {
        LOGE("Magisk not available");
        return -1;
    }
    
    char path[512];
    snprintf(path, sizeof(path), "/data/adb/modules/%s", module_name);
    
    char command[512];
    snprintf(command, sizeof(command), "rm -rf '%s'", path);
    
    return root_execute_command(command);
}

// تفعيل وحدة Magisk
int root_magisk_enable_module(const char* module_name) {
    char path[512];
    snprintf(path, sizeof(path), "/data/adb/modules/%s/remove", module_name);
    
    char command[512];
    snprintf(command, sizeof(command), "rm -f '%s'", path);
    
    return root_execute_command(command);
}

// تعطيل وحدة Magisk
int root_magisk_disable_module(const char* module_name) {
    char path[512];
    snprintf(path, sizeof(path), "/data/adb/modules/%s/disable", module_name);
    
    char command[512];
    snprintf(command, sizeof(command), "touch '%s'", path);
    
    return root_execute_command(command);
}

// التحقق من تفعيل وحدة Magisk
bool root_magisk_is_module_enabled(const char* module_name) {
    char path[512];
    snprintf(path, sizeof(path), "/data/adb/modules/%s/disable", module_name);
    
    return !file_exists(path);
}

// تعيين خاصية النظام
int root_set_property(const char* key, const char* value) {
    if (!key || !value) return -1;
    
    char command[512];
    snprintf(command, sizeof(command), "setprop '%s' '%s'", key, value);
    
    return root_execute_command(command);
}

// الحصول على خاصية النظام
char* root_get_property(const char* key) {
    if (!key) return NULL;
    
    char command[256];
    snprintf(command, sizeof(command), "getprop '%s'", key);
    
    return root_execute_command_with_output(command);
}

// عمل mount
int root_mount(const char* source, const char* target, 
               const char* fs_type, unsigned long flags) {
    if (!source || !target) return -1;
    
    char command[1024];
    snprintf(command, sizeof(command), "mount -t '%s' '%s' '%s'", 
             fs_type ? fs_type : "auto", source, target);
    
    return root_execute_command(command);
}

// عمل umount
int root_umount(const char* target) {
    if (!target) return -1;
    
    char command[512];
    snprintf(command, sizeof(command), "umount '%s'", target);
    
    return root_execute_command(command);
}

// فتح جهاز
int root_open_device(const char* device_path, int flags) {
    if (!device_path) return -1;
    
    // This requires the daemon to be running
    // For now, just try to open directly
    return open(device_path, flags);
}

// تغيير صلاحيات الملف
int root_chmod(const char* path, mode_t mode) {
    if (!path) return -1;
    
    char command[512];
    snprintf(command, sizeof(command), "chmod %o '%s'", mode, path);
    
    return root_execute_command(command);
}

// تغيير مالك الملف
int root_chown(const char* path, uid_t uid, gid_t gid) {
    if (!path) return -1;
    
    char command[512];
    snprintf(command, sizeof(command), "chown %d:%d '%s'", uid, gid, path);
    
    return root_execute_command(command);
}

// الحصول على الحالة
daemon_state_t root_daemon_get_state(void) {
    return g_daemon.state;
}

// الحصول على نص الحالة
const char* root_daemon_get_state_string(void) {
    switch (g_daemon.state) {
        case DAEMON_STATE_STOPPED: return "stopped";
        case DAEMON_STATE_STARTING: return "starting";
        case DAEMON_STATE_RUNNING: return "running";
        case DAEMON_STATE_ERROR: return "error";
        default: return "unknown";
    }
}

// التحقق من التشغيل
bool root_daemon_is_running(void) {
    return g_daemon.state == DAEMON_STATE_RUNNING;
}
