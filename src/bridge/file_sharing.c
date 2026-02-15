#include "file_sharing.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <pthread.h>
#include <android/log.h>

#define LOG_TAG "FileSharing"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define MAX_PATH_LENGTH 1024
#define MAX_FILES 10000

// المتغير العام
static struct file_sharing g_sharing = {0};
static pthread_t g_watch_thread;
static pthread_mutex_t g_sharing_mutex = PTHREAD_MUTEX_INITIALIZER;

// دالة مراقبة الملفات
static void* watch_thread_func(void* arg);

// الحصول على امتداد الملف
static const char* get_extension(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot || dot == path) return "";
    return dot + 1;
}

// تهيئة مشاركة الملفات
int file_sharing_init(void) {
    if (g_sharing.initialized) {
        return 0;
    }
    
    memset(&g_sharing, 0, sizeof(g_sharing));
    
    // الإعدادات الافتراضية
    strcpy(g_sharing.config.android_downloads, "/sdcard/Download");
    strcpy(g_sharing.config.linux_home, "/data/data/com.winland.server/files/home");
    strcpy(g_sharing.config.shared_folder, "/data/data/com.winland.server/files/shared");
    
    g_sharing.config.auto_sync = true;
    g_sharing.config.sync_interval_seconds = 300;  // 5 دقائق
    g_sharing.config.bidirectional = true;
    
    g_sharing.config.allow_text = true;
    g_sharing.config.allow_images = true;
    g_sharing.config.allow_videos = true;
    g_sharing.config.allow_documents = true;
    g_sharing.config.allow_archives = true;
    
    g_sharing.config.max_file_size = 100 * 1024 * 1024;  // 100 MB
    g_sharing.config.max_sync_files = 1000;
    
    // إنشاء المجلدات
    mkdir(g_sharing.config.linux_home, 0755);
    mkdir(g_sharing.config.shared_folder, 0755);
    
    g_sharing.initialized = 1;
    LOGI("File sharing initialized");
    return 0;
}

// إنهاء مشاركة الملفات
void file_sharing_terminate(void) {
    if (!g_sharing.initialized) return;
    
    file_sharing_stop_watching();
    
    g_sharing.initialized = 0;
    LOGI("File sharing terminated");
}

// تعيين الإعدادات
void file_sharing_set_config(const struct sharing_config* config) {
    if (!config) return;
    
    pthread_mutex_lock(&g_sharing_mutex);
    memcpy(&g_sharing.config, config, sizeof(*config));
    pthread_mutex_unlock(&g_sharing_mutex);
}

// الحصول على الإعدادات
const struct sharing_config* file_sharing_get_config(void) {
    return &g_sharing.config;
}

// إعداد المجلد المشترك
int file_sharing_setup_shared_folder(void) {
    if (!g_sharing.initialized) return -1;
    
    // إنشاء روابط رمزية
    char downloads_link[MAX_PATH_LENGTH];
    snprintf(downloads_link, sizeof(downloads_link), "%s/Downloads",
             g_sharing.config.shared_folder);
    
    // إزالة الرابط القديم إذا existed
    unlink(downloads_link);
    
    // إنشاء رابط رمزي
    if (symlink(g_sharing.config.android_downloads, downloads_link) < 0) {
        LOGE("Failed to create symlink: %s", strerror(errno));
        return -1;
    }
    
    LOGI("Shared folder setup complete");
    return 0;
}

// تركيب مجلد Android Downloads
int file_sharing_mount_android_downloads(void) {
    // TODO: Implement bind mount for root users
    LOGI("Android Downloads folder mounted");
    return 0;
}

// تركيب مجلد Linux home
int file_sharing_mount_linux_home(void) {
    // TODO: Implement bind mount for root users
    LOGI("Linux home folder mounted");
    return 0;
}

// مزامنة
int file_sharing_sync(void) {
    if (!g_sharing.initialized || g_sharing.syncing) return -1;
    
    g_sharing.syncing = true;
    g_sharing.sync_progress = 0;
    
    LOGI("Starting file sync...");
    
    // TODO: Implement actual sync logic
    // Scan both folders and copy new/modified files
    
    g_sharing.sync_progress = 100;
    g_sharing.syncing = false;
    
    LOGI("File sync complete");
    return 0;
}

// مزامنة إلى Android
int file_sharing_sync_to_android(const char* linux_path) {
    if (!linux_path) return -1;
    
    char* android_path = file_sharing_linux_to_android_path(linux_path);
    if (!android_path) return -1;
    
    int result = file_sharing_copy(linux_path, android_path);
    
    free(android_path);
    return result;
}

// مزامنة إلى Linux
int file_sharing_sync_to_linux(const char* android_path) {
    if (!android_path) return -1;
    
    char* linux_path = file_sharing_android_to_linux_path(android_path);
    if (!linux_path) return -1;
    
    int result = file_sharing_copy(android_path, linux_path);
    
    free(linux_path);
    return result;
}

// إلغاء المزامنة
void file_sharing_cancel_sync(void) {
    g_sharing.syncing = false;
    LOGI("Sync cancelled");
}

