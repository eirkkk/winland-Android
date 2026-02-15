#ifndef OUTPUT_MANAGER_H
#define OUTPUT_MANAGER_H

#include <wayland-server.h>

#ifdef __cplusplus
extern "C" {
#endif

struct winland_compositor;

// هيكل مخرج Winland
struct winland_output {
    struct wl_resource *resource;
    struct winland_compositor *compositor;
    
    // معلومات المخرج
    char name[32];
    char make[32];
    char model[32];
    
    // الأبعاد والوضع
    int32_t width;
    int32_t height;
    int32_t refresh_rate;
    int32_t physical_width;  // mm
    int32_t physical_height; // mm
    int32_t subpixel;
    int32_t transform;
    int32_t scale;
    
    // الحالة
    int active;
    int enabled;
    
    // الارتباطات
    struct wl_list link;
};

// دوال مدير المخرجات
int output_manager_init(struct winland_compositor *compositor);
void output_manager_terminate(struct winland_compositor *compositor);

// دوال المخرج
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
void winland_output_set_physical_size(
    struct winland_output *output,
    int width_mm, int height_mm
);
void winland_output_set_scale(struct winland_output *output, int32_t scale);
void winland_output_set_transform(struct winland_output *output, int32_t transform);
void winland_output_send_info(struct winland_output *output);

#ifdef __cplusplus
}
#endif

#endif // OUTPUT_MANAGER_H
