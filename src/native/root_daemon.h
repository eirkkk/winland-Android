#ifndef ROOT_DAEMON_H
#define ROOT_DAEMON_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// حالات الـ daemon
typedef enum {
    DAEMON_STATE_STOPPED,
    DAEMON_STATE_STARTING,
    DAEMON_STATE_RUNNING,
    DAEMON_STATE_ERROR
} daemon_state_t;

// أنواع الـ root
typedef enum {
    ROOT_TYPE_NONE,
    ROOT_TYPE_MAGISK,
    ROOT_TYPE_SUPERSU,
    ROOT_TYPE_KERNELSU,
    ROOT_TYPE_OTHER
} root_type_t;

// هيكل الـ root daemon
struct root_daemon {
    daemon_state_t state;
    root_type_t root_type;
    
    // معلومات الـ root
    bool has_root;
    bool has_magisk;
    int magisk_version;
    char magisk_path[256];
    
    // إعدادات
    bool use_root_for_graphics;
    bool use_root_for_input;
    bool use_root_for_audio;
    bool use_root_for_usb;
    
    // الصلاحيات
    uid_t original_uid;
    gid_t original_gid;
    
    // المقبس
    int socket_fd;
    char socket_path[256];
};

// دوال الـ daemon
int root_daemon_init(void);
void root_daemon_terminate(void);
int root_daemon_start(void);
void root_daemon_stop(void);

// التحقق من الـ root
root_type_t root_detect_type(void);
bool root_has_magisk(void);
bool root_has_su_binary(void);
int root_get_magisk_version(void);
const char* root_get_magisk_path(void);

// رفع الصلاحيات
int root_elevate(void);
int root_drop_privileges(void);
int root_run_as_root(void (*callback)(void));
int root_run_as_user(uid_t uid, gid_t gid, void (*callback)(void));

// تنفيذ أوامر
int root_execute_command(const char* command);
int root_execute_command_async(const char* command);
char* root_execute_command_with_output(const char* command);

// إدارة الوحدات (Magisk)
int root_magisk_install_module(const char* module_path);
int root_magisk_uninstall_module(const char* module_name);
int root_magisk_enable_module(const char* module_name);
int root_magisk_disable_module(const char* module_name);
bool root_magisk_is_module_enabled(const char* module_name);

// إعدادات النظام
int root_set_property(const char* key, const char* value);
char* root_get_property(const char* key);
int root_mount(const char* source, const char* target, const char* fs_type, unsigned long flags);
int root_umount(const char* target);

// الوصول إلى الأجهزة
int root_open_device(const char* device_path, int flags);
int root_chmod(const char* path, mode_t mode);
int root_chown(const char* path, uid_t uid, gid_t gid);

// الحصول على الحالة
daemon_state_t root_daemon_get_state(void);
const char* root_daemon_get_state_string(void);
bool root_daemon_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // ROOT_DAEMON_H
