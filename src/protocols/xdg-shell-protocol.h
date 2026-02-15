#ifndef XDG_SHELL_PROTOCOL_H
#define XDG_SHELL_PROTOCOL_H

#include <wayland-server.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @page page_xdg_shell_xdg_wm_base The xdg_wm_base protocol
 * @section page_xdg_shell_xdg_wm_base_desc Description
 *
 * The xdg_wm_base interface is exposed as a global object enabling clients
 * to turn their wl_surfaces into windows in a desktop environment.
 */

struct xdg_wm_base_interface {
    /**
     * destroy - destroy xdg_wm_base
     */
    void (*destroy)(struct wl_client *client, struct wl_resource *resource);
    
    /**
     * create_positioner - create a positioner object
     * @id: the new positioner
     */
    void (*create_positioner)(struct wl_client *client, struct wl_resource *resource, uint32_t id);
    
    /**
     * get_xdg_surface - create a shell surface from a surface
     * @id: the new xdg_surface object
     * @surface: the underlying wl_surface
     */
    void (*get_xdg_surface)(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface);
    
    /**
     * pong - respond to a ping event
     * @serial: serial of the ping event
     */
    void (*pong)(struct wl_client *client, struct wl_resource *resource, uint32_t serial);
};

#define XDG_WM_BASE_PING 0

static inline void xdg_wm_base_send_ping(struct wl_resource *resource_, uint32_t serial) {
    wl_resource_post_event(resource_, XDG_WM_BASE_PING, serial);
}

extern const struct wl_interface xdg_wm_base_interface;

/**
 * @page page_xdg_shell_xdg_positioner The xdg_positioner protocol
 * @section page_xdg_shell_xdg_positioner_desc Description
 *
 * The xdg_positioner provides a collection of rules for the placement of a
 * child surface relative to a parent surface.
 */

struct xdg_positioner_interface {
    /**
     * destroy - destroy the xdg_positioner object
     */
    void (*destroy)(struct wl_client *client, struct wl_resource *resource);
    
    /**
     * set_size - set the size of the to-be positioned rectangle
     */
    void (*set_size)(struct wl_client *client, struct wl_resource *resource, int32_t width, int32_t height);
    
    /**
     * set_anchor_rect - set the anchor rectangle within the parent surface
     */
    void (*set_anchor_rect)(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height);
    
    /**
     * set_anchor - set anchor rectangle anchor edges
     */
    void (*set_anchor)(struct wl_client *client, struct wl_resource *resource, uint32_t anchor);
    
    /**
     * set_gravity - set child surface gravity
     */
    void (*set_gravity)(struct wl_client *client, struct wl_resource *resource, uint32_t gravity);
    
    /**
     * set_constraint_adjustment - set the adjustment to be done when constrained
     */
    void (*set_constraint_adjustment)(struct wl_client *client, struct wl_resource *resource, uint32_t constraint_adjustment);
    
    /**
     * set_offset - set surface position offset
     */
    void (*set_offset)(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y);
};

extern const struct wl_interface xdg_positioner_interface;

/**
 * @page page_xdg_shell_xdg_surface The xdg_surface protocol
 * @section page_xdg_shell_xdg_surface_desc Description
 *
 * An interface that may be implemented by a wl_surface, for
 * implementations that provide a desktop-style user interface.
 */

struct xdg_surface_interface {
    /**
     * destroy - destroy xdg_surface
     */
    void (*destroy)(struct wl_client *client, struct wl_resource *resource);
    
    /**
     * get_toplevel - assign the xdg_toplevel role and set the parent
     */
    void (*get_toplevel)(struct wl_client *client, struct wl_resource *resource, uint32_t id);
    
    /**
     * get_popup - assign the xdg_popup role and set the parent
     */
    void (*get_popup)(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *parent, struct wl_resource *positioner);
    
    /**
     * set_window_geometry - set the new window geometry
     */
    void (*set_window_geometry)(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height);
    
    /**
     * ack_configure - ack a configure event
     */
    void (*ack_configure)(struct wl_client *client, struct wl_resource *resource, uint32_t serial);
};

#define XDG_SURFACE_CONFIGURE 0

static inline void xdg_surface_send_configure(struct wl_resource *resource_, uint32_t serial) {
    wl_resource_post_event(resource_, XDG_SURFACE_CONFIGURE, serial);
}

extern const struct wl_interface xdg_surface_interface;

/**
 * @page page_xdg_shell_xdg_toplevel The xdg_toplevel protocol
 * @section page_xdg_shell_xdg_toplevel_desc Description
 *
 * This interface defines an xdg_surface role which allows a surface to,
 * among other things, set window-like properties such as maximize,
 * fullscreen, and minimize, set application-specific metadata like title and
 * id, and well as present transient windows.
 */

