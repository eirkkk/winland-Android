#include "output_manager.h"
#include "wayland_compositor.h"
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "OutputManager"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Bind function للـ wl_output
static void bind_output(
    struct wl_client *client,
    void *data,
    uint32_t version,
    uint32_t id
) {
    struct winland_output *output = data;
    
    struct wl_resource *resource = wl_resource_create(
        client,
        &wl_output_interface,
        version,
        id
    );
    
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    
    wl_resource_set_implementation(resource, NULL, output, NULL);
    
    // إرسال معلومات المخرج
    wl_output_send_geometry(
        resource,
        0, 0,  // x, y
        output->physical_width,
        output->physical_height,
        output->subpixel,
        output->make,
        output->model,
        output->transform
    );
    
    wl_output_send_mode(
        resource,
        WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED,
        output->width,
        output->height,
        output->refresh_rate
    );
    
    if (version >= WL_OUTPUT_SCALE_SINCE_VERSION) {
        wl_output_send_scale(resource, output->scale);
    }
    
    if (version >= WL_OUTPUT_DONE_SINCE_VERSION) {
        wl_output_send_done(resource);
    }
    
    LOGI("Output bound by client %p", client);
}

// تهيئة مدير المخرجات
int output_manager_init(struct winland_compositor *compositor) {
    if (!compositor) return -1;
    
    // إنشاء global output
    compositor->output_global = wl_global_create(
        compositor->display,
        &wl_output_interface,
        3,  // version
        NULL,  // data
        bind_output
    );
    
    if (!compositor->output_global) {
        LOGE("Failed to create output global");
        return -1;
    }
    
    LOGI("Output manager initialized");
    return 0;
}

// إنهاء مدير المخرجات
void output_manager_terminate(struct winland_compositor *compositor) {
    if (!compositor) return;
    
    if (compositor->output_global) {
        wl_global_destroy(compositor->output_global);
        compositor->output_global = NULL;
    }
    
    LOGI("Output manager terminated");
}

// إنشاء مخرج جديد
struct winland_output* winland_output_create(
    struct winland_compositor *compositor,
    const char *name,
    int width, int height
) {
    struct winland_output *output = calloc(1, sizeof(*output));
    if (!output) {
        LOGE("Failed to allocate output");
        return NULL;
    }
    
    output->compositor = compositor;
    strncpy(output->name, name, sizeof(output->name) - 1);
    strncpy(output->make, "Winland", sizeof(output->make) - 1);
    strncpy(output->model, "Android Display", sizeof(output->model) - 1);
    
    output->width = width;
    output->height = height;
    output->refresh_rate = 60000; // 60 Hz
    output->physical_width = width / 10;  // تقدير تقريبي
    output->physical_height = height / 10;
    output->subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;
    output->transform = WL_OUTPUT_TRANSFORM_NORMAL;
    output->scale = 1;
    output->active = 1;
    output->enabled = 1;
    
    // إضافة إلى قائمة المخرجات
    wl_list_insert(&compositor->outputs, &output->link);
    
    LOGI("Output created: %s (%dx%d@%dHz)", 
         name, width, height, output->refresh_rate / 1000);
    
    return output;
}

// تدمير مخرج
void winland_output_destroy(struct winland_output *output) {
    if (!output) return;
    
    LOGI("Destroying output: %s", output->name);
    
    // إزالة من القائمة
    wl_list_remove(&output->link);
    
    free(output);
}

// تعيين وضع المخرج
void winland_output_set_mode(
    struct winland_output *output,
    int width, int height,
    int refresh_rate
) {
    if (!output) return;
    
    output->width = width;
    output->height = height;
    output->refresh_rate = refresh_rate;
    
    LOGI("Output mode set: %dx%d@%dHz", width, height, refresh_rate / 1000);
}

// تعيين الحجم الفيزيائي
void winland_output_set_physical_size(
    struct winland_output *output,
    int width_mm, int height_mm
) {
    if (!output) return;
    
    output->physical_width = width_mm;
    output->physical_height = height_mm;
}

// تعيين المقياس
void winland_output_set_scale(struct winland_output *output, int32_t scale) {
    if (!output) return;
    
    output->scale = scale;
    LOGI("Output scale set: %d", scale);
}

// تعيين التحويل
void winland_output_set_transform(struct winland_output *output, int32_t transform) {
    if (!output) return;
    
    output->transform = transform;
    LOGI("Output transform set: %d", transform);
}

// إرسال معلومات المخرج
void winland_output_send_info(struct winland_output *output) {
    if (!output || !output->resource) return;
    
    // TODO: Send updated output info to clients
}
