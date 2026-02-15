#include "seat_manager.h"
#include "wayland_compositor.h"
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "SeatManager"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Bind function للـ wl_seat
static void bind_seat(
    struct wl_client *client,
    void *data,
    uint32_t version,
    uint32_t id
) {
    struct winland_seat *seat = data;
    
    struct wl_resource *resource = wl_resource_create(
        client,
        &wl_seat_interface,
        version,
        id
    );
    
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    
    wl_resource_set_implementation(resource, NULL, seat, NULL);
    
    // إرسال الاسم والقدرات
    wl_seat_send_name(resource, seat->name);
    wl_seat_send_capabilities(resource, seat->capabilities);
    
    LOGI("Seat bound by client %p", client);
}

// Callbacks لـ wl_pointer_interface
static void pointer_set_cursor(
    struct wl_client *client,
    struct wl_resource *resource,
    uint32_t serial,
    struct wl_resource *surface_resource,
    int32_t hotspot_x,
    int32_t hotspot_y
) {
    // TODO: Implement cursor setting
    LOGI("Set cursor requested");
}

static void pointer_release(struct wl_client *client, struct wl_resource *resource) {
    wl_resource_destroy(resource);
}

static const struct wl_pointer_interface pointer_interface = {
    .set_cursor = pointer_set_cursor,
    .release = pointer_release
};

// Callbacks لـ wl_keyboard_interface
static void keyboard_release(struct wl_client *client, struct wl_resource *resource) {
    wl_resource_destroy(resource);
}

static const struct wl_keyboard_interface keyboard_interface = {
    .release = keyboard_release
};

// Callbacks لـ wl_touch_interface
static void touch_release(struct wl_client *client, struct wl_resource *resource) {
    wl_resource_destroy(resource);
}

static const struct wl_touch_interface touch_interface = {
    .release = touch_release
};

// تهيئة مدير مقاعد المستخدم
int seat_manager_init(struct winland_compositor *compositor) {
    if (!compositor) return -1;
    
    // إنشاء global seat
    compositor->seat_global = wl_global_create(
        compositor->display,
        &wl_seat_interface,
        5,  // version
        NULL,  // data
        bind_seat
    );
    
    if (!compositor->seat_global) {
        LOGE("Failed to create seat global");
        return -1;
    }
    
    LOGI("Seat manager initialized");
    return 0;
}

// إنهاء مدير مقاعد المستخدم
void seat_manager_terminate(struct winland_compositor *compositor) {
    if (!compositor) return;
    
    if (compositor->seat_global) {
        wl_global_destroy(compositor->seat_global);
        compositor->seat_global = NULL;
    }
    
    LOGI("Seat manager terminated");
}

// إنشاء مقعد جديد
struct winland_seat* winland_seat_create(
    struct winland_compositor *compositor,
    const char *name
) {
    struct winland_seat *seat = calloc(1, sizeof(*seat));
    if (!seat) {
        LOGE("Failed to allocate seat");
        return NULL;
    }
    
    seat->compositor = compositor;
    strncpy(seat->name, name, sizeof(seat->name) - 1);
    
    // تعيين القدرات الافتراضية (كل شيء)
    seat->capabilities = SEAT_CAPABILITY_POINTER | 
                         SEAT_CAPABILITY_KEYBOARD | 
                         SEAT_CAPABILITY_TOUCH;
    
    seat->pointer_x = wl_fixed_from_int(0);
    seat->pointer_y = wl_fixed_from_int(0);
    seat->pointer_buttons = 0;
    seat->pointer_serial = 0;
    seat->keyboard_serial = 0;
    seat->touch_serial = 0;
    
    wl_array_init(&seat->keys);
    
    // إضافة إلى قائمة المقاعد
    wl_list_insert(&compositor->seats, &seat->link);
    
    LOGI("Seat created: %s (capabilities: 0x%x)", name, seat->capabilities);
    
    return seat;
}

// تدمير مقعد
void winland_seat_destroy(struct winland_seat *seat) {
    if (!seat) return;
    
    LOGI("Destroying seat: %s", seat->name);
    
    // إزالة من القائمة
    wl_list_remove(&seat->link);
    
    // تدمير الموارد
    if (seat->pointer_resource) {
        wl_resource_destroy(seat->pointer_resource);
    }
    if (seat->keyboard_resource) {
        wl_resource_destroy(seat->keyboard_resource);
    }
    if (seat->touch_resource) {
        wl_resource_destroy(seat->touch_resource);
    }
    
    wl_array_release(&seat->keys);
    
    free(seat);
}

// تعيين القدرات
void winland_seat_set_capabilities(struct winland_seat *seat, uint32_t capabilities) {
    if (!seat) return;
    
    seat->capabilities = capabilities;
    
    // TODO: Notify clients of capability change
    
    LOGI("Seat capabilities set: 0x%x", capabilities);
}

// إشعار دخول المؤشر
void winland_seat_notify_pointer_enter(
    struct winland_seat *seat,
    struct wl_resource *surface_resource,
    wl_fixed_t x, wl_fixed_t y
) {
    if (!seat || !seat->pointer_resource) return;
    
    seat->pointer_serial++;
    seat->pointer_x = x;
    seat->pointer_y = y;
    
    wl_pointer_send_enter(
        seat->pointer_resource,
        seat->pointer_serial,
        surface_resource,
        x, y
    );
}

