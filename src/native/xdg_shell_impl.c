#include "xdg_shell_impl.h"
#include "wayland_compositor.h"
#include "surface_manager.h"
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "XdgShell"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// تضمين بروتوكول xdg-shell
#include "xdg-shell-protocol.h"

// هيكل xdg_wm_base
struct xdg_wm_base {
    struct wl_resource *resource;
    struct winland_compositor *compositor;
    struct wl_client *client;
    uint32_t ping_serial;
};

// هيكل xdg_surface
struct xdg_surface {
    struct wl_resource *resource;
    struct xdg_wm_base *wm_base;
    struct winland_surface *surface;
    struct wl_resource *surface_resource;
    
    // النافذة المنبثقة أو الرئيسية
    union {
        struct xdg_toplevel *toplevel;
        struct xdg_popup *popup;
    } role;
    int has_role;
    
    // الهندسة
    int32_t x, y, width, height;
    uint32_t configure_serial;
};

// هيكل xdg_toplevel
struct xdg_toplevel {
    struct wl_resource *resource;
    struct xdg_surface *xdg_surface;
    
    // الخصائص
    char title[256];
    char app_id[256];
    struct xdg_toplevel *parent;
    
    // القيود
    int32_t max_width, max_height;
    int32_t min_width, min_height;
    
    // الحالة
    uint32_t state;
    int maximized;
    int fullscreen;
    int minimized;
    int activated;
};

// هيكل xdg_popup
struct xdg_popup {
    struct wl_resource *resource;
    struct xdg_surface *xdg_surface;
    struct xdg_surface *parent;
    struct xdg_positioner *positioner;
    
    // الموضع
    int32_t x, y;
    int grabbed;
};

// Callbacks لـ xdg_wm_base_interface
static void xdg_wm_base_destroy(struct wl_client *client, struct wl_resource *resource) {
    wl_resource_destroy(resource);
}

static void xdg_wm_base_create_positioner(
    struct wl_client *client,
    struct wl_resource *resource,
    uint32_t id
) {
    struct xdg_wm_base *wm_base = wl_resource_get_user_data(resource);
    xdg_positioner_create(wm_base, id);
}

static void xdg_wm_base_get_xdg_surface(
    struct wl_client *client,
    struct wl_resource *resource,
    uint32_t id,
    struct wl_resource *surface_resource
) {
    struct xdg_wm_base *wm_base = wl_resource_get_user_data(resource);
    xdg_surface_create(wm_base, surface_resource, id);
}

static void xdg_wm_base_pong(struct wl_client *client, struct wl_resource *resource, uint32_t serial) {
    LOGI("Pong received: serial %u", serial);
}

static const struct xdg_wm_base_interface xdg_wm_base_impl = {
    .destroy = xdg_wm_base_destroy,
    .create_positioner = xdg_wm_base_create_positioner,
    .get_xdg_surface = xdg_wm_base_get_xdg_surface,
    .pong = xdg_wm_base_pong
};

// Bind function للـ xdg_wm_base
static void bind_xdg_wm_base(
    struct wl_client *client,
    void *data,
    uint32_t version,
    uint32_t id
) {
    struct winland_compositor *compositor = data;
    
    struct xdg_wm_base *wm_base = malloc(sizeof(*wm_base));
    if (!wm_base) {
        wl_client_post_no_memory(client);
        return;
    }
    
    wm_base->compositor = compositor;
    wm_base->client = client;
    wm_base->ping_serial = 0;
    
    wm_base->resource = wl_resource_create(client, &xdg_wm_base_interface, version, id);
    if (!wm_base->resource) {
        free(wm_base);
        wl_client_post_no_memory(client);
        return;
    }
    
    wl_resource_set_implementation(wm_base->resource, &xdg_wm_base_impl, wm_base, NULL);
    
    LOGI("xdg_wm_base bound by client %p", client);
}

// Callbacks لـ xdg_surface_interface
static void xdg_surface_destroy(struct wl_client *client, struct wl_resource *resource) {
    wl_resource_destroy(resource);
}

static void xdg_surface_get_toplevel(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
    struct xdg_surface *xdg_surface = wl_resource_get_user_data(resource);
    xdg_toplevel_create(xdg_surface, id);
}

static void xdg_surface_get_popup(
    struct wl_client *client,
    struct wl_resource *resource,
    uint32_t id,
    struct wl_resource *parent_resource,
    struct wl_resource *positioner_resource
) {
    // TODO: Implement popup creation
    LOGI("Get popup requested");
}

static void xdg_surface_set_window_geometry(
    struct wl_client *client,
    struct wl_resource *resource,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height
) {
    struct xdg_surface *xdg_surface = wl_resource_get_user_data(resource);
    if (xdg_surface) {
        xdg_surface->x = x;
        xdg_surface->y = y;
        xdg_surface->width = width;
        xdg_surface->height = height;
    }
}

