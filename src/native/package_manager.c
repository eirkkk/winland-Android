#include "package_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_PACKAGES 10000
#define MAX_CATEGORIES 20
#define MAX_LINE_LENGTH 4096

static struct package_manager g_pm = {0};
static pthread_mutex_t g_pm_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_pm_thread;

// الحزم الشائعة المحددة مسبقاً
static const struct {
    const char* name;
    const char* description;
    const char* category;
    const char* apt_name;
    const char* pacman_name;
    const char* dnf_name;
    const char* apk_name;
} common_apps[] = {
    // المتصفحات
    {"Firefox", "Web browser from Mozilla", "Internet", 
     "firefox", "firefox", "firefox", "firefox"},
    {"Chromium", "Open source web browser", "Internet",
     "chromium", "chromium", "chromium", "chromium"},
    {"LibreWolf", "Privacy-focused Firefox fork", "Internet",
     "librewolf", "librewolf-bin", "librewolf", "librewolf"},
    
    // المكتب
    {"LibreOffice", "Full office suite", "Office",
     "libreoffice", "libreoffice-still", "libreoffice", "libreoffice"},
    {"OnlyOffice", "Microsoft Office compatible suite", "Office",
     "onlyoffice-desktopeditors", "onlyoffice-bin", "onlyoffice", "onlyoffice"},
    
    // التطوير
    {"VS Code", "Code editor from Microsoft", "Development",
     "code", "visual-studio-code-bin", "code", "code"},
    {"Neovim", "Hyperextensible Vim-based editor", "Development",
     "neovim", "neovim", "neovim", "neovim"},
    {"Git", "Distributed version control", "Development",
     "git", "git", "git", "git"},
    
    // الوسائط
    {"VLC", "Universal media player", "Media",
     "vlc", "vlc", "vlc", "vlc"},
    {"Audacity", "Audio editor and recorder", "Media",
     "audacity", "audacity", "audacity", "audacity"},
    {"OBS Studio", "Streaming and recording software", "Media",
     "obs-studio", "obs-studio", "obs-studio", "obs-studio"},
    
    // الرسوميات
    {"GIMP", "GNU Image Manipulation Program", "Graphics",
     "gimp", "gimp", "gimp", "gimp"},
    {"Inkscape", "Vector graphics editor", "Graphics",
     "inkscape", "inkscape", "inkscape", "inkscape"},
    {"Krita", "Digital painting application", "Graphics",
     "krita", "krita", "krita", "krita"},
    
    // الألعاب
    {"Steam", "Game distribution platform", "Games",
     "steam", "steam", "steam", ""},
    {"Lutris", "Game launcher for Linux", "Games",
     "lutris", "lutris", "lutris", ""},
    
    // الأدوات
    {"FileZilla", "FTP client", "Tools",
     "filezilla", "filezilla", "filezilla", "filezilla"},
    {"qBittorrent", "BitTorrent client", "Tools",
     "qbittorrent", "qbittorrent", "qbittorrent", "qbittorrent"},
    {"KeePassXC", "Password manager", "Tools",
     "keepassxc", "keepassxc", "keepassxc", "keepassxc"},
    
    // التواصل
    {"Discord", "Chat and voice application", "Communication",
     "discord", "discord", "discord", ""},
    {"Telegram", "Messaging app", "Communication",
     "telegram-desktop", "telegram-desktop", "telegram-desktop", "telegram-desktop"},
    {"Zoom", "Video conferencing", "Communication",
     "zoom", "zoom", "zoom", ""},
    
    // البيئات المكتبية
    {"XFCE", "Lightweight desktop environment", "Desktop",
     "xfce4", "xfce4", "xfce4", "xfce4"},
    {"LXDE", "Extremely lightweight desktop", "Desktop",
     "lxde", "lxde", "lxde", "lxde"},
    {"MATE", "Traditional desktop environment", "Desktop",
     "mate-desktop", "mate", "mate", "mate"},
};

static const int common_apps_count = sizeof(common_apps) / sizeof(common_apps[0]);

