#ifndef FILE_SHARING_H
#define FILE_SHARING_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// أنواع الملفات
typedef enum {
    FILE_TYPE_UNKNOWN,
    FILE_TYPE_TEXT,
    FILE_TYPE_IMAGE,
    FILE_TYPE_VIDEO,
    FILE_TYPE_AUDIO,
    FILE_TYPE_DOCUMENT,
    FILE_TYPE_ARCHIVE,
    FILE_TYPE_DIRECTORY
} file_type_t;

// عمليات الملفات
typedef enum {
    FILE_OP_NONE,
    FILE_OP_COPY,
    FILE_OP_MOVE,
    FILE_OP_DELETE,
    FILE_OP_RENAME
} file_operation_t;

// معلومات الملف
struct file_info {
    char path[1024];
    char name[256];
    file_type_t type;
    uint64_t size;
    uint64_t modified_time;
    bool is_directory;
    bool is_hidden;
    bool is_readonly;
};

// إعدادات المشاركة
struct sharing_config {
    // المجلدات المشتركة
    char android_downloads[1024];
    char linux_home[1024];
    char shared_folder[1024];
    
    // السلوك
    bool auto_sync;
    int sync_interval_seconds;
    bool bidirectional;
    
    // الأنواع المسموح بها
    bool allow_text;
    bool allow_images;
    bool allow_videos;
    bool allow_documents;
    bool allow_archives;
    
    // الحدود
    uint64_t max_file_size;
    int max_sync_files;
};

// هيكل مدير المشاركة
struct file_sharing {
    int initialized;
    
    // الإعدادات
    struct sharing_config config;
    
    // الحالة
    bool syncing;
    int sync_progress;
    
    // قائمة الملفات المعلقة
    struct {
        char path[1024];
        file_operation_t operation;
    } pending_ops[100];
    int pending_count;
    
    // معاودات الاتصال
    void (*on_file_added)(const char* path);
    void (*on_file_removed)(const char* path);
    void (*on_file_modified)(const char* path);
    void (*on_sync_progress)(int percent);
};

// دوال التهيئة
int file_sharing_init(void);
void file_sharing_terminate(void);

// التكوين
void file_sharing_set_config(const struct sharing_config* config);
const struct sharing_config* file_sharing_get_config(void);

// إدارة المجلدات
int file_sharing_setup_shared_folder(void);
int file_sharing_mount_android_downloads(void);
int file_sharing_mount_linux_home(void);

// المزامنة
int file_sharing_sync(void);
int file_sharing_sync_to_android(const char* linux_path);
int file_sharing_sync_to_linux(const char* android_path);
void file_sharing_cancel_sync(void);
bool file_sharing_is_syncing(void);
int file_sharing_get_sync_progress(void);

// عمليات الملفات
int file_sharing_copy(const char* src, const char* dst);
int file_sharing_move(const char* src, const char* dst);
int file_sharing_delete(const char* path);
int file_sharing_rename(const char* old_path, const char* new_name);

// البحث والفحص
int file_sharing_scan_folder(const char* path, struct file_info** files, int* count);
file_type_t file_sharing_detect_type(const char* path);
bool file_sharing_is_allowed_type(file_type_t type);

// المراقبة
int file_sharing_start_watching(void);
void file_sharing_stop_watching(void);

// المسارات
char* file_sharing_linux_to_android_path(const char* linux_path);
char* file_sharing_android_to_linux_path(const char* android_path);

// معاودات الاتصال
void file_sharing_set_callbacks(
    void (*on_file_added)(const char* path),
    void (*on_file_removed)(const char* path),
    void (*on_file_modified)(const char* path),
    void (*on_sync_progress)(int percent)
);

#ifdef __cplusplus
}
#endif

#endif // FILE_SHARING_H
