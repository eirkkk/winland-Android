#ifndef XDG_SHELL_IMPL_H
#define XDG_SHELL_IMPL_H

#include <wayland-server.h>

#ifdef __cplusplus
extern "C" {
#endif

struct winland_compositor;

// Forward declarations
struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;
struct xdg_popup;

// Positioner
struct xdg_positioner {
    struct wl_resource *resource;
    
    // خصائص الموضع
    struct {
        int32_t width;
        int32_t height;
        struct wl_resource *anchor_rect;
        uint32_t anchor;
        uint32_t gravity;
        uint32_t constraint_adjustment;
        int32_t offset_x;
        int32_t offset_y;
    } attrs;
};

// دوال XDG Shell
int xdg_shell_init(struct winland_compositor *compositor);
void xdg_shell_terminate(struct winland_compositor *compositor);

// دوال xdg_wm_base
struct xdg_wm_base* xdg_wm_base_create(
    struct winland_compositor *compositor,
    struct wl_client *client,
    uint32_t version,
    uint32_t id
);
void xdg_wm_base_destroy(struct xdg_wm_base *wm_base);
void xdg_wm_base_ping(struct xdg_wm_base *wm_base, uint32_t serial);

// دوال xdg_surface
struct xdg_surface* xdg_surface_create(
    struct xdg_wm_base *wm_base,
    struct wl_resource *surface_resource,
    uint32_t id
);
void xdg_surface_destroy(struct xdg_surface *xdg_surface);
void xdg_surface_set_window_geometry(
    struct xdg_surface *xdg_surface,
    int32_t x, int32_t y,
    int32_t width, int32_t height
);
void xdg_surface_ack_configure(struct xdg_surface *xdg_surface, uint32_t serial);

// دوال xdg_toplevel
struct xdg_toplevel* xdg_toplevel_create(
    struct xdg_surface *xdg_surface,
    uint32_t id
);
void xdg_toplevel_destroy(struct xdg_toplevel *toplevel);
void xdg_toplevel_set_parent(
    struct xdg_toplevel *toplevel,
    struct xdg_toplevel *parent
);
void xdg_toplevel_set_title(struct xdg_toplevel *toplevel, const char *title);
void xdg_toplevel_set_app_id(struct xdg_toplevel *toplevel, const char *app_id);
void xdg_toplevel_show_window_menu(
    struct xdg_toplevel *toplevel,
    struct wl_resource *seat_resource,
    uint32_t serial,
    int32_t x, int32_t y
);
void xdg_toplevel_set_max_size(struct xdg_toplevel *toplevel, int32_t width, int32_t height);
void xdg_toplevel_set_min_size(struct xdg_toplevel *toplevel, int32_t width, int32_t height);
void xdg_toplevel_set_maximized(struct xdg_toplevel *toplevel);
void xdg_toplevel_unset_maximized(struct xdg_toplevel *toplevel);
void xdg_toplevel_set_fullscreen(
    struct xdg_toplevel *toplevel,
    struct wl_resource *output_resource
);
void xdg_toplevel_unset_fullscreen(struct xdg_toplevel *toplevel);
void xdg_toplevel_set_minimized(struct xdg_toplevel *toplevel);

// دوال xdg_popup
struct xdg_popup* xdg_popup_create(
    struct xdg_surface *xdg_surface,
    struct xdg_surface *parent,
    struct xdg_positioner *positioner,
    uint32_t id
);
void xdg_popup_destroy(struct xdg_popup *popup);
void xdg_popup_grab(
    struct xdg_popup *popup,
    struct wl_resource *seat_resource,
    uint32_t serial
);

// دوال xdg_positioner
struct xdg_positioner* xdg_positioner_create(
    struct xdg_wm_base *wm_base,
    uint32_t id
);
void xdg_positioner_destroy(struct xdg_positioner *positioner);
void xdg_positioner_set_size(struct xdg_positioner *positioner, int32_t width, int32_t height);
void xdg_positioner_set_anchor_rect(
    struct xdg_positioner *positioner,
    int32_t x, int32_t y,
    int32_t width, int32_t height
);
void xdg_positioner_set_anchor(struct xdg_positioner *positioner, uint32_t anchor);
void xdg_positioner_set_gravity(struct xdg_positioner *positioner, uint32_t gravity);
void xdg_positioner_set_constraint_adjustment(
    struct xdg_positioner *positioner,
    uint32_t constraint_adjustment
);
void xdg_positioner_set_offset(
    struct xdg_positioner *positioner,
    int32_t x, int32_t y
);

#ifdef __cplusplus
}
#endif

#endif // XDG_SHELL_IMPL_H