// الحصول على اسم مدير الحزم
static const char* get_package_manager_name(pkg_manager_type_t type) {
    switch (type) {
        case PKG_MANAGER_APT: return "apt";
        case PKG_MANAGER_PACMAN: return "pacman";
        case PKG_MANAGER_DNF: return "dnf";
        case PKG_MANAGER_APK: return "apk";
        case PKG_MANAGER_PKG: return "pkg";
        default: return "unknown";
    }
}

// الحصول على أمر التثبيت
static const char* get_install_command(pkg_manager_type_t type) {
    switch (type) {
        case PKG_MANAGER_APT: return "apt install -y";
        case PKG_MANAGER_PACMAN: return "pacman -S --noconfirm";
        case PKG_MANAGER_DNF: return "dnf install -y";
        case PKG_MANAGER_APK: return "apk add";
        case PKG_MANAGER_PKG: return "pkg install -y";
        default: return "";
    }
}

// الحصول على أمر الإزالة
static const char* get_remove_command(pkg_manager_type_t type) {
    switch (type) {
        case PKG_MANAGER_APT: return "apt remove -y";
        case PKG_MANAGER_PACMAN: return "pacman -R --noconfirm";
        case PKG_MANAGER_DNF: return "dnf remove -y";
        case PKG_MANAGER_APK: return "apk del";
        case PKG_MANAGER_PKG: return "pkg remove -y";
        default: return "";
    }
}

// الحصول على أمر التحديث
static const char* get_update_command(pkg_manager_type_t type) {
    switch (type) {
        case PKG_MANAGER_APT: return "apt update";
        case PKG_MANAGER_PACMAN: return "pacman -Sy";
        case PKG_MANAGER_DNF: return "dnf check-update";
        case PKG_MANAGER_APK: return "apk update";
        case PKG_MANAGER_PKG: return "pkg update";
        default: return "";
    }
}

// الحصول على أمر الترقية
static const char* get_upgrade_command(pkg_manager_type_t type) {
    switch (type) {
        case PKG_MANAGER_APT: return "apt upgrade -y";
        case PKG_MANAGER_PACMAN: return "pacman -Su --noconfirm";
        case PKG_MANAGER_DNF: return "dnf upgrade -y";
        case PKG_MANAGER_APK: return "apk upgrade";
        case PKG_MANAGER_PKG: return "pkg upgrade -y";
        default: return "";
    }
}

// تنفيذ أمر shell مع تتبع التقدم
static int execute_command_with_progress(const char* cmd, 
                                          void (*progress_cb)(const char*, int)) {
    FILE* fp;
    char line[MAX_LINE_LENGTH];
    char buffer[MAX_LINE_LENGTH * 2];
    
    snprintf(buffer, sizeof(buffer), "%s 2>&1", cmd);
    
    fp = popen(buffer, "r");
    if (!fp) {
        return -1;
    }
    
    int progress = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        // محاولة استخراج نسبة التقدم من الخرج
        if (strstr(line, "%")) {
            char* percent = strchr(line, '%');
            if (percent) {
                *percent = '\0';
                char* num = percent - 1;
                while (num > line && *num >= '0' && *num <= '9') num--;
                if (num < percent - 1) {
                    progress = atoi(num + 1);
                    if (progress_cb) {
                        progress_cb("", progress);
                    }
                }
            }
        }
    }
    
    int status = pclose(fp);
    return WEXITSTATUS(status);
}

// كشف مدير الحزم المثبت
static pkg_manager_type_t detect_package_manager(void) {
    if (access("/usr/bin/apt", X_OK) == 0 || access("/usr/bin/apt-get", X_OK) == 0) {
        return PKG_MANAGER_APT;
    }
    if (access("/usr/bin/pacman", X_OK) == 0) {
        return PKG_MANAGER_PACMAN;
    }
    if (access("/usr/bin/dnf", X_OK) == 0) {
        return PKG_MANAGER_DNF;
    }
    if (access("/sbin/apk", X_OK) == 0) {
        return PKG_MANAGER_APK;
    }
    if (access("/usr/bin/pkg", X_OK) == 0) {
        return PKG_MANAGER_PKG;
    }
    return PKG_MANAGER_APT; // افتراضي
}