// التحقق من المزامنة
bool file_sharing_is_syncing(void) {
    return g_sharing.syncing;
}

// الحصول على تقدم المزامنة
int file_sharing_get_sync_progress(void) {
    return g_sharing.sync_progress;
}

// نسخ ملف
int file_sharing_copy(const char* src, const char* dst) {
    if (!src || !dst) return -1;
    
    FILE* src_file = fopen(src, "rb");
    if (!src_file) {
        LOGE("Failed to open source file: %s", src);
        return -1;
    }
    
    FILE* dst_file = fopen(dst, "wb");
    if (!dst_file) {
        LOGE("Failed to create destination file: %s", dst);
        fclose(src_file);
        return -1;
    }
    
    char buffer[8192];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
        if (fwrite(buffer, 1, bytes_read, dst_file) != bytes_read) {
            LOGE("Failed to write to destination file");
            fclose(src_file);
            fclose(dst_file);
            return -1;
        }
    }
    
    fclose(src_file);
    fclose(dst_file);
    
    LOGI("File copied: %s -> %s", src, dst);
    return 0;
}

// نقل ملف
int file_sharing_move(const char* src, const char* dst) {
    if (!src || !dst) return -1;
    
    if (rename(src, dst) == 0) {
        LOGI("File moved: %s -> %s", src, dst);
        return 0;
    }
    
    // إذا فشل rename، جرب copy ثم delete
    if (file_sharing_copy(src, dst) == 0) {
        return file_sharing_delete(src);
    }
    
    return -1;
}

// حذف ملف
int file_sharing_delete(const char* path) {
    if (!path) return -1;
    
    struct stat st;
    if (stat(path, &st) < 0) {
        LOGE("File not found: %s", path);
        return -1;
    }
    
    if (S_ISDIR(st.st_mode)) {
        // حذف مجلد بشكل متكرر
        // TODO: Implement recursive delete
        if (rmdir(path) < 0) {
            LOGE("Failed to delete directory: %s", strerror(errno));
            return -1;
        }
    } else {
        if (unlink(path) < 0) {
            LOGE("Failed to delete file: %s", strerror(errno));
            return -1;
        }
    }
    
    LOGI("File deleted: %s", path);
    return 0;
}

// إعادة تسمية ملف
int file_sharing_rename(const char* old_path, const char* new_name) {
    if (!old_path || !new_name) return -1;
    
    // استخراج المجلد الأب
    char parent[MAX_PATH_LENGTH];
    strncpy(parent, old_path, sizeof(parent));
    char* last_slash = strrchr(parent, '/');
    if (last_slash) {
        *(last_slash + 1) = '\0';
    }
    
    // إنشاء المسار الجديد
    char new_path[MAX_PATH_LENGTH];
    snprintf(new_path, sizeof(new_path), "%s%s", parent, new_name);
    
    return file_sharing_move(old_path, new_path);
}

// فحص مجلد
int file_sharing_scan_folder(const char* path, struct file_info** files, int* count) {
    if (!path || !files || !count) return -1;
    
    DIR* dir = opendir(path);
    if (!dir) {
        LOGE("Failed to open directory: %s", path);
        return -1;
    }
    
    // تخصيص المصفوفة
    *files = malloc(MAX_FILES * sizeof(struct file_info));
    if (!*files) {
        closedir(dir);
        return -1;
    }
    
    *count = 0;
    struct dirent* entry;
    
    while ((entry = readdir(dir)) != NULL && *count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        struct file_info* info = &(*files)[*count];
        
        snprintf(info->path, sizeof(info->path), "%s/%s", path, entry->d_name);
        strncpy(info->name, entry->d_name, sizeof(info->name) - 1);
        
        struct stat st;
        if (stat(info->path, &st) == 0) {
            info->size = st.st_size;
            info->modified_time = st.st_mtime;
            info->is_directory = S_ISDIR(st.st_mode);
            info->is_hidden = (entry->d_name[0] == '.');
            info->is_readonly = !(st.st_mode & S_IWUSR);
            info->type = file_sharing_detect_type(info->path);
        }
        
        (*count)++;
    }
    
    closedir(dir);
    return 0;
}

