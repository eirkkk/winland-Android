#include "tiling_layout.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <android/log.h>

#define LOG_TAG "TilingLayout"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define DEFAULT_SPLIT_RATIO 0.5f
#define MIN_CONTAINER_SIZE 50

// إنشاء مدير التخطيط
struct tiling_manager* tiling_manager_create(int32_t width, int32_t height) {
    struct tiling_manager* manager = calloc(1, sizeof(*manager));
    if (!manager) {
        LOGE("Failed to allocate tiling manager");
        return NULL;
    }
    
    wl_list_init(&manager->workspaces);
    wl_list_init(&manager->floating);
    
    manager->output_width = width;
    manager->output_height = height;
    manager->bar_height = 30;
    
    manager->default_layout = LAYOUT_TILING;
    manager->gap_size = 4;
    manager->border_width = 2;
    manager->border_color_focused = 0xFF4C7899;
    manager->border_color_unfocused = 0xFF333333;
    manager->border_color_urgent = 0xFFFF0000;
    
    manager->num_workspaces = 10;
    manager->current_workspace = 1;
    
    LOGI("Tiling manager created: %dx%d", width, height);
    return manager;
}

// تدمير مدير التخطيط
void tiling_manager_destroy(struct tiling_manager* manager) {
    if (!manager) return;
    
    // تدمير جميع الحاويات
    struct tiling_container* container, *tmp;
    wl_list_for_each_safe(container, tmp, &manager->workspaces, link) {
        tiling_container_destroy(container);
    }
    
    wl_list_for_each_safe(container, tmp, &manager->floating, link) {
        tiling_container_destroy(container);
    }
    
    free(manager);
    LOGI("Tiling manager destroyed");
}

// إنشاء حاوية
struct tiling_container* tiling_container_create(
    struct tiling_manager* manager,
    struct wl_resource* surface,
    const char* title,
    const char* app_id
) {
    struct tiling_container* container = calloc(1, sizeof(*container));
    if (!container) {
        LOGE("Failed to allocate container");
        return NULL;
    }
    
    wl_list_init(&container->children);
    container->parent = NULL;
    container->surface = surface;
    container->layout = manager->default_layout;
    container->split = SPLIT_VERTICAL;
    container->split_ratio = DEFAULT_SPLIT_RATIO;
    container->border_width = manager->border_width;
    container->mapped = false;
    container->floating = false;
    container->fullscreen = false;
    container->urgent = false;
    container->focused = false;
    
    if (title) {
        strncpy(container->title, title, sizeof(container->title) - 1);
    }
    if (app_id) {
        strncpy(container->app_id, app_id, sizeof(container->app_id) - 1);
    }
    
    // إضافة إلى القائمة
    wl_list_insert(&manager->workspaces, &container->link);
    
    LOGI("Container created: %s (%s)", title ? title : "untitled", 
         app_id ? app_id : "unknown");
    
    return container;
}

// تدمير حاوية
void tiling_container_destroy(struct tiling_container* container) {
    if (!container) return;
    
    // إزالة من القائمة
    wl_list_remove(&container->link);
    
    // تدمير الأطفال
    struct tiling_container* child, *tmp;
    wl_list_for_each_safe(child, tmp, &container->children, link) {
        tiling_container_destroy(child);
    }
    
    LOGI("Container destroyed: %s", container->title);
    free(container);
}

// تركيز حاوية
void tiling_container_focus(struct tiling_container* container) {
    if (!container) return;
    
    struct tiling_manager* manager = NULL;
    // TODO: Get manager from container
    
    if (manager && manager->focused_container) {
        manager->focused_container->focused = false;
    }
    
    container->focused = true;
    if (manager) {
        manager->focused_container = container;
    }
    
    LOGI("Container focused: %s", container->title);
}

// تعيين حالة الطفو
void tiling_container_set_floating(struct tiling_container* container, bool floating) {
    if (!container) return;
    
    container->floating = floating;
    
    // TODO: Move between workspaces and floating lists
    
    LOGI("Container %s: %s", floating ? "floating" : "tiled", container->title);
}

// تعيين حالة ملء الشاشة
void tiling_container_set_fullscreen(struct tiling_container* container, bool fullscreen) {
    if (!container) return;
    
    container->fullscreen = fullscreen;
    
    LOGI("Container %s: %s", fullscreen ? "fullscreen" : "normal", container->title);
}