// تهيئة الفئات الافتراضية
static void init_default_categories(void) {
    const char* category_names[] = {
        "Internet", "Office", "Development", "Media", 
        "Graphics", "Games", "Tools", "Communication", "Desktop"
    };
    const char* category_icons[] = {
        "globe", "file-text", "code", "play-circle",
        "image", "gamepad", "wrench", "message-circle", "monitor"
    };
    const char* category_desc[] = {
        "Web browsers and internet tools",
        "Office suites and productivity",
        "Development tools and editors",
        "Media players and editors",
        "Image editing and design tools",
        "Gaming platforms and tools",
        "System utilities and tools",
        "Chat and communication apps",
        "Desktop environments"
    };
    
    int num_categories = sizeof(category_names) / sizeof(category_names[0]);
    g_pm.categories = calloc(num_categories, sizeof(struct package_category));
    g_pm.category_count = num_categories;
    
    for (int i = 0; i < num_categories; i++) {
        strncpy(g_pm.categories[i].name, category_names[i], 63);
        strncpy(g_pm.categories[i].icon, category_icons[i], 63);
        strncpy(g_pm.categories[i].description, category_desc[i], 255);
        g_pm.categories[i].packages = NULL;
        g_pm.categories[i].package_count = 0;
    }
}

// تهيئة الحزم المتاحة
static void init_available_packages(void) {
    g_pm.available_packages = calloc(common_apps_count, sizeof(struct package_info));
    g_pm.available_count = common_apps_count;
    
    for (int i = 0; i < common_apps_count; i++) {
        strncpy(g_pm.available_packages[i].name, common_apps[i].name, 255);
        strncpy(g_pm.available_packages[i].description, common_apps[i].description, 1023);
        strncpy(g_pm.available_packages[i].category, common_apps[i].category, 63);
        g_pm.available_packages[i].state = PKG_STATE_NOT_INSTALLED;
        g_pm.available_packages[i].is_installed = false;
        g_pm.available_packages[i].has_update = false;
    }
}

// التحقق مما إذا كانت الحزمة مثبتة
static bool check_package_installed(const char* name) {
    char cmd[512];
    const char* pm_name = get_package_manager_name(g_pm.config.type);
    
    switch (g_pm.config.type) {
        case PKG_MANAGER_APT:
            snprintf(cmd, sizeof(cmd), "dpkg -l %s 2>/dev/null | grep -q '^ii'", name);
            break;
        case PKG_MANAGER_PACMAN:
            snprintf(cmd, sizeof(cmd), "pacman -Q %s >/dev/null 2>&1", name);
            break;
        case PKG_MANAGER_DNF:
            snprintf(cmd, sizeof(cmd), "rpm -q %s >/dev/null 2>&1", name);
            break;
        case PKG_MANAGER_APK:
            snprintf(cmd, sizeof(cmd), "apk info -e %s >/dev/null 2>&1", name);
            break;
        default:
            return false;
    }
    
    return system(cmd) == 0;
}

// تحديث حالة الحزم المثبتة
static void refresh_installed_status(void) {
    for (int i = 0; i < g_pm.available_count; i++) {
        const char* pkg_name = NULL;
        switch (g_pm.config.type) {
            case PKG_MANAGER_APT: pkg_name = common_apps[i].apt_name; break;
            case PKG_MANAGER_PACMAN: pkg_name = common_apps[i].pacman_name; break;
            case PKG_MANAGER_DNF: pkg_name = common_apps[i].dnf_name; break;
            case PKG_MANAGER_APK: pkg_name = common_apps[i].apk_name; break;
            default: continue;
        }
        
        if (pkg_name && pkg_name[0]) {
            g_pm.available_packages[i].is_installed = check_package_installed(pkg_name);
            if (g_pm.available_packages[i].is_installed) {
                g_pm.available_packages[i].state = PKG_STATE_INSTALLED;
            }
        }
    }
}