// إشعار خروج المؤشر
void winland_seat_notify_pointer_leave(
    struct winland_seat *seat,
    struct wl_resource *surface_resource
) {
    if (!seat || !seat->pointer_resource) return;
    
    seat->pointer_serial++;
    
    wl_pointer_send_leave(
        seat->pointer_resource,
        seat->pointer_serial,
        surface_resource
    );
}

// إشعار حركة المؤشر
void winland_seat_notify_pointer_motion(
    struct winland_seat *seat,
    wl_fixed_t x, wl_fixed_t y
) {
    if (!seat) return;
    
    seat->pointer_x = x;
    seat->pointer_y = y;
    
    if (seat->pointer_resource) {
        uint32_t time = 0; // TODO: Get actual time
        wl_pointer_send_motion(seat->pointer_resource, time, x, y);
    }
}

// إشعار زر المؤشر
void winland_seat_notify_pointer_button(
    struct winland_seat *seat,
    uint32_t button,
    uint32_t state
) {
    if (!seat || !seat->pointer_resource) return;
    
    seat->pointer_serial++;
    
    if (state) {
        seat->pointer_buttons |= (1 << button);
    } else {
        seat->pointer_buttons &= ~(1 << button);
    }
    
    uint32_t time = 0; // TODO: Get actual time
    wl_pointer_send_button(
        seat->pointer_resource,
        seat->pointer_serial,
        time,
        button,
        state
    );
}

// إشعار محور المؤشر
void winland_seat_notify_pointer_axis(
    struct winland_seat *seat,
    uint32_t axis,
    wl_fixed_t value
) {
    if (!seat || !seat->pointer_resource) return;
    
    uint32_t time = 0; // TODO: Get actual time
    wl_pointer_send_axis(seat->pointer_resource, time, axis, value);
}

// إشعار دخول لوحة المفاتيح
void winland_seat_notify_keyboard_enter(
    struct winland_seat *seat,
    struct wl_resource *surface_resource
) {
    if (!seat || !seat->keyboard_resource) return;
    
    seat->keyboard_serial++;
    
    struct wl_array keys;
    wl_array_init(&keys);
    
    wl_keyboard_send_enter(
        seat->keyboard_resource,
        seat->keyboard_serial,
        surface_resource,
        &keys
    );
    
    wl_array_release(&keys);
}

// إشعار خروج لوحة المفاتيح
void winland_seat_notify_keyboard_leave(
    struct winland_seat *seat,
    struct wl_resource *surface_resource
) {
    if (!seat || !seat->keyboard_resource) return;
    
    seat->keyboard_serial++;
    
    wl_keyboard_send_leave(
        seat->keyboard_resource,
        seat->keyboard_serial,
        surface_resource
    );
}

// إشعار مفتاح لوحة المفاتيح
void winland_seat_notify_keyboard_key(
    struct winland_seat *seat,
    uint32_t key,
    uint32_t state
) {
    if (!seat || !seat->keyboard_resource) return;
    
    seat->keyboard_serial++;
    
    uint32_t time = 0; // TODO: Get actual time
    wl_keyboard_send_key(
        seat->keyboard_resource,
        seat->keyboard_serial,
        time,
        key,
        state
    );
}

// إشعار معدّلات لوحة المفاتيح
void winland_seat_notify_keyboard_modifiers(
    struct winland_seat *seat,
    uint32_t mods_depressed,
    uint32_t mods_latched,
    uint32_t mods_locked,
    uint32_t group
) {
    if (!seat || !seat->keyboard_resource) return;
    
    seat->keyboard_serial++;
    
    wl_keyboard_send_modifiers(
        seat->keyboard_resource,
        seat->keyboard_serial,
        mods_depressed,
        mods_latched,
        mods_locked,
        group
    );
}

// إشعار لمس down
void winland_seat_notify_touch_down(
    struct winland_seat *seat,
    struct wl_resource *surface_resource,
    int32_t touch_id,
    wl_fixed_t x, wl_fixed_t y
) {
    if (!seat || !seat->touch_resource) return;
    
    seat->touch_serial++;
    
    uint32_t time = 0; // TODO: Get actual time
    wl_touch_send_down(
        seat->touch_resource,
        seat->touch_serial,
        time,
        surface_resource,
        touch_id,
        x, y
    );
}

// إشعار لمس up
void winland_seat_notify_touch_up(
    struct winland_seat *seat,
    int32_t touch_id
) {
    if (!seat || !seat->touch_resource) return;
    
    seat->touch_serial++;
    
    uint32_t time = 0; // TODO: Get actual time
    wl_touch_send_up(
        seat->touch_resource,
        seat->touch_serial,
        time,
        touch_id
    );
}

// إشعار حركة لمس
void winland_seat_notify_touch_motion(
    struct winland_seat *seat,
    int32_t touch_id,
    wl_fixed_t x, wl_fixed_t y
) {
    if (!seat || !seat->touch_resource) return;
    
    uint32_t time = 0; // TODO: Get actual time
    wl_touch_send_motion(seat->touch_resource, time, touch_id, x, y);
}

// إشعار إطار لمس
void winland_seat_notify_touch_frame(struct winland_seat *seat) {
    if (!seat || !seat->touch_resource) return;
    
    wl_touch_send_frame(seat->touch_resource);
}
