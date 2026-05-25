use jni::objects::JObject;
use jni::JNIEnv;

pub fn enable_fullscreen_immersive_mode(env: &mut JNIEnv, activity: &JObject) {
    let window = env.call_method(activity, "getWindow", "()Landroid/view/Window;", &[]).unwrap().l().unwrap();
    let decor_view = env.call_method(window, "getDecorView", "()Landroid/view/View;", &[]).unwrap().l().unwrap();
    let view_class = env.find_class("android/view/View").unwrap();

    let flag_fullscreen = env.get_static_field(&view_class, "SYSTEM_UI_FLAG_FULLSCREEN", "I").unwrap().i().unwrap();
    let flag_hide_navigation = env.get_static_field(&view_class, "SYSTEM_UI_FLAG_HIDE_NAVIGATION", "I").unwrap().i().unwrap();
    let flag_immersive_sticky = env.get_static_field(&view_class, "SYSTEM_UI_FLAG_IMMERSIVE_STICKY", "I").unwrap().i().unwrap();

    let flags = flag_fullscreen | flag_hide_navigation | flag_immersive_sticky;
    env.call_method(decor_view, "setSystemUiVisibility", "(I)V", &[jni::objects::JValue::from(flags)]).unwrap();
}

pub fn keep_screen_on(env: &mut JNIEnv, activity: &JObject) {
    let window = env.call_method(activity, "getWindow", "()Landroid/view/Window;", &[]).unwrap().l().unwrap();
    let layout_params_class = env.find_class("android/view/WindowManager$LayoutParams").unwrap();
    let flag_keep_screen_on = env.get_static_field(&layout_params_class, "FLAG_KEEP_SCREEN_ON", "I").unwrap().i().unwrap();

    env.call_method(window, "addFlags", "(I)V", &[jni::objects::JValue::from(flag_keep_screen_on)]).unwrap();
}