int package_manager_init(pkg_manager_type_t type) {
    pthread_mutex_lock(&g_pm_mutex);
    
    if (g_pm.initialized) {
        pthread_mutex_unlock(&g_pm_mutex);
        return 0;
    }
    
    memset(&g_pm, 0, sizeof(g_pm));
    
    // كشف مدير الحزم تلقائياً إذا لم يتم تحديده
    if (type < 0 || type > PKG_MANAGER_PKG) {
        g_pm.config.type = detect_package_manager();
    } else {
        g_pm.config.type = type;
    }
    
    // الإعدادات الافتراضية
    g_pm.config.auto_update = true;
    g_pm.config.update_interval_hours = 24;
    g_pm.config.verify_signatures = true;
    
    // تهيئة الفئات والحزم
    init_default_categories();
    init_available_packages();
    
    // تحديث حالة الحزم المثبتة
    refresh_installed_status();
    
    g_pm.initialized = 1;
    
    pthread_mutex_unlock(&g_pm_mutex);
    return 0;
}

void package_manager_terminate(void) {
    pthread_mutex_lock(&g_pm_mutex);
    
    if (!g_pm.initialized) {
        pthread_mutex_unlock(&g_pm_mutex);
        return;
    }
    
    // تحرير الذاكرة
    if (g_pm.categories) {
        for (int i = 0; i < g_pm.category_count; i++) {
            if (g_pm.categories[i].packages) {
                free(g_pm.categories[i].packages);
            }
        }
        free(g_pm.categories);
    }
    
    if (g_pm.available_packages) {
        free(g_pm.available_packages);
    }
    
    if (g_pm.installed_packages) {
        free(g_pm.installed_packages);
    }
    
    memset(&g_pm, 0, sizeof(g_pm));
    
    pthread_mutex_unlock(&g_pm_mutex);
}

void package_manager_set_config(const struct package_manager_config* config) {
    pthread_mutex_lock(&g_pm_mutex);
    if (config) {
        memcpy(&g_pm.config, config, sizeof(*config));
    }
    pthread_mutex_unlock(&g_pm_mutex);
}

const struct package_manager_config* package_manager_get_config(void) {
    return &g_pm.config;
}

int package_manager_update_repos(void) {
    pthread_mutex_lock(&g_pm_mutex);
    
    if (g_pm.updating) {
        pthread_mutex_unlock(&g_pm_mutex);
        return -1;
    }
    
    g_pm.updating = true;
    strncpy(g_pm.current_operation, "Updating package lists...", 255);
    
    const char* update_cmd = get_update_command(g_pm.config.type);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "sudo %s", update_cmd);
    
    pthread_mutex_unlock(&g_pm_mutex);
    
    int ret = execute_command_with_progress(cmd, g_pm.on_progress);
    
    pthread_mutex_lock(&g_pm_mutex);
    g_pm.updating = false;
    g_pm.current_operation[0] = '\0';
    
    if (ret == 0) {
        refresh_installed_status();
        if (g_pm.on_complete) {
            g_pm.on_complete(true, "Package lists updated successfully");
        }
    } else {
        if (g_pm.on_error) {
            g_pm.on_error("Failed to update package lists");
        }
    }
    
    pthread_mutex_unlock(&g_pm_mutex);
    return ret;
}

int package_manager_search(const char* query, struct package_info** results, int* count) {
    if (!query || !results || !count) {
        return -1;
    }
    
    pthread_mutex_lock(&g_pm_mutex);
    
    // البحث في الحزم المتاحة
    int matches = 0;
    for (int i = 0; i < g_pm.available_count; i++) {
        if (strcasestr(g_pm.available_packages[i].name, query) ||
            strcasestr(g_pm.available_packages[i].description, query)) {
            matches++;
        }
    }
    
    if (matches == 0) {
        *results = NULL;
        *count = 0;
        pthread_mutex_unlock(&g_pm_mutex);
        return 0;
    }
    
    *results = calloc(matches, sizeof(struct package_info));
    int idx = 0;
    for (int i = 0; i < g_pm.available_count && idx < matches; i++) {
        if (strcasestr(g_pm.available_packages[i].name, query) ||
            strcasestr(g_pm.available_packages[i].description, query)) {
            memcpy(&(*results)[idx], &g_pm.available_packages[i], sizeof(struct package_info));
            idx++;
        }
    }
    
    *count = matches;
    pthread_mutex_unlock(&g_pm_mutex);
    return 0;
}

