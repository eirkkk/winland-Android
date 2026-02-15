#ifndef KEYBOARD_MANAGER_H
#define KEYBOARD_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <linux/input.h>

#ifdef __cplusplus
extern "C" {
#endif

// أنواع لوحات المفاتيح
typedef enum {
    KEYBOARD_TYPE_NONE,
    KEYBOARD_TYPE_PHYSICAL_USB,
    KEYBOARD_TYPE_PHYSICAL_BLUETOOTH,
    KEYBOARD_TYPE_VIRTUAL_ANDROID,
    KEYBOARD_TYPE_REMOTE_VNC
} keyboard_type_t;

// أنماط لوحة المفاتيح
typedef enum {
    KEYMAP_QWERTY,
    KEYMAP_AZERTY,
    KEYMAP_QWERTZ,
    KEYMAP_DVORAK,
    KEYMAP_COLEMAK,
    KEYMAP_ARABIC,
    KEYMAP_CUSTOM
} keymap_type_t;

// معدّلات المفاتيح
#define MOD_SHIFT   (1 << 0)
#define MOD_CTRL    (1 << 1)
#define MOD_ALT     (1 << 2)
#define MOD_SUPER   (1 << 3)
#define MOD_ALT_GR  (1 << 4)
#define MOD_CAPS    (1 << 5)
#define MOD_NUM     (1 << 6)

// اختصارات لوحة المفاتيح
typedef enum {
    SHORTCUT_NONE = 0,
    SHORTCUT_ALT_TAB,
    SHORTCUT_ALT_F4,
    SHORTCUT_CTRL_C,
    SHORTCUT_CTRL_V,
    SHORTCUT_CTRL_X,
    SHORTCUT_CTRL_Z,
    SHORTCUT_CTRL_SHIFT_T,
    SHORTCUT_SUPER_ENTER,
    SHORTCUT_SUPER_Q,
    SHORTCUT_SUPER_ARROW_UP,
    SHORTCUT_SUPER_ARROW_DOWN,
    SHORTCUT_SUPER_ARROW_LEFT,
    SHORTCUT_SUPER_ARROW_RIGHT,
    SHORTCUT_SUPER_NUMBER_1,
    // ... حتى 9
    SHORTCUT_COUNT
} shortcut_type_t;

// هيكل إعدادات لوحة المفاتيح
struct keyboard_config {
    keymap_type_t keymap;
    bool emulate_numpad;
    bool swap_ctrl_caps;
    bool enable_shortcuts;
    bool passthrough_mode;
    
    // أحجام مفاتيح الشاشة
    float key_width;
    float key_height;
    float key_spacing;
    
    // الألوان
    uint32_t key_bg_color;
    uint32_t key_text_color;
    uint32_t key_pressed_color;
};

// هيكل مدير لوحة المفاتيح
struct keyboard_manager {
    int initialized;
    
    // الإعدادات
    struct keyboard_config config;
    
    // الحالة
    uint32_t modifiers;
    bool caps_lock;
    bool num_lock;
    bool scroll_lock;
    
    // لوحات المفاتيح المتصلة
    keyboard_type_t active_keyboards[8];
    int keyboard_count;
    
    // معاودة الاتصال للاختصارات
    void (*on_shortcut)(shortcut_type_t shortcut);
    void (*on_key)(uint32_t keycode, bool pressed, uint32_t modifiers);
};

// دوال التهيئة
int keyboard_manager_init(void);
void keyboard_manager_terminate(void);

// إدارة لوحات المفاتيح
int keyboard_manager_add_keyboard(keyboard_type_t type);
void keyboard_manager_remove_keyboard(keyboard_type_t type);
void keyboard_manager_set_active_keyboard(keyboard_type_t type);

// التكوين
void keyboard_manager_set_keymap(keymap_type_t keymap);
void keyboard_manager_set_config(const struct keyboard_config* config);
const struct keyboard_config* keyboard_manager_get_config(void);

// معالجة المفاتيح
void keyboard_manager_process_key(uint32_t keycode, bool pressed);
void keyboard_manager_process_modifiers(uint32_t modifiers);
uint32_t keyboard_manager_translate_key(uint32_t keycode, keymap_type_t from, keymap_type_t to);

// الاختصارات
bool keyboard_manager_check_shortcut(shortcut_type_t shortcut);
void keyboard_manager_register_shortcut(shortcut_type_t shortcut, uint32_t keycode, uint32_t modifiers);
void keyboard_manager_set_shortcut_callback(void (*callback)(shortcut_type_t shortcut));

// لوحة المفاتيح الافتراضية
void keyboard_manager_show_virtual(void);
void keyboard_manager_hide_virtual(void);
void keyboard_manager_toggle_virtual(void);
bool keyboard_manager_is_virtual_visible(void);

// أحجام مخصصة
void keyboard_manager_set_key_size(float width, float height, float spacing);
void keyboard_manager_set_key_colors(uint32_t bg, uint32_t text, uint32_t pressed);

// وضع التمرير
void keyboard_manager_set_passthrough(bool enable);
bool keyboard_manager_get_passthrough(void);

// الحصول على الحالة
uint32_t keyboard_manager_get_modifiers(void);
bool keyboard_manager_is_caps_lock(void);
bool keyboard_manager_is_num_lock(void);

#ifdef __cplusplus
}
#endif

#endif // KEYBOARD_MANAGER_H
