#ifndef PACKAGE_MANAGER_H
#define PACKAGE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// أنواع مديري الحزم
typedef enum {
    PKG_MANAGER_APT,
    PKG_MANAGER_PACMAN,
    PKG_MANAGER_DNF,
    PKG_MANAGER_APK,
    PKG_MANAGER_PKG
} pkg_manager_type_t;

// حالات الحزمة
typedef enum {
    PKG_STATE_NOT_INSTALLED,
    PKG_STATE_INSTALLING,
    PKG_STATE_INSTALLED,
    PKG_STATE_UPDATING,
    PKG_STATE_REMOVING,
    PKG_STATE_ERROR
} pkg_state_t;

// معلومات الحزمة
struct package_info {
    char name[256];
    char version[64];
    char description[1024];
    char category[64];
    uint64_t size_download;
    uint64_t size_installed;
    pkg_state_t state;
    bool is_installed;
    bool has_update;
    char installed_version[64];
};

// فئات الحزم
struct package_category {
    char name[64];
    char icon[64];
    char description[256];
    struct package_info* packages;
    int package_count;
};

// إعدادات مدير الحزم
struct package_manager_config {
    pkg_manager_type_t type;
    char repo_url[512];
    bool auto_update;
    int update_interval_hours;
    bool verify_signatures;
    char mirror[256];
};

// هيكل مدير الحزم
struct package_manager {
    int initialized;
    
    // الإعدادات
    struct package_manager_config config;
    
    // الفئات
    struct package_category* categories;
    int category_count;
    
    // الحزم المثبتة
    struct package_info* installed_packages;
    int installed_count;
    
    // الحزم المتاحة
    struct package_info* available_packages;
    int available_count;
    
    // الحالة
    bool updating;
    bool installing;
    int current_progress;
    char current_operation[256];
    
    // معاودات الاتصال
    void (*on_progress)(const char* package, int percent);
    void (*on_complete)(bool success, const char* message);
    void (*on_error)(const char* error);
};

// دوال التهيئة
int package_manager_init(pkg_manager_type_t type);
void package_manager_terminate(void);

// التكوين
void package_manager_set_config(const struct package_manager_config* config);
const struct package_manager_config* package_manager_get_config(void);

// إدارة المستودعات
int package_manager_update_repos(void);
int package_manager_add_repo(const char* url);
int package_manager_remove_repo(const char* url);

// البحث والفحص
int package_manager_search(const char* query, struct package_info** results, int* count);
int package_manager_get_categories(struct package_category** categories, int* count);
int package_manager_get_installed(struct package_info** packages, int* count);
int package_manager_get_updates(struct package_info** packages, int* count);
struct package_info* package_manager_get_info(const char* name);

// تثبيت وإزالة
int package_manager_install(const char* name);
int package_manager_install_multiple(const char** names, int count);
int package_manager_remove(const char* name);
int package_manager_purge(const char* name);

// التحديث
int package_manager_upgrade(const char* name);
int package_manager_upgrade_all(void);

// التنظيف
int package_manager_clean_cache(void);
int package_manager_autoremove(void);

// الحالة
bool package_manager_is_updating(void);
bool package_manager_is_installing(void);
int package_manager_get_progress(void);
const char* package_manager_get_current_operation(void);

// الحزم الشائعة
int package_manager_install_common_apps(void);
int package_manager_install_desktop_environment(const char* de);  // "xfce", "lxde", "mate"

// معاودات الاتصال
void package_manager_set_callbacks(
    void (*on_progress)(const char* package, int percent),
    void (*on_complete)(bool success, const char* message),
    void (*on_error)(const char* error)
);

#ifdef __cplusplus
}
#endif

#endif // PACKAGE_MANAGER_H
