#include "surface_manager.h"
#include "wayland_compositor.h"
#include <stdlib.h>
#include <string.h>
#include <android/log.h>
#include <pixman.h>

#define LOG_TAG "SurfaceManager"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Callbacks لـ wl_surface_interface
static void surface_destroy(struct wl_client *client, struct wl_resource *resource) {
    struct winland_surface *surface = wl_resource_get_user_data(resource);
    if (surface) {
        winland_surface_destroy(surface);
    }
}

static void surface_attach(
    struct wl_client *client,
    struct wl_resource *resource,
    struct wl_resource *buffer_resource,
    int32_t x,
    int32_t y
) {
    struct winland_surface *surface = wl_resource_get_user_data(resource);
    if (surface) {
        winland_surface_attach_buffer(surface, buffer_resource);
    }
}

static void surface_damage(
    struct wl_client *client,
    struct wl_resource *resource,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height
) {
    struct winland_surface *surface = wl_resource_get_user_data(resource);
    if (surface) {
        winland_surface_damage(surface, x, y, width, height);
    }
}

static void surface_frame(
    struct wl_client *client,
    struct wl_resource *resource,
    uint32_t callback_id
) {
    struct wl_resource *callback = wl_resource_create(
        client,
        &wl_callback_interface,
        1,
        callback_id
    );
    
    if (callback) {
        // إرسال رد الاستدعاء فوراً (TODO: تحسين هذا لاحقاً)
        wl_callback_send_done(callback, 0);
        wl_resource_destroy(callback);
    }
}

static void surface_set_opaque_region(
    struct wl_client *client,
    struct wl_resource *resource,
    struct wl_resource *region_resource
) {
    // TODO: Implement opaque region
}

static void surface_set_input_region(
    struct wl_client *client,
    struct wl_resource *resource,
    struct wl_resource *region_resource
) {
    // TODO: Implement input region
}

static void surface_commit(struct wl_client *client, struct wl_resource *resource) {
    struct winland_surface *surface = wl_resource_get_user_data(resource);
    if (surface) {
        winland_surface_commit(surface);
    }
}

static void surface_set_buffer_transform(
    struct wl_client *client,
    struct wl_resource *resource,
    int32_t transform
) {
    // TODO: Implement buffer transform
}

static void surface_set_buffer_scale(
    struct wl_client *client,
    struct wl_resource *resource,
    int32_t scale
) {
    // TODO: Implement buffer scale
}

static void surface_damage_buffer(
    struct wl_client *client,
    struct wl_resource *resource,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height
) {
    // نفس damage العادي في معظم الحالات
    surface_damage(client, resource, x, y, width, height);
}

static const struct wl_surface_interface surface_interface = {
    .destroy = surface_destroy,
    .attach = surface_attach,
    .damage = surface_damage,
    .frame = surface_frame,
    .set_opaque_region = surface_set_opaque_region,
    .set_input_region = surface_set_input_region,
    .commit = surface_commit,
    .set_buffer_transform = surface_set_buffer_transform,
    .set_buffer_scale = surface_set_buffer_scale,
    .damage_buffer = surface_damage_buffer
};

// استدعاء عند تدمير المخزن المؤقت
static void buffer_destroy_notify(struct wl_listener *listener, void *data) {
    struct winland_surface *surface = wl_container_of(listener, surface, buffer_destroy_listener);
    surface->buffer_resource = NULL;
    LOGI("Buffer destroyed for surface %p", surface);
}

// تهيئة مدير الأسطح
int surface_manager_init(struct winland_compositor *compositor) {
    if (!compositor) return -1;
    
    LOGI("Surface manager initialized");
    return 0;
}

// إنهاء مدير الأسطح
void surface_manager_terminate(struct winland_compositor *compositor) {
    if (!compositor) return;
    
    LOGI("Surface manager terminated");
}

