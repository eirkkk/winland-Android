#ifndef SURFACE_MANAGER_H
#define SURFACE_MANAGER_H

#include <wayland-server.h>
#include <wayland-server-protocol.h>

#ifdef __cplusplus
extern "C" {
#endif

struct winland_compositor;

// هيكل سطح Winland
struct winland_surface {
    struct wl_resource *resource;
    struct wl_resource *buffer_resource;
    struct winland_compositor *compositor;
    
    // الموضع والحجم
    int32_t x, y;
    int32_t width, height;
    
    // المنطقة التالفة (damage)
    pixman_region32_t damage;
    
    // الحالة
    int mapped;
    int active;
    
    // الارتباطات
    struct wl_list link;
    struct wl_listener buffer_destroy_listener;
};

// دوال مدير الأسطح
int surface_manager_init(struct winland_compositor *compositor);
void surface_manager_terminate(struct winland_compositor *compositor);

// دوال السطح
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
void winland_surface_set_position(struct winland_surface *surface, int32_t x, int32_t y);
void winland_surface_get_size(struct winland_surface *surface, int32_t *width, int32_t *height);

// البحث عن الأسطح
struct winland_surface* surface_manager_find_surface_at(
    struct winland_compositor *compositor,
    int32_t x, int32_t y
);

#ifdef __cplusplus
}
#endif

#endif // SURFACE_MANAGER_H