static void xdg_surface_ack_configure(struct wl_client *client, struct wl_resource *resource, uint32_t serial) {
    LOGI("Configure acknowledged: serial %u", serial);
}

static const struct xdg_surface_interface xdg_surface_impl = {
    .destroy = xdg_surface_destroy,
    .get_toplevel = xdg_surface_get_toplevel,
    .get_popup = xdg_surface_get_popup,
    .set_window_geometry = xdg_surface_set_window_geometry,
    .ack_configure = xdg_surface_ack_configure
};

// Callbacks لـ xdg_toplevel_interface
static void xdg_toplevel_destroy(struct wl_client *client, struct wl_resource *resource) {
    wl_resource_destroy(resource);
}

static void xdg_toplevel_set_parent(
    struct wl_client *client,
    struct wl_resource *resource,
    struct wl_resource *parent_resource
) {
    // TODO: Implement parent setting
}

static void xdg_toplevel_set_title(struct wl_client *client, struct wl_resource *resource, const char *title) {
    struct xdg_toplevel *toplevel = wl_resource_get_user_data(resource);
    if (toplevel) {
        strncpy(toplevel->title, title, sizeof(toplevel->title) - 1);
        LOGI("Window title: %s", title);
    }
}

static void xdg_toplevel_set_app_id(struct wl_client *client, struct wl_resource *resource, const char *app_id) {
    struct xdg_toplevel *toplevel = wl_resource_get_user_data(resource);
    if (toplevel) {
        strncpy(toplevel->app_id, app_id, sizeof(toplevel->app_id) - 1);
        LOGI("App ID: %s", app_id);
    }
}

static void xdg_toplevel_show_window_menu(
    struct wl_client *client,
    struct wl_resource *resource,
    struct wl_resource *seat_resource,
    uint32_t serial,
    int32_t x,
    int32_t y
) {
    // TODO: Implement window menu
}

static void xdg_toplevel_set_max_size(
    struct wl_client *client,
    struct wl_resource *resource,
    int32_t width,
    int32_t height
) {
    struct xdg_toplevel *toplevel = wl_resource_get_user_data(resource);
    if (toplevel) {
        toplevel->max_width = width;
        toplevel->max_height = height;
    }
}

static void xdg_toplevel_set_min_size(
    struct wl_client *client,
    struct wl_resource *resource,
    int32_t width,
    int32_t height
) {
    struct xdg_toplevel *toplevel = wl_resource_get_user_data(resource);
    if (toplevel) {
        toplevel->min_width = width;
        toplevel->min_height = height;
    }
}

static void xdg_toplevel_set_maximized(struct wl_client *client, struct wl_resource *resource) {
    struct xdg_toplevel *toplevel = wl_resource_get_user_data(resource);
    if (toplevel) {
        toplevel->maximized = 1;
        // TODO: Send configure with maximized state
    }
}

static void xdg_toplevel_unset_maximized(struct wl_client *client, struct wl_resource *resource) {
    struct xdg_toplevel *toplevel = wl_resource_get_user_data(resource);
    if (toplevel) {
        toplevel->maximized = 0;
        // TODO: Send configure with normal state
    }
}

static void xdg_toplevel_set_fullscreen(
    struct wl_client *client,
    struct wl_resource *resource,
    struct wl_resource *output_resource
) {
    struct xdg_toplevel *toplevel = wl_resource_get_user_data(resource);
    if (toplevel) {
        toplevel->fullscreen = 1;
        // TODO: Send configure with fullscreen state
    }
}

static void xdg_toplevel_unset_fullscreen(struct wl_client *client, struct wl_resource *resource) {
    struct xdg_toplevel *toplevel = wl_resource_get_user_data(resource);
    if (toplevel) {
        toplevel->fullscreen = 0;
        // TODO: Send configure with normal state
    }
}

static void xdg_toplevel_set_minimized(struct wl_client *client, struct wl_resource *resource) {
    struct xdg_toplevel *toplevel = wl_resource_get_user_data(resource);
    if (toplevel) {
        toplevel->minimized = 1;
    }
}

static const struct xdg_toplevel_interface xdg_toplevel_impl = {
    .destroy = xdg_toplevel_destroy,
    .set_parent = xdg_toplevel_set_parent,
    .set_title = xdg_toplevel_set_title,
    .set_app_id = xdg_toplevel_set_app_id,
    .show_window_menu = xdg_toplevel_show_window_menu,
    .set_max_size = xdg_toplevel_set_max_size,
    .set_min_size = xdg_toplevel_set_min_size,
    .set_maximized = xdg_toplevel_set_maximized,
    .unset_maximized = xdg_toplevel_unset_maximized,
    .set_fullscreen = xdg_toplevel_set_fullscreen,
    .unset_fullscreen = xdg_toplevel_unset_fullscreen,
    .set_minimized = xdg_toplevel_set_minimized
};

