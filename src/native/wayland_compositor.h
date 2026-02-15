#ifndef WAYLAND_COMPOSITOR_H
#define WAYLAND_COMPOSITOR_H

#include <wayland-server.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct winland_surface;
struct winland_output;
struct winland_seat;

// هيكل المؤلف الرئيسي
struct winland_compositor {
    struct wl_display *display;
    struct wl_event_loop *event_loop;
    struct wl_list surfaces;
    struct wl_list outputs;
    struct wl_list seats;
    
    // EGL
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLConfig egl_config;
    
    // Global objects
    struct wl_global *compositor_global;
    struct wl_global *shell_global;
    struct wl_global *xdg_wm_base_global;
    struct wl_global *output_global;
    struct wl_global *seat_global;
    
    // الحالة
    int running;
    int width;
    int height;
    uint32_t serial;
};

// دوال المؤلف
struct winland_compositor* winland_compositor_create(struct wl_display *display);
void winland_compositor_destroy(struct winland_compositor *compositor);
int winland_compositor_init(struct winland_compositor *compositor);
void winland_compositor_run(struct winland_compositor *compositor);
void winland_compositor_stop(struct winland_compositor *compositor);

// إدارة الأسطح
struct winland_surface* winland_surface_create(
    struct winland_compositor *compositor,
    struct wl_client *client,
    uint32_t id
);
void winland_surface_destroy(struct winland_surface *surface);
void winland_surface_attach_buffer(
    struct winland_surface *surface,
    struct wl_resource *buffer_resource
);
void winland_surface_damage(
    struct winland_surface *surface,
    int32_t x, int32_t y,
    int32_t width, int32_t height
);
void winland_surface_commit(struct winland_surface *surface);

// إدارة المخرجات
struct winland_output* winland_output_create(
    struct winland_compositor *compositor,
    const char *name,
    int width, int height
);
void winland_output_destroy(struct winland_output *output);
void winland_output_set_mode(
    struct winland_output *output,
    int width, int height,
    int refresh_rate
);

// إدارة مقاعد المستخدم
struct winland_seat* winland_seat_create(
    struct winland_compositor *compositor,
    const char *name
);
void winland_seat_destroy(struct winland_seat *seat);
void winland_seat_set_capabilities(
    struct winland_seat *seat,
    uint32_t capabilities
);
void winland_seat_notify_pointer_motion(
    struct winland_seat *seat,
    wl_fixed_t x,
    wl_fixed_t y
);
void winland_seat_notify_pointer_button(
    struct winland_seat *seat,
    uint32_t button,
    uint32_t state
);
void winland_seat_notify_keyboard_key(
    struct winland_seat *seat,
    uint32_t key,
    uint32_t state
);

// دوال العرض
void winland_compositor_repaint(struct winland_compositor *compositor);
void winland_compositor_present_surface(
    struct winland_compositor *compositor,
    struct winland_surface *surface
);

#ifdef __cplusplus
}
#endif

#endif // WAYLAND_COMPOSITOR_H
