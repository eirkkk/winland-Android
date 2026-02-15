#ifndef TILING_LAYOUT_H
#define TILING_LAYOUT_H

#include <wayland-server.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// أنواع التخطيط
typedef enum {
    LAYOUT_FLOATING,        // نوافذ حرة
    LAYOUT_TILING,          // نوافذ متجاورة
    LAYOUT_MONOCLE,         // نافذة واحدة ملء الشاشة
    LAYOUT_GRID,            // شبكة
    LAYOUT_STACKED,         // مكدسة
    LAYOUT_TABBED,          // تبويبات
    LAYOUT_SPIRAL,          // حلزوني
    LAYOUT_DWINDLE          // متناقص
} layout_type_t;

// أنواع التقسيم
typedef enum {
    SPLIT_NONE,
    SPLIT_VERTICAL,
    SPLIT_HORIZONTAL
} split_type_t;

// هيكل حاوية التخطيط
struct tiling_container {
    struct wl_list link;
    struct wl_list children;
    struct tiling_container* parent;
    
    // النوع
    layout_type_t layout;
    split_type_t split;
    
    // السطح المرتبط (إذا كان نافذة)
    struct wl_resource* surface;
    char title[256];
    char app_id[256];
    
    // الموضع والحجم
    int32_t x, y;
    int32_t width, height;
    
    // الحدود
    int32_t border_width;
    uint32_t border_color;
    
    // الحالة
    bool focused;
    bool fullscreen;
    bool floating;
    bool urgent;
    bool mapped;
    
    // نسبة التقسيم (0.1 - 0.9)
    float split_ratio;
};

// هيكل مدير التخطيط
struct tiling_manager {
    struct wl_list workspaces;
    struct wl_list floating;
    
    // مساحة العمل الحالية
    int current_workspace;
    int num_workspaces;
    
    // الإعدادات
    layout_type_t default_layout;
    int32_t gap_size;
    int32_t border_width;
    uint32_t border_color_focused;
    uint32_t border_color_unfocused;
    uint32_t border_color_urgent;
    
    // الحالة
    struct tiling_container* focused_container;
    struct tiling_container* fullscreen_container;
    
    // الأبعاد
    int32_t output_width;
    int32_t output_height;
    int32_t bar_height;
};

// دوال المدير
struct tiling_manager* tiling_manager_create(int32_t width, int32_t height);
void tiling_manager_destroy(struct tiling_manager* manager);

// إدارة الحاويات
struct tiling_container* tiling_container_create(
    struct tiling_manager* manager,
    struct wl_resource* surface,
    const char* title,
    const char* app_id
);
void tiling_container_destroy(struct tiling_container* container);
void tiling_container_focus(struct tiling_container* container);
void tiling_container_set_floating(struct tiling_container* container, bool floating);
void tiling_container_set_fullscreen(struct tiling_container* container, bool fullscreen);
void tiling_container_move(struct tiling_container* container, int32_t x, int32_t y);
void tiling_container_resize(struct tiling_container* container, int32_t width, int32_t height);

// التخطيط
void tiling_manager_arrange(struct tiling_manager* manager);
void tiling_manager_set_layout(struct tiling_manager* manager, layout_type_t layout);
void tiling_manager_next_layout(struct tiling_manager* manager);
void tiling_manager_prev_layout(struct tiling_manager* manager);

// التنقل
void tiling_focus_next(struct tiling_manager* manager);
void tiling_focus_prev(struct tiling_manager* manager);
void tiling_focus_up(struct tiling_manager* manager);
void tiling_focus_down(struct tiling_manager* manager);
void tiling_focus_left(struct tiling_manager* manager);
void tiling_focus_right(struct tiling_manager* manager);

// نقل النوافذ
void tiling_move_to_workspace(struct tiling_container* container, int workspace);
void tiling_swap_with_next(struct tiling_manager* manager);
void tiling_swap_with_prev(struct tiling_manager* manager);

// تغيير الحجم
void tiling_increase_split_ratio(struct tiling_container* container);
void tiling_decrease_split_ratio(struct tiling_container* container);
void tiling_resize_container(struct tiling_container* container, int32_t dx, int32_t dy);

// مساحات العمل
void tiling_switch_workspace(struct tiling_manager* manager, int workspace);
void tiling_move_to_workspace_and_switch(struct tiling_container* container, int workspace);

// دوال التخطيط المختلفة
void layout_apply_tiling(struct tiling_container* parent, int32_t x, int32_t y, int32_t width, int32_t height);
void layout_apply_monocle(struct tiling_container* parent, int32_t x, int32_t y, int32_t width, int32_t height);
void layout_apply_grid(struct tiling_container* parent, int32_t x, int32_t y, int32_t width, int32_t height);
void layout_apply_stacked(struct tiling_container* parent, int32_t x, int32_t y, int32_t width, int32_t height);
void layout_apply_tabbed(struct tiling_container* parent, int32_t x, int32_t y, int32_t width, int32_t height);
void layout_apply_spiral(struct tiling_container* parent, int32_t x, int32_t y, int32_t width, int32_t height);
void layout_apply_dwindle(struct tiling_container* parent, int32_t x, int32_t y, int32_t width, int32_t height);

// معلومات
const char* tiling_layout_to_string(layout_type_t layout);
int tiling_get_container_count(struct tiling_manager* manager);
struct tiling_container* tiling_get_focused_container(struct tiling_manager* manager);

#ifdef __cplusplus
}
#endif

#endif // TILING_LAYOUT_H