int package_manager_get_categories(struct package_category** categories, int* count) {
    if (!categories || !count) {
        return -1;
    }
    
    pthread_mutex_lock(&g_pm_mutex);
    
    *categories = calloc(g_pm.category_count, sizeof(struct package_category));
    memcpy(*categories, g_pm.categories, g_pm.category_count * sizeof(struct package_category));
    *count = g_pm.category_count;
    
    pthread_mutex_unlock(&g_pm_mutex);
    return 0;
}

int package_manager_get_installed(struct package_info** packages, int* count) {
    if (!packages || !count) {
        return -1;
    }
    
    pthread_mutex_lock(&g_pm_mutex);
    
    // عد الحزم المثبتة
    int installed = 0;
    for (int i = 0; i < g_pm.available_count; i++) {
        if (g_pm.available_packages[i].is_installed) {
            installed++;
        }
    }
    
    if (installed == 0) {
        *packages = NULL;
        *count = 0;
        pthread_mutex_unlock(&g_pm_mutex);
        return 0;
    }
    
    *packages = calloc(installed, sizeof(struct package_info));
    int idx = 0;
    for (int i = 0; i < g_pm.available_count && idx < installed; i++) {
        if (g_pm.available_packages[i].is_installed) {
            memcpy(&(*packages)[idx], &g_pm.available_packages[i], sizeof(struct package_info));
            idx++;
        }
    }
    
    *count = installed;
    pthread_mutex_unlock(&g_pm_mutex);
    return 0;
}

int package_manager_install(const char* name) {
    if (!name) {
        return -1;
    }
    
    pthread_mutex_lock(&g_pm_mutex);
    
    if (g_pm.installing) {
        pthread_mutex_unlock(&g_pm_mutex);
        return -1;
    }
    
    // البحث عن الحزمة
    int idx = -1;
    for (int i = 0; i < g_pm.available_count; i++) {
        if (strcasecmp(g_pm.available_packages[i].name, name) == 0) {
            idx = i;
            break;
        }
    }
    
    if (idx < 0) {
        pthread_mutex_unlock(&g_pm_mutex);
        if (g_pm.on_error) {
            g_pm.on_error("Package not found");
        }
        return -1;
    }
    
    // الحصول على اسم الحزمة لمدير الحزم
    const char* pkg_name = NULL;
    switch (g_pm.config.type) {
        case PKG_MANAGER_APT: pkg_name = common_apps[idx].apt_name; break;
        case PKG_MANAGER_PACMAN: pkg_name = common_apps[idx].pacman_name; break;
        case PKG_MANAGER_DNF: pkg_name = common_apps[idx].dnf_name; break;
        case PKG_MANAGER_APK: pkg_name = common_apps[idx].apk_name; break;
        default: 
            pthread_mutex_unlock(&g_pm_mutex);
            return -1;
    }
    
    if (!pkg_name || !pkg_name[0]) {
        pthread_mutex_unlock(&g_pm_mutex);
        if (g_pm.on_error) {
            g_pm.on_error("Package not available for this distribution");
        }
        return -1;
    }
    
    g_pm.installing = true;
    g_pm.current_progress = 0;
    snprintf(g_pm.current_operation, 255, "Installing %s...", name);
    g_pm.available_packages[idx].state = PKG_STATE_INSTALLING;
    
    const char* install_cmd = get_install_command(g_pm.config.type);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "sudo %s %s", install_cmd, pkg_name);
    
    pthread_mutex_unlock(&g_pm_mutex);
    
    int ret = execute_command_with_progress(cmd, g_pm.on_progress);
    
    pthread_mutex_lock(&g_pm_mutex);
    g_pm.installing = false;
    g_pm.current_progress = 100;
    g_pm.current_operation[0] = '\0';
    
    if (ret == 0) {
        g_pm.available_packages[idx].is_installed = true;
        g_pm.available_packages[idx].state = PKG_STATE_INSTALLED;
        if (g_pm.on_complete) {
            char msg[256];
            snprintf(msg, sizeof(msg), "%s installed successfully", name);
            g_pm.on_complete(true, msg);
        }
    } else {
        g_pm.available_packages[idx].state = PKG_STATE_ERROR;
        if (g_pm.on_error) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Failed to install %s", name);
            g_pm.on_error(msg);
        }
    }
    
    pthread_mutex_unlock(&g_pm_mutex);
    return ret;
}