// ترتيب التخطيط
void tiling_manager_arrange(struct tiling_manager* manager) {
    if (!manager) return;
    
    int32_t x = manager->gap_size;
    int32_t y = manager->gap_size + manager->bar_height;
    int32_t width = manager->output_width - 2 * manager->gap_size;
    int32_t height = manager->output_height - 2 * manager->gap_size - manager->bar_height;
    
    // تطبيق التخطيط المناسب
    switch (manager->default_layout) {
        case LAYOUT_TILING:
        case LAYOUT_FLOATING:
            layout_apply_tiling(NULL, x, y, width, height);
            break;
        case LAYOUT_MONOCLE:
            layout_apply_monocle(NULL, x, y, width, height);
            break;
        case LAYOUT_GRID:
            layout_apply_grid(NULL, x, y, width, height);
            break;
        case LAYOUT_STACKED:
            layout_apply_stacked(NULL, x, y, width, height);
            break;
        case LAYOUT_TABBED:
            layout_apply_tabbed(NULL, x, y, width, height);
            break;
        case LAYOUT_SPIRAL:
            layout_apply_spiral(NULL, x, y, width, height);
            break;
        case LAYOUT_DWINDLE:
            layout_apply_dwindle(NULL, x, y, width, height);
            break;
    }
    
    LOGI("Layout arranged");
}

// تطبيق تخطيط التجانب
void layout_apply_tiling(struct tiling_container* parent, int32_t x, int32_t y, int32_t width, int32_t height) {
    if (!parent) return;
    
    int count = wl_list_length(&parent->children);
    if (count == 0) return;
    
    struct tiling_container* child;
    int i = 0;
    
    if (parent->split == SPLIT_VERTICAL) {
        int child_width = width / count;
        wl_list_for_each(child, &parent->children, link) {
            child->x = x + i * child_width;
            child->y = y;
            child->width = child_width;
            child->height = height;
            i++;
        }
    } else {
        int child_height = height / count;
        wl_list_for_each(child, &parent->children, link) {
            child->x = x;
            child->y = y + i * child_height;
            child->width = width;
            child->height = child_height;
            i++;
        }
    }
}

// تطبيق تخطيط أحادي
void layout_apply_monocle(struct tiling_container* parent, int32_t x, int32_t y, int32_t width, int32_t height) {
    if (!parent) return;
    
    struct tiling_container* child;
    wl_list_for_each(child, &parent->children, link) {
        child->x = x;
        child->y = y;
        child->width = width;
        child->height = height;
    }
}

// تطبيق تخطيط الشبكة
void layout_apply_grid(struct tiling_container* parent, int32_t x, int32_t y, int32_t width, int32_t height) {
    if (!parent) return;
    
    int count = wl_list_length(&parent->children);
    if (count == 0) return;
    
    // حساب أبعاد الشبكة
    int cols = (int)ceil(sqrt(count));
    int rows = (int)ceil((double)count / cols);
    
    int child_width = width / cols;
    int child_height = height / rows;
    
    struct tiling_container* child;
    int i = 0;
    wl_list_for_each(child, &parent->children, link) {
        int col = i % cols;
        int row = i / cols;
        
        child->x = x + col * child_width;
        child->y = y + row * child_height;
        child->width = child_width;
        child->height = child_height;
        i++;
    }
}

// تطبيق تخطيط مكدس
void layout_apply_stacked(struct tiling_container* parent, int32_t x, int32_t y, int32_t width, int32_t height) {
    if (!parent) return;
    
    int count = wl_list_length(&parent->children);
    if (count == 0) return;
    
    int title_height = 20;
    int content_height = height - (count * title_height);
    
    struct tiling_container* child;
    int i = 0;
    wl_list_for_each(child, &parent->children, link) {
        child->x = x;
        child->y = y + i * title_height;
        child->width = width;
        
        if (child->focused) {
            child->height = content_height;
        } else {
            child->height = title_height;
        }
        i++;
    }
}

// تطبيق تخطيط تبويبات
void layout_apply_tabbed(struct tiling_container* parent, int32_t x, int32_t y, int32_t width, int32_t height) {
    if (!parent) return;
    
    int title_height = 20;
    int content_height = height - title_height;
    
    struct tiling_container* child;
    wl_list_for_each(child, &parent->children, link) {
        child->x = x;
        child->y = y + title_height;
        child->width = width;
        child->height = content_height;
    }
}

// تطبيق تخطيط حلزوني
void layout_apply_spiral(struct tiling_container* parent, int32_t x, int32_t y, int32_t width, int32_t height) {
    if (!parent) return;
    
    int count = wl_list_length(&parent->children);
    if (count == 0) return;
    
    struct tiling_container* child;
    int i = 0;
    int cx = x, cy = y;
    int cw = width, ch = height;
    
    wl_list_for_each(child, &parent->children, link) {
        if (i % 2 == 0) {
            // تقسيم عمودي
            child->x = cx;
            child->y = cy;
            child->width = cw / 2;
            child->height = ch;
            cx += child->width;
            cw -= child->width;
        } else {
            // تقسيم أفقي
            child->x = cx;
            child->y = cy;
            child->width = cw;
            child->height = ch / 2;
            cy += child->height;
            ch -= child->height;
        }
        i++;
    }
}