// كشف نوع الملف
file_type_t file_sharing_detect_type(const char* path) {
    if (!path) return FILE_TYPE_UNKNOWN;
    
    const char* ext = get_extension(path);
    
    // نص
    if (strcasecmp(ext, "txt") == 0 ||
        strcasecmp(ext, "md") == 0 ||
        strcasecmp(ext, "csv") == 0 ||
        strcasecmp(ext, "log") == 0) {
        return FILE_TYPE_TEXT;
    }
    
    // صور
    if (strcasecmp(ext, "jpg") == 0 ||
        strcasecmp(ext, "jpeg") == 0 ||
        strcasecmp(ext, "png") == 0 ||
        strcasecmp(ext, "gif") == 0 ||
        strcasecmp(ext, "bmp") == 0 ||
        strcasecmp(ext, "webp") == 0) {
        return FILE_TYPE_IMAGE;
    }
    
    // فيديو
    if (strcasecmp(ext, "mp4") == 0 ||
        strcasecmp(ext, "avi") == 0 ||
        strcasecmp(ext, "mkv") == 0 ||
        strcasecmp(ext, "mov") == 0) {
        return FILE_TYPE_VIDEO;
    }
    
    // صوت
    if (strcasecmp(ext, "mp3") == 0 ||
        strcasecmp(ext, "wav") == 0 ||
        strcasecmp(ext, "ogg") == 0 ||
        strcasecmp(ext, "flac") == 0) {
        return FILE_TYPE_AUDIO;
    }
    
    // مستندات
    if (strcasecmp(ext, "pdf") == 0 ||
        strcasecmp(ext, "doc") == 0 ||
        strcasecmp(ext, "docx") == 0 ||
        strcasecmp(ext, "xls") == 0 ||
        strcasecmp(ext, "xlsx") == 0 ||
        strcasecmp(ext, "ppt") == 0 ||
        strcasecmp(ext, "pptx") == 0) {
        return FILE_TYPE_DOCUMENT;
    }
    
    // أرشيف
    if (strcasecmp(ext, "zip") == 0 ||
        strcasecmp(ext, "tar") == 0 ||
        strcasecmp(ext, "gz") == 0 ||
        strcasecmp(ext, "bz2") == 0 ||
        strcasecmp(ext, "7z") == 0 ||
        strcasecmp(ext, "rar") == 0) {
        return FILE_TYPE_ARCHIVE;
    }
    
    return FILE_TYPE_UNKNOWN;
}

// التحقق من نوع مسموح
bool file_sharing_is_allowed_type(file_type_t type) {
    switch (type) {
        case FILE_TYPE_TEXT: return g_sharing.config.allow_text;
        case FILE_TYPE_IMAGE: return g_sharing.config.allow_images;
        case FILE_TYPE_VIDEO: return g_sharing.config.allow_videos;
        case FILE_TYPE_DOCUMENT: return g_sharing.config.allow_documents;
        case FILE_TYPE_ARCHIVE: return g_sharing.config.allow_archives;
        default: return true;
    }
}

// بدء المراقبة
int file_sharing_start_watching(void) {
    if (!g_sharing.initialized) return -1;
    
    if (pthread_create(&g_watch_thread, NULL, watch_thread_func, NULL) != 0) {
        LOGE("Failed to create watch thread");
        return -1;
    }
    
    LOGI("File watching started");
    return 0;
}

// إيقاف المراقبة
void file_sharing_stop_watching(void) {
    // TODO: Implement proper thread shutdown
    LOGI("File watching stopped");
}

// دالة مراقبة الملفات
static void* watch_thread_func(void* arg) {
    LOGI("Watch thread started");
    
    while (g_sharing.initialized) {
        // TODO: Implement file system watching using inotify
        sleep(g_sharing.config.sync_interval_seconds);
        
        if (g_sharing.config.auto_sync) {
            file_sharing_sync();
        }
    }
    
    LOGI("Watch thread stopped");
    return NULL;
}

// تحويل مسار Linux إلى Android
char* file_sharing_linux_to_android_path(const char* linux_path) {
    if (!linux_path) return NULL;
    
    char* android_path = malloc(MAX_PATH_LENGTH);
    if (!android_path) return NULL;
    
    // استبدال المسار
    if (strncmp(linux_path, g_sharing.config.linux_home, 
                strlen(g_sharing.config.linux_home)) == 0) {
        snprintf(android_path, MAX_PATH_LENGTH, "%s%s",
                 g_sharing.config.android_downloads,
                 linux_path + strlen(g_sharing.config.linux_home));
    } else {
        strncpy(android_path, linux_path, MAX_PATH_LENGTH - 1);
    }
    
    return android_path;
}

// تحويل مسار Android إلى Linux
char* file_sharing_android_to_linux_path(const char* android_path) {
    if (!android_path) return NULL;
    
    char* linux_path = malloc(MAX_PATH_LENGTH);
    if (!linux_path) return NULL;
    
    // استبدال المسار
    if (strncmp(android_path, g_sharing.config.android_downloads,
                strlen(g_sharing.config.android_downloads)) == 0) {
        snprintf(linux_path, MAX_PATH_LENGTH, "%s%s",
                 g_sharing.config.linux_home,
                 android_path + strlen(g_sharing.config.android_downloads));
    } else {
        strncpy(linux_path, android_path, MAX_PATH_LENGTH - 1);
    }
    
    return linux_path;
}

// تعيين معاودات الاتصال
void file_sharing_set_callbacks(
    void (*on_file_added)(const char* path),
    void (*on_file_removed)(const char* path),
    void (*on_file_modified)(const char* path),
    void (*on_sync_progress)(int percent)
) {
    g_sharing.on_file_added = on_file_added;
    g_sharing.on_file_removed = on_file_removed;
    g_sharing.on_file_modified = on_file_modified;
    g_sharing.on_sync_progress = on_sync_progress;
}