int package_manager_remove(const char* name) {
    if (!name) {
        return -1;
    }
    
    pthread_mutex_lock(&g_pm_mutex);
    
    // البحث عن الحزمة
    int idx = -1;
    for (int i = 0; i < g_pm.available_count; i++) {
        if (strcasecmp(g_pm.available_packages[i].name, name) == 0) {
            idx = i;
            break;
        }
    }
    
    if (idx < 0) {
        pthread_mutex_unlock(&g_pm_mutex);
        return -1;
    }
    
    const char* pkg_name = NULL;
    switch (g_pm.config.type) {
        case PKG_MANAGER_APT: pkg_name = common_apps[idx].apt_name; break;
        case PKG_MANAGER_PACMAN: pkg_name = common_apps[idx].pacman_name; break;
        case PKG_MANAGER_DNF: pkg_name = common_apps[idx].dnf_name; break;
        case PKG_MANAGER_APK: pkg_name = common_apps[idx].apk_name; break;
        default: 
            pthread_mutex_unlock(&g_pm_mutex);
            return -1;
    }
    
    g_pm.available_packages[idx].state = PKG_STATE_REMOVING;
    
    const char* remove_cmd = get_remove_command(g_pm.config.type);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "sudo %s %s", remove_cmd, pkg_name);
    
    pthread_mutex_unlock(&g_pm_mutex);
    
    int ret = system(cmd);
    
    pthread_mutex_lock(&g_pm_mutex);
    
    if (ret == 0) {
        g_pm.available_packages[idx].is_installed = false;
        g_pm.available_packages[idx].state = PKG_STATE_NOT_INSTALLED;
        if (g_pm.on_complete) {
            char msg[256];
            snprintf(msg, sizeof(msg), "%s removed successfully", name);
            g_pm.on_complete(true, msg);
        }
    } else {
        g_pm.available_packages[idx].state = PKG_STATE_ERROR;
    }
    
    pthread_mutex_unlock(&g_pm_mutex);
    return ret;
}

int package_manager_upgrade_all(void) {
    pthread_mutex_lock(&g_pm_mutex);
    
    strncpy(g_pm.current_operation, "Upgrading all packages...", 255);
    
    const char* upgrade_cmd = get_upgrade_command(g_pm.config.type);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "sudo %s", upgrade_cmd);
    
    pthread_mutex_unlock(&g_pm_mutex);
    
    int ret = execute_command_with_progress(cmd, g_pm.on_progress);
    
    pthread_mutex_lock(&g_pm_mutex);
    g_pm.current_operation[0] = '\0';
    
    if (ret == 0) {
        refresh_installed_status();
        if (g_pm.on_complete) {
            g_pm.on_complete(true, "All packages upgraded successfully");
        }
    } else {
        if (g_pm.on_error) {
            g_pm.on_error("Failed to upgrade packages");
        }
    }
    
    pthread_mutex_unlock(&g_pm_mutex);
    return ret;
}

int package_manager_install_common_apps(void) {
    const char* essential_apps[] = {
        "Firefox",
        "LibreOffice", 
        "VS Code",
        "VLC"
    };
    
    int count = sizeof(essential_apps) / sizeof(essential_apps[0]);
    
    for (int i = 0; i < count; i++) {
        int ret = package_manager_install(essential_apps[i]);
        if (ret != 0) {
            return ret;
        }
    }
    
    return 0;
}

int package_manager_install_desktop_environment(const char* de) {
    if (!de) {
        return -1;
    }
    
    if (strcasecmp(de, "xfce") == 0) {
        return package_manager_install("XFCE");
    } else if (strcasecmp(de, "lxde") == 0) {
        return package_manager_install("LXDE");
    } else if (strcasecmp(de, "mate") == 0) {
        return package_manager_install("MATE");
    }
    
    return -1;
}