// تطبيق تخطيط متناقص
void layout_apply_dwindle(struct tiling_container* parent, int32_t x, int32_t y, int32_t width, int32_t height) {
    if (!parent) return;
    
    int count = wl_list_length(&parent->children);
    if (count == 0) return;
    
    struct tiling_container* child;
    int i = 0;
    float ratio = 0.5f;
    int cx = x, cy = y;
    int cw = width, ch = height;
    
    wl_list_for_each(child, &parent->children, link) {
        if (i % 2 == 0) {
            // تقسيم عمودي
            child->x = cx;
            child->y = cy;
            child->width = (int)(cw * ratio);
            child->height = ch;
            cx += child->width;
            cw -= child->width;
        } else {
            // تقسيم أفقي
            child->x = cx;
            child->y = cy;
            child->width = cw;
            child->height = (int)(ch * ratio);
            cy += child->height;
            ch -= child->height;
        }
        i++;
    }
}

// تعيين نوع التخطيط
void tiling_manager_set_layout(struct tiling_manager* manager, layout_type_t layout) {
    if (!manager) return;
    
    manager->default_layout = layout;
    tiling_manager_arrange(manager);
    
    LOGI("Layout set to: %s", tiling_layout_to_string(layout));
}

// التخطيط التالي
void tiling_manager_next_layout(struct tiling_manager* manager) {
    if (!manager) return;
    
    manager->default_layout = (manager->default_layout + 1) % (LAYOUT_DWINDLE + 1);
    tiling_manager_arrange(manager);
    
    LOGI("Next layout: %s", tiling_layout_to_string(manager->default_layout));
}

// التخطيط السابق
void tiling_manager_prev_layout(struct tiling_manager* manager) {
    if (!manager) return;
    
    if (manager->default_layout == 0) {
        manager->default_layout = LAYOUT_DWINDLE;
    } else {
        manager->default_layout--;
    }
    tiling_manager_arrange(manager);
    
    LOGI("Previous layout: %s", tiling_layout_to_string(manager->default_layout));
}

// التركيز على التالي
void tiling_focus_next(struct tiling_manager* manager) {
    if (!manager || !manager->focused_container) return;
    
    struct tiling_container* next = wl_container_of(
        manager->focused_container->link.next,
        manager->focused_container,
        link
    );
    
    if (&next->link != &manager->workspaces) {
        tiling_container_focus(next);
    }
}

// التركيز على السابق
void tiling_focus_prev(struct tiling_manager* manager) {
    if (!manager || !manager->focused_container) return;
    
    struct tiling_container* prev = wl_container_of(
        manager->focused_container->link.prev,
        manager->focused_container,
        link
    );
    
    if (&prev->link != &manager->workspaces) {
        tiling_container_focus(prev);
    }
}

// التركيز للأعلى
void tiling_focus_up(struct tiling_manager* manager) {
    // TODO: Implement spatial focus
}

// التركيز للأسفل
void tiling_focus_down(struct tiling_manager* manager) {
    // TODO: Implement spatial focus
}

// التركيز لليسار
void tiling_focus_left(struct tiling_manager* manager) {
    // TODO: Implement spatial focus
}

// التركيز لليمين
void tiling_focus_right(struct tiling_manager* manager) {
    // TODO: Implement spatial focus
}

// زيادة نسبة التقسيم
void tiling_increase_split_ratio(struct tiling_container* container) {
    if (!container) return;
    
    container->split_ratio += 0.05f;
    if (container->split_ratio > 0.9f) {
        container->split_ratio = 0.9f;
    }
    
    LOGI("Split ratio increased to %.2f", container->split_ratio);
}

// تقليل نسبة التقسيم
void tiling_decrease_split_ratio(struct tiling_container* container) {
    if (!container) return;
    
    container->split_ratio -= 0.05f;
    if (container->split_ratio < 0.1f) {
        container->split_ratio = 0.1f;
    }
    
    LOGI("Split ratio decreased to %.2f", container->split_ratio);
}

// تحويل التخطيط إلى نص
const char* tiling_layout_to_string(layout_type_t layout) {
    switch (layout) {
        case LAYOUT_FLOATING: return "floating";
        case LAYOUT_TILING: return "tiling";
        case LAYOUT_MONOCLE: return "monocle";
        case LAYOUT_GRID: return "grid";
        case LAYOUT_STACKED: return "stacked";
        case LAYOUT_TABBED: return "tabbed";
        case LAYOUT_SPIRAL: return "spiral";
        case LAYOUT_DWINDLE: return "dwindle";
        default: return "unknown";
    }
}

// الحصول على عدد الحاويات
int tiling_get_container_count(struct tiling_manager* manager) {
    if (!manager) return 0;
    return wl_list_length(&manager->workspaces);
}

// الحصول على الحاوية المحددة
struct tiling_container* tiling_get_focused_container(struct tiling_manager* manager) {
    if (!manager) return NULL;
    return manager->focused_container;
}
