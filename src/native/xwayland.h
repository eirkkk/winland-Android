#ifndef XWAYLAND_H
#define XWAYLAND_H

#include <stdint.h>
#include <stdbool.h>
#include <wayland-server.h>

#ifdef __cplusplus
extern "C" {
#endif

// حالات XWayland
typedef enum {
    XWAYLAND_STATE_STOPPED,
    XWAYLAND_STATE_STARTING,
    XWAYLAND_STATE_RUNNING,
    XWAYLAND_STATE_ERROR
} xwayland_state_t;

// أنواع نوافذ X11
typedef enum {
    X11_WINDOW_TYPE_NORMAL,
    X11_WINDOW_TYPE_DIALOG,
    X11_WINDOW_TYPE_MENU,
    X11_WINDOW_TYPE_TOOLBAR,
    X11_WINDOW_TYPE_SPLASH,
    X11_WINDOW_TYPE_UTILITY,
    X11_WINDOW_TYPE_DOCK,
    X11_WINDOW_TYPE_DESKTOP,
    X11_WINDOW_TYPE_POPUP_MENU,
    X11_WINDOW_TYPE_DROPDOWN_MENU,
    X11_WINDOW_TYPE_TOOLTIP,
    X11_WINDOW_TYPE_NOTIFICATION,
    X11_WINDOW_TYPE_COMBO,
    X11_WINDOW_TYPE_DND
} x11_window_type_t;

// خصائص نافذة X11
struct x11_window_props {
    char title[256];
    char class[128];
    char instance[128];
    char role[128];
    x11_window_type_t window_type;
    bool modal;
    bool fullscreen;
    bool maximized_vert;
    bool maximized_horz;
    bool minimized;
    bool shaded;
    bool skip_taskbar;
    bool skip_pager;
    bool above;
    bool below;
    bool sticky;
    int x, y;
    int width, height;
    int min_width, min_height;
    int max_width, max_height;
    uint32_t workspace;
};

// هيكل نافذة XWayland
struct xwayland_surface {
    struct wl_resource* resource;
    struct wl_resource* surface_resource;
    struct wl_list link;
    
    // معرفات X11
    uint32_t window_id;
    uint32_t parent_id;
    
    // الخصائص
    struct x11_window_props props;
    
    // الحالة
    bool mapped;
    bool configured;
    bool has_alpha;
    
    // السطح المرتبط
    struct wl_surface* wl_surface;
    
    // معاودات الاتصال
    void (*on_configure)(struct xwayland_surface* surface, int x, int y, 
                         int width, int height);
    void (*on_map)(struct xwayland_surface* surface);
    void (*on_unmap)(struct xwayland_surface* surface);
    void (*on_destroy)(struct xwayland_surface* surface);
    void (*on_activate)(struct xwayland_surface* surface);
    void (*on_close)(struct xwayland_surface* surface);
};

// هيكل XWayland الرئيسي
struct xwayland_context {
    // الحالة
    xwayland_state_t state;
    int exit_code;
    char error_message[512];
    
    // العملية
    pid_t pid;
    int display_number;
    char display_name[32];
    
    // الأنابيب
    int wm_fd[2];      // نافذة المدير
    int x11_fd[2];     // X11 socket
    
    // موارد Wayland
    struct wl_display* wl_display;
    struct wl_event_source* sigchld_source;
    
    // النوافذ
    struct wl_list surfaces;
    struct wl_list unpaired_surfaces;
    int surface_count;
    
    // التركيز
    struct xwayland_surface* focused_surface;
    struct xwayland_surface* cursor_surface;
    
    // الإعدادات
    bool lazy_mode;           // بدء كسول
    bool terminate_delay;     // تأخير الإنهاء
    bool no_touch_pointer;    // تعطيل مؤشر اللمس
    int listen_fd;
    
    // الأحداث
    struct wl_listener display_destroy;
    struct wl_listener compositor_destroy;
    
    // معاودات الاتصال
    void (*on_ready)(struct xwayland_context* ctx);
    void (*on_surface_create)(struct xwayland_context* ctx, 
                               struct xwayland_surface* surface);
    void (*on_surface_destroy)(struct xwayland_context* ctx,
                                struct xwayland_surface* surface);
    void (*on_surface_activate)(struct xwayland_context* ctx,
                                 struct xwayland_surface* surface);
};

// دوال التهيئة والتشغيل
int xwayland_init(struct wl_display* display);
void xwayland_terminate(void);
struct xwayland_context* xwayland_get_context(void);

// التحكم في العملية
int xwayland_start(void);
int xwayland_stop(void);
int xwayland_restart(void);
bool xwayland_is_running(void);
xwayland_state_t xwayland_get_state(void);
const char* xwayland_get_state_string(void);
const char* xwayland_get_display_name(void);
int xwayland_get_display_number(void);

// إدارة النوافذ
struct xwayland_surface* xwayland_surface_create(uint32_t window_id);
void xwayland_surface_destroy(struct xwayland_surface* surface);
struct xwayland_surface* xwayland_surface_lookup(uint32_t window_id);
struct xwayland_surface* xwayland_surface_from_wl_surface(struct wl_surface* surface);
int xwayland_surface_configure(struct xwayland_surface* surface, int x, int y,
                                int width, int height);
int xwayland_surface_set_mapped(struct xwayland_surface* surface, bool mapped);
int xwayland_surface_set_activated(struct xwayland_surface* surface, bool activated);
int xwayland_surface_close(struct xwayland_surface* surface);
int xwayland_surface_set_fullscreen(struct xwayland_surface* surface, bool fullscreen);
int xwayland_surface_set_maximized(struct xwayland_surface* surface, bool maximized);
int xwayland_surface_set_minimized(struct xwayland_surface* surface, bool minimized);
int xwayland_surface_set_workspace(struct xwayland_surface* surface, uint32_t workspace);

// خصائص النوافذ
const struct x11_window_props* xwayland_surface_get_props(
    struct xwayland_surface* surface);
int xwayland_surface_set_title(struct xwayland_surface* surface, const char* title);
int xwayland_surface_set_class(struct xwayland_surface* surface, 
                                const char* class, const char* instance);

// التركيز والتنشيط
int xwayland_set_focused_surface(struct xwayland_surface* surface);
struct xwayland_surface* xwayland_get_focused_surface(void);
int xwayland_activate_surface(struct xwayland_surface* surface);

// إدارة المؤشر
int xwayland_set_cursor_surface(struct xwayland_surface* surface);
struct xwayland_surface* xwayland_get_cursor_surface(void);

// التكوين
void xwayland_set_lazy_mode(bool lazy);
void xwayland_set_terminate_delay(bool delay);
void xwayland_set_no_touch_pointer(bool no_touch);

// معاودات الاتصال
void xwayland_set_callbacks(
    void (*on_ready)(struct xwayland_context* ctx),
    void (*on_surface_create)(struct xwayland_context* ctx,
                               struct xwayland_surface* surface),
    void (*on_surface_destroy)(struct xwayland_context* ctx,
                                struct xwayland_surface* surface),
    void (*on_surface_activate)(struct xwayland_context* ctx,
                                 struct xwayland_surface* surface)
);

// التوافق مع التطبيقات
bool xwayland_is_x11_surface(struct wl_surface* surface);
bool xwayland_handle_client_message(struct xwayland_surface* surface,
                                     uint32_t type, const uint32_t* data);

// إحصائيات
int xwayland_get_surface_count(void);
void xwayland_get_stats(int* x11_windows, int* wayland_windows, 
                        int* total_windows);

// الأخطاء
const char* xwayland_get_last_error(void);
void xwayland_clear_error(void);

#ifdef __cplusplus
}
#endif

#endif // XWAYLAND_H