bool package_manager_is_updating(void) {
    pthread_mutex_lock(&g_pm_mutex);
    bool updating = g_pm.updating;
    pthread_mutex_unlock(&g_pm_mutex);
    return updating;
}

bool package_manager_is_installing(void) {
    pthread_mutex_lock(&g_pm_mutex);
    bool installing = g_pm.installing;
    pthread_mutex_unlock(&g_pm_mutex);
    return installing;
}

int package_manager_get_progress(void) {
    pthread_mutex_lock(&g_pm_mutex);
    int progress = g_pm.current_progress;
    pthread_mutex_unlock(&g_pm_mutex);
    return progress;
}

const char* package_manager_get_current_operation(void) {
    pthread_mutex_lock(&g_pm_mutex);
    const char* op = g_pm.current_operation;
    pthread_mutex_unlock(&g_pm_mutex);
    return op;
}

void package_manager_set_callbacks(
    void (*on_progress)(const char* package, int percent),
    void (*on_complete)(bool success, const char* message),
    void (*on_error)(const char* error)
) {
    pthread_mutex_lock(&g_pm_mutex);
    g_pm.on_progress = on_progress;
    g_pm.on_complete = on_complete;
    g_pm.on_error = on_error;
    pthread_mutex_unlock(&g_pm_mutex);
}

// دوال إضافية
int package_manager_install_multiple(const char** names, int count) {
    if (!names || count <= 0) {
        return -1;
    }
    
    for (int i = 0; i < count; i++) {
        int ret = package_manager_install(names[i]);
        if (ret != 0) {
            return ret;
        }
    }
    
    return 0;
}

int package_manager_purge(const char* name) {
    // لـ apt، يمكن استخدام purge بدلاً من remove
    if (g_pm.config.type == PKG_MANAGER_APT) {
        pthread_mutex_lock(&g_pm_mutex);
        
        int idx = -1;
        for (int i = 0; i < g_pm.available_count; i++) {
            if (strcasecmp(g_pm.available_packages[i].name, name) == 0) {
                idx = i;
                break;
            }
        }
        
        if (idx < 0) {
            pthread_mutex_unlock(&g_pm_mutex);
            return -1;
        }
        
        const char* pkg_name = common_apps[idx].apt_name;
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "sudo apt purge -y %s", pkg_name);
        
        pthread_mutex_unlock(&g_pm_mutex);
        return system(cmd);
    }
    
    // للمديرين الآخرين، استخدم الإزالة العادية
    return package_manager_remove(name);
}

int package_manager_upgrade(const char* name) {
    // تحديث حزمة واحدة
    pthread_mutex_lock(&g_pm_mutex);
    
    int idx = -1;
    for (int i = 0; i < g_pm.available_count; i++) {
        if (strcasecmp(g_pm.available_packages[i].name, name) == 0) {
            idx = i;
            break;
        }
    }
    
    if (idx < 0 || !g_pm.available_packages[idx].is_installed) {
        pthread_mutex_unlock(&g_pm_mutex);
        return -1;
    }
    
    const char* pkg_name = NULL;
    switch (g_pm.config.type) {
        case PKG_MANAGER_APT: pkg_name = common_apps[idx].apt_name; break;
        case PKG_MANAGER_PACMAN: pkg_name = common_apps[idx].pacman_name; break;
        case PKG_MANAGER_DNF: pkg_name = common_apps[idx].dnf_name; break;
        case PKG_MANAGER_APK: pkg_name = common_apps[idx].apk_name; break;
        default: 
            pthread_mutex_unlock(&g_pm_mutex);
            return -1;
    }
    
    g_pm.available_packages[idx].state = PKG_STATE_UPDATING;
    
    char cmd[512];
    if (g_pm.config.type == PKG_MANAGER_APT) {
        snprintf(cmd, sizeof(cmd), "sudo apt install --only-upgrade -y %s", pkg_name);
    } else {
        snprintf(cmd, sizeof(cmd), "sudo %s %s", get_upgrade_command(g_pm.config.type), pkg_name);
    }
    
    pthread_mutex_unlock(&g_pm_mutex);
    
    int ret = system(cmd);
    
    pthread_mutex_lock(&g_pm_mutex);
    g_pm.available_packages[idx].state = PKG_STATE_INSTALLED;
    pthread_mutex_unlock(&g_pm_mutex);
    
    return ret;
}

