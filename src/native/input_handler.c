#include "input_handler.h"
#include "wayland_compositor.h"
#include "seat_manager.h"
#include <stdlib.h>
#include <android/log.h>
#include <android/keycodes.h>

#define LOG_TAG "InputHandler"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// المتغير العام لمعالج المدخلات
static struct input_handler g_input = {0};

// جدول تحويل رموز مفاتيح Android إلى Wayland
static uint32_t key_conversion_table[] = {
    [AKEYCODE_UNKNOWN] = 0,
    [AKEYCODE_SOFT_LEFT] = 0,
    [AKEYCODE_SOFT_RIGHT] = 0,
    [AKEYCODE_HOME] = 0,
    [AKEYCODE_BACK] = 0,
    [AKEYCODE_CALL] = 0,
    [AKEYCODE_ENDCALL] = 0,
    [AKEYCODE_0] = 19,
    [AKEYCODE_1] = 10,
    [AKEYCODE_2] = 11,
    [AKEYCODE_3] = 12,
    [AKEYCODE_4] = 13,
    [AKEYCODE_5] = 14,
    [AKEYCODE_6] = 15,
    [AKEYCODE_7] = 16,
    [AKEYCODE_8] = 17,
    [AKEYCODE_9] = 18,
    [AKEYCODE_STAR] = 0,
    [AKEYCODE_POUND] = 0,
    [AKEYCODE_DPAD_UP] = 111,
    [AKEYCODE_DPAD_DOWN] = 116,
    [AKEYCODE_DPAD_LEFT] = 113,
    [AKEYCODE_DPAD_RIGHT] = 114,
    [AKEYCODE_DPAD_CENTER] = 0,
    [AKEYCODE_VOLUME_UP] = 123,
    [AKEYCODE_VOLUME_DOWN] = 122,
    [AKEYCODE_POWER] = 124,
    [AKEYCODE_CAMERA] = 0,
    [AKEYCODE_CLEAR] = 0,
    [AKEYCODE_A] = 38,
    [AKEYCODE_B] = 56,
    [AKEYCODE_C] = 54,
    [AKEYCODE_D] = 40,
    [AKEYCODE_E] = 26,
    [AKEYCODE_F] = 41,
    [AKEYCODE_G] = 42,
    [AKEYCODE_H] = 43,
    [AKEYCODE_I] = 31,
    [AKEYCODE_J] = 44,
    [AKEYCODE_K] = 45,
    [AKEYCODE_L] = 46,
    [AKEYCODE_M] = 58,
    [AKEYCODE_N] = 57,
    [AKEYCODE_O] = 32,
    [AKEYCODE_P] = 33,
    [AKEYCODE_Q] = 24,
    [AKEYCODE_R] = 27,
    [AKEYCODE_S] = 39,
    [AKEYCODE_T] = 28,
    [AKEYCODE_U] = 30,
    [AKEYCODE_V] = 55,
    [AKEYCODE_W] = 25,
    [AKEYCODE_X] = 53,
    [AKEYCODE_Y] = 29,
    [AKEYCODE_Z] = 52,
    [AKEYCODE_COMMA] = 59,
    [AKEYCODE_PERIOD] = 60,
    [AKEYCODE_ALT_LEFT] = 64,
    [AKEYCODE_ALT_RIGHT] = 108,
    [AKEYCODE_SHIFT_LEFT] = 50,
    [AKEYCODE_SHIFT_RIGHT] = 62,
    [AKEYCODE_TAB] = 23,
    [AKEYCODE_SPACE] = 65,
    [AKEYCODE_SYM] = 0,
    [AKEYCODE_EXPLORER] = 0,
    [AKEYCODE_ENVELOPE] = 0,
    [AKEYCODE_ENTER] = 36,
    [AKEYCODE_DEL] = 119,
    [AKEYCODE_GRAVE] = 49,
    [AKEYCODE_MINUS] = 20,
    [AKEYCODE_EQUALS] = 21,
    [AKEYCODE_LEFT_BRACKET] = 34,
    [AKEYCODE_RIGHT_BRACKET] = 35,
    [AKEYCODE_BACKSLASH] = 51,
    [AKEYCODE_SEMICOLON] = 47,
    [AKEYCODE_APOSTROPHE] = 48,
    [AKEYCODE_SLASH] = 61,
    [AKEYCODE_AT] = 0,
    [AKEYCODE_NUM] = 0,
    [AKEYCODE_HEADSETHOOK] = 0,
    [AKEYCODE_FOCUS] = 0,
    [AKEYCODE_PLUS] = 0,
    [AKEYCODE_MENU] = 135,
    [AKEYCODE_NOTIFICATION] = 0,
    [AKEYCODE_SEARCH] = 0,
    [AKEYCODE_MEDIA_PLAY_PAUSE] = 172,
    [AKEYCODE_MEDIA_STOP] = 174,
    [AKEYCODE_MEDIA_NEXT] = 171,
    [AKEYCODE_MEDIA_PREVIOUS] = 173,
    [AKEYCODE_MEDIA_REWIND] = 0,
    [AKEYCODE_MEDIA_FAST_FORWARD] = 0,
    [AKEYCODE_MUTE] = 121,
    [AKEYCODE_PAGE_UP] = 112,
    [AKEYCODE_PAGE_DOWN] = 117,
    [AKEYCODE_ESCAPE] = 9,
    [AKEYCODE_CTRL_LEFT] = 37,
    [AKEYCODE_CTRL_RIGHT] = 105,
    [AKEYCODE_CAPS_LOCK] = 66,
    [AKEYCODE_SCROLL_LOCK] = 78,
    [AKEYCODE_META_LEFT] = 133,
    [AKEYCODE_META_RIGHT] = 134,
    [AKEYCODE_FUNCTION] = 0,
    [AKEYCODE_SYSRQ] = 107,
    [AKEYCODE_BREAK] = 0,
    [AKEYCODE_MOVE_HOME] = 110,
    [AKEYCODE_MOVE_END] = 115,
    [AKEYCODE_INSERT] = 118,
    [AKEYCODE_FORWARD] = 0,
    [AKEYCODE_MEDIA_PLAY] = 172,
    [AKEYCODE_MEDIA_PAUSE] = 172,
    [AKEYCODE_MEDIA_CLOSE] = 0,
    [AKEYCODE_MEDIA_EJECT] = 169,
    [AKEYCODE_MEDIA_RECORD] = 0,
    [AKEYCODE_F1] = 67,
    [AKEYCODE_F2] = 68,
    [AKEYCODE_F3] = 69,
    [AKEYCODE_F4] = 70,
    [AKEYCODE_F5] = 71,
    [AKEYCODE_F6] = 72,
    [AKEYCODE_F7] = 73,
    [AKEYCODE_F8] = 74,
    [AKEYCODE_F9] = 75,
    [AKEYCODE_F10] = 76,
    [AKEYCODE_F11] = 95,
    [AKEYCODE_F12] = 96,
};

