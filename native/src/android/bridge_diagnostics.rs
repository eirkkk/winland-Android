use jni::{JNIEnv, objects::JClass};

#[no_mangle]
pub extern "system" fn Java_com_winland_server_NativeBridge_getBackendSnapshot(
    env: JNIEnv,
    _class: JClass,
) -> jni::sys::jstring {
    let snapshot = crate::android::command_channel::get_backend_snapshot();
    match env.new_string(snapshot) {
        Ok(s) => s.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "system" fn Java_com_winland_server_NativeBridge_getLastNativeError(
    env: JNIEnv,
    _class: JClass,
) -> jni::sys::jstring {
    let message = crate::android::bridge_init::get_latest_error();

    match message {
        Some(message) => match env.new_string(message) {
            Ok(value) => value.into_raw(),
            Err(_) => std::ptr::null_mut(),
        },
        None => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "system" fn Java_com_winland_server_NativeBridge_getWaylandRuntimeStats(
    env: JNIEnv,
    _class: JClass,
) -> jni::sys::jstring {
    let backend = crate::android::command_channel::get_backend_snapshot();
    let injector = crate::android::backend::wayland::seat_injector::snapshot_stats();
    let smithay = crate::android::command_channel::get_runtime_stats();
    let scroll = crate::android::command_channel::get_scroll_sensitivity();

    let summary = format!(
        "{} | injector(td={},tm={},tu={},tc={},kd={},ku={},tx={},gs={},last_ms={}) | scroll_sensitivity={} | {}",
        backend,
        injector.touch_down,
        injector.touch_move,
        injector.touch_up,
        injector.touch_cancel,
        injector.key_down,
        injector.key_up,
        injector.text_commit,
        injector.gesture_scroll,
        injector.last_event_time_ms,
        scroll,
        smithay
    );

    match env.new_string(summary) {
        Ok(s) => s.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}