int package_manager_get_updates(struct package_info** packages, int* count) {
    if (!packages || !count) {
        return -1;
    }
    
    // تحديث قائمة الحزم المتاحة أولاً
    package_manager_update_repos();
    
    pthread_mutex_lock(&g_pm_mutex);
    
    // عد الحزم التي لديها تحديثات
    int updates = 0;
    for (int i = 0; i < g_pm.available_count; i++) {
        if (g_pm.available_packages[i].is_installed && g_pm.available_packages[i].has_update) {
            updates++;
        }
    }
    
    if (updates == 0) {
        *packages = NULL;
        *count = 0;
        pthread_mutex_unlock(&g_pm_mutex);
        return 0;
    }
    
    *packages = calloc(updates, sizeof(struct package_info));
    int idx = 0;
    for (int i = 0; i < g_pm.available_count && idx < updates; i++) {
        if (g_pm.available_packages[i].is_installed && g_pm.available_packages[i].has_update) {
            memcpy(&(*packages)[idx], &g_pm.available_packages[i], sizeof(struct package_info));
            idx++;
        }
    }
    
    *count = updates;
    pthread_mutex_unlock(&g_pm_mutex);
    return 0;
}

struct package_info* package_manager_get_info(const char* name) {
    if (!name) {
        return NULL;
    }
    
    pthread_mutex_lock(&g_pm_mutex);
    
    for (int i = 0; i < g_pm.available_count; i++) {
        if (strcasecmp(g_pm.available_packages[i].name, name) == 0) {
            struct package_info* info = malloc(sizeof(struct package_info));
            memcpy(info, &g_pm.available_packages[i], sizeof(struct package_info));
            pthread_mutex_unlock(&g_pm_mutex);
            return info;
        }
    }
    
    pthread_mutex_unlock(&g_pm_mutex);
    return NULL;
}

int package_manager_add_repo(const char* url) {
    if (!url) {
        return -1;
    }
    
    char cmd[1024];
    
    switch (g_pm.config.type) {
        case PKG_MANAGER_APT:
            snprintf(cmd, sizeof(cmd), "echo 'deb %s' | sudo tee /etc/apt/sources.list.d/winland.list", url);
            break;
        case PKG_MANAGER_PACMAN:
            snprintf(cmd, sizeof(cmd), "echo '[winland]\\nServer = %s' | sudo tee -a /etc/pacman.conf", url);
            break;
        default:
            return -1;
    }
    
    return system(cmd);
}

int package_manager_remove_repo(const char* url) {
    // تبسيط - إزالة ملف المصادر المخصص
    if (g_pm.config.type == PKG_MANAGER_APT) {
        return system("sudo rm -f /etc/apt/sources.list.d/winland.list");
    }
    return 0;
}

int package_manager_clean_cache(void) {
    const char* clean_cmd = NULL;
    
    switch (g_pm.config.type) {
        case PKG_MANAGER_APT: clean_cmd = "sudo apt clean"; break;
        case PKG_MANAGER_PACMAN: clean_cmd = "sudo pacman -Sc --noconfirm"; break;
        case PKG_MANAGER_DNF: clean_cmd = "sudo dnf clean all"; break;
        case PKG_MANAGER_APK: clean_cmd = "sudo apk cache clean"; break;
        default: return -1;
    }
    
    return system(clean_cmd);
}

int package_manager_autoremove(void) {
    const char* autoremove_cmd = NULL;
    
    switch (g_pm.config.type) {
        case PKG_MANAGER_APT: autoremove_cmd = "sudo apt autoremove -y"; break;
        case PKG_MANAGER_PACMAN: autoremove_cmd = "sudo pacman -Rns $(pacman -Qtdq) --noconfirm 2>/dev/null || true"; break;
        case PKG_MANAGER_DNF: autoremove_cmd = "sudo dnf autoremove -y"; break;
        default: return -1;
    }
    
    return system(autoremove_cmd);
}