struct xdg_toplevel_interface {
    void (*destroy)(struct wl_client *client, struct wl_resource *resource);
    void (*set_parent)(struct wl_client *client, struct wl_resource *resource, struct wl_resource *parent);
    void (*set_title)(struct wl_client *client, struct wl_resource *resource, const char *title);
    void (*set_app_id)(struct wl_client *client, struct wl_resource *resource, const char *app_id);
    void (*show_window_menu)(struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial, int32_t x, int32_t y);
    void (*set_max_size)(struct wl_client *client, struct wl_resource *resource, int32_t width, int32_t height);
    void (*set_min_size)(struct wl_client *client, struct wl_resource *resource, int32_t width, int32_t height);
    void (*set_maximized)(struct wl_client *client, struct wl_resource *resource);
    void (*unset_maximized)(struct wl_client *client, struct wl_resource *resource);
    void (*set_fullscreen)(struct wl_client *client, struct wl_resource *resource, struct wl_resource *output);
    void (*unset_fullscreen)(struct wl_client *client, struct wl_resource *resource);
    void (*set_minimized)(struct wl_client *client, struct wl_resource *resource);
};

#define XDG_TOPLEVEL_CONFIGURE 0
#define XDG_TOPLEVEL_CLOSE 1
#define XDG_TOPLEVEL_CONFIGURE_BOUNDS 2
#define XDG_TOPLEVEL_WM_CAPABILITIES 3

static inline void xdg_toplevel_send_configure(struct wl_resource *resource_, int32_t width, int32_t height, struct wl_array *states) {
    wl_resource_post_event(resource_, XDG_TOPLEVEL_CONFIGURE, width, height, states);
}

static inline void xdg_toplevel_send_close(struct wl_resource *resource_) {
    wl_resource_post_event(resource_, XDG_TOPLEVEL_CLOSE);
}

extern const struct wl_interface xdg_toplevel_interface;

/**
 * @page page_xdg_shell_xdg_popup The xdg_popup protocol
 * @section page_xdg_shell_xdg_popup_desc Description
 *
 * A popup surface is a short-lived, temporary surface. It can be used to
 * implement for example menus, popovers, tooltips and other similar user
 * interface concepts.
 */

struct xdg_popup_interface {
    void (*destroy)(struct wl_client *client, struct wl_resource *resource);
    void (*grab)(struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial);
    void (*reposition)(struct wl_client *client, struct wl_resource *resource, struct wl_resource *positioner, uint32_t token);
};

#define XDG_POPUP_CONFIGURE 0
#define XDG_POPUP_POPUP_DONE 1
#define XDG_POPUP_REPOSITIONED 2

static inline void xdg_popup_send_configure(struct wl_resource *resource_, int32_t x, int32_t y, int32_t width, int32_t height) {
    wl_resource_post_event(resource_, XDG_POPUP_CONFIGURE, x, y, width, height);
}

static inline void xdg_popup_send_popup_done(struct wl_resource *resource_) {
    wl_resource_post_event(resource_, XDG_POPUP_POPUP_DONE);
}

extern const struct wl_interface xdg_popup_interface;

// Constants
#define XDG_POSITIONER_ANCHOR_NONE 0
#define XDG_POSITIONER_ANCHOR_TOP 1
#define XDG_POSITIONER_ANCHOR_BOTTOM 2
#define XDG_POSITIONER_ANCHOR_LEFT 4
#define XDG_POSITIONER_ANCHOR_RIGHT 8

#define XDG_POSITIONER_GRAVITY_NONE 0
#define XDG_POSITIONER_GRAVITY_TOP 1
#define XDG_POSITIONER_GRAVITY_BOTTOM 2
#define XDG_POSITIONER_GRAVITY_LEFT 4
#define XDG_POSITIONER_GRAVITY_RIGHT 8

#define XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE 0
#define XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X 1
#define XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y 2
#define XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X 4
#define XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y 8
#define XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X 16
#define XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y 32

#define XDG_TOPLEVEL_STATE_MAXIMIZED 1
#define XDG_TOPLEVEL_STATE_FULLSCREEN 2
#define XDG_TOPLEVEL_STATE_RESIZING 3
#define XDG_TOPLEVEL_STATE_ACTIVATED 4
#define XDG_TOPLEVEL_STATE_TILED_LEFT 5
#define XDG_TOPLEVEL_STATE_TILED_RIGHT 6
#define XDG_TOPLEVEL_STATE_TILED_TOP 7
#define XDG_TOPLEVEL_STATE_TILED_BOTTOM 8
#define XDG_TOPLEVEL_STATE_SUSPENDED 9

#ifdef __cplusplus
}
#endif

#endif // XDG_SHELL_PROTOCOL_H