// دوال xdg_positioner
struct xdg_positioner* xdg_positioner_create(struct xdg_wm_base *wm_base, uint32_t id) {
    struct xdg_positioner *positioner = malloc(sizeof(*positioner));
    if (!positioner) return NULL;
    
    memset(&positioner->attrs, 0, sizeof(positioner->attrs));
    
    positioner->resource = wl_resource_create(
        wm_base->client,
        &xdg_positioner_interface,
        1,
        id
    );
    
    if (!positioner->resource) {
        free(positioner);
        return NULL;
    }
    
    wl_resource_set_implementation(positioner->resource, NULL, positioner, NULL);
    
    return positioner;
}

void xdg_positioner_destroy(struct xdg_positioner *positioner) {
    if (positioner) {
        free(positioner);
    }
}

// تهيئة XDG Shell
int xdg_shell_init(struct winland_compositor *compositor) {
    if (!compositor) return -1;
    
    // إنشاء global xdg_wm_base
    compositor->xdg_wm_base_global = wl_global_create(
        compositor->display,
        &xdg_wm_base_interface,
        2,  // version
        compositor,
        bind_xdg_wm_base
    );
    
    if (!compositor->xdg_wm_base_global) {
        LOGE("Failed to create xdg_wm_base global");
        return -1;
    }
    
    LOGI("XDG Shell initialized");
    return 0;
}

// إنهاء XDG Shell
void xdg_shell_terminate(struct winland_compositor *compositor) {
    if (!compositor) return;
    
    if (compositor->xdg_wm_base_global) {
        wl_global_destroy(compositor->xdg_wm_base_global);
        compositor->xdg_wm_base_global = NULL;
    }
    
    LOGI("XDG Shell terminated");
}

// إنشاء xdg_surface
struct xdg_surface* xdg_surface_create(
    struct xdg_wm_base *wm_base,
    struct wl_resource *surface_resource,
    uint32_t id
) {
    struct xdg_surface *xdg_surface = malloc(sizeof(*xdg_surface));
    if (!xdg_surface) {
        wl_client_post_no_memory(wm_base->client);
        return NULL;
    }
    
    memset(xdg_surface, 0, sizeof(*xdg_surface));
    
    xdg_surface->wm_base = wm_base;
    xdg_surface->surface_resource = surface_resource;
    xdg_surface->x = 0;
    xdg_surface->y = 0;
    xdg_surface->width = 0;
    xdg_surface->height = 0;
    xdg_surface->configure_serial = 0;
    xdg_surface->has_role = 0;
    
    xdg_surface->resource = wl_resource_create(
        wm_base->client,
        &xdg_surface_interface,
        wl_resource_get_version(wm_base->resource),
        id
    );
    
    if (!xdg_surface->resource) {
        free(xdg_surface);
        wl_client_post_no_memory(wm_base->client);
        return NULL;
    }
    
    wl_resource_set_implementation(xdg_surface->resource, &xdg_surface_impl, xdg_surface, NULL);
    
    LOGI("xdg_surface created");
    return xdg_surface;
}

// إنشاء xdg_toplevel
struct xdg_toplevel* xdg_toplevel_create(struct xdg_surface *xdg_surface, uint32_t id) {
    struct xdg_toplevel *toplevel = malloc(sizeof(*toplevel));
    if (!toplevel) {
        wl_client_post_no_memory(xdg_surface->wm_base->client);
        return NULL;
    }
    
    memset(toplevel, 0, sizeof(*toplevel));
    
    toplevel->xdg_surface = xdg_surface;
    toplevel->max_width = 0;
    toplevel->max_height = 0;
    toplevel->min_width = 0;
    toplevel->min_height = 0;
    toplevel->maximized = 0;
    toplevel->fullscreen = 0;
    toplevel->minimized = 0;
    toplevel->activated = 0;
    
    xdg_surface->role.toplevel = toplevel;
    xdg_surface->has_role = 1;
    
    toplevel->resource = wl_resource_create(
        xdg_surface->wm_base->client,
        &xdg_toplevel_interface,
        wl_resource_get_version(xdg_surface->resource),
        id
    );
    
    if (!toplevel->resource) {
        free(toplevel);
        wl_client_post_no_memory(xdg_surface->wm_base->client);
        return NULL;
    }
    
    wl_resource_set_implementation(toplevel->resource, &xdg_toplevel_impl, toplevel, NULL);
    
    // إرسال configure أولي
    uint32_t serial = wl_display_next_serial(xdg_surface->wm_base->compositor->display);
    xdg_toplevel_send_configure(toplevel->resource, 0, 0, NULL);
    xdg_surface_send_configure(xdg_surface->resource, serial);
    
    LOGI("xdg_toplevel created");
    return toplevel;
}