// تهيئة معالج المدخلات
int input_handler_init(struct winland_compositor *compositor) {
    if (!compositor) {
        LOGE("Invalid compositor");
        return -1;
    }
    
    g_input.compositor = compositor;
    
    // البحث عن مقعد موجود أو إنشاء واحد جديد
    if (!wl_list_empty(&compositor->seats)) {
        g_input.seat = wl_container_of(compositor->seats.next, g_input.seat, link);
    } else {
        g_input.seat = winland_seat_create(compositor, "seat0");
        if (!g_input.seat) {
            LOGE("Failed to create seat");
            return -1;
        }
    }
    
    // تهيئة الحالة
    g_input.pointer_x = wl_fixed_from_int(0);
    g_input.pointer_y = wl_fixed_from_int(0);
    g_input.pointer_buttons = 0;
    g_input.touch_active = 0;
    g_input.touch_id = -1;
    g_input.key_modifiers = 0;
    
    LOGI("Input handler initialized");
    return 0;
}

// إنهاء معالج المدخلات
void input_handler_terminate(void) {
    memset(&g_input, 0, sizeof(g_input));
    LOGI("Input handler terminated");
}

// تحويل رمز مفتاح Android إلى Wayland
uint32_t android_key_to_wayland(int android_key_code) {
    if (android_key_code < 0 || android_key_code >= sizeof(key_conversion_table) / sizeof(key_conversion_table[0])) {
        return 0;
    }
    return key_conversion_table[android_key_code];
}

// تحويل معدّلات Android إلى Wayland
uint32_t android_modifier_to_wayland(int android_modifiers) {
    uint32_t wayland_mods = 0;
    
    if (android_modifiers & INPUT_MODIFIER_SHIFT) wayland_mods |= 1; // Shift
    if (android_modifiers & INPUT_MODIFIER_CTRL) wayland_mods |= 4;  // Ctrl
    if (android_modifiers & INPUT_MODIFIER_ALT) wayland_mods |= 8;   // Alt
    if (android_modifiers & INPUT_MODIFIER_META) wayland_mods |= 64; // Meta
    
    return wayland_mods;
}

