#include "keyboard_manager.h"
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "KeyboardManager"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// المتغير العام
static struct keyboard_manager g_manager = {0};

// جداول تحويل المفاتيح
static const uint32_t qwerty_to_azerty[] = {
    // A-Z mappings
    [KEY_A] = KEY_Q,
    [KEY_Q] = KEY_A,
    [KEY_W] = KEY_Z,
    [KEY_Z] = KEY_W,
    [KEY_M] = KEY_SEMICOLON,
    [KEY_SEMICOLON] = KEY_M,
};

// اختصارات افتراضية
static struct {
    shortcut_type_t shortcut;
    uint32_t keycode;
    uint32_t modifiers;
} default_shortcuts[] = {
    {SHORTCUT_ALT_TAB, KEY_TAB, MOD_ALT},
    {SHORTCUT_ALT_F4, KEY_F4, MOD_ALT},
    {SHORTCUT_CTRL_C, KEY_C, MOD_CTRL},
    {SHORTCUT_CTRL_V, KEY_V, MOD_CTRL},
    {SHORTCUT_CTRL_X, KEY_X, MOD_CTRL},
    {SHORTCUT_CTRL_Z, KEY_Z, MOD_CTRL},
    {SHORTCUT_CTRL_SHIFT_T, KEY_T, MOD_CTRL | MOD_SHIFT},
    {SHORTCUT_SUPER_ENTER, KEY_ENTER, MOD_SUPER},
    {SHORTCUT_SUPER_Q, KEY_Q, MOD_SUPER},
    {SHORTCUT_SUPER_ARROW_UP, KEY_UP, MOD_SUPER},
    {SHORTCUT_SUPER_ARROW_DOWN, KEY_DOWN, MOD_SUPER},
    {SHORTCUT_SUPER_ARROW_LEFT, KEY_LEFT, MOD_SUPER},
    {SHORTCUT_SUPER_ARROW_RIGHT, KEY_RIGHT, MOD_SUPER},
    {SHORTCUT_SUPER_NUMBER_1, KEY_1, MOD_SUPER},
};

// تهيئة مدير لوحة المفاتيح
int keyboard_manager_init(void) {
    if (g_manager.initialized) {
        return 0;
    }
    
    memset(&g_manager, 0, sizeof(g_manager));
    
    // الإعدادات الافتراضية
    g_manager.config.keymap = KEYMAP_QWERTY;
    g_manager.config.emulate_numpad = false;
    g_manager.config.swap_ctrl_caps = false;
    g_manager.config.enable_shortcuts = true;
    g_manager.config.passthrough_mode = false;
    
    g_manager.config.key_width = 60.0f;
    g_manager.config.key_height = 60.0f;
    g_manager.config.key_spacing = 4.0f;
    
    g_manager.config.key_bg_color = 0xFF333333;
    g_manager.config.key_text_color = 0xFFFFFFFF;
    g_manager.config.key_pressed_color = 0xFF4C7899;
    
    g_manager.modifiers = 0;
    g_manager.caps_lock = false;
    g_manager.num_lock = false;
    g_manager.scroll_lock = false;
    
    g_manager.keyboard_count = 0;
    
    g_manager.initialized = 1;
    LOGI("Keyboard manager initialized");
    return 0;
}

// إنهاء مدير لوحة المفاتيح
void keyboard_manager_terminate(void) {
    if (!g_manager.initialized) return;
    
    g_manager.initialized = 0;
    LOGI("Keyboard manager terminated");
}

// إضافة لوحة مفاتيح
int keyboard_manager_add_keyboard(keyboard_type_t type) {
    if (!g_manager.initialized) return -1;
    
    // التحقق من عدم التكرار
    for (int i = 0; i < g_manager.keyboard_count; i++) {
        if (g_manager.active_keyboards[i] == type) {
            return 0;
        }
    }
    
    if (g_manager.keyboard_count >= 8) {
        LOGE("Maximum keyboards reached");
        return -1;
    }
    
    g_manager.active_keyboards[g_manager.keyboard_count++] = type;
    LOGI("Keyboard added: %d", type);
    return 0;
}

// إزالة لوحة مفاتيح
void keyboard_manager_remove_keyboard(keyboard_type_t type) {
    if (!g_manager.initialized) return;
    
    for (int i = 0; i < g_manager.keyboard_count; i++) {
        if (g_manager.active_keyboards[i] == type) {
            // إزالة بنقل العناصر
            for (int j = i; j < g_manager.keyboard_count - 1; j++) {
                g_manager.active_keyboards[j] = g_manager.active_keyboards[j + 1];
            }
            g_manager.keyboard_count--;
            LOGI("Keyboard removed: %d", type);
            return;
        }
    }
}

// تعيين لوحة المفاتيح النشطة
void keyboard_manager_set_active_keyboard(keyboard_type_t type) {
    // TODO: Implement active keyboard switching
    LOGI("Active keyboard set to: %d", type);
}

// تعيين نمط لوحة المفاتيح
void keyboard_manager_set_keymap(keymap_type_t keymap) {
    if (!g_manager.initialized) return;
    g_manager.config.keymap = keymap;
    LOGI("Keymap set to: %d", keymap);
}

// تعيين الإعدادات
void keyboard_manager_set_config(const struct keyboard_config* config) {
    if (!config) return;
    memcpy(&g_manager.config, config, sizeof(*config));
}

// الحصول على الإعدادات
const struct keyboard_config* keyboard_manager_get_config(void) {
    return &g_manager.config;
}

