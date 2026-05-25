use jni::{JNIEnv, objects::JClass};
use jni::sys::{jint, jfloat, jboolean};

#[no_mangle]
pub extern "system" fn Java_com_winland_server_NativeBridge_sendTouchEvent(
    _env: JNIEnv,
    _class: JClass,
    action: jint,
    id: jint,
    x: jfloat,
    y: jfloat,
) {
    crate::android::command_channel::send_command(
        crate::android::command_channel::JniCommand::TouchInput { action, id, x, y },
    );
}

#[no_mangle]
pub extern "system" fn Java_com_winland_server_NativeBridge_sendKeyEvent(
    _env: JNIEnv,
    _class: JClass,
    keycode: jint,
    is_down: jboolean,
) {
    crate::android::command_channel::send_command(
        crate::android::command_channel::JniCommand::KeyInput { keycode, is_down: is_down != 0 },
    );
}

#[no_mangle]
/// # Safety
///
/// This JNI entrypoint must be invoked by the JVM with a valid `JNIEnv` and
/// `JString` that is valid for the duration of this call.
pub unsafe extern "system" fn Java_com_winland_server_NativeBridge_sendTextInput(
    mut env: JNIEnv,
    _class: JClass,
    text: jni::objects::JString,
) {
    let input: String = match env.get_string(&text) {
        Ok(s) => s.into(),
        Err(_) => return,
    };

    crate::android::command_channel::send_command(
        crate::android::command_channel::JniCommand::TextInput { text: input },
    );
}

#[no_mangle]
pub extern "system" fn Java_com_winland_server_NativeBridge_setInputMode(
    _env: JNIEnv,
    _class: JClass,
    mode: jint,
) {
    crate::android::command_channel::send_command(
        crate::android::command_channel::JniCommand::SetInputMode { mode },
    );
}

#[no_mangle]
pub extern "system" fn Java_com_winland_server_NativeBridge_setScrollSensitivity(
    _env: JNIEnv,
    _class: JClass,
    value: jfloat,
) {
    crate::android::command_channel::send_command(
        crate::android::command_channel::JniCommand::SetScrollSensitivity { value },
    );
}

#[no_mangle]
pub extern "system" fn Java_com_winland_server_NativeBridge_setRelativeSensitivity(
    _env: JNIEnv,
    _class: JClass,
    value: jfloat,
) {
    crate::android::command_channel::send_command(
        crate::android::command_channel::JniCommand::SetRelativeSensitivity { value },
    );
}
