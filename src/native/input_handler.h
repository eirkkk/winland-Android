#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <wayland-server.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// أنواع أحداث اللمس
#define INPUT_TOUCH_DOWN    0
#define INPUT_TOUCH_UP      1
#define INPUT_TOUCH_MOVE    2
#define INPUT_TOUCH_CANCEL  3

// أنواع أحداث المفاتيح
#define INPUT_KEY_PRESS     0
#define INPUT_KEY_RELEASE   1

// معدّلات المفاتيح
#define INPUT_MODIFIER_SHIFT    (1 << 0)
#define INPUT_MODIFIER_CTRL     (1 << 1)
#define INPUT_MODIFIER_ALT      (1 << 2)
#define INPUT_MODIFIER_META     (1 << 3)

// هيكل معالج المدخلات
struct input_handler {
    struct winland_compositor *compositor;
    struct winland_seat *seat;
    
    // حالة المؤشر
    wl_fixed_t pointer_x;
    wl_fixed_t pointer_y;
    uint32_t pointer_buttons;
    
    // حالة اللمس
    int touch_active;
    int touch_id;
    wl_fixed_t touch_x;
    wl_fixed_t touch_y;
    
    // حالة لوحة المفاتيح
    uint32_t key_modifiers;
};

// دوال التهيئة والتدمير
int input_handler_init(struct winland_compositor *compositor);
void input_handler_terminate(void);

// دوال أحداث اللمس
void input_handler_send_touch_event(int action, float x, float y, int pointer_id);
void input_handler_send_touch_down(float x, float y, int pointer_id);
void input_handler_send_touch_up(float x, float y, int pointer_id);
void input_handler_send_touch_move(float x, float y, int pointer_id);

// دوال أحداث المؤشر
void input_handler_send_pointer_motion(wl_fixed_t x, wl_fixed_t y);
void input_handler_send_pointer_button(uint32_t button, uint32_t state);
void input_handler_send_pointer_axis(uint32_t axis, wl_fixed_t value);

// دوال أحداث لوحة المفاتيح
void input_handler_send_key_event(int key_code, int action, int modifiers);
void input_handler_send_key_press(uint32_t key_code);
void input_handler_send_key_release(uint32_t key_code);
void input_handler_send_modifiers(uint32_t modifiers);

// تحويل رموز Android إلى رموز Wayland
uint32_t android_key_to_wayland(int android_key_code);
uint32_t android_modifier_to_wayland(int android_modifiers);

#ifdef __cplusplus
}
#endif

#endif // INPUT_HANDLER_H