// معالجة مفتاح
void keyboard_manager_process_key(uint32_t keycode, bool pressed) {
    if (!g_manager.initialized) return;
    
    // تحديث المعدّلات
    switch (keycode) {
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
            if (pressed) g_manager.modifiers |= MOD_SHIFT;
            else g_manager.modifiers &= ~MOD_SHIFT;
            break;
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL:
            if (pressed) g_manager.modifiers |= MOD_CTRL;
            else g_manager.modifiers &= ~MOD_CTRL;
            break;
        case KEY_LEFTALT:
            if (pressed) g_manager.modifiers |= MOD_ALT;
            else g_manager.modifiers &= ~MOD_ALT;
            break;
        case KEY_RIGHTALT:
            if (pressed) g_manager.modifiers |= MOD_ALT_GR;
            else g_manager.modifiers &= ~MOD_ALT_GR;
            break;
        case KEY_LEFTMETA:
        case KEY_RIGHTMETA:
            if (pressed) g_manager.modifiers |= MOD_SUPER;
            else g_manager.modifiers &= ~MOD_SUPER;
            break;
        case KEY_CAPSLOCK:
            if (pressed) g_manager.caps_lock = !g_manager.caps_lock;
            break;
        case KEY_NUMLOCK:
            if (pressed) g_manager.num_lock = !g_manager.num_lock;
            break;
    }
    
    // التحقق من الاختصارات
    if (pressed && g_manager.config.enable_shortcuts && !g_manager.config.passthrough_mode) {
        for (int i = 0; i < sizeof(default_shortcuts) / sizeof(default_shortcuts[0]); i++) {
            if (default_shortcuts[i].keycode == keycode &&
                default_shortcuts[i].modifiers == g_manager.modifiers) {
                if (g_manager.on_shortcut) {
                    g_manager.on_shortcut(default_shortcuts[i].shortcut);
                }
                LOGI("Shortcut triggered: %d", default_shortcuts[i].shortcut);
                return;
            }
        }
    }
    
    // ترجمة المفتاح إذا لزم الأمر
    uint32_t translated = keycode;
    if (g_manager.config.keymap != KEYMAP_QWERTY) {
        translated = keyboard_manager_translate_key(keycode, KEYMAP_QWERTY, g_manager.config.keymap);
    }
    
    // إرسال الحدث
    if (g_manager.on_key) {
        g_manager.on_key(translated, pressed, g_manager.modifiers);
    }
}

// ترجمة مفتاح
uint32_t keyboard_manager_translate_key(uint32_t keycode, keymap_type_t from, keymap_type_t to) {
    if (from == to) return keycode;
    
    // TODO: Implement full keymap translation
    if (from == KEYMAP_QWERTY && to == KEYMAP_AZERTY) {
        if (keycode < sizeof(qwerty_to_azerty) / sizeof(qwerty_to_azerty[0])) {
            return qwerty_to_azerty[keycode] ? qwerty_to_azerty[keycode] : keycode;
        }
    }
    
    return keycode;
}

// التحقق من اختصار
bool keyboard_manager_check_shortcut(shortcut_type_t shortcut) {
    for (int i = 0; i < sizeof(default_shortcuts) / sizeof(default_shortcuts[0]); i++) {
        if (default_shortcuts[i].shortcut == shortcut) {
            return (g_manager.modifiers & default_shortcuts[i].modifiers) == default_shortcuts[i].modifiers;
        }
    }
    return false;
}

// تسجيل اختصار
void keyboard_manager_register_shortcut(shortcut_type_t shortcut, uint32_t keycode, uint32_t modifiers) {
    // TODO: Implement custom shortcut registration
    LOGI("Shortcut registered: %d -> key=%u, mods=%u", shortcut, keycode, modifiers);
}

// تعيين معاودة الاتصال للاختصارات
void keyboard_manager_set_shortcut_callback(void (*callback)(shortcut_type_t shortcut)) {
    g_manager.on_shortcut = callback;
}

// إظهار لوحة المفاتيح الافتراضية
void keyboard_manager_show_virtual(void) {
    // TODO: Implement via JNI
    LOGI("Virtual keyboard shown");
}

// إخفاء لوحة المفاتيح الافتراضية
void keyboard_manager_hide_virtual(void) {
    // TODO: Implement via JNI
    LOGI("Virtual keyboard hidden");
}

// تبديل لوحة المفاتيح الافتراضية
void keyboard_manager_toggle_virtual(void) {
    // TODO: Implement via JNI
    LOGI("Virtual keyboard toggled");
}

// التحقق من ظهور لوحة المفاتيح الافتراضية
bool keyboard_manager_is_virtual_visible(void) {
    // TODO: Implement via JNI
    return false;
}

// تعيين حجم المفاتيح
void keyboard_manager_set_key_size(float width, float height, float spacing) {
    g_manager.config.key_width = width;
    g_manager.config.key_height = height;
    g_manager.config.key_spacing = spacing;
}

// تعيين ألوان المفاتيح
void keyboard_manager_set_key_colors(uint32_t bg, uint32_t text, uint32_t pressed) {
    g_manager.config.key_bg_color = bg;
    g_manager.config.key_text_color = text;
    g_manager.config.key_pressed_color = pressed;
}

// تعيين وضع التمرير
void keyboard_manager_set_passthrough(bool enable) {
    g_manager.config.passthrough_mode = enable;
    LOGI("Passthrough mode: %s", enable ? "enabled" : "disabled");
}

// الحصول على وضع التمرير
bool keyboard_manager_get_passthrough(void) {
    return g_manager.config.passthrough_mode;
}

// الحصول على المعدّلات
uint32_t keyboard_manager_get_modifiers(void) {
    return g_manager.modifiers;
}

// التحقق من Caps Lock
bool keyboard_manager_is_caps_lock(void) {
    return g_manager.caps_lock;
}

// التحقق من Num Lock
bool keyboard_manager_is_num_lock(void) {
    return g_manager.num_lock;
}
