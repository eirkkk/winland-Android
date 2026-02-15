#include "wayland_compositor.h"
#include "surface_manager.h"
#include "output_manager.h"
#include "seat_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <android/log.h>

#define LOG_TAG "WaylandCompositor"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Callbacks لـ wl_compositor_interface
static void compositor_create_surface(
    struct wl_client *client,
    struct wl_resource *resource,
    uint32_t id
) {
    struct winland_compositor *compositor = wl_resource_get_user_data(resource);
    
    LOGI("Creating surface for client %p, id %u", client, id);
    
    struct winland_surface *surface = winland_surface_create(compositor, client, id);
    if (!surface) {
        wl_client_post_no_memory(client);
        return;
    }
}

static void compositor_create_region(
    struct wl_client *client,
    struct wl_resource *resource,
    uint32_t id
) {
    // TODO: Implement region creation
    LOGI("Creating region (not fully implemented)");
}

static const struct wl_compositor_interface compositor_interface = {
    .create_surface = compositor_create_surface,
    .create_region = compositor_create_region
};

// Bind function للـ wl_compositor
static void bind_compositor(
    struct wl_client *client,
    void *data,
    uint32_t version,
    uint32_t id
) {
    struct winland_compositor *compositor = data;
    
    struct wl_resource *resource = wl_resource_create(
        client,
        &wl_compositor_interface,
        version,
        id
    );
    
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    
    wl_resource_set_implementation(
        resource,
        &compositor_interface,
        compositor,
        NULL
    );
    
    LOGI("Compositor bound by client %p", client);
}

// إنشاء المؤلف
struct winland_compositor* winland_compositor_create(struct wl_display *display) {
    struct winland_compositor *compositor = calloc(1, sizeof(*compositor));
    if (!compositor) {
        LOGE("Failed to allocate compositor");
        return NULL;
    }
    
    compositor->display = display;
    compositor->event_loop = wl_display_get_event_loop(display);
    compositor->width = 1920;
    compositor->height = 1080;
    compositor->serial = 0;
    compositor->running = 0;
    
    wl_list_init(&compositor->surfaces);
    wl_list_init(&compositor->outputs);
    wl_list_init(&compositor->seats);
    
    LOGI("Compositor created");
    return compositor;
}

// تهيئة المؤلف
int winland_compositor_init(struct winland_compositor *compositor) {
    if (!compositor) return -1;
    
    // إنشاء global compositor
    compositor->compositor_global = wl_global_create(
        compositor->display,
        &wl_compositor_interface,
        4,  // version
        compositor,
        bind_compositor
    );
    
    if (!compositor->compositor_global) {
        LOGE("Failed to create compositor global");
        return -1;
    }
    
    // تهيئة مدير الأسطح
    if (surface_manager_init(compositor) != 0) {
        LOGE("Failed to initialize surface manager");
        return -1;
    }
    
    // تهيئة مدير المخرجات
    if (output_manager_init(compositor) != 0) {
        LOGE("Failed to initialize output manager");
        return -1;
    }
    
    // تهيئة مدير مقاعد المستخدم
    if (seat_manager_init(compositor) != 0) {
        LOGE("Failed to initialize seat manager");
        return -1;
    }
    
    // إنشاء مخرج افتراضي
    struct winland_output *output = winland_output_create(
        compositor,
        "WL-1",
        compositor->width,
        compositor->height
    );
    
    if (!output) {
        LOGE("Failed to create default output");
        return -1;
    }
    
    // إنشاء مقعد افتراضي
    struct winland_seat *seat = winland_seat_create(compositor, "seat0");
    if (!seat) {
        LOGE("Failed to create default seat");
        return -1;
    }
    
    compositor->running = 1;
    LOGI("Compositor initialized successfully");
    return 0;
}

// تشغيل حلقة المؤلف
void winland_compositor_run(struct winland_compositor *compositor) {
    if (!compositor || !compositor->running) return;
    
    LOGI("Starting compositor main loop");
    
    while (compositor->running) {
        wl_event_loop_dispatch(compositor->event_loop, -1);
        wl_display_flush_clients(compositor->display);
    }
}

// إيقاف المؤلف
void winland_compositor_stop(struct winland_compositor *compositor) {
    if (!compositor) return;
    
    LOGI("Stopping compositor");
    compositor->running = 0;
}

// تدمير المؤلف
void winland_compositor_destroy(struct winland_compositor *compositor) {
    if (!compositor) return;
    
    LOGI("Destroying compositor");
    
    compositor->running = 0;
    
    // تدمير جميع الأسطح
    struct winland_surface *surface, *tmp_surface;
    wl_list_for_each_safe(surface, tmp_surface, &compositor->surfaces, link) {
        winland_surface_destroy(surface);
    }
    
    // تدمير جميع المخرجات
    struct winland_output *output, *tmp_output;
    wl_list_for_each_safe(output, tmp_output, &compositor->outputs, link) {
        winland_output_destroy(output);
    }
    
    // تدمير جميع المقاعد
    struct winland_seat *seat, *tmp_seat;
    wl_list_for_each_safe(seat, tmp_seat, &compositor->seats, link) {
        winland_seat_destroy(seat);
    }
    
    // تدمير global objects
    if (compositor->compositor_global) {
        wl_global_destroy(compositor->compositor_global);
    }
    
    free(compositor);
    LOGI("Compositor destroyed");
}

// إعادة رسم المؤلف
void winland_compositor_repaint(struct winland_compositor *compositor) {
    if (!compositor) return;
    
    // TODO: Implement proper rendering
    // This should iterate through all surfaces and render them
}

// عرض سطح
void winland_compositor_present_surface(
    struct winland_compositor *compositor,
    struct winland_surface *surface
) {
    if (!compositor || !surface) return;
    
    // TODO: Implement surface presentation
    // This should present the surface's buffer to the screen
}