// إنشاء سطح جديد
struct winland_surface* winland_surface_create(
    struct winland_compositor *compositor,
    struct wl_client *client,
    uint32_t id
) {
    struct winland_surface *surface = calloc(1, sizeof(*surface));
    if (!surface) {
        LOGE("Failed to allocate surface");
        return NULL;
    }
    
    surface->compositor = compositor;
    surface->x = 0;
    surface->y = 0;
    surface->width = 0;
    surface->height = 0;
    surface->mapped = 0;
    surface->active = 1;
    
    pixman_region32_init(&surface->damage);
    
    // إنشاء مورد Wayland للسطح
    surface->resource = wl_resource_create(
        client,
        &wl_surface_interface,
        wl_resource_get_version(compositor->compositor_global),
        id
    );
    
    if (!surface->resource) {
        LOGE("Failed to create surface resource");
        free(surface);
        return NULL;
    }
    
    wl_resource_set_implementation(
        surface->resource,
        &surface_interface,
        surface,
        NULL
    );
    
    // إضافة إلى قائمة الأسطح
    wl_list_insert(&compositor->surfaces, &surface->link);
    
    LOGI("Surface created: %p", surface);
    return surface;
}

// تدمير سطح
void winland_surface_destroy(struct winland_surface *surface) {
    if (!surface) return;
    
    LOGI("Destroying surface: %p", surface);
    
    // إزالة من القائمة
    wl_list_remove(&surface->link);
    
    // تدمير المنطقة التالفة
    pixman_region32_fini(&surface->damage);
    
    // تدمير المورد
    if (surface->resource) {
        wl_resource_destroy(surface->resource);
    }
    
    free(surface);
}

// ربط مخزن مؤقت بالسطح
void winland_surface_attach_buffer(
    struct winland_surface *surface,
    struct wl_resource *buffer_resource
) {
    if (!surface) return;
    
    // إلغاء ربط المخزن المؤقت القديم
    if (surface->buffer_resource) {
        wl_list_remove(&surface->buffer_destroy_listener.link);
    }
    
    surface->buffer_resource = buffer_resource;
    
    if (buffer_resource) {
        // إعداد الاستماع لتدمير المخزن المؤقت
        surface->buffer_destroy_listener.notify = buffer_destroy_notify;
        wl_resource_add_destroy_listener(buffer_resource, &surface->buffer_destroy_listener);
        
        // الحصول على أبعاد المخزن المؤقت
        struct wl_shm_buffer *shm_buffer = wl_shm_buffer_get(buffer_resource);
        if (shm_buffer) {
            surface->width = wl_shm_buffer_get_width(shm_buffer);
            surface->height = wl_shm_buffer_get_height(shm_buffer);
        }
    }
    
    LOGI("Buffer attached to surface %p: %dx%d", surface, surface->width, surface->height);
}

// إضافة منطقة تالفة
void winland_surface_damage(
    struct winland_surface *surface,
    int32_t x, int32_t y,
    int32_t width, int32_t height
) {
    if (!surface) return;
    
    pixman_region32_union_rect(
        &surface->damage,
        &surface->damage,
        x, y,
        width, height
    );
}

// تطبيق التغييرات (commit)
void winland_surface_commit(struct winland_surface *surface) {
    if (!surface) return;
    
    // TODO: Implement proper commit logic
    // This should:
    // 1. Apply pending state
    // 2. Update surface content
    // 3. Trigger repaint
    
    surface->mapped = 1;
    
    LOGI("Surface committed: %p", surface);
}

// تعيين موضع السطح
void winland_surface_set_position(struct winland_surface *surface, int32_t x, int32_t y) {
    if (!surface) return;
    surface->x = x;
    surface->y = y;
}

// الحصول على حجم السطح
void winland_surface_get_size(struct winland_surface *surface, int32_t *width, int32_t *height) {
    if (!surface) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }
    if (width) *width = surface->width;
    if (height) *height = surface->height;
}

// البحث عن سطح عند موقع معين
struct winland_surface* surface_manager_find_surface_at(
    struct winland_compositor *compositor,
    int32_t x, int32_t y
) {
    if (!compositor) return NULL;
    
    struct winland_surface *surface;
    wl_list_for_each(surface, &compositor->surfaces, link) {
        if (surface->mapped && surface->active) {
            if (x >= surface->x && x < surface->x + surface->width &&
                y >= surface->y && y < surface->y + surface->height) {
                return surface;
            }
        }
    }
    
    return NULL;
}