// إرسال حدث لمس
void input_handler_send_touch_event(int action, float x, float y, int pointer_id) {
    switch (action) {
        case INPUT_TOUCH_DOWN:
            input_handler_send_touch_down(x, y, pointer_id);
            break;
        case INPUT_TOUCH_UP:
            input_handler_send_touch_up(x, y, pointer_id);
            break;
        case INPUT_TOUCH_MOVE:
            input_handler_send_touch_move(x, y, pointer_id);
            break;
        case INPUT_TOUCH_CANCEL:
            // معالجة الإلغاء
            g_input.touch_active = 0;
            g_input.touch_id = -1;
            break;
    }
}

// إرسال حدث لمس down
void input_handler_send_touch_down(float x, float y, int pointer_id) {
    if (!g_input.seat) return;
    
    wl_fixed_t fixed_x = wl_fixed_from_double(x);
    wl_fixed_t fixed_y = wl_fixed_from_double(y);
    
    // تحديث حالة اللمس
    g_input.touch_active = 1;
    g_input.touch_id = pointer_id;
    g_input.touch_x = fixed_x;
    g_input.touch_y = fixed_y;
    
    // تحديث موقع المؤشر أيضاً
    input_handler_send_pointer_motion(fixed_x, fixed_y);
    
    // إرسال حدث زر المؤشر (محاكاة النقر)
    input_handler_send_pointer_button(0x110, 1); // BTN_LEFT = 0x110, pressed
    
    LOGI("Touch down: x=%.2f, y=%.2f, id=%d", x, y, pointer_id);
}

// إرسال حدث لمس up
void input_handler_send_touch_up(float x, float y, int pointer_id) {
    if (!g_input.seat) return;
    
    // إرسال حدث رفع زر المؤشر
    input_handler_send_pointer_button(0x110, 0); // BTN_LEFT = 0x110, released
    
    // تحديث حالة اللمس
    g_input.touch_active = 0;
    g_input.touch_id = -1;
    
    LOGI("Touch up: x=%.2f, y=%.2f, id=%d", x, y, pointer_id);
}

// إرسال حدث لمس move
void input_handler_send_touch_move(float x, float y, int pointer_id) {
    if (!g_input.seat) return;
    if (g_input.touch_id != pointer_id) return;
    
    wl_fixed_t fixed_x = wl_fixed_from_double(x);
    wl_fixed_t fixed_y = wl_fixed_from_double(y);
    
    // تحديث موقع اللمس
    g_input.touch_x = fixed_x;
    g_input.touch_y = fixed_y;
    
    // تحديث موقع المؤشر
    input_handler_send_pointer_motion(fixed_x, fixed_y);
}

// إرسال حركة المؤشر
void input_handler_send_pointer_motion(wl_fixed_t x, wl_fixed_t y) {
    if (!g_input.seat) return;
    
    g_input.pointer_x = x;
    g_input.pointer_y = y;
    
    // إرسال إلى مقعد Wayland
    winland_seat_notify_pointer_motion(g_input.seat, x, y);
}

// إرسال حدث زر المؤشر
void input_handler_send_pointer_button(uint32_t button, uint32_t state) {
    if (!g_input.seat) return;
    
    if (state) {
        g_input.pointer_buttons |= (1 << button);
    } else {
        g_input.pointer_buttons &= ~(1 << button);
    }
    
    winland_seat_notify_pointer_button(g_input.seat, button, state);
}

// إرسال حدث محور المؤشر (العجلة)
void input_handler_send_pointer_axis(uint32_t axis, wl_fixed_t value) {
    if (!g_input.seat) return;
    
    // TODO: Implement axis events
}

// إرسال حدث مفتاح
void input_handler_send_key_event(int key_code, int action, int modifiers) {
    uint32_t wayland_key = android_key_to_wayland(key_code);
    if (!wayland_key) {
        LOGI("Unknown key code: %d", key_code);
        return;
    }
    
    // تحديث المعدّلات
    g_input.key_modifiers = android_modifier_to_wayland(modifiers);
    input_handler_send_modifiers(g_input.key_modifiers);
    
    // إرسال حدث المفتاح
    if (action == INPUT_KEY_PRESS) {
        input_handler_send_key_press(wayland_key);
    } else {
        input_handler_send_key_release(wayland_key);
    }
    
    LOGI("Key event: code=%d (wayland=%d), action=%d", key_code, wayland_key, action);
}

// إرسال ضغطة مفتاح
void input_handler_send_key_press(uint32_t key_code) {
    if (!g_input.seat) return;
    winland_seat_notify_keyboard_key(g_input.seat, key_code, 1);
}

// إرسال إفلات مفتاح
void input_handler_send_key_release(uint32_t key_code) {
    if (!g_input.seat) return;
    winland_seat_notify_keyboard_key(g_input.seat, key_code, 0);
}

// إرسال المعدّلات
void input_handler_send_modifiers(uint32_t modifiers) {
    if (!g_input.seat) return;
    
    // TODO: Send modifier state to Wayland clients
}
