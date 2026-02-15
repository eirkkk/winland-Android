#ifndef SEAT_MANAGER_H
#define SEAT_MANAGER_H

#include <wayland-server.h>

#ifdef __cplusplus
extern "C" {
#endif

struct winland_compositor;

// هيكل مقعد Winland
struct winland_seat {
    struct wl_resource *resource;
    struct winland_compositor *compositor;
    
    // الاسم
    char name[32];
    
    // القدرات
    uint32_t capabilities;
    
    // مؤشر
    struct wl_resource *pointer_resource;
    wl_fixed_t pointer_x;
    wl_fixed_t pointer_y;
    uint32_t pointer_buttons;
    uint32_t pointer_serial;
    
    // لوحة مفاتيح
    struct wl_resource *keyboard_resource;
    uint32_t keyboard_serial;
    struct wl_array keys;
    
    // لمس
    struct wl_resource *touch_resource;
    uint32_t touch_serial;
    
    // الارتباطات
    struct wl_list link;
};

// قدرات المقعد
#define SEAT_CAPABILITY_POINTER     (1 << 0)
#define SEAT_CAPABILITY_KEYBOARD    (1 << 1)
#define SEAT_CAPABILITY_TOUCH       (1 << 2)

// دوال مدير مقاعد المستخدم
int seat_manager_init(struct winland_compositor *compositor);
void seat_manager_terminate(struct winland_compositor *compositor);

// دوال المقعد
struct winland_seat* winland_seat_create(
    struct winland_compositor *compositor,
    const char *name
);
void winland_seat_destroy(struct winland_seat *seat);
void winland_seat_set_capabilities(struct winland_seat *seat, uint32_t capabilities);

// إشعارات المؤشر
void winland_seat_notify_pointer_enter(
    struct winland_seat *seat,
    struct wl_resource *surface_resource,
    wl_fixed_t x, wl_fixed_t y
);
void winland_seat_notify_pointer_leave(
    struct winland_seat *seat,
    struct wl_resource *surface_resource
);
void winland_seat_notify_pointer_motion(
    struct winland_seat *seat,
    wl_fixed_t x, wl_fixed_t y
);
void winland_seat_notify_pointer_button(
    struct winland_seat *seat,
    uint32_t button,
    uint32_t state
);
void winland_seat_notify_pointer_axis(
    struct winland_seat *seat,
    uint32_t axis,
    wl_fixed_t value
);

// إشعارات لوحة المفاتيح
void winland_seat_notify_keyboard_enter(
    struct winland_seat *seat,
    struct wl_resource *surface_resource
);
void winland_seat_notify_keyboard_leave(
    struct winland_seat *seat,
    struct wl_resource *surface_resource
);
void winland_seat_notify_keyboard_key(
    struct winland_seat *seat,
    uint32_t key,
    uint32_t state
);
void winland_seat_notify_keyboard_modifiers(
    struct winland_seat *seat,
    uint32_t mods_depressed,
    uint32_t mods_latched,
    uint32_t mods_locked,
    uint32_t group
);

// إشعارات اللمس
void winland_seat_notify_touch_down(
    struct winland_seat *seat,
    struct wl_resource *surface_resource,
    int32_t touch_id,
    wl_fixed_t x, wl_fixed_t y
);
void winland_seat_notify_touch_up(
    struct winland_seat *seat,
    int32_t touch_id
);
void winland_seat_notify_touch_motion(
    struct winland_seat *seat,
    int32_t touch_id,
    wl_fixed_t x, wl_fixed_t y
);
void winland_seat_notify_touch_frame(struct winland_seat *seat);

#ifdef __cplusplus
}
#endif

#endif // SEAT_MANAGER_H
